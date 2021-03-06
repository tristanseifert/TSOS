#import <types.h>
#import "bus.h"

#define DEBUG_DRIVER_REG	0
#define DEBUG_DEVICE_REG	0
#define DEBUG_BUS_REG		0
#define DEBUG_DRIVER_MATCH	0

static hashmap_t *busses;
static list_t *bus_names;

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
static int hal_bus_sys_init(void) {
	root.children = list_allocate();
	bus_names = list_allocate();
	busses = hashmap_allocate();

	KSUCCESS("Bus subsystem initialised");

	return 0;
}
module_early_init(hal_bus_sys_init);

/*
 * Called once all drivers are loaded to match them to a device.
 */
int hal_bus_match_devices(void) {
	char *name;
	bus_t *bus;
	device_t *device;
	driver_t *driver;

	// Loop through all the busses
	for(unsigned int i = 0; i < bus_names->num_entries; i++) {
		// Get name and bus
		name = (char *) list_get(bus_names, i);
		hal_bus_load_drivers(name);
	}

	return 0;
}
module_post_driver_init(hal_bus_match_devices);

/*
 * Tries to match devices that haven't got a loaded driver with a driver. This
 * is useful for hotplug events and whatnot.
 */
void hal_bus_load_drivers(char *name) {
	bus_t *bus = (bus_t *) hashmap_get(busses, name);
	driver_t *driver;
	device_t *device;

	#if DEBUG_DRIVER_MATCH
	KDEBUG("Matching drivers on bus '%s'", name);
	#endif

	// make sure the bus exists
	if(bus) {
		// Iterate over all the devices
		for(unsigned int j = 0; j < bus->devices->num_entries; j++) {
			device = (device_t *) list_get(bus->devices, j);

			// Does this device have a driver loaded?
			if(!device->driver) {
				#if DEBUG_DRIVER_MATCH
				KDEBUG(" Device '%s'", device->node.name);
				#endif
				
				// Iterate through all drivers
				for(unsigned int k = 0; k < bus->drivers->num_entries; k++) {
					driver = (driver_t *) list_get(bus->drivers, k);

					// This driver supports this device
					if(driver->supportsDevice(device)) {

						// Run driver data fetching method, if defined
						if(driver->getDriverData) {
							void *driverInfo = driver->getDriverData(device);

							// Ignore drivers that return NULL
							if(driverInfo) {
								device->device_info = driverInfo;
								device->driver = driver;
							}
						}

						#if DEBUG_DRIVER_MATCH
						KDEBUG("  Found driver '%s': 0x%X 0x%X", driver->name, (unsigned int) driver, (unsigned int) device->device_info);
						#endif
					}
				}
			}
		}
	}
}

/*
 * Registers a bus with the system.
 */
void hal_bus_register(bus_t *bus, char *name) {
	// Check if name is used already
	if(hashmap_get(busses, name) != NULL) {
		#if DEBUG_BUS_REG
		KERROR("A bus named '%s' is already registered!", name);
		#endif

		return;
	}

	// If not, we can proceed.
	bus->drivers = list_allocate();
	bus->devices = list_allocate();

	bus->node.name = name;
	bus->node.parent = &root;
	bus->node.children = list_allocate();

	// Add to root bus
	list_add(root.children, bus);

	// Remember name of the bus
	list_add(bus_names, name);

	// Save bus
	hashmap_insert(busses, name, (void *) bus);

	// Store a pointer to the list in the bus structure
	bus->drivers = list_allocate();

	#if DEBUG_BUS_REG
	KDEBUG("Registered bus '%s'", name);
	#endif
}

/*
 * Registers a driver for a certain bus.
 */
bus_error_t hal_bus_register_driver(driver_t *driver, char* busName) {
	bus_t *bus = (bus_t *) hashmap_get(busses, busName);

	if(bus != NULL) {
		// Insert driver into the array.
		list_add(bus->drivers, driver);
		
		#if DEBUG_DRIVER_REG
		KDEBUG("Initialised driver '%s' for bus '%s'", driver->name, busName);
		#endif

		return kHALBusNoError;
	} else {
		#if DEBUG_DRIVER_REG
		KERROR("Attempted to register driver for bus '%s' without such a bus", busName);
		#endif

		return kHALBusNotExistant;
	}
}

/*
 * Tries to find a bus with the specified name.
 */
bus_t *hal_bus_get_by_name(char *name) {
	return (bus_t *) hashmap_get(busses, name);
}

/*
 * Adds a device to the specified bus.
 */
bus_error_t hal_bus_add_device(device_t *device, bus_t *bus) {
	// Ensure the device doesn't already exist
	if(!list_contains(bus->devices, device)) {
		// Add to bus
		list_add(bus->devices, device);

		// Add to bus node as a child
		list_add(bus->node.children, device);

		#if DEBUG_DEVICE_REG
		KDEBUG("Registered '%s' on bus '%s'", device->node.name, bus->node.name);
		#endif
	} else {
		#if DEBUG_DEVICE_REG
		KERROR("Bus device '%s' contains device '%s' already!", bus->node.name, device->node.name);
		#endif

		return kHALDeviceAlreadyRegistered;
	}

	return kHALBusNoError;
}