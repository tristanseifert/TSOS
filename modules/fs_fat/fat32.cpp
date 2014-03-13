/*
 * General FAT32 implementation, with long filename support.
 *
 * Some notes about FAT32:
 * - The first two CLUSTERS are reserved. The first two FAT entries thus have
 *	 special meanings: the first is 0xFFFFFFF8, the second contains dirty
 *	 volume flags.
 */
#import <module.h>
#import <fat32.hpp>

/*
 * Initialises a FAT32 filesystem from the specified partition table entry.
 */
fs_fat32::fs_fat32(hal_disk_partition_t *p, hal_disk_t *d) : hal_fs::hal_fs(p, d) {
	unsigned int err = 0;

	// Read sector 0 of partition synchronously
	if(!this->hal_fs::read_sectors(0, 1, &bpb, &err)) {
		#if PRINT_ERROR
		KERROR("Error reading BPB: %u", err);
		#endif
		return;
	}

	// Get data sector count
	root_dir_sectors = ((bpb.root_entry_count * 32) + (bpb.bytes_per_sector - 1)) / bpb.bytes_per_sector;

	num_data_sectors = bpb.total_sectors_32 - (bpb.reserved_sector_count + (bpb.table_count * bpb.table_size_32)) + root_dir_sectors;
	num_data_clusters = num_data_sectors / bpb.sectors_per_cluster;

	// Calculate size of a cluster (in bytes)
	cluster_size = bpb.sectors_per_cluster * bpb.bytes_per_sector;

	// Calculate address of first data sector
	first_data_sector = bpb.reserved_sector_count + (bpb.table_count * bpb.table_size_32) + root_dir_sectors;

	// Determine volume type
	if(num_data_clusters < 4085) {
		KERROR("Tried to initialise FAT12 volume as FAT32");
		return;
	} else if(num_data_clusters < 65525) { 
		KERROR("Tried to initialise FAT16 volume as FAT32");
		return;
	}

	// Read FSINFO sector
	if(!this->hal_fs::read_sectors(bpb.fat_info, 1, &fs_info, &err)) {
		#if PRINT_ERROR
		KERROR("Error reading FSInfo: %u", err);
		#endif
		return;
	}

	// Verify FSInfo struct
	if(fs_info.signature != 0x41615252 || fs_info.signature2 != 0x61417272 || fs_info.trailSig != 0xAA550000) {
		KWARNING("Corrupted FSInfo: 0x%08X 0x%08X 0x%08X", 
			(unsigned int) fs_info.signature, (unsigned int) fs_info.signature2,
			(unsigned int) fs_info.trailSig);
	} else {
		KDEBUG("%u clusters, %u free, start search at %u",
			(unsigned int) num_data_clusters,
			(unsigned int) fs_info.last_known_free_sec_cnt,
			(unsigned int) fs_info.free_cluster_search_start);

		fs_info.free_cluster_search_start = 0;

		// Set up pointer to legacy volume label
		volumeLabel = (char *) kmalloc(16);
		memcpy(volumeLabel, &bpb.volume_label, 11);

		// Trim spaces at the end
		for(unsigned int i = 10; i > 0; i--) {
			if(volumeLabel[i] == ' ') {
				volumeLabel[i] = 0x00;
			} else {
				break;
			}
		}
	}

	// Allocate more memory for the filesystem
	fatBuffer = (uint32_t *) kmalloc(cluster_size);
	freeClusterBuffer = (uint32_t *) kmalloc(cluster_size);
	clusterBuffer = kmalloc(cluster_size);

	dirHandleCache = hashmap_allocate();

	// Read FAT sector 0 to get dirty flags
	if(!this->hal_fs::read_sectors(bpb.reserved_sector_count, 1, fatBuffer, &err)) {
		#if PRINT_ERROR
		KERROR("%s: Error reading FAT: %u", __PRETTY_FUNCTION__, err);
		#endif
		return;
	} else {
		fs_clealyUnmounted = (fatBuffer[1] & FAT32_VOLUME_DIRTY_MASK);

		if(!fs_clealyUnmounted) {
			KWARNING("Filesystem not cleanly unmounted after last use!");
		}
	}

	// Read root directory
	this->read_root_dir();

	KSUCCESS("Volume initialised.");
}

/*
 * Clean up the filesystem's internal data structures, and cleanly unmount it.
 */
fs_fat32::~fs_fat32() {

}

/*
 * Read the root directory from the disk, bypassing caching.
 */
void fs_fat32::read_root_dir(void) {
	unsigned int err;

	// Release previous root directory, if it exists
	if(root_directory) {
		fs_directory_t *new_root = hal_vfs_allocate_directory(false);

		hal_vfs_deallocate_directory(root_directory, new_root);

		root_directory = new_root;
	} else {
		root_directory = hal_vfs_allocate_directory(true);
	}

	// Follow the cluster chain for the root directory.
	unsigned int *root_clusters = this->clusterChainForCluster(bpb.root_cluster & FAT32_MASK);
	unsigned int cnt = 0;

	while(root_clusters[cnt] != FAT32_END_CHAIN) {
		cnt++;
	}

	// Allocate memory for root directory
	unsigned int root_dir_len = cnt * cluster_size;
	unsigned int root_dir_num_entries = root_dir_len / sizeof(fat_dirent_t);

	fat_dirent_t *buffer = (fat_dirent_t *) kmalloc(root_dir_len);

	// Read the root directory's sectors.
	cnt = 0;

	while(true) {
		if(root_clusters[cnt] != FAT32_END_CHAIN) {
			unsigned int cluster = root_clusters[cnt];

			if(!this->readCluster(cluster, ((uint8_t *) buffer) + (cnt * cluster_size), &err)) {
				#if PRINT_ERROR
				KERROR("Error reading root dir cluster %u: %u", cluster, err);
				#endif
				return;
			}
		} else {
			break;
		}

		cnt++;
	}

	// Process the read FAT directory entries into fs_file_t structs
	this->processFATDirEnt(buffer, root_dir_num_entries, root_directory);

	// Set root directory's cluster
	root_directory->i.userData = bpb.root_cluster & FAT32_MASK;

	// Clean up temporary buffers needed to read root directory
	kfree(root_clusters);
	kfree(buffer);
}

