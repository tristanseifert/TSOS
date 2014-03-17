#import <types.h>
#import "vga_console.h"

#if VGA_USE_BOLD_FONT
#import "font_bold.h"
#else
#import "font_normal.h"
#endif

// State
static unsigned int vga_x, vga_y;
static uint16_t *video_memory;
static uint16_t space;

static unsigned int tempx, tempy;

static enum vga_colour fg_colour;
static enum vga_colour bg_colour;

// Private functions
static void vga_scroll_up(void);
static void vga_move_cursor();
static void vga_load_font(unsigned char *font);
static void vga_update_regs(void);

// VGA register access
static inline void vga_write_reg(uint16_t iport, uint8_t reg, uint8_t val){
	io_outb(iport, reg);
	io_outb(iport + 1, val);
}

static inline uint8_t vga_read_reg(uint16_t iport, uint8_t reg){
	io_outb(iport, reg); 
	return io_inb(iport + 1);
}

// Selected screen mode size
#define SCREEN_COLS 			90
#define SCREEN_ROWS				30

// VGA sequencer registers
#define VGA_SEQ_INDEX_PORT 0x3C4
#define VGA_SEQ_DATA_PORT 0x3C5

// VGA Graphics Controller register (VRAM read/write)
#define VGA_GC_INDEX_PORT 0x3CE
#define VGA_GC_DATA_PORT 0x3CF

// VGA Attribute Controller registers
#define	VGA_AC_INDEX_PORT		0x3C0
#define	VGA_AC_WRITE_PORT		0x3C0
#define	VGA_AC_READ_PORT		0x3C1

// VGA CRT controller registers (timing generator)
#define VGA_CRTC_INDEX_PORT 0x3D4
#define VGA_CRTC_DATA_PORT 0x3D5

// Sequencer registers
#define VGA_SEQ_MAP_MASK_REG 0x02
#define VGA_SEQ_CHARSET_REG 0x03
#define VGA_SEQ_MEMORY_MODE_REG 0x04

// Graphic controller registers
#define VGA_GC_READ_MAP_SELECT_REG 0x04
#define VGA_GC_GRAPHICS_MODE_REG 0x05
#define VGA_GC_MISC_REG 0x06

// Miscellaneous register write
#define	VGA_MISC_WRITE				0x3C2

static uint8_t vga_text_mode_regs[] = {
	// MISC
	0xE7,
	// SEQ
	0x03, 0x01, 0x03, 0x00, 0x02,
	// CRTC
	0x6B, 0x59, 0x5A, 0x82, 0x60, 0x8D, 0x0B, 0x3E,
	0x00, 0x4F, 0x0D, 0x0E, 0x00, 0x00, 0x00, 0x00,
	0xEA, 0x0C, 0xDF, 0x2D, 0x10, 0xE8, 0x05, 0xA3,
	0xFF,
	// GC
	0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x0E, 0x00,
	0xFF,
	// AC
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x14, 0x07,
	0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
	0x0C, 0x00, 0x0F, 0x08, 0x00,
};

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
	for(int i = 0; i < SCREEN_ROWS*SCREEN_COLS; i++) {
		video_memory[i] = space;
	}

	// Load the new font
	#if VGA_USE_BOLD_FONT
	vga_load_font((unsigned char *) &vga_font_bold);
	#else
	vga_load_font((unsigned char *) &vga_font_normal);
	#endif

	// Enter new text mode
	vga_update_regs();
}

/*
 * Called once VGA memory is remapped.
 */
void vga_textmem_remap(unsigned int newaddr) {
	video_memory = (uint16_t *) newaddr;
}

/*
 * Prints a character to the console, except newlines.
 */
