#import <types.h>
#import "bus/bus.h"
#import "8042_ps2.h"
#import "interrupts.h"

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

static void i8042_irq_port1(void);
static void i8042_irq_port2(void);

// Driver definition
static const driver_t driver = {
	.name = DRIVER_NAME,
	.supportsDevice = i8042_match,
	.getDriverData = i8042_init_device
};

static i8042_ps2_t *shared_driver;

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
	shared_driver = info;

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

	// Write register back to controller
	i8042_wait_input_buffer();
	io_outb(I8042_COMMAND_PORT, 0x60);
	io_outb(I8042_DATA_PORT, ctrl_reg);

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

	// Register IRQs
	irq_register_handler(1, i8042_irq_port1);
	irq_register_handler(12, i8042_irq_port2);

	// Enable IRQs
	i8042_wait_input_buffer();
	io_outb(I8042_COMMAND_PORT, 0x20);
	ctrl_reg = i8042_read_byte_polling();

	ctrl_reg |= 0x03;

	// Write register back to controller
	i8042_wait_input_buffer();
	io_outb(I8042_COMMAND_PORT, 0x60);
	io_outb(I8042_DATA_PORT, ctrl_reg);

	// Reset device on port 0
	info->devices[0].state = kI8042StateWaitingForResetAck;

	if(i8042_wait_input_buffer()) {
		io_outb(I8042_DATA_PORT, 0xFF);
	} else {
		klog(kLogLevelWarning, "i8042: Timeout waiting for input buffer to clear (port 1)");
	}

	// Reset device on port 1, if port exists
	if(info->isDualPort) {
		info->devices[1].state = kI8042StateWaitingForResetAck;

		i8042_wait_input_buffer();
		io_outb(I8042_COMMAND_PORT, 0xD4);

		if(i8042_wait_input_buffer()) {
			io_outb(I8042_DATA_PORT, 0xFF);
		} else {
			klog(kLogLevelWarning, "i8042: Timeout waiting for input buffer to clear (port 2)");
		}
	}

	// Return driver struct
	return info;
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

/*
 * IRQ callback for when the first device sends a byte to the controller
 */
static void i8042_irq_port1(void) {
	uint8_t byte = io_inb(I8042_DATA_PORT);

	// State machine
	switch(shared_driver->devices[0].state) {
		// Waiting for reset ack
		case kI8042StateWaitingForResetAck: {
			if(byte == 0xFA) {
				shared_driver->devices[0].state = kI8042StateWaitingForPOST;
			} else {
				shared_driver->devices[0].state = kI8042StateFailed;
			}

			break;
		}

		// Waiting for POST response
		case kI8042StateWaitingForPOST: {
			if(byte == 0xAA) {
				shared_driver->devices[0].state = kI8042StateWaitingReady;
				klog(kLogLevelSuccess, "Device on port 0 successed");
			} else {
				klog(kLogLevelError, "Device on port 0 failed");
			}

			break;
		}

		default: {
			klog(kLogLevelDebug, "Read 0x%X from port 1", byte);
			break;
		}
	}
}

/*
 * Called when the device on the second port sends a message to the controller.
 */
static void i8042_irq_port2(void) {
	uint8_t byte = io_inb(I8042_DATA_PORT);

	// State machine
	switch(shared_driver->devices[1].state) {
		// Waiting for reset ack
		case kI8042StateWaitingForResetAck: {
			if(byte == 0xFA) {
				shared_driver->devices[1].state = kI8042StateWaitingForPOST;
			} else {
				shared_driver->devices[1].state = kI8042StateFailed;
			}

			break;
		}

		// Waiting for POST response
		case kI8042StateWaitingForPOST: {
			if(byte == 0xAA) {
				shared_driver->devices[1].state = kI8042StateWaitingReady;
				klog(kLogLevelSuccess, "Device on port 1 successed");
			} else {
				klog(kLogLevelError, "Device on port 1 failed");
			}

			break;
		}

		default: {
			klog(kLogLevelDebug, "Read 0x%X from port 2", byte);
			break;
		}
	}
}