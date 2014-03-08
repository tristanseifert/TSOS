#import <types.h>
#import "bus/bus.h"
#import "8042_ps2.h"
#import "interrupts.h"

#import "ps2_kbd.h"

#import "task/systimer.h"

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

static bool i8042_send_byte(uint8_t, uint8_t);
static void i8042_load_driver(i8042_ps2_device_t *);

static void i8042_irq_port1(void);
static void i8042_irq_port2(void);

static void i8042_flush_send_queue(void);

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
	// Install IRQs
	irq_register_handler(1, i8042_irq_port1);
	irq_register_handler(12, i8042_irq_port2);

	// Set up memory for the structs
	i8042_ps2_t *info = kmalloc(sizeof(i8042_ps2_t));
	memclr(info, sizeof(i8042_ps2_t));

	info->bus_device = dev;
	shared_driver = info;

	// Device states
	info->devices[0].state = kI8042StateNone;
	info->devices[1].state = kI8042StateNone;

	// Disable output from attached devices
	i8042_wait_input_buffer();
	io_outb(I8042_COMMAND_PORT, 0xAD);
	i8042_wait_input_buffer();
	io_outb(I8042_COMMAND_PORT, 0xA7);

	// Flush output buffer
	io_inb(I8042_DATA_PORT);

	// Assume dual-port controller (all supported machines should have one)
	info->isDualPort = true;

	// Disable IRQs and translation
	i8042_wait_input_buffer();
	io_outb(I8042_COMMAND_PORT, 0x60);
	i8042_wait_input_buffer();
	io_outb(I8042_DATA_PORT, 0x30);

	// Perform self-test
	i8042_wait_input_buffer();
	io_outb(I8042_COMMAND_PORT, 0xAA);
	uint8_t self_test_response = i8042_read_byte_polling();

	if(self_test_response != 0x55) {
		klog(kLogLevelError, "i8042: Faulty controller (self-test response 0x%X)", self_test_response);
		return NULL;
	} else {
		/*if(info->isDualPort) {
			klog(kLogLevelSuccess, "i8042: Dual-port controller at 0x%X", I8042_DATA_PORT);
		} else {
			klog(kLogLevelSuccess, "i8042: Single-port controller at 0x%X", I8042_DATA_PORT);
		}*/
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

	info->devices[0].port = 0;

	// Enable second port
	i8042_wait_input_buffer();
	io_outb(I8042_COMMAND_PORT, 0xA9);
	self_test_response = i8042_read_byte_polling();

	if(self_test_response != 0x00) {
		klog(kLogLevelError, "i8042: Port 2 failed (0x%X)", self_test_response);
		info->devices[1].isUsable = false;
	} else {
		info->devices[1].isUsable = true;
	}

	info->devices[1].port = 1;

	// Enable PS2 ports
	i8042_wait_input_buffer();
	io_outb(I8042_COMMAND_PORT, 0xAE);
	
	// Enable second port
	i8042_wait_input_buffer();
	io_outb(I8042_COMMAND_PORT, 0xA8);

	// Enable IRQs
	i8042_wait_input_buffer();
	io_outb(I8042_COMMAND_PORT, 0x20);
	uint8_t ctrl_reg = i8042_read_byte_polling();

	ctrl_reg |= 0x03;

	// Write register back to controller
	i8042_wait_input_buffer();
	io_outb(I8042_COMMAND_PORT, 0x60);
	i8042_wait_input_buffer();
	io_outb(I8042_DATA_PORT, ctrl_reg);

	i8042_send_byte(0, 0xFF);

	// Reset device on second port
	i8042_send_byte(1, 0xFF);

	// Register handler for send queue flushage
	kern_timer_register_handler(i8042_flush_send_queue);

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
 * Sends a byte to the specified device's port: accessed by device drivers
 */
void i8042_send(i8042_ps2_device_t *d, uint8_t b) {
	i8042_send_byte(d->port, b);
}

/*
 * Sends a byte to the specified PS2 port.
 */
static bool i8042_send_byte(uint8_t port, uint8_t command) {
	port &= 0x01;
	i8042_ps2_device_t *dev = &shared_driver->devices[port];

	// Don't write past the end of the buffer
	if(dev->sendqueue_writeoff == I8042_SENDQUEUE_SIZE) {
		dev->sendqueue_writeoff = 0;
	}

	// Stuff into the send queue
	dev->sendqueue[dev->sendqueue_writeoff++] = command;
	dev->sendqueue_bytes_waiting++;

	return true;
}

/*
 * IRQ callback for when the first device sends a byte to the controller
 */
static void i8042_irq_port1(void) {
	uint8_t byte = io_inb(I8042_DATA_PORT);
	// klog(kLogLevelDebug, "Received 0x%02X from PS2 port 0", byte);

	// State machine
	switch(shared_driver->devices[0].state) {
		// Waiting for reset ack
		case kI8042StateWaitingForResetAck: {
			if(byte == 0xFA) {
				shared_driver->devices[0].state = kI8042StateWaitingForPOST;
				shared_driver->devices[0].connected = true;
			} else {
				shared_driver->devices[0].state = kI8042StateFailed;
			}

			break;
		}

		// Waiting for POST response
		case kI8042StateWaitingForPOST: {
			if(byte == 0xAA) {
				// klog(kLogLevelSuccess, "Device on port 0 reset successfully");

				shared_driver->devices[0].state = kI8042StateWaitingDisableScanningAck;
				i8042_send_byte(0, 0xF5);
			} else {
				klog(kLogLevelError, "Device on port 0 failed (0x%X)", byte);
			}

			break;
		}

		// Waiting for acknowledge to "disable scanning" command
		case kI8042StateWaitingDisableScanningAck: {
			if(byte == 0xFA) {
				shared_driver->devices[0].state = kI8042StateWaitingIdentifyAck;
				i8042_send_byte(0, 0xF2);
			}

			break;
		}

		// Waiting for acknowledge to "identify" command
		case kI8042StateWaitingIdentifyAck: {
			if(byte == 0xFA) {
				shared_driver->devices[0].state = kI8042StateWaitingIdentifyResponse;
			}

			break;
		}

		// Waiting for response to "identify" command, up to two bytes
		// This will time out for the second byte after a while by registering
		// a timer for 100ms from now
		case kI8042StateWaitingIdentifyResponse: {
			shared_driver->devices[0].identify[shared_driver->devices[0].identify_bytes_read] = byte;

			// Increment identify response offset
			shared_driver->devices[0].identify_bytes_read++;

			// Two identify bytes have been read, so this device is ready for use now.
			if(shared_driver->devices[0].identify_bytes_read == 2) {
				shared_driver->devices[0].state = kI8042StateWaitingEnableScanningAck;

				// Send the "enable scanning" command.
				i8042_send_byte(0, 0xF4);
			} else if(shared_driver->devices[0].identify_bytes_read == 1 && byte != 0xAB) {
				// The device is NOT a keyboard, so expect only one byte.
				shared_driver->devices[0].state = kI8042StateWaitingEnableScanningAck;

				// Send the "enable scanning" command.
				i8042_send_byte(0, 0xF4);
			}

			// klog(kLogLevelDebug, "Identify response byte: 0x%X (byte %u)", byte, shared_driver->devices[0].identify_bytes_read);
			break;
		}

		// Acknowledge byte for "enable scanning" command
		case kI8042StateWaitingEnableScanningAck: {
			if(byte == 0xFA) {
				// klog(kLogLevelSuccess, "i8042: device on port 0 initialised: 0x%02X%02X", shared_driver->devices[0].identify[0], shared_driver->devices[0].identify[1]);

				// Determine driver
				i8042_load_driver(&shared_driver->devices[0]);

				// Go to ready state
				shared_driver->devices[0].state = kI8042StateReady;
				shared_driver->devices[0].isUsable = true;
			} else {
				klog(kLogLevelError, "i8042: device on port 0 failed to enable");
			}

			break;
		}

		// Data should be forwarded to the proper device driver in this state
		case kI8042StateReady: {
			i8042_ps2_device_driver_t *drv = shared_driver->devices[0].device.device_info;

			if(drv->byte_from_device) {
				drv->byte_from_device(byte);
			}

			break;
		}

		default: {
			klog(kLogLevelDebug, "Unhandled data 0x%X from PS2 port 0", byte);
			break;
		}
	}
}

/*
 * Called when the device on the second port sends a message to the controller.
 */
static void i8042_irq_port2(void) {
	uint8_t byte = io_inb(I8042_DATA_PORT);
	// klog(kLogLevelDebug, "Received 0x%02X from PS2 port 1", byte);

	// State machine
	switch(shared_driver->devices[1].state) {
		// Waiting for reset ack
		case kI8042StateWaitingForResetAck: {
			if(byte == 0xFA) {
				shared_driver->devices[1].state = kI8042StateWaitingForPOST;
				shared_driver->devices[1].connected = true;
			} else {
				shared_driver->devices[1].state = kI8042StateFailed;
			}

			break;
		}

		// Waiting for POST response
		case kI8042StateWaitingForPOST: {
			if(byte == 0xAA) {
				// klog(kLogLevelSuccess, "Device on port 1 reset successfully");

				shared_driver->devices[1].state = kI8042StateWaitingDisableScanningAck;
				i8042_send_byte(1, 0xF5);
			} else {
				klog(kLogLevelError, "Device on port 1 failed");
			}

			break;
		}

		// Waiting for acknowledge to "disable scanning" command
		case kI8042StateWaitingDisableScanningAck: {
			if(byte == 0xFA) {
				shared_driver->devices[1].state = kI8042StateWaitingIdentifyAck;
				i8042_send_byte(1, 0xF2);
			}

			break;
		}

		// Waiting for acknowledge to "identify" command
		case kI8042StateWaitingIdentifyAck: {
			if(byte == 0xFA) {
				shared_driver->devices[1].state = kI8042StateWaitingIdentifyResponse;
			}

			break;
		}

		// Waiting for response to "identify" command, up to two bytes
		// This will time out for the second byte after a while by registering
		// a timer for 100ms from now
		case kI8042StateWaitingIdentifyResponse: {
			shared_driver->devices[1].identify[shared_driver->devices[1].identify_bytes_read] = byte;

			// Increment identify response offset
			shared_driver->devices[1].identify_bytes_read++;

			// Two identify bytes have been read, so this device is ready for use now.
			if(shared_driver->devices[1].identify_bytes_read == 2) {
				shared_driver->devices[1].state = kI8042StateWaitingEnableScanningAck;

				// Send the "enable scanning" command.
				i8042_send_byte(1, 0xF4);
			} else if(shared_driver->devices[1].identify_bytes_read == 1 && byte != 0xAB) {
				// The device is NOT a keyboard, so expect only one byte.
				shared_driver->devices[1].state = kI8042StateWaitingEnableScanningAck;

				// Send the "enable scanning" command.
				i8042_send_byte(1, 0xF4);
			}

			// klog(kLogLevelDebug, "Identify response byte: 0x%X (byte %u)", byte, shared_driver->devices[1].identify_bytes_read);
			break;
		}

		// Acknowledge byte for "enable scanning" command
		case kI8042StateWaitingEnableScanningAck: {
			if(byte == 0xFA) {
				// klog(kLogLevelSuccess, "i8042: device on port 1 initialised: 0x%02X%02X", shared_driver->devices[1].identify[0], shared_driver->devices[1].identify[1]);

				// Load driver
				i8042_load_driver(&shared_driver->devices[1]);

				// Go to ready state
				shared_driver->devices[1].state = kI8042StateReady;
				shared_driver->devices[1].isUsable = true;
			} else {
				klog(kLogLevelError, "i8042: device on port 1 failed to enable");
			}

			break;
		}

		// Data should be forwarded to the proper device driver in this state
		case kI8042StateReady: {
			break;
		}

		default: {
			// klog(kLogLevelDebug, "Unhandled data 0x%X from PS2 port 1", byte);
			break;
		}
	}
}

/*
 * Flush each device's send queue. Note that this gives priority to bytes being
 * sent to the devie on port 0.
 */
static void i8042_flush_send_queue(void) {
	bool bytesSent = false;

	// First, ensure that the i8042 can accept bytes at this time
	if(io_inb(I8042_STATUS_PORT) & 0x02) return;

	// See if each port has a byte to send.
	for(int port = 0; port < 2; port++) {
		i8042_ps2_device_t *dev = &shared_driver->devices[port];

		// There's bytes to send on this port
		if(dev->sendqueue_bytes_waiting != 0 && dev->isUsable) {
			// Is the device being reset?
			if(dev->sendqueue[dev->sendqueue_readoff] == 0xFF) {
				// Update state machine
				dev->state = kI8042StateWaitingForResetAck;
				// klog(kLogLevelDebug, "i8042: Resetting device %u", port);
			}

			// Each port has a different send procedure
			if(port == 0) {
				io_outb(I8042_DATA_PORT, dev->sendqueue[dev->sendqueue_readoff]);

				// Increment read pointer
				dev->sendqueue_bytes_waiting--;
				bytesSent = true;
			} else { // port 2
				// SEND TO SECOND PORT command
				io_outb(I8042_COMMAND_PORT, 0xD4);
				i8042_wait_input_buffer();

				io_outb(I8042_DATA_PORT, dev->sendqueue[dev->sendqueue_readoff]);

				// Increment read pointer
				dev->sendqueue_bytes_waiting--;
				bytesSent = true;
			}

			// Send next byte on the next loop
			if(bytesSent) {
				// klog(kLogLevelDebug, "Sent 0x%02X to port %u", dev->sendqueue[dev->sendqueue_readoff], port);

				// Read the next byte
				if(dev->sendqueue_readoff++ == I8042_SENDQUEUE_SIZE) {
					dev->sendqueue_readoff = 0;
					dev->sendqueue_bytes_waiting--;
				}

				return;
			}
		}
	}
}

/*
 * Determines the type of device and loads the appropriate driver. Also adds
 * the device as a child of the i8042 controller.
 */
static void i8042_load_driver(i8042_ps2_device_t *device) {
	if(device->identify[0] == 0xAB) {
		// PS2 keyboard
		device->type = kI8042DeviceKeyboard;
		device->device.node.name = "Generic PS/2 Keyboard";

		device->device.driver = ps2_kbd_driver();

		// klog(kLogLevelDebug, "i8042: keyboard on port %u", device->port);
	} else if(device->identify[0] == 0x00 || device->identify[0] == 0x03) {
		// Regular PS2 mouse or scrollwheel mouse
		device->type = kI8042DeviceMouse;
		device->device.node.name = "Generic PS/2 Mouse";

		// klog(kLogLevelDebug, "i8042: mouse on port %u", device->port);
	} else {
		// All other kinds of devices
		device->type = kI8042DeviceUnknown;
		return;
	}

	// Configure device object
	device->device.connection = kDeviceConnectionOther;
	device->device.bus_info = device;

	// Parent is PS2 controller, but there may not be children on this device
	device->device.node.parent = &shared_driver->bus_device->node;
	device->device.node.children = NULL;

	// Initialise driver, if needed
	if(device->device.driver) {
		device->device.device_info = device->device.driver->getDriverData(&device->device);
	}
}