/*
 * Reads the directory file (index of files in the directory) of a directory.
 */
fat_dirent_t *fs_fat32::read_dir_file(fs_directory_t *parent, char *childName, unsigned int *entries) {
	fs_directory_t *target = NULL;

	// Locate the child
	for(unsigned int i = 0; i < parent->children->num_entries; i++) {
		fs_directory_t *dir = (fs_directory_t *) list_get(parent->children, i);

		// Ignore non-directory files
		if(dir->i.type == kFSItemTypeDirectory) {
			if(!strcasecmp(childName, dir->i.name)) {
				target = dir;
				break;
			}
		}
	}

	if(target) {
		// Read cluster chain
		unsigned int *chain = this->clusterChainForCluster(target->i.userData & FAT32_MASK);
		unsigned int cnt = 0;
		unsigned int err = 0;

		while(chain[cnt] != FAT32_END_CHAIN) {
			cnt++;
		}

		// Allocate required buffer
		unsigned int dir_length = cnt * cluster_size;
		*entries = dir_length / sizeof(fat_dirent_t);

		fat_dirent_t *buffer = (fat_dirent_t *) kmalloc(dir_length);

		// Perform read
		cnt = 0;

		while(true) {
			if(chain[cnt] != FAT32_END_CHAIN) {
				unsigned int cluster = chain[cnt];

				if(!this->readCluster(cluster, ((uint8_t *) buffer) + (cnt * cluster_size), &err)) {
					#if PRINT_ERROR
					KERROR("Error reading directory file %u: %u", cluster, err);
					#endif
					return NULL;
				}
			} else {
				break;
			}

			cnt++;
		}

		return buffer;
	}

	// Directory not found
	#if DEBUG_FILE_NOT_FOUND
	KERROR("Could not find directory %s", childName);
	#endif

	return NULL;
}

/*
 * Reads a directory, and constructs an fs_directory_t object for it.
 */
fs_directory_t *fs_fat32::read_directory(fs_directory_t *dir, char *name, bool cache, char *fullpath) {
	ASSERT(dir);

	fs_directory_t *directory = NULL;
	fat_dirent_t *dirBuf = NULL;
	unsigned int dirBufEntries = 0;

	// If caching is enabled, check if this directory's been read
	hal_handle_t handle = (hal_handle_t) hashmap_get(dirHandleCache, fullpath);

	// Verify the handle is valid
	if(handle) {
		if(hal_handle_get_type(handle) != kFSItemTypeDirectory) {
			handle = 0;
			goto processHandle;
		}

		// Get object from the handle and verify it is good
		directory = (fs_directory_t *) hal_handle_get_object(handle);
		if(directory->i.type != kFSItemTypeDirectory) {
			handle = 0;
		}
	}

	processHandle: ;

	// No handle? Perform directory read.	
	if(!handle) {
		// Could we read the child dir?
		if(!(dirBuf = this->read_dir_file(dir, name, &dirBufEntries))) {
			return NULL;
		}

		// Convert to directory handle
		fs_directory_t *currentDir = hal_vfs_allocate_directory(true);
		this->processFATDirEnt(dirBuf, dirBufEntries, currentDir);

		kfree(dirBuf);

		currentDir->parent = dir->i.handle;

		if(cache) {
			hashmap_insert(dirHandleCache, fullpath, (void *) currentDir->i.handle);

			#if DEBUG_DIRECTORY_CACHING
			KDEBUG("dir_cache add: %s: 0x%08X", fullpath, (unsigned int) currentDir->i.handle);
			#endif
		}

		directory = currentDir;

		// Increment cache count
		directory->i.cache_accesses++;
	}

	// Return directory
	return directory;
}

fs_directory_t* fs_fat32::list_directory(char* dirname, bool cache) {
	fs_directory_t *directory = root_directory;

	char *currentPath = (char *) kmalloc(strlen(dirname) + 2);
	unsigned int currentPathOffset = 0;

	currentPath[currentPathOffset++] = '/';

	// Separate path string
	list_t *components = this->split_path(dirname);

	// Iterate over each component
	for(unsigned int i = 0; i < components->num_entries; i++) {
		// Append path name
		char *component = (char *) list_get(components, i);

		strcat(currentPath + currentPathOffset, component);
		currentPathOffset += strlen(component);
		currentPath[currentPathOffset++] = '/';

		// Get the directoryn
		if(!(directory = this->read_directory(directory, component, cache, currentPath))) {
			return NULL;
		}
	}

	// Clean up
	kfree(currentPath);
	kfree(list_get(components, 0));
	list_destroy(components, false);

	return directory;
}

