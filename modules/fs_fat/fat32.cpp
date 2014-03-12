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

#define DEBUG_DIRECTORY_CACHING	0

/*
 * Initialises a FAT32 filesystem from the specified partition table entry.
 */
fs_fat32::fs_fat32(hal_disk_partition_t *p, hal_disk_t *d) : hal_fs::hal_fs(p, d) {
	unsigned int err = 0;

	// Read sector 0 of partition synchronously
	if(!this->hal_fs::read_sectors(0, 1, &bpb, &err)) {
		KERROR("Error reading BPB: %u", err);
		return;
	}

	// Get data sector count
	root_dir_sectors = ((bpb.root_entry_count * 32) + (bpb.bytes_per_sector - 1)) / bpb.bytes_per_sector;

	num_data_sectors = bpb.total_sectors_32 - (bpb.reserved_sector_count + (bpb.table_count * bpb.table_size_32)) + root_dir_sectors;
	num_data_clusters = num_data_sectors / bpb.sectors_per_cluster;

	// Calculate size of a cluster (in bytes)
	cluster_size = bpb.sectors_per_cluster * bpb.bytes_per_sector;
	KDEBUG("Cluster size of %u bytes", cluster_size);

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
		KERROR("Error reading FSInfo: %u", err);
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
	dirHandleCache = hashmap_allocate();

	// Read FAT sector 0 to get dirty flags
	if(!this->hal_fs::read_sectors(bpb.reserved_sector_count, 1, fatBuffer, &err)) {
		KERROR("Error reading FAT: %u", err);
		return;
	} else {
		fs_clealyUnmounted = (fatBuffer[1] & FAT32_VOLUME_DIRTY_MASK);

		if(!fs_clealyUnmounted) {
			KWARNING("Filesystem not cleanly unmounted after last use!");
		}
	}

	// Read root directory
	this->read_root_dir();

	// List it
/*	KDEBUG("Root directory of '%s': %u items (handle: %u)", volumeLabel, root_directory->children->num_entries, root_directory->i.handle);
	for(unsigned int i = 0; i < root_directory->children->num_entries; i++) {
		fs_item_t *item = (fs_item_t *) list_get(root_directory->children, i);

		if(item->type == kFSItemTypeFile) {
			fs_file_t *file = (fs_file_t *) item;

			KDEBUG("File: %s %u Bytes", file->i.name, (unsigned int) file->size);
		} else if(item->type == kFSItemTypeDirectory) {
			fs_directory_t *dir = (fs_directory_t *) item;

			KDEBUG(" Dir: %s", dir->i.name);
		}
	}*/

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
				KERROR("Error reading root dir cluster %u: %u", cluster, err);
				return;
			}
		} else {
			break;
		}

		cnt++;
	}

	// Process the read FAT directory entries into fs_file_t structs
	this->processFATDirEnt(buffer, root_dir_num_entries, root_directory);

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
					KERROR("Error reading directory file %u: %u", cluster, err);
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

		currentDir->parent = dir->i.handle;

		if(cache) {
			hashmap_insert(dirHandleCache, fullpath, (void *) currentDir->i.handle);

			#if DEBUG_DIRECTORY_CACHING
			KDEBUG("dir_cache add: %s: 0x%08X", fullpath, (unsigned int) currentDir->i.handle);
			#endif
		}

		directory = currentDir;
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
			KERROR("Couldn't read sector %u for FAT", off.sector);
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
 * Gets the sector, relative to the start of the partition, for a certain file.
 */
unsigned int fs_fat32::sector_for_file(char *path, unsigned int offset) {
	return 0;
}