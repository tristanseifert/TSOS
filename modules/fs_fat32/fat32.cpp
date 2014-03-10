#import <module.h>
#import <fat32.hpp>

/*
 * Initialises a FAT32 filesystem from the specified partition table entry.
 */
fs_fat32::fs_fat32(hal_disk_partition_t *p, hal_disk_t *d) : hal_fs::hal_fs(p, d) {
	KDEBUG("pls ooh kill 'em 0x%08X 0x%08X", (unsigned int) partition, (unsigned int) disk);
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