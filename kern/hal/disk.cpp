#import <types.h>
#import "hal.h"
#import "disk.h"

// Internal state
static volatile bool hal_disk_null_callback_called = false;
static unsigned int hal_disk_null_callback_error;
static list_t *disks;

// Private functions
static void hal_disk_read_ptables(void);

static void hal_disk_register_mbr_read_callback(unsigned int id, void *buf, void *ctx);
static void hal_disk_null_callback(unsigned int id, void* buf, void* ctx);

// A single MBR partition entry
struct mbr_ent {
	uint8_t status;

	uint8_t start_head;
	uint8_t start_sector;
	uint8_t start_cylinder;

	uint8_t partition_type;

	uint8_t last_head;
	uint8_t last_sector;
	uint8_t last_cylinder;

	uint32_t lba_start;
	uint32_t length;
} __attribute__((packed));

/*
 * Initialises the disk HAL.
 */
static int hal_disk_init(void) {
	disks = list_allocate();
	hal_register_init_handler(hal_disk_read_ptables);

	return 0;
}
module_early_init(hal_disk_init);

extern "C" {
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
	 * Initialises the disk for access.
	 */
	hal_disk_error_t hal_disk_setup(hal_disk_t *disk) {
		return disk->f.init(disk);
	}

	/*
	 * Registers a new disk with the HAL.
	 */
	void hal_disk_register(hal_disk_t *disk) {
		list_add(disks, disk);
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

				return hal_disk_null_callback_error;
			} else {
				return r;
			}
		}
	}
}

/*
 * Callback used when NULL is specified. Basically emulates the behaviour of a
 * synchronous disk access.
 */
static void hal_disk_null_callback(unsigned int id, void* buf, void* ctx) {
	hal_disk_null_callback_called = true;

	if(!buf) {
		hal_disk_null_callback_error = *((unsigned int *) ctx);
	}
}


/*
 * Reads the partition tables of all hard drives.
 */
static void hal_disk_read_ptables(void) {
	for(unsigned int i = 0; i < disks->num_entries; i++) {
		hal_disk_t *disk = (hal_disk_t *) list_get(disks, i);

		// Read MBR, only on hard drives
		if(hal_disk_setup(disk) == kDiskErrorNone && disk->type == kDiskTypeHardDrive) {
			void *buffer = kmalloc(1024);
			hal_disk_read(disk, 0, 1, buffer, NULL, hal_disk_register_mbr_read_callback, disk);
		}
	}
}

/*
 * Callback for when the read of sector zero completes
 */
static void hal_disk_register_mbr_read_callback(unsigned int id, void *buf, void *ctx) {
	if(!buf) {
		KERROR("Error reading MBR");
		return;
	}

	hal_disk_t *disk = (hal_disk_t *) ctx;
	uint8_t *readPtr = (uint8_t *) buf;

	// Valid MBR
	if(readPtr[0x1FE] == 0x55 && readPtr[0x1FF] == 0xAA) {
		// Process four partitions
		struct mbr_ent *partitions = (struct mbr_ent *) (readPtr + 0x1BE);

		for(unsigned int p = 0; p < 4; p++) {
			struct mbr_ent *mbr_entry = &partitions[p];

			// Is there a partition mapped here?
			if(mbr_entry->partition_type) {
				disk->partitions[p].type = mbr_entry->partition_type;
				disk->partitions[p].lba_start = mbr_entry->lba_start;
				disk->partitions[p].size = mbr_entry->length;

				// Partition loaded
				hal_vfs_load(&disk->partitions[p], disk);
			}
		}
	}

	// Clean up read buffer
	kfree(buf);
}