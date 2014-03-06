#import <types.h>

#define I8042_SENDQUEUE_SIZE	32

typedef struct i8042_ps2 i8042_ps2_t;
typedef struct i8042_ps2_device i8042_ps2_device_t;

// Types of devices supported
typedef enum {
	kI8042DeviceNone = -1,
	kI8042DeviceKeyboard = 1,
	kI8042DeviceMouse,
	kI8042DeviceUnknown
} i8042_device_type_t;

// Device state
typedef enum {
	kI8042StateFailed = -2,
	kI8042StateNone = -1,
	kI8042StateWaitingForResetAck,
	kI8042StateWaitingForPOST,
	kI8042StateWaitingDisableScanningAck,
	kI8042StateWaitingIdentifyAck,
	kI8042StateWaitingIdentifyResponse,
	kI8042StateWaitingEnableScanningAck,

	kI8042StateReady // ready for device drivers to accept data
} i8042_device_state_t;

// Device struct
struct i8042_ps2_device {
	uint8_t port;
	bool isUsable;
	bool connected;

	i8042_device_type_t type;
	i8042_device_state_t state;

	// used for driver purposes
	device_t device;

	// Identify response
	uint8_t identify[2];
	uint8_t identify_bytes_read;

	// Send queue
	uint8_t sendqueue[I8042_SENDQUEUE_SIZE];
	uint8_t sendqueue_bytes;
};

// Driver struct
struct i8042_ps2 {
	i8042_ps2_device_t devices[2];
	device_t *bus_device;

	// Whether the controller has one or two ports
	bool isDualPort;
};