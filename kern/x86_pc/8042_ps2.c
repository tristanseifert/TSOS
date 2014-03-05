#import <types.h>
#import "bus/bus.h"
#import "8042_ps2.h"

#define DRIVER_NAME "i8042 PS2 Controller"

// Tuneables
#define I8042_READ_TIMEOUT	0x20000
#define I8042_WAIT_TIMEOUT	0x20000

// IO Ports
#define I8042_DATA_PORT		0x60
#define I8042_STATUS_PORT	0x64
#define I8042_COMMAND_PORT	0x64

// Private functions
static bool i8042_match(device_t *);
static void* i8042_init_device(device_t *);

static uint8_t i8042_read_byte_polling(void);
static bool i8042_wait_input_buffer(void);

// Driver definition
static const driver_t driver = {
	.name = DRIVER_NAME,
	.supportsDevice = i8042_match,
	.getDriverData = i8042_init_device
};

/*
 * Register the driver.
 */
static int i8042_ps2_init(void) {
	bus_register_driver((driver_t *) &driver, PLATFORM_BUS_NAME);
	return 0;
}

module_driver_init(i8042_ps2_init);

/*
 * All devices that match DRIVER_NAME will work under this driver.
 */
static bool i8042_match(device_t *dev) {
	if(strcmp(dev->node.name, DRIVER_NAME) == 0) return true;

	return false;
}

/*
 * Initialises an 8042 KBC.
 */
static void* i8042_init_device(device_t *dev) {
	i8042_ps2_t *info = kmalloc(sizeof(i8042_ps2_t));
	memclr(info, sizeof(i8042_ps2_t));

	info->device = dev;

	// Disable output from attached devices
	i8042_wait_input_buffer();
	io_outb(I8042_COMMAND_PORT, 0xAD);
	i8042_wait_input_buffer();
	io_outb(I8042_COMMAND_PORT, 0xA7);

	// Flush output buffer
	io_inb(I8042_DATA_PORT);

	// Read control register
	i8042_wait_input_buffer();
	io_outb(I8042_COMMAND_PORT, 0x20);
	uint8_t ctrl_reg = i8042_read_byte_polling();

	// Disable IRQs and translation
	ctrl_reg &= ~(0x43);

	// Is this controller dual-port? (bit 5 set)
	info->isDualPort = (ctrl_reg & 0x20);
	if(info->isDualPort) {
		klog(kLogLevelDebug, "i8042: Dual-port controller at 0x%X", I8042_DATA_PORT);
	} else {
		klog(kLogLevelDebug, "i8042: Single-port controller at 0x%X", I8042_DATA_PORT);
	}

	// Perform self-test
	i8042_wait_input_buffer();
	io_outb(I8042_COMMAND_PORT, 0xAA);
	uint8_t self_test_response = i8042_read_byte_polling();

	if(self_test_response != 0x55) {
		klog(kLogLevelError, "i8042: Faulty controller (self-test response 0x%X)", self_test_response);
		return NULL;
	}

	// Check first PS2 port
	i8042_wait_input_buffer();
	io_outb(I8042_COMMAND_PORT, 0xAB);
	self_test_response = i8042_read_byte_polling();

	if(self_test_response != 0x00) {
		klog(kLogLevelError, "i8042: Port 1 failed (0x%X)", self_test_response);
		info->devices[0].isUsable = false;
	} else {
		info->devices[0].isUsable = true;
	}

	// If there's another port, check it as well
	if(info->isDualPort) {
		i8042_wait_input_buffer();
		io_outb(I8042_COMMAND_PORT, 0xA9);
		self_test_response = i8042_read_byte_polling();

		if(self_test_response != 0x00) {
			klog(kLogLevelError, "i8042: Port 2 failed (0x%X)", self_test_response);
			info->devices[1].isUsable = false;
		} else {
			info->devices[1].isUsable = true;
		}
	}

	// Enable PS2 ports
	i8042_wait_input_buffer();
	io_outb(I8042_COMMAND_PORT, 0xAE);
	// klog(kLogLevelDebug, "i8042: Enabled port 1");
	
	if(info->isDualPort) {
		// Wait for the controller to accept another command
		i8042_wait_input_buffer();

		io_outb(I8042_COMMAND_PORT, 0xA8);
		// klog(kLogLevelDebug, "i8042: Enabled port 2");
	}

	// Reset device on port 0
	int r = 0;
	if((r = i8042_reset_device(0))) {
		klog(kLogLevelDebug, "i8042: Error resetting device 1 (%i)", r);
	} else {
		klog(kLogLevelSuccess, "i8042: Port 1 Device OK");
	}

	// Reset device on port 1, if port exists
	if(info->isDualPort) {
		if((r = i8042_reset_device(1))) {
			klog(kLogLevelDebug, "i8042: Error resetting device 2 (%i)", r);
		} else {
			klog(kLogLevelSuccess, "i8042: Port 2 Device OK");
		}
	}

	// Return driver struct
	return info;
}

/*
 * Sends a reset command to the device on the specified port.
 *
 * @param port PS2 port
 * @return -1 if timeout, 0 if success, 1 if error
 */
int i8042_reset_device(uint8_t port) {
	port &= 0x01;
	uint8_t response = 0;

	// Port 0?
	if(port == 0) {
		// Wait for the controller to be able to accept data
		if(i8042_wait_input_buffer()) {
			io_outb(I8042_DATA_PORT, 0xFF);

			response = i8042_read_byte_polling();
		} else {
			klog(kLogLevelWarning, "i8042: Timeout waiting for input buffer to clear (port 1)");
			return -1;
		}
	} else { // Port 1
		// Send to second PS2 port
		i8042_wait_input_buffer();
		io_outb(I8042_COMMAND_PORT, 0xD4);

		// Wait for the controller to be able to accept data
		if(i8042_wait_input_buffer()) {
			io_outb(I8042_DATA_PORT, 0xFF);

			response = i8042_read_byte_polling();
		} else {
			klog(kLogLevelWarning, "i8042: Timeout waiting for input buffer to clear (port 2)");
			return -1;
		}
	}

	// Process return value
	if(response == 0xFA) {
		unsigned int timeout = 0;

		// Wait for a response from the device (up to 2 seconds)
		while(!(io_inb(I8042_STATUS_PORT) & 0x01)) {
			if(timeout++ > 0x1800000) {
				PANIC("i8042 read timeout");
			}
		}

		// Read byte from device
		response = io_inb(I8042_DATA_PORT);

		if(response != 0xAA) {
			klog(kLogLevelWarning, "i8042: Device reset error (0x%X) on port %u", response, port);
			return 1;
		}

		return 0;
	} else {
		klog(kLogLevelWarning, "i8042: Device reset ACK failed (0x%X) on port %u", response, port);
		return 1;
	}
}

/*
 * Reads a byte from the PS2 controller.
 */
static uint8_t i8042_read_byte_polling(void) {
	unsigned int timeout = 0;

	// Wait for the byte to become available
	while(!(io_inb(I8042_STATUS_PORT) & 0x01)) {
		if(timeout++ > I8042_READ_TIMEOUT) {
			PANIC("i8042 read timeout");
		}
	}

	// Read from the controller
	return io_inb(I8042_DATA_PORT);
}

/*
 * Waits for the input buffer to become empty.
 */
static bool i8042_wait_input_buffer(void) {
	unsigned int timeout = 0;

	// Wait for the input port to become empty
	while((io_inb(I8042_STATUS_PORT) & 0x02)) {
		if(timeout++ > I8042_WAIT_TIMEOUT) {
			return false;
		}
	}

	return true;
}