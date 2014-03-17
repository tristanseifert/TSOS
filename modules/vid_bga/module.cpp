#import <module.h>

// Initialisers
extern "C" void _init(void);

// Private functions
static bool supportsDevice(device_t *dev);
static void *initDriver(device_t *dev);

// Module definition
static const module_t mod = {
	/*.name = */ MODULE_NAME
};

// PCI device
static const driver_t drv = {
	/* .name = */ (char *) "Bochs Graphics Adapter Driver",
	/* .supportsDevice = */ supportsDevice,
	/* . getDriverData = */ initDriver
};

/*
 * Initialisation function called by kernel
 */
extern "C" {
	 __attribute__ ((section (".module_init"))) module_t *start(void) {
		// Call constructors and whatnot
		_init();

		// Register driver
		hal_bus_register_driver((driver_t *) &drv, (char *) BUS_NAME_PCI);

		return (module_t *) &mod;
	}
}

/*
 * Determines if the device can be supported.
 */
static bool supportsDevice(device_t *dev) {
	KDEBUG("device: 0x%08X", (unsigned int) dev);
	return false;
}

/*
 * Initialises the driver on the specific device.
 */
static void *initDriver(device_t *dev) {
	return NULL;
}