/*
 * Converts FAT32-style directory entries into fat_file_t or fat_directory_t
 * objects, and adds them as children of the specified directory.
 */
void fs_fat32::processFATDirEnt(fat_dirent_t *entries, unsigned int number, fs_directory_t *root) {
	// Sanity checking
	ASSERT(root);
	ASSERT(entries);

	if(!number) {
		return;
	}

	fat_dirent_t *entry;
	fs_item_t *item;

	// Long name buffer
	bool foundLongName = false;
	unsigned int longname_num = 0;
	uint8_t longname_checksum = 0;

	struct {
		uint16_t name1[5]; // characters 1-5
		uint16_t name2[6]; // characters 6-11
		uint16_t name3[2]; // characters 12-13
	} longname_buffer[0x3F];

	// Loop over all entries
	for(unsigned int i = 0; i < number; i++) {
		entry = &entries[i];

		// Is this entry usable?
		if(entry->name[0] != 0xE5 && entry->name[0] != 0x00) {
			// Clear state
			item = NULL;

			// Long Filenames
			if((entry->attributes & FAT_ATTR_LFN) == FAT_ATTR_LFN) {
				// Get longname entry
				fat_longname_dirent_t *ln = (fat_longname_dirent_t *) entry;

				// Ignore invalid longname entries
				if(ln->type == 0) {
					foundLongName = true;

					unsigned int longname_offset = (ln->order & 0x3F) - 1;

					// Last entry?
					if(ln->order & 0x40) {
						longname_num = longname_offset + 1;
					}

					// Copy strings
					unsigned int c = 0;

					for(c = 0; c < 5; c++){
						longname_buffer[longname_offset].name1[c] = ln->name1[c];
					}

					for(c = 0; c < 6; c++){
						longname_buffer[longname_offset].name2[c] = ln->name2[c];
					}

					for(c = 0; c < 2; c++){
						longname_buffer[longname_offset].name3[c] = ln->name3[c];
					}

					// Copy checksum
					longname_checksum = ln->checksum;
				}
			} else if(entry->attributes & FAT_ATTR_DIRECTORY) { // directory
				char *name = fs_fat32::dirent_get_8_3_name(entry);

				// Ignore dot and dotdot
				if(strcmp(".", name) && strcmp("..", name)) {
					// Lowercase directory name?
					if((entry->nt_reserved & 0x08) || (entry->nt_reserved & 0x10)) {
						for(unsigned int c = 0; c < 11; c++) {
							name[c] = tolower(name[c]);
						}
					}

					fs_directory_t *dir = hal_vfs_allocate_directory(true);
					dir->i.name = name;
					dir->parent = root->i.handle;
					item = &dir->i;

					// Add directory as child
					list_add(root->children, dir);
				}
			} else if(entry->attributes & FAT_ATTR_VOLUME_ID) { // Volume label
				if(volumeLabel) {
					memclr(volumeLabel, 16);
				}

				memcpy(volumeLabel, &entry->name, 11);

				// Trim spaces at the end
				for(unsigned int i = 10; i > 0; i--) {
					if(volumeLabel[i] == ' ') {
						volumeLabel[i] = 0x00;
					} else {
						break;
					}
				}
			} else { // regular file
				// Allocate a file object
				fs_file_t *file = hal_vfs_allocate_file(root);
				file->size = entry->filesize;

				unsigned int c = 0;

				// Handle long name
				if(foundLongName) {
					// Allocate memory
					char *newName = (char *) kmalloc((longname_num * 13) + 1);
					unsigned int newNameLen = 0;

					// Copy the individual characters
					if(longname_checksum == this->lfnCheckSum((unsigned char *) &entry->name)) {
						for(unsigned int l = 0; l < longname_num; l++) {
							// Copy characters
							for(c = 0; c < 5; c++){
								if(longname_buffer[l].name1[c]) {
									newName[newNameLen++] = longname_buffer[l].name1[c];
								} else {
									goto longname_done;
								}
							}

							for(c = 0; c < 6; c++){
								if(longname_buffer[l].name2[c]) {
									newName[newNameLen++] = longname_buffer[l].name2[c];
								} else {
									goto longname_done;
								}
							}

							for(c = 0; c < 2; c++){
								if(longname_buffer[l].name3[c]) {
									newName[newNameLen++] = longname_buffer[l].name3[c];
								} else {
									goto longname_done;
								}
							}
						}

						longname_done: ;
					}

					// Free old name and save new
					kfree(file->i.name);
					file->i.name = newName;

					longname_checksum = longname_num = 0;
					foundLongName = false;
				} else {
					// Lowercase basename?
					if(entry->nt_reserved & 0x08) {
						for(c = 0; c < 8; c++) {
							entry->name[c] = tolower(entry->name[c]);
						}
					} 

					// Lowercase extension?
					if(entry->nt_reserved & 0x10) {
						for(c = 0; c < 3; c++) {
							entry->ext[c] = tolower(entry->ext[c]);
						}
					}

					// Regular shortname
					char *name = fs_fat32::dirent_get_8_3_name(entry);
					file->i.name = name;
				}

				// Save the item
				item = &file->i;
			}

			// Set flags
			if(item) {
				item->is_hidden = (entry->attributes & FAT_ATTR_HIDDEN);
				item->is_system = (entry->attributes & FAT_ATTR_SYSTEM);
				item->is_readonly = (entry->attributes & FAT_ATTR_READ_ONLY);

				// Convert the timestamps
				item->time_created = this->convert_timestamp(entry->created_date, entry->created_time, 0);
				item->time_written = this->convert_timestamp(entry->write_date, entry->write_time, 0);
				item->time_created = this->convert_timestamp(entry->accessed_date, 0, 0);

				/*
				 * To speed up file reads, store the first cluster of this file
				 * in the low 32 bits of the userData field of the item.
				 */
				item->userData = (entry->cluster_high << 16) | (entry->cluster_low);
			}
		} else if(entry->name[0] == 0x00) {
			// Byte 0 being 0x00 indicates the entry is free.
			goto done;
		}
	}

	// Perform some cleanup after the conversion finishes.
	done: ;
}

