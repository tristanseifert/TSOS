#import <types.h>
#import <hal/disk.h>
#import <hal/handle.h>

/*
 * Supported types of filesystem items
 */
typedef enum {
	kFSItemTypeNone = -1,
	kFSItemTypeFile = 'FILE',
	kFSItemTypeDirectory = 'DIRE',
	kFSItemTypeRoot = 'ROOT',
	kFSItemTypeMountPoint = 'MNPT'
} fs_item_type_t;

/*
 * Abstract type representing an object on a filesystem
 */
typedef struct fs_item fs_item_t;
struct fs_item {
	fs_item_type_t type;

	// Name of this item
	char* name;

	// Who owns this item?
	struct {
		uint16_t user;
		uint16_t group;
	} owner;

	// Permission bitmap (UNIX-style)
	uint16_t permissions;

	// Various file attributes
	bool is_hidden;
	bool is_system;
	bool is_readonly;

	// Kernel handle for this object
	hal_handle_t handle;
};

/*
 * Kernel representation of a directory on the filesystem
 *
 * It's important to note that the root of a filesystem is represented by the
 * "parent" member pointing to the directory in which it is mounted in the parent
 * filesystem. The root of the root filesystem (/) has parent set to NULL.
 * Therefore, you must verify that the object at the address pointed to by
 * parent is is actually what you expect it to be.
 *
 * "children" is a list_t type, which contains either fs_file or fs_directory
 * items: their type can be determined by treating them as an fs_item type and
 * reading its type member.
 */
typedef struct fs_directory fs_directory_t;
struct fs_directory {
	fs_item_t i;

	// Handle to the directory (or filesystem) containing this directory
	hal_handle_t parent;

	// Files (or directories) contained by this directory
	list_t *children;

	// Number of file handles open for this directory and the files within
	unsigned int handles_open;
};

/*
 * Kernel representation of a file on a filesystem
 *
 * The directory member points to the directory that the file is contained in.
 */
typedef struct fs_file fs_file_t;
struct fs_file {
	fs_item_t i;

	// Number of file handles open for this file
	unsigned int handles_open;

	// Directory this file is contained in
	hal_handle_t parent;
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

// Allocates memory for various structures
fs_directory_t *hal_vfs_allocate_directory(bool createHandle);
fs_file_t *hal_vfs_allocate_file(fs_directory_t *d);

// Deallocates a directory or file
void hal_vfs_deallocate_directory(fs_directory_t *d, fs_directory_t *n);
void hal_vfs_deallocate_file(fs_file_t *f);