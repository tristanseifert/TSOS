#import <types.h>
#import "8254_pit.h"
#import "8259_pic.h"

#define PIT_IO_PORT		0x40
#define PIT_IO_CMD		0x43

/*
 * Updates the mode of a channel.
 */
void i8254_set_mode(uint8_t channel, i8254_mode_t mode) {
	channel &= 0x03;

	// Select channel, lobyte/hibyte mode, binary
	uint8_t value = (channel << 6) | 0x30;

	io_outb(PIT_IO_CMD, value | ((mode & 0x7) << 1));
}

/*
 * Sets number of ticks between IRQs for a certain channel.
 */
void i8254_set_ticks(uint8_t channel, uint16_t ticks) {
	channel &= 0x03;

	// Disable timer interrupts
	// i8259_set_mask(0);

	// Set ticks
	io_outb(PIT_IO_PORT + channel, ticks & 0xFF);
	io_outb(PIT_IO_PORT + channel, (ticks & 0xFF00) >> 8);

	// Allow timer interrupts again
	// i8259_clear_mask(0);
}