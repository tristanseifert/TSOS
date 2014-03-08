#import <types.h>
#import "runtime/list.h"

// Bus names
#define PLATFORM_BUS_NAME "platform"

// Error codes
typedef enum {
	kHALBusNoError,
	kHALBusNotExistant,
	kHALDriverAlreadyRegistered,
	kHALDeviceAlreadyRegistered,
} bus_error_t;

// Types
typedef struct node node_t;
typedef struct bus bus_t;
typedef struct device device_t;
typedef struct driver driver_t;

typedef enum {
	kDeviceConnectionInternal = 1,
	kDeviceConnectionISA,
	kDeviceConnectionPCI,
	kDeviceConnectionUSB,

	kDeviceConnectionOther = 0x7FFFFFFF
} device_connection_t;

struct node {
	char *name;
	node_t *parent;
	list_t *children;
};

struct device {
	node_t node;

	device_connection_t connection; // how the device is connected

	void *bus_info; // pointer to bus-specific structure (bus regs, etc)
	void *device_info; // pointer to device-specific info (generated by driver)
	void *sysInfo; // pointer to system-specific info (ACPI, etc)

	// Resources used by device
	list_t *resources;

	// Set if this device has a matched driver
	driver_t *driver;
};

struct driver {
	char* name;

	bool (*supportsDevice)(device_t *); // function to query driver support
	void* (*getDriverData)(device_t *); // initialises a supported device
};

struct bus {
	node_t node;

	list_t *drivers; // drivers loaded for the bus
	list_t *devices; // devices on the bus
};

// Registers a bus with the driver
void hal_bus_register(bus_t*, char*);
// Gets a bus object from a name
bus_t *hal_bus_get_by_name(char*);

// Adds a device to a bus
bus_error_t hal_bus_add_device(device_t*, bus_t*);

// Registers a driver with a named bus
bus_error_t hal_bus_register_driver(driver_t*, char*);

// Tries to load drivers for devices on a bus that lack them
void hal_bus_load_drivers(char *);