/*
 * Calculates the checksum for a long name.
 */
uint8_t fs_fat32::lfnCheckSum(unsigned char *shortName) {
	uint16_t FcbNameLen;
	uint8_t sum = 0;

	for(FcbNameLen = 11; FcbNameLen != 0; FcbNameLen--) {
		// NOTE: The operation is an unsigned char rotate right
		sum = ((sum & 1) ? 0x80 : 0) + (sum >> 1) + *shortName++;
	}

	return sum;
}

/*
 * Converts from the FAT timestamp format (separate date, time, and millis) to
 * more acceptable, sane, (and indubitably) less shitty UNIX epoch.
 */
time_t fs_fat32::convert_timestamp(uint16_t date, uint16_t time, uint8_t millis) {
	time_t t = 315532800;

	// Process date
	if(date) {
		unsigned int month = (date & 0x1E0) >> 5;
		unsigned int year = (date & 0xFE00) >> 9;

		t += (date & 0x1F) * 86400; // day
	}

	// Process time
	if(time) {
		t += (time & 0x1F) * 2; // stored as multiples of twos
		t += ((time & 0x7E0) >> 5) * 60;
		t += ((time & 0xF800) >> 11) * 3600;
	}

	// Milliseconds
	if(millis) {
		if(millis > 99) t++;
	}

	return t;
}

/*
 * Calculates a cluster's offset into the FAT. The returned structure indicates
 * the sector to read, and the dword offset into that sector. In other words,
 * if the sector is read as an array of bytes, the offset must be multiplied
 * by four.
 */
fat32_secoff_t fs_fat32::fatEntryOffsetForCluster(unsigned int cluster) {
	fat32_secoff_t offset;

	// Number of FAT entries per cluster
	unsigned int entries_per_cluster = cluster_size / 4;

	// Determine size of FAT and normalise cluster
	offset.sector = bpb.reserved_sector_count + (cluster / entries_per_cluster);
	offset.offset = cluster % entries_per_cluster;

	return offset;
}

/*
 * Follows the cluster chain to find all clusters on which the specified
 * starting cluster has data. This *can* be slow.
 *
 * Note that the array returned is terminated by FAT32_END_CHAIN.
 */
unsigned int *fs_fat32::clusterChainForCluster(unsigned int cluster) {
	unsigned int chain_len = 32;
	unsigned int chain_offset = 0;
	unsigned int *chain = (unsigned int *) kmalloc(sizeof(unsigned int) * chain_len);
	unsigned int err;

	fat32_secoff_t off;

	// Guard against broken FS implementations
	if(cluster == 0) {
		cluster = 2;
	}

	// Place the initial cluster as the first entry in the chain.
	chain[chain_offset++] = cluster;

	// Repeat until the end is reached (marked by FAT32_END_CHAIN)
	unsigned int nextCluster = cluster;
	while(nextCluster != FAT32_END_CHAIN) {
		// Read the FAT for this sector
		off = this->fatEntryOffsetForCluster(nextCluster);

		// Read out cluster
		if(this->hal_fs::read_sectors(off.sector, bpb.sectors_per_cluster, fatBuffer, &err)) {
			nextCluster = fatBuffer[off.offset];
		} else { // read error?
			#if PRINT_ERROR
			KERROR("Couldn't read sector %u for FAT", off.sector);
			#endif

			goto error;
		}

		// Is this the end of the chain?
		if(nextCluster >= FAT32_END_CHAIN) {
			nextCluster = FAT32_END_CHAIN;
		}

		// Expand chain buffer
		if(chain_offset == chain_len) {
			chain_len += 32;
			chain = (unsigned int *) krealloc(chain, sizeof(unsigned int) * chain_len);
		}

		// Write into chain array
		chain[chain_offset++] = nextCluster & FAT32_MASK;
	}

	return chain;

	// Handle an error condition
	error: ;
	kfree(chain);
	return NULL;
}

/*
 * Reads the specified cluster.
 */
void *fs_fat32::readCluster(unsigned int cluster, void *buffer, unsigned int *error) {
	unsigned int sector = ((cluster - 2) * bpb.sectors_per_cluster) + first_data_sector;

	if(!this->hal_fs::read_sectors(sector, bpb.sectors_per_cluster, buffer, error)) {
		return NULL;
	}

	return buffer;
}

/*
 * Reads the specified cluster.
 */
