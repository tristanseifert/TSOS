#import <types.h>

typedef int hal_disk_error_t;

typedef struct disk hal_disk_t;
typedef struct hal_disk_functions hal_disk_functions_t;

// Read/write callback (gets read ID, data ptr and context ptr)
typedef void (*hal_disk_callback_t)(unsigned int, void*, void*);

// Interfaces a disk could be attached through
typedef enum hal_disk_if {
	kDiskInterfaceNone = -1,
	kDiskInterfaceATA = 0,
	kDiskInterfaceSATA,
	kDiskInterfaceATAPI,
	kDiskInterfaceSCSI,
	kDiskInterfaceUSB,
	kDiskInterfaceOther = 0x7FFFFFFF
} hal_disk_if_t;

// Common disk error codes
enum hal_disk_err {
	kDiskErrorNone = 0,
	kDiskErrorUnknown = 0x70000000
};

// Function calls for interfacing with a disk
struct hal_disk_functions {
	// Initialisation
	hal_disk_error_t (*init)(hal_disk_t*);
	hal_disk_error_t (*reset)(hal_disk_t*);

	// Disk, LBA start, length, destination buffer, ID assigned to read, callback
	hal_disk_error_t (*read)(hal_disk_t*, uint32_t, uint32_t, void*, unsigned int*, hal_disk_callback_t, void*);

	// Disk, LBA start, length, read buffer, ID assigned to write, callback
	hal_disk_error_t (*write)(hal_disk_t*, uint32_t, uint32_t, void*, unsigned int*, hal_disk_callback_t, void*);

	// Miscellaneous
	hal_disk_error_t (*flush_cache)(hal_disk_t*);

	hal_disk_error_t (*sleep)(hal_disk_t*);
	hal_disk_error_t (*wake)(hal_disk_t*);

	// Removable media support
	hal_disk_error_t (*lock)(hal_disk_t*, bool); // Sets disk lock state, if supported
	hal_disk_error_t (*eject)(hal_disk_t*);
};

// Structure defining a disk
struct disk {
//	ptable_t partition_table;

	// identify the drive
	hal_disk_if_t interface;
	unsigned int drive_number;
	bool media_loaded;

	void *driver;

	// functions to interact with the drive
	hal_disk_functions_t f;

	// Disk sleeping
	bool sleep_enabled;
	unsigned int sleep_interval; // seconds
};

hal_disk_t *hal_disk_alloc();
void hal_disk_register(hal_disk_t *disk);

hal_disk_error_t hal_disk_setup(hal_disk_t* disk);
hal_disk_error_t hal_disk_read(hal_disk_t* disk, uint32_t lba, uint32_t length, void* buffer, unsigned int* id, hal_disk_callback_t callback, void* ctx);
