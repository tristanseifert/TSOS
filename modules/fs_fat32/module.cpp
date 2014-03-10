#import <module.h>

// Filesystem specifics
#import "fat32.hpp"

// Static functions
static bool fat32_part_verify(hal_disk_partition_t *);
static void *fat32_create_superblocK(hal_disk_partition_t *part, hal_disk_t *disk);

// Module definition
static const module_t mod = {
	/*.name = */ MODULE_NAME
};

// File system definition
static const hal_vfs_t vfs = {
	/*.name = */ "FAT32",
	/*.supports_partition = */ fat32_part_verify,
	/*.create_superblock = */ fat32_create_superblocK
};

/*
 * Initialisation function for the FAT32 driver (called by kernel)
 */
extern "C" {
	module_t *start(void) {
		hal_vfs_register((hal_vfs_t *) &vfs);
		return (module_t *) &mod;
	}
}

/*
 * Verifies if the FAT32 driver can support the partition.
 */
static bool fat32_part_verify(hal_disk_partition_t *part) {
	if(part->type == 0x0C) {
		return true;
	}

	return false;
}

/*
 * Creates a superblock (in this case, the fat32/fat16 classes) for the
 * appropriate partition.
 *
 * This code assumes it's only called if it's a FAT fs, which is usually true.
 */
static void *fat32_create_superblocK(hal_disk_partition_t *part, hal_disk_t *disk) {
	// FAT32
	if(part->type == 0x0C) {
		return (void *) new fs_fat32(part, disk);
	}

	return NULL;
}