#import <types.h>
#import "vga_console.h"

// Colour values
enum vga_color {
	COLOR_BLACK = 0,
	COLOR_BLUE = 1,
	COLOR_GREEN = 2,
	COLOR_CYAN = 3,
	COLOR_RED = 4,
	COLOR_MAGENTA = 5,
	COLOR_BROWN = 6,
	COLOR_LIGHT_GREY = 7,
	COLOR_DARK_GREY = 8,
	COLOR_LIGHT_BLUE = 9,
	COLOR_LIGHT_GREEN = 10,
	COLOR_LIGHT_CYAN = 11,
	COLOR_LIGHT_RED = 12,
	COLOR_LIGHT_MAGENTA = 13,
	COLOR_LIGHT_BROWN = 14,
	COLOR_WHITE = 15,
};

// State
static unsigned int vga_x, vga_y;
static uint16_t *video_memory;
static uint16_t space;

// Private functions
static void vga_scroll_up(void);
static void vga_move_cursor();

/*
 * Clears the screen.
 */
static int vga_init(void) {
	vga_y = vga_x = 0;
	video_memory = (uint16_t *) 0xB8000;
	space = (((COLOR_BLACK << 4) | (COLOR_WHITE & 0x0F)) << 0x8) | ' ';

	memclr(video_memory, 25*80*2);

	return 0;
}

module_driver_init(vga_init);

/*
 * Prints a character to the console, except newlines.
 */
void vga_console_putchar(char c) {
	// Process newline
	if(unlikely(c == '\n')) {
		vga_x = 0;

		// Scroll lines up if needed
		if(++vga_y == 25) {
			vga_scroll_up();
			vga_y--;
		}

		return;
	} else if(unlikely(c == '\r')) { // carriage return
		vga_x = 0;
		return;
	}

	// Write to VGA memory (BG black, FG white)
	video_memory[vga_x + (vga_y * 80)] = (((COLOR_BLACK << 4) | (COLOR_WHITE & 0x0F)) << 0x08) | c;

	// Next column
	vga_x++;

	// Line wrapping
	if(vga_x == 80) {
		vga_x = 0;
		vga_y++;

		// Scroll lines up if needed
		if(vga_y == 25) {
			vga_scroll_up();

			vga_x = 0;
			vga_y--;
		}
	}

	// Update cursor location
	vga_move_cursor();
}

/*
 * Scrolls the video memory up one line
 */
static void vga_scroll_up(void) {
	// Move everything upwards
	memmove(video_memory, video_memory+80, 24*80*2);

	// Clear bottom row
	memclr(video_memory+24*80, 160);
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

/*
 * Resets the VGA console
 */
void vga_console_reset() {
	// Clear screen
	vga_init();

	// Reset cursor
	vga_x = vga_y = 0;
	vga_move_cursor();
}