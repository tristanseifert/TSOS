#import <types.h>
#import <hal/disk.h>
#import <hal/handle.h>

// Type defining a file handle
typedef struct hal_vfs_file FILE;

struct hal_vfs_file {
	hal_handle_t h;
};

// Structure defining a VFS driver
typedef struct hal_vfs hal_vfs_t;
struct hal_vfs {
	const char name[32];

	// Determines if a fs driver supports a partition
	bool (*supports_partition)(hal_disk_partition_t*);

	// Creates a superblock for a filesystem (hal_fs subclass)
	void* (*create_superblock)(hal_disk_partition_t*, hal_disk_t*);
};

// Include filesystem root class
#ifdef __cplusplus
#import <hal/filesystem.hpp>
#endif

// Register a VFS
int hal_vfs_register(hal_vfs_t *);

// Attempts to load a filesystem for the specified partition
bool hal_vfs_load(hal_disk_partition_t *partition, hal_disk_t *disk);