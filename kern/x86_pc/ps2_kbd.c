#import <types.h>
#import "ps2_kbd.h"
#import "8042_ps2.h"

// Private functions
static void *ps2_kbd_init(device_t *);
static void ps2_kbd_byte_received(uint8_t b);

// Internal state
static i8042_ps2_device_t *ps2_dev;

// Driver struct
static const driver_t driver = {
	.name = "Generic PS2 Keyboard Driver",
	.supportsDevice = NULL,
	.getDriverData = ps2_kbd_init
};

/*
 * Return shared driver object
 */
driver_t *ps2_kbd_driver(void) {
	return (driver_t *) &driver;
}

/*
 * Initialise the sphere.
 */
static void *ps2_kbd_init(device_t *dev) {
	ps2_dev = dev->bus_info;
	i8042_ps2_device_driver_t *ret = kmalloc(sizeof(i8042_ps2_device_driver_t));

	klog(kLogLevelDebug, "Initialising PS2 keyboard driver for '%s'", dev->node.name);

	// Assign byte handler
	ret->byte_from_device = ps2_kbd_byte_received;

	return ret;
}

/*
 * Called when a byte is sent by the keyboard
 */
static void ps2_kbd_byte_received(uint8_t b) {
	klog(kLogLevelDebug, "Byte received from keyboard: 0x%02X", b);
}