void vga_console_putchar(char c) {
	// Process newline
	if(unlikely(c == '\n')) {
		vga_x = 0;

		// Scroll lines up if needed
		if(++vga_y == SCREEN_ROWS) {
			vga_scroll_up();
			vga_y--;
		}

		return;
	} else if(unlikely(c == '\r')) { // carriage return
		vga_x = 0;
		return;
	}

	// Write to VGA memory (BG black, FG white)
	video_memory[vga_x + (vga_y * SCREEN_COLS)] = (((bg_colour << 4) | (fg_colour & 0x0F)) << 0x08) | (c & 0xFF);

	// Next column
	vga_x++;

	// Line wrapping
	if(vga_x == SCREEN_COLS) {
		vga_x = 0;
		
		// Scroll lines up if needed
		if(++vga_y == SCREEN_ROWS) {
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
	memmove(video_memory, video_memory+SCREEN_COLS, 24*SCREEN_COLS*2);

	// Clear bottom row
	memclr(video_memory+24*SCREEN_COLS, 160);
}

/*
 * Set cursor to current X and Y coordinate
 */
static void vga_move_cursor() {
	uint16_t cursorLocation = vga_y * SCREEN_COLS + vga_x;

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

/*
 * Loads a new font into the VGA hardware.
 */
static void vga_load_font(unsigned char *font) {
	unsigned char *p = (unsigned char *) video_memory;
	
	// Enable writing to plane 2
	vga_write_reg(VGA_SEQ_INDEX_PORT, VGA_SEQ_MAP_MASK_REG, 0x04);
	
	// Ensure first font is selected
	vga_write_reg(VGA_SEQ_INDEX_PORT, VGA_SEQ_CHARSET_REG, 0x00);
	
	// Enable sequential memory access
	uint8_t mem_mode = vga_read_reg(VGA_SEQ_INDEX_PORT, VGA_SEQ_MEMORY_MODE_REG);
	vga_write_reg(VGA_SEQ_INDEX_PORT, VGA_SEQ_MEMORY_MODE_REG, 0x06);
	
	vga_write_reg(VGA_GC_INDEX_PORT, VGA_GC_READ_MAP_SELECT_REG, 0x02);
	
	// Set up the graphics mode
	uint8_t graphics_mode = vga_read_reg(VGA_GC_INDEX_PORT, VGA_GC_GRAPHICS_MODE_REG);
	vga_write_reg(VGA_GC_INDEX_PORT, VGA_GC_GRAPHICS_MODE_REG, 0x00);
	

	vga_write_reg(VGA_GC_INDEX_PORT, VGA_GC_MISC_REG, 0x0C);
	
	// Write each character
	for(unsigned int i = 0; i < 256; i++){
		for(unsigned int j = 0; j < 16; j++){
			*p++= *font++;
		}

		// Skip 16 bytes between glyphs
		p += 16;
	}
	
	// Enable write to planes 0 and 1
	vga_write_reg(VGA_SEQ_INDEX_PORT, VGA_SEQ_MAP_MASK_REG, 0x03);
	vga_write_reg(VGA_SEQ_INDEX_PORT, VGA_SEQ_MEMORY_MODE_REG, mem_mode);
	
	vga_write_reg(VGA_GC_INDEX_PORT, VGA_GC_READ_MAP_SELECT_REG, 0x00);
	vga_write_reg(VGA_GC_INDEX_PORT, VGA_GC_GRAPHICS_MODE_REG, graphics_mode);

	// Restore VGA text mode
	vga_write_reg(VGA_GC_INDEX_PORT, VGA_GC_MISC_REG, 0x0C);	
}

/*
 * Enters the 90x30 text mode.
 */
static void vga_update_regs(void) {
	unsigned int i;

	// Read pointer
	uint8_t *regs = (uint8_t *) &vga_text_mode_regs;

	// Miscellaneous register
	io_outb(VGA_MISC_WRITE, *regs);
	regs++;

	// Sequencer regs
	for(i = 0; i < 5; i++) {
		vga_write_reg(VGA_SEQ_INDEX_PORT, i, *regs++);
	}

	// Unlock CRTC regs
	uint8_t temp = vga_read_reg(VGA_CRTC_INDEX_PORT, 0x03);
	vga_write_reg(VGA_CRTC_INDEX_PORT, 0x03, temp | 0x80);

	temp = vga_read_reg(VGA_CRTC_INDEX_PORT, 0x11);
	vga_write_reg(VGA_CRTC_INDEX_PORT, 0x11, temp & ~0x80);

	// Ensure they remain unlocked whilst we write them
	regs[0x03] |= 0x80;
	regs[0x11] &= ~0x80;

	// CRTC regs
	for(i = 0; i < 25; i++) {
		vga_write_reg(VGA_CRTC_INDEX_PORT, i, *regs++);
	}

	// Graphics controller regs
	for(i = 0; i < 9; i++) {
		vga_write_reg(VGA_GC_INDEX_PORT, i, *regs++);
	}

	// Attribute controller
	for(i = 0; i < 21; i++) {
		io_inb(0x3DA);
		vga_write_reg(VGA_AC_INDEX_PORT, i, *regs++);
	}

	// Lock 16 colour palette and blank display
	io_inb(0x3DA);
	io_outb(VGA_AC_INDEX_PORT, 0x20);
}