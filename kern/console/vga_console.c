#import <types.h>
#import "vga_console.h"

// State
static unsigned int vga_x, vga_y;
static uint16_t *video_memory;
static uint16_t space;

static enum vga_colour fg_colour;
static enum vga_colour bg_colour;

// Private functions
static void vga_scroll_up(void);
static void vga_move_cursor();

/*
 * Clears the screen.
 */
void vga_init(void) {
	fg_colour = vga_colour_light_grey;
	bg_colour = vga_colour_black;

	vga_y = vga_x = 0;
	video_memory = (uint16_t *) 0xB8000;

	space = (((bg_colour << 4) | (fg_colour & 0x0F)) << 0x8) | ' ';

	// Fill screen with spaces
	for(int i = 0; i < 25*80; i++) {
		video_memory[i] = space;
	}
}

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
	video_memory[vga_x + (vga_y * 80)] = (((bg_colour << 4) | (fg_colour & 0x0F)) << 0x08) | (c & 0xFF);

	// Next column
	vga_x++;

	// Line wrapping
	if(vga_x == 80) {
		vga_x = 0;
		
		// Scroll lines up if needed
		if(++vga_y == 25) {
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

/*
 * Gets the current foreground colour of the console.
 */
enum vga_colour vga_get_fg_colour(void) {
	return fg_colour;
}

/*
 * Sets the foreground colour of the console.
 */
void vga_set_fg_colour(enum vga_colour colour) {
	fg_colour = colour;
}

/*
 * Gets the current background colour of the console.
 */
enum vga_colour vga_get_bg_colour(void) {
	return bg_colour;
}

/*
 * Sets the background colour of the console.
 */
void vga_set_bg_colour(enum vga_colour colour) {
	bg_colour = colour;
}