void *fs_fat32::writeCluster(unsigned int cluster, void *buffer, unsigned int *error) {
	unsigned int sector = ((cluster - 2) * bpb.sectors_per_cluster) + first_data_sector;

	if(!this->hal_fs::write_sectors(sector, bpb.sectors_per_cluster, buffer, error)) {
		return NULL;
	}

	return buffer;
}

/*
 * Gets a pointer to a VFS file object
 */
fs_file_handle_t* fs_fat32::get_file_handle(char *name, fs_file_open_mode_t mode) {
	searchAgain: ;
	// Store address of final file
	fs_file_t *file = NULL;
	fs_directory_t *dir = NULL;
	char *fileName = NULL;

	// Allocate memory for directory
	char *dirName = (char *) kmalloc(strlen(name));
	unsigned int dirNameOffset = 0;

	dirName[dirNameOffset++] = '/';

	// Separate path string
	list_t *components = this->split_path(name);

	// If only one element exists, it's at the root level
	if(components->num_entries > 1) {
		// Get the base directory name
		for(unsigned int i = 0; i < components->num_entries-1; i++) {
			char *component = (char *) list_get(components, i);

			strcat(dirName + dirNameOffset, component);
			dirNameOffset += strlen(component);
			dirName[dirNameOffset++] = '/';
		}

		fileName = (char *) list_get(components, components->num_entries - 1);

		// Try to get directory
		dir = this->list_directory(dirName, true);
	} else {
		fileName = (char *) list_get(components, 0);
		dir = root_directory;
	}

	// Directory found, search for file.
	if(dir) {
		// Locate the child
		for(unsigned int i = 0; i < dir->children->num_entries; i++) {
			file = (fs_file_t *) list_get(dir->children, i);

			// Ignore non-directory files
			if(file->i.type == kFSItemTypeFile) {
				if(!strcasecmp(fileName, file->i.name)) {
					goto fileFound;
				}
			}
		}

		// We need to create the file if requested and it didn't exist
		if(mode & kFSFileModeCreate) {
			int crtErr = 0;
			if(!(crtErr = this->createEmptyFile(dir, fileName))) {
				goto searchAgain;
			} else {
				#if PRINT_ERROR
				KERROR("Could not create %s: %i", name, crtErr);
				#endif
			}
		}

		goto notFound;
	} else {
		goto notFound;
	}

	// File not found
	notFound: ;
	#if DEBUG_FILE_NOT_FOUND
	KERROR("Couldn't find '%s' in dir '%s'", fileName, dirName);
	#endif

	kfree(dirName);
	kfree(list_get(components, 0));
	list_destroy(components, false);

	return NULL;

	// File found
	fileFound: ;

	// Clean up
	kfree(dirName);
	kfree(list_get(components, 0));
	list_destroy(components, false);

	// Open a file handle object
	fs_file_handle_t *handle = (fs_file_handle_t *) kmalloc(sizeof(fs_file_handle_t));

	handle->file = file->i.handle;
	handle->can_seek = true;
	handle->position = 0;

	handle->isOpen = true;

	// Increment file's cache reference
	file->i.cache_accesses++;

	return handle;
}

/*
 * Performs a read operation from the file this file handle is opened on,
 * reading num_bytes bytes into buffer, starting at the current location of
 * the file handle.
 *
 * This function returns between zero and LONG_LONG_MAX 
 */
long long fs_fat32::read_handle(fs_file_handle_t *h, size_t bytes, void *buffer) {
	unsigned int err = 0;

	// Get the file object associated with it
	fs_file_t *fileObj = (fs_file_t *) hal_handle_get_object(h->file);

	// Verify the file object is still valid
	if(fileObj->i.type != kFSItemTypeFile) {
		h->isOpen = false;
		return -1;
	}

	// Is there any data available? (not EOF)
	if(h->position == fileObj->size) {
		return 0;
	}

	// Determine starting cluster for this file (from userData)
	unsigned int start_cluster = (fileObj->i.userData & FAT32_MASK);

	// Get the number of clusters into the file to read
	unsigned int clusters_in = h->position / cluster_size;
	unsigned int cluster_offset = h->position % cluster_size;

	// Read cluster chain
	unsigned int *chain = this->clusterChainForCluster(start_cluster);
	unsigned int chain_len = (fileObj->size + (cluster_size - 1)) / cluster_size;

	#if DEBUG_READ
	KDEBUG("File size %u, chain %u entries", (unsigned int) fileObj->size, chain_len);
	#endif

	// Calculate how many bytes we can actually read
	size_t bytes_to_read = 0;
	long long bytes_read = 0;

	// Reading more than the file has?
	if(fileObj->size - h->position < bytes) {
		bytes_to_read = fileObj->size - h->position;
	} else {
		bytes_to_read = bytes;
	}

	// Get number of clusters to read
	unsigned int clusters_to_read = (bytes_to_read + (cluster_size - 1)) / cluster_size;

	// Pointer to the current location in the outbuf
	uint8_t *outbuf = (uint8_t *) buffer;

	// Begin the process of reading
	for(unsigned int c = cluster_offset; c < clusters_to_read + cluster_offset; c++) {
		// Read cluster
		unsigned int cluster = chain[c];

		#if DEBUG_READ
		KDEBUG("Cluster %u: 0x%08X, %u bytes left", c, cluster, (unsigned int) bytes_to_read);
		#endif

		if(!this->readCluster(cluster, clusterBuffer, &err)) {
			#if PRINT_ERROR
			KERROR("Cluster read error: %u", err);
			#endif

			goto done;
		}

		// We needn't read any more clusters
		if(bytes_to_read <= cluster_size) {
			#if DEBUG_READ
			KDEBUG("Read %u bytes", (unsigned int) bytes_to_read);
			#endif

			bytes_read += bytes_to_read;
			memcpy(outbuf, clusterBuffer, bytes_to_read);

			bytes_to_read = 0;
			goto done;
		} else {
			#if DEBUG_READ
			KDEBUG("Read %u bytes", (unsigned int) cluster_size);
			#endif

			// We read an entire cluster
			bytes_to_read -= cluster_size;
			memcpy(outbuf, clusterBuffer, cluster_size);	

			// Account for the amount of bytes read
			bytes_read += cluster_size;
			outbuf += cluster_size;
		}
	}

	done: ;
	#if DEBUG_READ
	KDEBUG("Total bytes read: %u", (unsigned int) bytes_read);
	#endif

	// Adjust file pointer
	h->position += bytes_read;

	return bytes_read;
}

