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
 * Modes that a file may be opened in
 */
typedef enum {
	kFSFileModeReadOnly = (1 << 0),
	kFSFileModeCreate = (1 << 1),

	// these two are mutually exclusive
	kFSFileModeTruncate = (1 << 2),
	kFSFileModeAppend = (1 << 3)
} fs_file_open_mode_t;

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

	// Timestamps
	time_t time_created;
	time_t time_written;
	time_t time_accessed;

	// User data
	unsigned long long userData;

	// To be used by drivers for caching purposes
	unsigned int cache_accesses;
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

	// Size (in bytes)
	unsigned long long size;
};

/*
 * Kernel representation of a file handle, used for accessing a file.
 */
typedef struct fs_file_handle fs_file_handle_t;
struct fs_file_handle {
	// File this handle is associated with
	hal_handle_t file;

	// Mode the file was opened in
	fs_file_open_mode_t mode;

	// Indicates whether the file can be repositioned
	bool can_seek;

	// Current read/write position
	unsigned long long position;

	// Various flags
	bool isOpen;
};

// Structure defining a VFS driver
typedef struct hal_vfs hal_vfs_t;
struct hal_vfs {
	const char name[32];

	// Determines if a fs driver supports a partition
	bool (*supports_partition)(hal_disk_partition_t*);

	// Creates a superblock for a filesystem (hal_fs subclass)
	void* (*create_superblock)(hal_disk_partition_t*, hal_disk_t*);

	/*
	 * Functions declared below are the functions that the VFS drivers areh
	 * expected to implement in order to receive requests to interface with the
	 * filesystem.
	 *
	 * The value passed in as "superblock" is what the "create_superblock"
	 * function returned earlier.
	 */

	// List a directory.
	fs_directory_t* (*list_directory)(void *superblock, char *dirname);

	// Create a directory
	int (*create_directory)(void *superblock, char *path);

	// Deletes an item
	int (*unlink)(void *superblock, char *path);

	// Opens a file, optionally creating it.
	fs_file_handle_t* (*file_open)(void *superblock, char *path, fs_file_open_mode_t mode);

	// Closes a file handle
	void (*file_close)(void *superblock, fs_file_handle_t *file);

	// Updates file metadata from struct
	void (*file_update)(void *superblock, fs_file_t *file);

	// Reads from the specified offset in the file.
	long long (*file_read)(void *superblock, void* buffer, size_t bytes, fs_file_handle_t *file);

	// Writes to the specified offset in the file.
	long long (*file_write)(void *superblock, void* buffer, size_t bytes, fs_file_handle_t *file);
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