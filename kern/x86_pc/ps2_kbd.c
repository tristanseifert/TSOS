#import <types.h>
#import "ps2_kbd.h"
#import "8042_ps2.h"
#import "hal/keyboard.h"

// Private functions
static void *ps2_kbd_init(device_t *);
static void ps2_kbd_byte_received(uint8_t b);

static bool ps2_kbd_special_key_down(uint32_t key);
static bool ps2_kbd_special_key_up(uint32_t key);

static void ps2_kbd_debug_modifiers(void);
static void ps2_kbd_update_leds(void);

// Driver struct
static const driver_t driver = {
	.name = "Generic PS2 Keyboard Driver",
	.supportsDevice = NULL,
	.getDriverData = ps2_kbd_init
};

// States that the keyboard driver can be in.
typedef enum {
	kStateWaitingForKey, // Waiting for a key

	kStateKeyExtended, // 0xE0 has been received

	kStateKeyRelease, // 0xF0 has been received
} ps2_kbd_states_t;

// Internal state
static i8042_ps2_device_t *ps2_dev;
static ps2_kbd_states_t state;
static unsigned int bytes_to_read;

// 0xE0 was received
static bool extended_key;
static bool printscr;

static struct {
	bool capslock, numlock;

	bool shift_l, shift_r;

	bool ctrl_l, ctrl_r;

	bool alt_l, alt_r;

	bool meta_l, meta_r;	
} modifier_state;

// Last received key
static uint32_t lastKey;

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

	// klog(kLogLevelDebug, "Initialising PS2 keyboard driver for '%s'", dev->node.name);

	// Assign byte handler
	ret->byte_from_device = ps2_kbd_byte_received;

	state = kStateWaitingForKey;

	return ret;
}

/*
 * Called when a byte is sent by the keyboard
 */
static void ps2_kbd_byte_received(uint8_t b) {
	// Ignore ACK packet
	if(b == 0xFA) {
		return;
	} else if(b == 0xFE) {
		// the keyboard sucks balls and wants resend of the LED SET
		ps2_kbd_update_leds();
		return;
	}

	// Check state
	switch(state) {
		// We are waiting for a key
		case kStateWaitingForKey: {
			extended_key = false;
			printscr = false;
			lastKey = 0;

			// Extended key?
			if(b == 0xE0) {
				state = kStateKeyExtended;
				extended_key = true;
			} else if(b == 0xF0) { // Regular key release
				state = kStateKeyRelease;
			} else { // Otherwise, it's a key code
				lastKey = b;

				if(!ps2_kbd_special_key_down(lastKey)) {
					// klog(kLogLevelDebug, "Key down: 0x%04X", (unsigned int) lastKey);
				}
			}

			break;
		}

		// 0xE0 received
		case kStateKeyExtended: {
			// If 0xF0 is received, it's a key release event
			if(b == 0xF0) {
				state = kStateKeyRelease;
			} 

			// Print screen
			else if(b == 0x12) {
				printscr = true;
				state = kStateWaitingForKey;
			} else if(b == 0x7C && printscr) {
				printscr = false;
				state = kStateWaitingForKey;

				klog(kLogLevelDebug, "PrintScr pressed");
			} 

			// Other keys
			else {
				lastKey = 0xE000 | b;
				state = kStateWaitingForKey;

				if(!ps2_kbd_special_key_down(lastKey)) {
					// klog(kLogLevelDebug, "Extended key down: 0x%04X", (unsigned int) lastKey);

					// Handle Ctrl+Alt+Del
					if(modifier_state.ctrl_l && modifier_state.alt_l && lastKey == 0xE071) {
						hid_keyboard_sas();
					}
				}
			}

			break;
		}

		// 0xF0 was previously released (key release code)
		case kStateKeyRelease: {
			if(extended_key) {
				// Print screen
				if(b == 0x7C) {
					state = kStateWaitingForKey;
				} else if(b == 0x12) {
					klog(kLogLevelDebug, "PrintScr released");
				} else {
					state = kStateWaitingForKey;
					lastKey = 0xE000 | b;
					
					if(!ps2_kbd_special_key_up(lastKey)) {
						// klog(kLogLevelDebug, "Extended key up: 0x%04X", (unsigned int) lastKey);
					}
				}
			} else {
				state = kStateWaitingForKey;
				lastKey = b;

				if(!ps2_kbd_special_key_up(lastKey)) {
					// klog(kLogLevelDebug, "Regular key up: 0x%04X", (unsigned int) lastKey);
				}
			}

			break;
		}

		default:
			klog(kLogLevelWarning, "Unhandled byte received from keyboard: 0x%02X", b);
			break;
	}
}

