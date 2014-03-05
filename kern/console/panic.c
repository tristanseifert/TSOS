#import <types.h>
#import "panic.h"
#import "runtime/error.h"

void panic_assert(char *file, unsigned int line, char *desc) {
	// An assertion failed, and we have to panic.
	IRQ_OFF();

	klog(kLogLevelCritical, "ASSERTION FAILURE(%s) at %s:%i", desc, file, line);

	uint32_t ebp;
	__asm__("mov %%ebp, %0" : "=r" (ebp));
	error_dump_stack_trace(256, ebp);
	
	// Halt by going into an infinite loop.
	for(;;);
}

void panic(char *message, char *file, unsigned int line) {
	// We encountered a massive problem and have to stop.
	IRQ_OFF();

	klog(kLogLevelCritical, "PANIC(%s) at %s:%i", message, file, line);

	uint32_t ebp;
	__asm__("mov %%ebp, %0" : "=r" (ebp));
	error_dump_stack_trace(256, ebp);

	// Halt by going into an infinite loop.
	for(;;);
}

/*
 * Prints a stack dump at the current position in the source file.
 */
void dump_stack_here(void) {
	uint32_t ebp;
	__asm__("mov %%ebp, %0" : "=r" (ebp));
	error_dump_stack_trace(256, ebp);
}