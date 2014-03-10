#import <types.h>

typedef int hal_disk_error_t;

typedef struct hal_disk hal_disk_t;
typedef struct hal_disk_functions hal_disk_functions_t;
typedef struct hal_disk_partition hal_disk_partition_t;

// Read/write callback (gets read ID, data ptr and context ptr)
typedef void (*hal_disk_callback_t)(unsigned int, void*, void*);

// Interfaces a disk could be attached through
typedef enum hal_disk_if {
	kDiskInterfaceNone = -1,
	kDiskInterfacePATA = 0,
	kDiskInterfaceSATA,
	kDiskInterfaceSCSI,
	kDiskInterfaceUSB,
	kDiskInterfaceOther = 0x7FFFFFFF
} hal_disk_if_t;

// Common disk error codes
enum hal_disk_err {
	kDiskErrorNone = 0,
	kDiskErrorUnknown = 0x70000000
};

// Disk type
typedef enum {
	kDiskTypeNone = -1,
	kDiskTypeHardDrive = 0,
	kDiskTypeOptical,
	kDiskTypeTape,
	kDiskTypeFloppy,
	kDiskTypeSolidState,
	kDiskTypeOther = 0x7FFFFFFF
} hal_disk_type_t;

// A single partition entry
struct hal_disk_partition {
	uint8_t type;

	uint32_t lba_start;
	uint32_t size;
};

// Function calls for interfacing with a disk
struct hal_disk_functions {
	// Initialisation
	hal_disk_error_t (*init)(hal_disk_t*);
	hal_disk_error_t (*reset)(hal_disk_t*);

	// Disk, LBA start, length, destination buffer, ID assigned to read, callback, ctx to pass to callback
	hal_disk_error_t (*read)(hal_disk_t*, uint32_t, uint32_t, void*, unsigned int*, hal_disk_callback_t, void*);

	// Disk, LBA start, length, read buffer, ID assigned to write, callback, ctx to pass to callback
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
struct hal_disk {
	// identify the drive
	hal_disk_if_t interface;
	hal_disk_type_t type;

	unsigned int drive_number;
	bool media_loaded;

	void *driver;

	// Partitions on the disk
	hal_disk_partition_t partitions[4];

	// functions to interact with the drive
	hal_disk_functions_t f;

	// Disk sleeping
	bool sleep_enabled;
	unsigned int sleep_interval; // seconds
};

#ifdef __cplusplus
extern "C" {
#endif

	hal_disk_t *hal_disk_alloc();
	void hal_disk_register(hal_disk_t *disk);

	hal_disk_error_t hal_disk_setup(hal_disk_t* disk);
	hal_disk_error_t hal_disk_read(hal_disk_t* disk, uint32_t lba, uint32_t length, void* buffer, unsigned int* id, hal_disk_callback_t callback, void* ctx);

#ifdef __cplusplus
}
#endif