/*
 * Finds the specified number of free clusters.
 */
unsigned int *fs_fat32::findFreeClusters(unsigned int numClusters) {
	// Allocate a buffer
	unsigned int *buf = (unsigned int *) kmalloc(sizeof(unsigned int) * (numClusters + 1));
	unsigned int bufOffset = 0;

	unsigned int err = 0;

	// Begin search at the offset in the FSInfo structure
	unsigned int currentCluster = fs_info.free_cluster_search_start;
	fat32_secoff_t off = this->fatEntryOffsetForCluster(currentCluster);

	unsigned int currentFATSector = off.sector;
	unsigned int currentFATOffset = off.offset;

	// Read the initial sector
	if(!this->hal_fs::read_sectors(currentFATSector, 1, freeClusterBuffer, &err)) {
		#if PRINT_ERROR
		KERROR("%s: Error reading FAT: %u", __PRETTY_FUNCTION__, err);
		#endif

		kfree(buf);
		return NULL;
	}

	while(true) {
		// Crossed the cluster boundary?
		if(currentFATOffset++ > (cluster_size / 4)) {
			currentFATSector++;
			currentFATOffset = 0;

			// Check we're not past the end of the FAT
			if(currentFATSector >= (bpb.table_count * bpb.table_size_32)) {
				goto error;
			} else {
				// Read the new FAT cluster otherwise
				if(!this->hal_fs::read_sectors(currentFATSector, 1, freeClusterBuffer, &err)) {
					#if PRINT_ERROR
					KERROR("%s: Error reading FAT: %u", __PRETTY_FUNCTION__, err);
					#endif

					kfree(buf);
					return NULL;
				}
			}
		}

		// Is this cluster free?
		if(!(freeClusterBuffer[currentFATOffset] & FAT32_MASK)) {
			currentCluster = ((currentFATSector - bpb.reserved_sector_count) * (cluster_size / 4)) + currentFATOffset;

			buf[bufOffset++] = currentCluster;

			// Check if enough clusters were found
			if(bufOffset >= numClusters) {
				return buf;
			}
		}
	}

	return buf;

	error: ;
	#if PRINT_ERROR
	KERROR("Couldn't find %u free clusters", numClusters);
	#endif

	kfree(buf);
	return NULL;
}

/*
 * Merges two cluster chains: Begins the insertion process at first_cluster,
 * replacing an end-of-file mark or zero.
 *
 * If the original chain has only one entry and this entry is not used, the
 * second chain is simply inserted starting at the specified position.
 *
 * This function will fail if attempts are made to assign a cluster in the FAT
 * that has a non-zero value, UNLESS it is an end-of-chain marker. This means
 * that assigning "broken" clusters will cause this function to fail.
 *
 * An important requirement here is that chain MUST be terminated with the FAT
 * end-of-chain marker, as that is needed to both complete the table, and for
 * the function to recognise the end of the chain.
 */
int fs_fat32::update_fat(unsigned int first_cluster, unsigned int *chain) {
	unsigned int err = 0;

	// Read first_cluster's FAT entry
	fat32_secoff_t off = this->fatEntryOffsetForCluster(first_cluster);
	
	if(!this->hal_fs::read_sectors(off.sector, 1, fatBuffer, &err)) {
		#if PRINT_ERROR
		KERROR("%s: Error reading FAT: %u", __PRETTY_FUNCTION__, err);
		#endif
		return -2;
	}

	// Is it an end-of-file marker?
	if((fatBuffer[off.offset] & FAT32_MASK) < FAT32_END_CHAIN && (fatBuffer[off.offset] & FAT32_MASK)) {
		#if PRINT_ERROR
		KERROR("%u is not the end of a chain nor 0, cannot append chain 0x%08X (Read 0x%08X)", 
			first_cluster, (unsigned int) chain, (unsigned int) fatBuffer[off.offset]);
		#endif

		return -3;
	}

	if(chain) {
		unsigned int chain_off = 0;
		fatBuffer[off.offset] = chain[chain_off++];

		// Write back to device
		if(!this->hal_fs::write_sectors(off.sector, 1, fatBuffer, &err)) {
			#if PRINT_ERROR
			KERROR("%s: Error writing FAT: %u", __PRETTY_FUNCTION__, err);
			#endif
			return -2;
		}

		// Process the rest of the chain
		while(chain[chain_off] < FAT32_END_CHAIN) {
			off = this->fatEntryOffsetForCluster(first_cluster);
			
			if(!this->hal_fs::read_sectors(off.sector, 1, fatBuffer, &err)) {
				#if PRINT_ERROR
				KERROR("%s: Error reading FAT: %u", __PRETTY_FUNCTION__, err);
				#endif
				return -2;
			}

			// Update FAT and write to device			
			fatBuffer[off.offset] = chain[chain_off++];

			if(!this->hal_fs::write_sectors(off.sector, 1, fatBuffer, &err)) {
				#if PRINT_ERROR
				KERROR("%s: Error writing FAT: %u", __PRETTY_FUNCTION__, err);
				#endif
				return -2;
			}
		}
	} else {
		fatBuffer[off.offset] = FAT32_END_CHAIN;

		// Write back to device
		if(!this->hal_fs::write_sectors(off.sector, 1, fatBuffer, &err)) {
			#if PRINT_ERROR
			KERROR("%s: Error writing FAT: %u", __PRETTY_FUNCTION__, err);
			#endif
			return -2;
		}
	}


	return 0;
}

