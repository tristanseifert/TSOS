#import <types.h>
#import "vga_console.h"

unsigned int vga_x, vga_y;
uint16_t *video_memory = (uint16_t *) 0xB8000;

// Private functions
static void vga_scroll_up(void);
static void vga_move_cursor();

/*
 * Prints a character to the console, except newlines.
 */
void vga_console_putchar(char c) {
	// Process newline
	if(unlikely(c == '\n')) {
		vga_x = 0;
		vga_y++;
		return;
	} else if(unlikely(c == '\r')) {
		vga_x = 0;
		return;
	}

	// Write to VGA memory (BG black, FG white)
	video_memory[vga_x + (vga_y * 80)] = (((0 << 4) | (15 & 0x0F)) << 0x08) | c;

	// Next column
	vga_x++;

	// Line wrapping
	if(vga_x >= 80) {
		vga_x = 0;
		vga_y++;
	}

	// Scroll lines up if needed
	if(vga_y >= 25) {
		vga_scroll_up();
	}

	// Update cursor location
	vga_move_cursor();
}

/*
 * Scrolls the video memory up one line
 */
static void vga_scroll_up(void) {
	uint16_t space = (((0 << 4) | (15 & 0x0F)) << 0x8) | 0x20;

	// Move everything upwards
	for (int i = 0; i < 24*80; i++) {
		video_memory[i] = video_memory[i + 80];
	}

	// Clear bottom line
	for (int i = 24*80; i < 25*80; i++) {
		video_memory[i] = space;
	}

	vga_y--;
}

/*
 * Set cursor to current X and Y coordinate
 */
static void vga_move_cursor() {
	uint16_t cursorLocation = vga_y * 80 + vga_x;

	// Set high cursor byte
	io_outb(0x3D4, 14);
	io_outb(0x3D5, cursorLocation >> 8);

	// Set low cursor byte
	io_outb(0x3D4, 15);
	io_outb(0x3D5, cursorLocation);
}