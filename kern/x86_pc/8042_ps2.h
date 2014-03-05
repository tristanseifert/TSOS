#import <types.h>

typedef struct i8042_ps2 i8042_ps2_t;
typedef struct i8042_ps2_device i8042_ps2_device_t;

// Types of devices supported
typedef enum {
	kI8042DeviceNone = -1,
	kI8042DeviceKeyboard = 1,
	kI8042DeviceMouse = 2
} i8042_device_type_t;

// Device struct
struct i8042_ps2_device {
	bool isUsable;
	bool connected;
	i8042_device_type_t type;

	// used for driver purposes
	device_t device;
};

// Driver struct
struct i8042_ps2 {
	i8042_ps2_device_t devices[2];
	device_t *device;

	// Whether the controller has one or two ports
	bool isDualPort;
};

/*
 * Sends a reset command to the device on the specified port.
 *
 * @param port PS2 port
 * @return -1 if timeout, 0 if success, 1 if error
 */
int i8042_reset_device(uint8_t port);