#import <types.h>
#import "hal/bus.h"
#import "platform.h"

// Global variables
static bus_t platform_bus;

// Various devices
static const device_t ps2 = {
	.connection = kDeviceConnectionInternal,
	.node.name = "i8042 PS2 Controller"
};

static const device_t rtc = {
	.connection = kDeviceConnectionInternal,
	.node.name = "CMOS Clock"
};

/*
 * Initialises the platform bus, and adds the standard devices that an x86
 * machine has, as well as those in the DSDT that match a list of PNP IDs.
 */
static int platform_init(void) {
	// Register bus
	hal_bus_register(&platform_bus, PLATFORM_BUS_NAME);

	// Register PS2 Controller
	hal_bus_add_device((device_t *) &ps2, &platform_bus);

	// Register RTC
	hal_bus_add_device((device_t *) &rtc, &platform_bus);

	return 0;
}

module_bus_init(platform_init);