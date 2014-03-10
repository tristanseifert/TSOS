#import <module.h>
#import <fat32.hpp>

/*
 * Initialises a FAT32 filesystem from the specified partition table entry.
 */
fs_fat32::fs_fat32(hal_disk_partition_t *p, hal_disk_t *d) : hal_fs::hal_fs(p, d) {
	// Allocate memory to hold the BPB
	bpb = (fat_fs_bpb32_t *) kmalloc(512);
	ASSERT(bpb);

	// Read sector 0 of partition synchronously
	KDEBUG("Reading FAT BPB from sector %u", p->lba_start);
	
	if(hal_disk_read(disk, p->lba_start, 1, bpb, NULL, NULL, NULL) != kDiskErrorNone) {
		KERROR("Error reading from disk");
		return;
	}

	// Get data sector count
	root_dir_sectors = ((bpb->root_entry_count * 32) + (bpb->bytes_per_sector - 1)) / bpb->bytes_per_sector;
	data_sectors = bpb->total_sectors_32 - (bpb->reserved_sector_count + (bpb->table_count * bpb->table_size_32)) + root_dir_sectors;
	cluster_count = data_sectors / bpb->sectors_per_cluster;

	// Determine volume type
	if(cluster_count < 4085) {
		KERROR("Tried to initialise FAT12 volume as FAT32");
		return;
	} else if(cluster_count < 65525) { 
		KERROR("Tried to initialise FAT16 volume as FAT32");
		return;
	} else {
		KDEBUG("%u clusters (%u sec/cluster, %u bytes/sector)", (unsigned int) cluster_count, (unsigned int) bpb->sectors_per_cluster, (unsigned int) bpb->bytes_per_sector);
	}

	KDEBUG("BPB at 0x%08X", (unsigned int) bpb);
}

/*
 * Clean up the filesystem's internal data structures.
 */
fs_fat32::~fs_fat32() {
	kfree(bpb);
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
	cluster *= 4;
	unsigned int FATSz = bpb->table_size_32;

	offset.sector = bpb->reserved_sector_count + (cluster / bpb->bytes_per_sector);
	offset.offset = cluster % bpb->bytes_per_sector;

	return offset;
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