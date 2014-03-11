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
	if(!read_sectors(0, 1, &bpb, &err)) {
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
	} else {
		KDEBUG("%u clusters (first data cluster at %u)", (unsigned int) num_data_clusters,
		first_data_sector);
	}

	// Read FSINFO sector
	if(!read_sectors(bpb.fat_info, 1, &fs_info, &err)) {
		KERROR("Error reading FSInfo: %u", err);
		return;
	}

	// Verify FSInfo struct
	if(fs_info.signature != 0x41615252 || fs_info.signature2 != 0x61417272 || fs_info.trailSig != 0xAA550000) {
		KWARNING("Corrupted FSInfo: 0x%08X 0x%08X 0x%08X", 
			(unsigned int) fs_info.signature, (unsigned int) fs_info.signature2,
			(unsigned int) fs_info.trailSig);
	} else {
		KDEBUG("%u clusters free, start free search at %u", 
			(unsigned int) fs_info.last_known_free_sec_cnt,
			(unsigned int) fs_info.free_cluster_search_start);
	}

	// Allocate various kinds of buffers
	fatBuffer = (uint32_t *) kmalloc(cluster_size);

	this->read_root_dir();

	// List it
	for(unsigned int i = 0; i < root_dir_num_entries; i++) {
		fat_dirent_t *ent = &root_dir[i];

		if((ent->attributes & FAT_ATTR_LFN) != FAT_ATTR_LFN && ent->name[0] != 0xE5 && ent->name[0] != 0x00) {
			char *name = fs_fat32::dirent_get_8_3_name(ent);
			KDEBUG("%s: %09u bytes, cluster %08X", name, (unsigned int) ent->filesize, (ent->cluster_high << 16 | ent->cluster_low));
			kfree(name);
		}
	}

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

	// Follow the cluster chain for the root directory.
	unsigned int *root_clusters = this->clusterChainForCluster(bpb.root_cluster & FAT32_MASK);
	unsigned int cnt = 0;

	while(root_clusters[cnt] != FAT32_END_CHAIN) {
		cnt++;
	}

	// Allocate memory for root directory
	unsigned int root_dir_len = cnt * cluster_size;
	root_dir = (fat_dirent_t *) kmalloc(root_dir_len);
	root_dir_num_entries = root_dir_len / sizeof(fat_dirent_t);

	// Read the root directory's sectors.
	cnt = 0;
	while(true) {
		if(root_clusters[cnt] != FAT32_END_CHAIN) {
			unsigned int sector = root_clusters[cnt];

			if(!this->readCluster(root_clusters[cnt], root_dir + (cnt * cluster_size), &err)) {
				KERROR("Error reading root dir sector %u: %u", sector, err);
				return;
			}
		} else {
			break;
		}

		cnt++;
	}

	// Clean up temporary buffers needed to read root directory
	kfree(root_clusters);
}

/*
 * Calculates a cluster's offset into the FAT. The returned structure indicates
 * the sector to read, and the dword offset into that sector. In other words,
 * if the sector is read as an array of bytes, the offset must be multiplied
 * by four.
 */
fat32_secoff_t fs_fat32::fatEntryOffsetForCluster(unsigned int cluster) {
	fat32_secoff_t offset;

	// Determine size of FAT and normalise cluster
	offset.sector = bpb.reserved_sector_count + (cluster / cluster_size);
	offset.offset = cluster % cluster_size;

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
		if(this->read_sectors(off.sector, bpb.sectors_per_cluster, fatBuffer, &err)) {
			nextCluster = fatBuffer[off.offset];
		} else { // error reading
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

	if(!this->read_sectors(sector, bpb.sectors_per_cluster, buffer, error)) {
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

/*
 * Returns a list of hal_vfs_file_t objects, representing the files contained
 * in the specified directory.
 */
list_t *fs_fat32::contents_of_directory(char *directory) {
	return NULL;
}