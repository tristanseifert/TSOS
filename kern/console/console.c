#import <types.h>
#import <stdarg.h>

#import "vga_console.h"
#import "console.h"

#import "task/systimer.h"

// Buffer for printf
static char printf_buffer[1024];

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
	static char buffer[1024];
	static char buffer2[1024+32];
	
	va_list ap;
	va_start(ap, format);
	vsprintf(buffer, format, ap);
	va_end(ap);

	// Prepend the timestamp
	int n = sprintf(buffer2, "%08X: %s\n", kern_get_ticks(), buffer);

	// Update the colour of the VGA console
	enum vga_colour old_fg = vga_get_fg_colour();
	enum vga_colour old_bg = vga_get_bg_colour();

	vga_set_fg_colour(log_level_colour[type][0]);
	vga_set_bg_colour(log_level_colour[type][1]);

	// Display each char of the buffer
	for(int i = 0; i < 1024+32; i++) {
		if(buffer2[i]) {
			vga_console_putchar(buffer2[i]);
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