/*
 * Checks if the key can be interpreted as a control key. If so, returns true.
 */
static bool ps2_kbd_special_key_down(uint32_t key) {
	switch(key) {
		// Left shift
		case 0x12:
			modifier_state.shift_l = true;
			return true;
		// Right shift
		case 0x59:
			modifier_state.shift_r = true;
			return true;

		// Left ctrl
		case 0x14:
			modifier_state.ctrl_l = true;
			return true;
		// Right ctrl
		case 0xE014:
			modifier_state.ctrl_r = true;
			return true;

		// Left alt
		case 0x11:
			modifier_state.alt_l = true;
			return true;
		// Right alt
		case 0xE011:
			modifier_state.alt_r = true;
			return true;

		// Left meta
		case 0xE01F:
			modifier_state.meta_l = true;
			return true;
		// Right meta
		case 0xE027:
			modifier_state.meta_r = true;
			return true;

		// Caps lock
		case 0x58:
			modifier_state.capslock = !modifier_state.capslock;
			ps2_kbd_update_leds();
			return true;

		// Numlock
		case 0x77:
			modifier_state.numlock = !modifier_state.numlock;
			ps2_kbd_update_leds();
			return true;

		default:
			return false;
	}
}

/*
 * Checks if the key can be interpreted as a control key. If so, returns true.
 */
static bool ps2_kbd_special_key_up(uint32_t key) {
	switch(key) {
		// Left shift
		case 0x12:
			modifier_state.shift_l = false;
			return true;
		// Right shift
		case 0x59:
			modifier_state.shift_r = false;
			return true;

		// Left ctrl
		case 0x14:
			modifier_state.ctrl_l = false;
			return true;
		// Right ctrl
		case 0xE014:
			modifier_state.ctrl_r = false;
			return true;

		// Left alt
		case 0x11:
			modifier_state.alt_l = false;
			return true;
		// Right alt
		case 0xE011:
			modifier_state.alt_r = false;
			return true;

		// Left meta
		case 0xE01F:
			modifier_state.meta_l = false;
			return true;
		// Right meta
		case 0xE027:
			modifier_state.meta_r = false;
			return true;


		// Caps lock or numlock
		case 0x58:
		case 0x77:
			return true;

		default:
			return false;
	}
}

/*
 * Debug: print modifier key state
 */
static void ps2_kbd_debug_modifiers(void) {
	klog(kLogLevelDebug, "Shift: %u %u, Ctrl: %u %u, Alt: %u %u, Meta: %u %u, Caps: %u, Num: %u", modifier_state.shift_l, modifier_state.shift_r, modifier_state.ctrl_l, modifier_state.ctrl_r, modifier_state.alt_l, modifier_state.alt_r, modifier_state.meta_l, modifier_state.meta_r, modifier_state.capslock, modifier_state.numlock);
}

/*
 * Updates the state of the LEDs.
 */
static void ps2_kbd_update_leds(void) {
	uint8_t ledState = 0;

	// Apply required bits
	if(modifier_state.capslock) {
		ledState |= (1 << 2);
	} if(modifier_state.numlock) {
		ledState |= (1 << 1);
	}

	i8042_send(ps2_dev, 0xED);
	i8042_send(ps2_dev, ledState);
}