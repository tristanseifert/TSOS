#import <types.h>
#import "keyboard.h"

// Called by keyboard driver when Ctrl+Alt+Del is pressed
void hid_keyboard_sas(void) {
	KWARNING("Ctrl+Alt+Del triggered");

	uint8_t good = 0x02;
	while (good & 0x02) {
		good = io_inb(0x64);
	}

	io_outb(0x64, 0xFE);
}