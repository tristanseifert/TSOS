// C component of VFS support
extern "C" {
	#import <types.h>
	#import "vfs.h"
}

// This keeps track of a superblock, VFS and mountpoint
typedef struct vfs_ptr {
	void *superblock;
	hal_vfs_t *fs;

	char *mountpoint;
} vfs_ptr_t;

// Turning a path into the filesystem it's on
static vfs_ptr_t *mount_to_vfs(char *mount);

// Data structure to keep track of registered filesystems
static list_t *registered_vfs;
static list_t *filesystem_superblocks;

/*
 * Initialises the VFS driver.
 */
static int hal_vfs_init(void) {
	registered_vfs = list_allocate();
	filesystem_superblocks = list_allocate();

	return 0;
}
module_early_init(hal_vfs_init);

/*
 * Registers a VFS driver with the HAL that can be matched on disks to allow
 * file IO to take place.
 */
int hal_vfs_register(hal_vfs_t *fs) {
	KDEBUG("Registered VFS '%s'", fs->name);
	list_add(registered_vfs, fs);

	return 0;
}

/*
 * Locates a filesystem that works on the specified partition.
 */
bool hal_vfs_load(hal_disk_partition_t *partition, hal_disk_t *disk) {
	for(unsigned int i = 0; i < registered_vfs->num_entries; i++) {
		hal_vfs_t *fs = (hal_vfs_t *) list_get(registered_vfs, i);

		// Does FS support this partition type
		if(fs->supports_partition(partition)) {
			// Set up this VFS and such
			vfs_ptr_t *thingie = (vfs_ptr_t *) kmalloc(sizeof(vfs_ptr_t));

			thingie->superblock = fs->create_superblock(partition, disk);
			thingie->fs = fs;
			thingie->mountpoint = NULL;

			list_add(filesystem_superblocks, thingie);

			// Check if the root directory contains "kernel.elf"
			fs_directory_t *dir = fs->list_directory(thingie->superblock, (char *) "/");

			if(dir) {
				for(unsigned int i = 0; i < dir->children->num_entries; i++) {
					fs_item_t *item = (fs_item_t *) list_get(dir->children, i);

					if(item->type == kFSItemTypeFile) {
						if(!strcmp("bootvol.txt", item->name)) {
							KDEBUG("Found bootup volume");

							thingie->mountpoint = (char *) "/";
						}
					}
				}
			}
		
			return true;
		}
	}

	return false;
}

/*
 * Allocates a directory structure, as well as any child objects that are
 * associated with it.
 */
fs_directory_t *hal_vfs_allocate_directory(bool createHandle) {
	fs_directory_t *dir = (fs_directory_t *) kmalloc(sizeof(fs_directory_t));

	// Handle out of memory conditions
	if(dir) {
		dir->children = list_allocate();
		dir->i.type = kFSItemTypeDirectory;

		// Create a handle for this directory, if requested
		if(createHandle) {
			dir->i.handle = hal_handle_allocate(dir, kFSItemTypeDirectory);
		}

		// Default permissions: no owner, anyone can access
		dir->i.owner = {0, 0};
		dir->i.permissions = 0775;

		return dir;
	}

	return NULL;
}

/*
 * Allocates a file structure, placed under a specific directory.
 */
fs_file_t *hal_vfs_allocate_file(fs_directory_t *d) {
	ASSERT(d);

	fs_file_t *file = (fs_file_t *) kmalloc(sizeof(fs_file_t));

	// Handle out of memory conditions
	if(file) {
		file->i.type = kFSItemTypeFile;
		file->i.handle = hal_handle_allocate(file, kFSItemTypeFile);

		// Default permissions: same as parent
		file->i.owner = d->i.owner;
		file->i.permissions = d->i.permissions;

		// Set parent
		file->parent = d;

		// Add as a child of the parent
		list_add(d->children, file);

		return file;
	}

	return NULL;
}

/*
 * Deallocates the memory allocated to a directory, optionally updating its
 * handle to point to a new directory structure.
 *
 * This function gets kind of complex in that if a file has a file handle open
 * for it, obviously it wouldn't make sense to leave it orphaned, so we just
 * force it closed by invalidating the associated handle. On the next attempt
 * to use the file handle, it will be closed and properly destroyed, or whenever
 * the process using it terminates.
 *
 * The case with directories is similar: Its handle is invlidated and this
 * function run on it to recursively take care of everything.
 */
void hal_vfs_deallocate_directory(fs_directory_t *d, fs_directory_t *n) {
	ASSERT(d);

	// If requested, update handle
	if(n) {
		hal_handle_update_object(d->i.handle, n, false);
	} else { // just destroy handle
		hal_handle_release(d->i.handle, false);
	}

	// Begin the task of deallocating memory
	kfree(d->i.name);

	// Check the children to see if we can deallocate any of them
	for(unsigned int i = 0; i < d->children->num_entries; i++) {
		fs_item_t *item = (fs_item_t *) list_get(d->children, i);

		// It's a directory: run this function on it.
		if(item->type == kFSItemTypeDirectory) {
			hal_vfs_deallocate_directory((fs_directory_t *) item, NULL);
		} else if(item->type == kFSItemTypeFile) {
			// Deallocate file
			hal_vfs_deallocate_file((fs_file_t *) item);
		}
	}

	// Free children list
	list_destroy(d->children, false);
}

