#import <types.h>
#import "bus.h"
#import "platform.h"

// Global variables
bus_t *platform_bus;

/*
 * Initialises the platform bus, and adds the standard devices that an x86
 * machine has, as well as those in the DSDT that match a list of PNP IDs.
 */
static int platform_init(void) {
	platform_bus = (bus_t *) kmalloc(sizeof(platform_bus));
	memclr(platform_bus, sizeof(platform_bus));

	// Register bus
	bus_register(platform_bus, PLATFORM_BUS_NAME);

	// Register PS2 Controller
	device_t *ps2 = kmalloc(sizeof(device_t));
	memclr(ps2, sizeof(device_t));

	ps2->connection = kDeviceConnectionInternal;
	ps2->node.name = "i8042 PS2 Controller";

	bus_add_device(ps2, platform_bus);

	// Register RTC
	device_t *rtc = kmalloc(sizeof(device_t));
	memclr(rtc, sizeof(device_t));

	rtc->connection = kDeviceConnectionInternal;
	rtc->node.name = "CMOS Clock";

	bus_add_device(rtc, platform_bus);

	return 0;
}

module_bus_init(platform_init);