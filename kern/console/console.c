#import <types.h>
#import <stdarg.h>

#import "vga_console.h"
#import "console.h"

char printf_buffer[1024];

int kprintf(const char* format, ...) {
	// Do printfness
	char* buffer = (char *) printf_buffer;
	va_list ap;
	va_start(ap, format);
	int n = vsprintf(buffer, format, ap);
	va_end(ap);

	// Excrete each char of the buffer
	while(*buffer != 0x00) {
		vga_console_putchar(*buffer++);
	}

	return n;
}