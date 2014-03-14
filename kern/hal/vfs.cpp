// C component of VFS support
extern "C" {
	#import <types.h>
	#import "vfs.h"
}

static list_t *registered_vfs;
static list_t *filesystem_superblocks;

struct vfs_ptr {
	void *superblock;
	hal_vfs_t *vfs;
};

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
int hal_vfs_register(hal_vfs_t *vfs) {
	KDEBUG("Registered VFS '%s'", vfs->name);
	list_add(registered_vfs, vfs);

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
			// Allocate a struct to hold this vfs
			struct vfs_ptr *thingie = (struct vfs_ptr *) kmalloc(sizeof(struct vfs_ptr));

			thingie->superblock = fs->create_superblock(partition, disk);
			thingie->vfs = fs;

			list_add(filesystem_superblocks, thingie);

			// Test: list root directory
			fs_directory_t *dir = fs->list_directory(thingie->superblock, (char *) "/test/test2/folder/");

	/*		if(dir) {
				for(unsigned int i = 0; i < dir->children->num_entries; i++) {
					fs_item_t *item = (fs_item_t *) list_get(dir->children, i);

					if(item->type == kFSItemTypeFile) {
						fs_file_t *file = (fs_file_t *) item;

						KDEBUG("File: %s %u Bytes", file->i.name, (unsigned int) file->size);
					} else if(item->type == kFSItemTypeDirectory) {
						fs_directory_t *dir = (fs_directory_t *) item;

						KDEBUG(" Dir: %s", dir->i.name);
					}
				}
			}*/

			// Open a file
			fs_file_handle_t *file = fs->file_open(thingie->superblock, (char *) "/test/test2/folder/test file with long filename.txt", kFSFileModeReadOnly);
			if(file) {
				KDEBUG("Opened file handle: %u", file->file);
			}

			// Read the first 1024 bytes
			void *data = kmalloc(1026);
			long long err = 0;

			if((err = fs->file_read(thingie->superblock, data, 1024, file)) >= 0) {
				KDEBUG("Read file to 0x%08X (%u bytes)", (unsigned int) data, (unsigned int) err);
			} else {
				KERROR("Error reading file: %i", (int) err);
			}

			kprintf("%s", (char *) data);

			// Close file
			fs->file_close(thingie->superblock, file);

			// Create a file
			file = fs->file_open(thingie->superblock, (char *) "/hello.TXT", kFSFileModeCreate);
			if(file) {
				KDEBUG("Opened file handle: %u", file->file);
			}

			char buffer[64];

			// This tries to create "long_filename_0.txt" through "long_filename_10.txt"
			for(unsigned int i = 0; i < 11; i++) {
				// Make filename
				snprintf((char *) &buffer, 64, "/long_filename_%u.txt", i);

				// Close file
				fs->file_close(thingie->superblock, file);

				// Create a file
				file = fs->file_open(thingie->superblock, (char *) &buffer, kFSFileModeCreate);
				if(file) {
					KDEBUG("Opened file handle: %u", file->file);
				}
			}

			// Close file
			fs->file_close(thingie->superblock, file);
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
		file->parent = d->i.handle;

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