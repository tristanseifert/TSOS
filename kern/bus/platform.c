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
	bus_register(platform_bus, "platform");

	return 0;
}

module_bus_init(platform_init);