/*
 * Deallocates a file.
 */
void hal_vfs_deallocate_file(fs_file_t *f) {
	// Release memory for the name
	kfree(f->i.name);

	// Release handle (and memory associated with the file struct)
	hal_handle_release(f->i.handle, true);
}


/*
 * Translates a mountpoint into the filesystem it represents. If no filesystem
 * matches, the root filesystem is returned.
 */
static vfs_ptr_t *mount_to_vfs(char *path, char **relPath) {
	unsigned int longestMount = 0;
	unsigned int longestMountIdx = 0; // array index

	// Variables used inside loop
	size_t mntLen;
	int cmp;

	// Check each filesystem
	for(unsigned int fs = 0; fs < filesystem_superblocks->num_entries; fs++) {
		vfs_ptr_t *filesystem = (vfs_ptr_t *) list_get(filesystem_superblocks, fs);

		// Is this filesystem mounted?
		if(!filesystem->mountpoint) continue;

		// Is this shorter than the longest matching mountpoint so far?
		mntLen = strlen(filesystem->mountpoint);
		if(mntLen < longestMount) continue;

		// String compare
		cmp = strncmp(path, filesystem->mountpoint, mntLen);

		// Not a match
		if(cmp != 0) continue;

		// It's the longest so far, so save it
		longestMount = mntLen;
		longestMountIdx = fs;
	}

	// Get the filesystem that matched
	vfs_ptr_t *matchedFS = (vfs_ptr_t *) list_get(filesystem_superblocks, longestMountIdx);

	// If requested, get the path relative to the root of the filesystem
	if(relPath) {
		size_t bufSize = strlen(path);

		*relPath = (char *) kmalloc(bufSize);
		strncpy(*relPath, path + longestMountIdx, bufSize);

	}

	return matchedFS;
}

/*
 * Returns a list of fs_item_t objects that can be used to enumerate the files
 * inside the directory.
 */
C_FUNCTION list_t *hal_vfs_list_directory(char *dir) {
	char *relPath;
	vfs_ptr_t *fs = mount_to_vfs(dir, &relPath);

	fs_directory_t *d = fs->fs->list_directory(fs->superblock, relPath);
	kfree(relPath);
	return d->children;
}

/*
 * Creates the specified directory.
 */
C_FUNCTION int hal_vfs_create_directory(char *dir) {
	char *relPath;
	vfs_ptr_t *fs = mount_to_vfs(dir, &relPath);

	int r = fs->fs->create_directory(fs->superblock, relPath);
	kfree(relPath);
	return r;
}

/*
 * Deletes a file.
 */
C_FUNCTION int hal_vfs_unlink(char *path) {
	char *relPath;
	vfs_ptr_t *fs = mount_to_vfs(path, &relPath);

	int r = fs->fs->unlink(fs->superblock, relPath);
	kfree(relPath);
	return r;
}

/*
 * Opens a file in the specified mode.
 */
C_FUNCTION fs_file_handle_t *hal_vfs_fopen(char *path, fs_file_open_mode_t mode) {
	char *relPath;
	vfs_ptr_t *fs = mount_to_vfs(path, &relPath);

	fs_file_handle_t *handle = fs->fs->file_open(fs->superblock, relPath, mode);
	kfree(relPath);
	return handle;
}

/*
 * Closes a previously-opened file handle.
 */
C_FUNCTION void hal_vfs_fclose(fs_file_handle_t *handle) {
	vfs_ptr_t *ptr = (vfs_ptr_t *) handle->fs;

	ptr->fs->file_close(ptr->superblock, handle);
}

/*
 * Synchronises the filesystem's state with the state of the file: The exact
 * implementation is up to the filesystem.
 */
C_FUNCTION void hal_vfs_fupdate(fs_file_t *file) {
	UNIMPLEMENTED_WARNING();
}

/*
 * Reads from the current position in the file into buffer.
 *
 * @return Number of bytes read, or 0 if end-of-file was encountered.
 */
C_FUNCTION long long hal_vfs_fread(void *buf, size_t bytes, fs_file_handle_t *handle) {
	vfs_ptr_t *ptr = (vfs_ptr_t *) handle->fs;

	return ptr->fs->file_read(ptr->superblock, buf, bytes, handle);
}

/*
 * Writes to the file that the handle represents. This increments the offset
 * in the file handle.
 *
 * @return Number of bytes read, or 0 if end-of-file was encountered.
 */
C_FUNCTION long long hal_vfs_fwrite(void *buf, size_t bytes, fs_file_handle_t *handle) {
	vfs_ptr_t *ptr = (vfs_ptr_t *) handle->fs;

	return ptr->fs->file_write(ptr->superblock, buf, bytes, handle);
}