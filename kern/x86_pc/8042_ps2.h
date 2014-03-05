#import <types.h>

typedef struct i8042_ps2 i8042_ps2_t;
typedef struct i8042_ps2_device i8042_ps2_device_t;

// Types of devices supported
typedef enum {
	kI8042DeviceNone = -1,
	kI8042DeviceKeyboard = 1,
	kI8042DeviceMouse = 2
} i8042_device_type_t;

// Device state
typedef enum {
	kI8042StateFailed = -2,
	kI8042StateNone = -1,
	kI8042StateWaitingForResetAck,
	kI8042StateWaitingForPOST,
	kI8042StateWaitingReady
} i8042_device_state_t;

// Device struct
struct i8042_ps2_device {
	bool isUsable;
	bool connected;

	i8042_device_type_t type;
	i8042_device_state_t state;

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