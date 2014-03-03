#import <types.h>
#import "bus.h"

#import "runtime/hashmap.h"

static list_t *bus_list;
static hashmap_t *driver_array;

// Type inserted into the bus to driver hashmap
typedef struct {
	bus_t *bus;
	list_t *drivers;
} bus_drivers_t;

/*
 * The parent of all busses.
 */
static node_t root = {
	.name = "/",
	.parent = NULL
};

/*
 * Initialises the bus subsystem.
 */
static int bus_sys_init(void) {
	bus_list = list_allocate();
	root.children = list_allocate();

	driver_array = hashmap_allocate();

	klog(kLogLevelDebug, "Bus subsystem initialised");

	return 0;
}

module_early_init(bus_sys_init);

/*
 * Registers a bus with the system.
 */
void bus_register(bus_t *bus, char *name) {
	// Check if name is used already
	if(unlikely(hashmap_get(driver_array, name) != NULL)) {
		klog(kLogLevelError, "A bus named '%s' is already registered!", name);
		return;
	}

	// If not, we can proceed.
	bus->drivers = list_allocate();
	bus->devices = list_allocate();

	bus->node.name = name;
	bus->node.parent = &root;
	bus->node.children = list_allocate();

	list_add(bus_list, bus);
	list_add(root.children, bus);

	// Allocate a structure to shove into the driver array
	bus_drivers_t *drivers = (bus_drivers_t *) kmalloc(sizeof(bus_drivers_t));
	memclr(drivers, sizeof(bus_drivers_t));

	drivers->bus = bus;
	drivers->drivers = list_allocate();

	hashmap_insert(driver_array, name, drivers);

	// Store a pointer to the list in the bus structure
	bus->drivers = drivers->drivers;

	klog(kLogLevelDebug, "Registered bus '%s'", name);
}

/*
 * Registers a driver for a certain bus.
 */
int bus_register_driver(driver_t *driver, char* busName) {
	bus_drivers_t *drivers = hashmap_get(driver_array, busName);

	if(likely(drivers != NULL)) {
		// Make sure we don't register the same driver twice
		if(list_contains(drivers->drivers, driver)) {
			return BUS_DRIVER_ALREADY_REGISTERED;
		}

		// Insert driver into the array.
		list_add(drivers->drivers, driver);
		klog(kLogLevelDebug, "Initialised driver '%s' for bus '%s'", driver->name, busName);

		return 0;
	} else {
		klog(kLogLevelError, "Attempted to register driver for bus '%s' without such a bus", busName);
		return BUS_NOT_EXISTANT;
	}
}

/*
 * Tries to find a bus with the specified name.
 */
bus_t *bus_get_by_name(char *name) {
	bus_drivers_t *drivers = hashmap_get(driver_array, name);

	if(likely(drivers)) {
		return drivers->bus;
	}

	return NULL;
}

/*
 * Enumerates all drivers registered for the specified bus to find one that can
 * support the device.
 */
driver_t *bus_find_driver(device_t *device, bus_t *bus) {
	driver_t *driver;

	// Loop through all drivers for the bus
	for(int i = 0; i < bus->drivers->num_entries; i++) {
		driver = list_get(bus->drivers, i);

		if(driver->supportsDevice(device)) {
			return driver;
		}
	}

	klog(kLogLevelWarning, "Couldn't find driver for '%s' on '%s'", device->node.name, bus->node.name);

	// We haven't found a driver that supports it.
	return NULL;
}

/*
 * Adds a device to the specified bus.
 */
int bus_add_device(device_t *device, bus_t *bus) {
	return BUS_DEVICE_REGISTERED;
}