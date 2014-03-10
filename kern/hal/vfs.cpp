// C component of VFS support
extern "C" {
	#import <types.h>
	#import "vfs.h"

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
}


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
			void* data = fs->create_superblock(partition, disk);
			list_add(filesystem_superblocks, data);
		}
	}

	return false;
}
