#import <types.h>
#import <stdarg.h>

#import "vga_console.h"
#import "console.h"

#import "task/systimer.h"

// Buffer for printf
static char printf_buffer[1024];
static char log_buffer[1024+32];

// Colours used for each log level
static enum vga_colour log_level_colour[6][2] = {
	{vga_colour_light_grey, vga_colour_black}, // debug
	{vga_colour_light_blue, vga_colour_black}, // info

	{vga_colour_light_green, vga_colour_black}, // success

	{vga_colour_light_brown, vga_colour_black}, // warning

	{vga_colour_light_red, vga_colour_black}, // error
	{vga_colour_white, vga_colour_red} // critical
};

/*
 * General printf function
 */
int kprintf(const char* format, ...) {
	// Do printfness
	char *buf = (char *) printf_buffer;

	va_list ap;
	va_start(ap, format);
	int n = vsprintf(buf, format, ap);
	va_end(ap);

	// Excrete each char of the buffer
	while(*buf != 0x00) {
		io_outb(0xE9, *buf);
		vga_console_putchar(*buf++);
	}

	return n;
}

/*
 * Prints a formatted string to the default kernel log output. Adds colour and
 * timestamps as needed.
 */
int klog(enum log_type type, const char* format, ...) {
	// Discard log levels lower than we care for
	if(type < CONSOLE_MIN_LOG_LEVEL) return 0;
	
	// Format our initial string
	va_list ap;
	va_start(ap, format);
	vsprintf(printf_buffer, format, ap);
	va_end(ap);

	// Prepend the timestamp
	int n = sprintf(log_buffer, "%08X: %s\n", (unsigned int) kern_get_ticks(), printf_buffer);

	// Update the colour of the VGA console
	enum vga_colour old_fg = vga_get_fg_colour();
	enum vga_colour old_bg = vga_get_bg_colour();

	vga_set_fg_colour(log_level_colour[type][0]);
	vga_set_bg_colour(log_level_colour[type][1]);

	// Display each char of the buffer
	for(int i = 0; i < 1024+32; i++) {
		if(log_buffer[i]) {
			io_outb(0xE9, log_buffer[i]);
			vga_console_putchar(log_buffer[i]);
		} else {
			// Terminating character reached
			break;
		}
	}

	// Reset console colour
	vga_set_fg_colour(old_fg);
	vga_set_bg_colour(old_bg);

	// Return characters processed
	return n;
}