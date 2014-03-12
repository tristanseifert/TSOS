#import <module.h>

// Filesystem specifics
#import "fat32.hpp"

// Static functions
static bool fat32_part_verify(hal_disk_partition_t *);
static void *fat32_create_superblock(hal_disk_partition_t *part, hal_disk_t *disk);

// FAT32 interface wrappers
static fs_directory_t *fat32_list_directory(void *superblock, char *dirname);
static int fat32_create_directory(void *superblock, char *path);
static int fat32_unlink(void *superblock, char *path);
static fs_file_t *fat32_file_open(void *superblock, char *path, fs_file_open_mode_t mode);
static void fat32_file_close(void *superblock, fs_file_t *file);
static void fat32_file_update(void *superblock, fs_file_t *file);
static void* fat32_file_read(void *superblock, void* buffer, unsigned int offset, unsigned int bytes, fs_file_t *file);
static void fat32_file_write(void *superblock, void* buffer, unsigned int offset, unsigned int bytes, fs_file_t *file);

// Module definition
static const module_t mod = {
	/*.name = */ MODULE_NAME
};

// File system definition
static const hal_vfs_t vfs = {
	/*.name = */ "FAT32",
	/*.supports_partition = */ fat32_part_verify,
	/*.create_superblock = */ fat32_create_superblock,
	/*.list_directory = */ fat32_list_directory,
	/*.create_directory = */ fat32_create_directory,
	/*.unlink = */ fat32_unlink,
	/*.file_open = */ fat32_file_open,
	/*.file_close = */ fat32_file_close,
	/*.file_update = */ fat32_file_update,
	/*.file_read = */ fat32_file_read,
	/*.file_write = */ fat32_file_write
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
static void *fat32_create_superblock(hal_disk_partition_t *part, hal_disk_t *disk) {
	// FAT32
	if(part->type == 0x0C) {
		fs_fat32 *fs = new fs_fat32(part, disk);

		return (void *) fs;
	}

	return NULL;
}

// List a directory.
static fs_directory_t *fat32_list_directory(void *superblock, char *dirname) {
	fs_fat32 *fs = (fs_fat32 *) superblock;

	return fs->list_directory(dirname, true);
}

// Create a directory
static int fat32_create_directory(void *superblock, char *path) {
	UNIMPLEMENTED_WARNING();
	return -1;
}

// Deletes an item
static int fat32_unlink(void *superblock, char *path) {
	UNIMPLEMENTED_WARNING();
	return -1;
}

// Opens a file, optionally creating it.
static fs_file_t *fat32_file_open(void *superblock, char *path, fs_file_open_mode_t mode) {
	UNIMPLEMENTED_WARNING();
	return NULL;
}

// Closes a file handle
static void fat32_file_close(void *superblock, fs_file_t *file) {
	UNIMPLEMENTED_WARNING();
}

// Updates file metadata from struct
static void fat32_file_update(void *superblock, fs_file_t *file) {
	UNIMPLEMENTED_WARNING();
}

// Reads from the specified offset in the file.
static void* fat32_file_read(void *superblock, void* buffer, unsigned int offset, unsigned int bytes, fs_file_t *file) {
	UNIMPLEMENTED_WARNING();
	return NULL;
}

// Writes to the specified offset in the file.
static void fat32_file_write(void *superblock, void* buffer, unsigned int offset, unsigned int bytes, fs_file_t *file) {
	UNIMPLEMENTED_WARNING();
}