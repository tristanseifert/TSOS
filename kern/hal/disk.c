#import <types.h>
#import "disk.h"

// Internal state
static volatile bool hal_disk_null_callback_called = false;
static list_t *disks;

// Private functions
static void hal_disk_null_callback(unsigned int id, void* buf, void* ctx);

/*
 * Initialises the disk HAL.
 */
static int hal_disk_init(void) {
	disks = list_allocate();

	return 0;
}
module_early_init(hal_disk_init);

/*
 * Allocates a new disk structure
 */
hal_disk_t *hal_disk_alloc() {
	hal_disk_t *disk = (hal_disk_t *) kmalloc(sizeof(hal_disk_t));
	
	if(disk) {
		memclr(disk, sizeof(hal_disk_t));
		return disk;
	}

	return NULL;
}

/*
 * Registers a new disk with the HAL.
 */
void hal_disk_register(hal_disk_t *disk) {
	list_add(disks, disk);
	// KDEBUG("hal: disk registered 0x%08X", (unsigned int) disk);
}

/*
 * Initialises the disk for access.
 */
hal_disk_error_t hal_disk_setup(hal_disk_t *disk) {
	return disk->f.init(disk);
}

/*
 * Reads from the disk
 */
hal_disk_error_t hal_disk_read(hal_disk_t* disk, uint32_t lba, uint32_t length, void* buffer, unsigned int* id, hal_disk_callback_t callback, void* ctx) {
	if(callback) {
		return disk->f.read(disk, lba, length, buffer, id, callback, ctx);
	} else {
		int r = disk->f.read(disk, lba, length, buffer, id, hal_disk_null_callback, NULL);

		// If there was no errors, wait for the callback to be called
		if(r == kDiskErrorNone) {
			while(!hal_disk_null_callback_called) {
				// Wait until the disk read happened
			}

			hal_disk_null_callback_called = false;

			return r;
		} else {
			return r;
		}
	}
}

/*
 * Callback used when NULL is specified. Basically emulates the behaviour of a
 * synchronous disk access.
 */
static void hal_disk_null_callback(unsigned int id, void* buf, void* ctx) {
	hal_disk_null_callback_called = true;
}