/*
 * Creates an empty file in the specified directory. A single cluster of data
 * is reserved, but zeroed, and a directory entry (and, if required, LFN
 * entries) is created.
 */
int fs_fat32::createEmptyFile(fs_directory_t *dir, char *in_name) {
	unsigned int err = 0;

	// Create a copy of the name
	char *name = (char *) kmalloc(strlen(in_name) + 2);
	strncpy(name, in_name, strlen(in_name) + 2);

	// Separate extension and filename
	size_t name_length = strlen(name);
	char *extension = NULL;

	// Find the extension
	for(unsigned int c = name_length; c > 0; c--) {
		if(name[c] == '.') {
			name[c] = 0x00;
			extension = name + (c + 1);
			break;
		}
	}

	// Determine if an LFN entry is needed
	bool lfnNeeded = false;

	// First, check length of the basename and extension
	lfnNeeded = (strlen(name) > 8) || (strlen(extension) > 3);

	string_case_t nameCase = this->getStringCase(name);
	string_case_t extCase = this->getStringCase(extension);

	// Check based on mixed caseness
	if(!lfnNeeded) {
		if(nameCase == kStringCaseMixed || extCase == kStringCaseMixed) {
			lfnNeeded = true;
			KDEBUG("LFN needed due to mixed case");
		}
	}

	// If there's no LFN needed, we just need a single directory entry.
	unsigned int dir_entries_needed = 0;

	if(!lfnNeeded) {
		dir_entries_needed = 1;
	} else {
		dir_entries_needed = 1;

		// Remove the zero terminator where the period used to be
		extension[-1] = '.';

		/*
		 * Each LFN entry is capable of holding 13 characters, so calculate how
		 * many of them are needed to express this string.
		 */
		dir_entries_needed += (name_length + (13 - 1)) / 13;
	}

	// Find the clusters this directory occupies
	unsigned int directory_cluster = dir->i.userData & FAT32_MASK;
	
	unsigned int *dir_chain = this->clusterChainForCluster(bpb.root_cluster & FAT32_MASK);
	unsigned int dir_chain_len = 0;

	while(dir_chain[dir_chain_len] != FAT32_END_CHAIN) {
		dir_chain_len++;
	}

	// Allocate a buffer and read in the directory
	fat_dirent_t *dirBuf = (fat_dirent_t *) kmalloc(dir_chain_len * cluster_size);

	for(unsigned int c = 0; c < dir_chain_len; c++) {
		unsigned int cluster = dir_chain[c];

		if(!this->readCluster(cluster, ((uint8_t *) dirBuf) + (c * cluster_size), &err)) {
			#if PRINT_ERROR
			KERROR("Error reading directory cluster %u: %u", cluster, err);
			#endif

			kfree(dirBuf);
			kfree(dir_chain);
			kfree(name);
			return -2;
		}		
	}

	// Points to the first consecutive empty directory entry
	fat_dirent_t *start_dirent = NULL;
	unsigned int start_dirent_offset = 0;

	/*
	 * First, loop through all entries and see if one starts with 0xE5, or 0x00
	 * which indicate that the entry is free or it's the end of the directory,
	 * respectively. If we only need a single entry and we find enough, we're
	 * done and can insert our item.
	 */
	for(unsigned int i = 0; i < (dir_chain_len * (cluster_size / sizeof(fat_dirent_t))); i++) {
		fat_dirent_t *d = &dirBuf[i];

		// Handle the case of where only a single entry is needed
		if(dir_entries_needed == 1) {
			if(d->name[0] == 0xE5 || d->name[0] == 0x00) {
				// Ensure it's not an LFN
				if((d->attributes & FAT_ATTR_LFN) != FAT_ATTR_LFN) {
					start_dirent = d;
					start_dirent_offset = i;
					goto writeDirEnt;
				}
			}
		}
	}

	/*
	 * We couldn't find any free items, and there's more than one needed, so
	 * re-arrange the directory structure to fill gaps and see if there is any
	 * space now.
	 */

	/*
	 * Gap closure didn't help, so just add another cluster to the end of the
	 * directory.
	 */

	/*
	 * Since the correct number of directory entries was found above, we can
	 * assemble the fs_dirent_t, and optionally, fat_longname_dirent_t structs.
	 */
	writeDirEnt: ;
	unsigned int shortNameOffset = 0;

	if(dir_entries_needed > 1) {
		shortNameOffset = dir_entries_needed - 1;
	}

	// Clear the directory entries' memory
	memclr(start_dirent, sizeof(fat_dirent_t) * dir_entries_needed);

	// If not an LFN-needing one, write original filename
	if(!lfnNeeded) {
			// Copy name
		bool hasFoundStringTerminator = false;
		for(unsigned int c = 0; c < 8; c++) {
			unsigned char character = ' ';

			if(name[c] && !hasFoundStringTerminator) {
				character = name[c];
			} else {
				hasFoundStringTerminator = true;
				character = ' ';
			}

			start_dirent[shortNameOffset].name[c] = toupper(character);
		}

		// Copy extension
		hasFoundStringTerminator = false;
		for(unsigned int c = 0; c < 3; c++) {
			unsigned char character = ' ';

			if(extension[c] && !hasFoundStringTerminator) {
				character = extension[c];
			} else {
				hasFoundStringTerminator = true;
				character = ' ';
			}

			start_dirent[shortNameOffset].ext[c] = toupper(character);
		}

		// Set the "Reserved for NT" extension bits
		if(nameCase == kStringCaseLower) {
			start_dirent[shortNameOffset].nt_reserved |= 0x08;
		}

		if(extCase == kStringCaseLower) {
			start_dirent[shortNameOffset].nt_reserved |= 0x10;
		}
	}

	// Give it a bullshit size
	start_dirent[shortNameOffset].filesize = cluster_size;

	// Allocate a cluster
	unsigned int *chain = this->findFreeClusters(1);
	// KDEBUG("Free cluster for file: 0x%08X", chain[0]);

	if((err = this->update_fat(chain[0], NULL))) {
		#if PRINT_ERROR
		KERROR("%s: Error writing file cluster: %i", __PRETTY_FUNCTION__, (int) err);
		#endif

		kfree(chain);
		kfree(dirBuf);
		kfree(dir_chain);
		kfree(name);

		return -3;
	}

	start_dirent[shortNameOffset].cluster_low = chain[0] & 0x0000FFFF;
	start_dirent[shortNameOffset].cluster_high = (chain[0] & 0xFFFF0000) >> 16;

	start_dirent[shortNameOffset].attributes = FAT_ATTR_ARCHIVE;

	// Insert LFNs, if needed
	if(lfnNeeded) {

	} else {
		// Write back the modified cluster
		unsigned int offset = start_dirent_offset / (cluster_size / sizeof(fat_dirent_t));
		unsigned int cluster = dir_chain[offset];

		// Write back to device
		if(!this->writeCluster(cluster, ((uint8_t *) dirBuf) + (offset * cluster_size), &err)) {
			#if PRINT_ERROR
			KERROR("%s: Error writing directory entry: %u", __PRETTY_FUNCTION__, err);
			#endif

			kfree(chain);
			kfree(dirBuf);
			kfree(dir_chain);
			kfree(name);

			return -4;
		}
	}

	// Create an fs_file_t object to represent this
	fs_file_t *file = hal_vfs_allocate_file(dir);

	file->size = start_dirent[shortNameOffset].filesize;

	// Convert the timestamps
	file->i.time_created = this->convert_timestamp(start_dirent[shortNameOffset].created_date, start_dirent[shortNameOffset].created_time, 0);
	file->i.time_written = this->convert_timestamp(start_dirent[shortNameOffset].write_date, start_dirent[shortNameOffset].write_time, 0);
	file->i.time_created = this->convert_timestamp(start_dirent[shortNameOffset].accessed_date, 0, 0);

	/*
	 * To speed up file reads, store the first cluster of this file
	 * in the low 32 bits of the userData field of the item.
	 */
	file->i.userData = (start_dirent[shortNameOffset].cluster_high << 16) | (start_dirent[shortNameOffset].cluster_low);

	// Create a copy of the input name for the file struct
	file->i.name = (char *) kmalloc(strlen(in_name) + 2);
	strncpy(file->i.name, in_name, strlen(in_name) + 2);

	// Perform cleanup
	kfree(dir_chain);
	kfree(name);

	// Zero out the cluster we allocated to the file
	void *nullBuf = (void *) dirBuf;
	memclr(nullBuf, cluster_size);

	// Write zeroes to device: a write error won't cause problems here.
	if(!this->writeCluster(chain[0], nullBuf, &err)) {
		#if PRINT_ERROR
		KERROR("Error zeroing cluster: %u", err);
		#endif
	}

	// Clean up directory buffer too
	kfree(dirBuf);
	kfree(chain);

	return 0;
}

/*
 * Checks if a string contains exclusively all uppercase or all lowercase
 * characters.
 */
string_case_t fs_fat32::getStringCase(char *string) {
	size_t length = strlen(string);

	bool foundUppercase = false, foundLowercase = false;

	for(unsigned int c = 0; c < length; c++) {
		if(isupper(string[c])) foundUppercase = true;
		else if(islower(string[c])) foundLowercase = true;
	}

	if(foundUppercase && !foundLowercase) return kStringCaseUpper;
	else if(!foundUppercase && foundLowercase) return kStringCaseLower;
	else return kStringCaseMixed;
}