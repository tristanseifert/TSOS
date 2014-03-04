#import <types.h>
#import "tss.h"
#import "x86_pc.h"

typedef struct {
	unsigned int limit_low:16;
	unsigned int base_low: 24;

	unsigned int accessed: 1;
	unsigned int read_write: 1;
	unsigned int conforming_expand_down: 1;
	unsigned int code: 1;
	unsigned int always_1: 1;
	unsigned int DPL: 2;
	unsigned int present: 1;

	unsigned int limit_high: 4;
	unsigned int available: 1;
	unsigned int always_0: 1;
	unsigned int big: 1;
	unsigned int gran: 1;
	unsigned int base_high: 8;
} gdt_entry_t;

// TSS
static tss_entry_t kern_tss;

// Stack to use (shared with syscalls)
extern uint8_t syscall_stack[1024 * 32];

// GDT entry pointing to the TSS
extern uint32_t gdt_kernel_tss;

/*
 * Initialises the memory that was already allocated to the TSS, and sets it
 * up to support kernel interrupts.
 */
void tss_init() {
	// Zero out TSS memory
	memclr(&kern_tss, sizeof(tss_entry_t));

	// Set up the stack segment and stack address
	kern_tss.ss0 = GDT_KERNEL_DATA;
	kern_tss.esp0 = (uint32_t) &syscall_stack;

	// Get address and size of TSS entry
	uint32_t base = (uint32_t) &kern_tss;
	uint32_t limit = base + sizeof(tss_entry_t);

	// Build a GDT entry
	gdt_entry_t entry;
	memclr(&entry, sizeof(gdt_entry_t));

	entry.base_low = base & 0xFFFFFF;
	entry.accessed = 1; // Indicate this is a TSS
	entry.read_write = 0; 
	entry.conforming_expand_down = 0;
	entry.code = 1;
	entry.always_1 = 0; // this is zero on a TSS
	entry.DPL = 3;
	entry.present = 1;
	entry.limit_high = (limit & 0xF0000) >> 16; // isolate top nibble
	entry.available = 0;
	entry.always_0 = 0;
	entry.big = 0;
	entry.gran = 0;
	entry.base_high = (base & 0xFF000000) >> 24; // isolate top byte.

	// Stick into GDT
	memcpy(&gdt_kernel_tss, &entry, 8);

	// Debug
	klog(kLogLevelDebug, "Set up TSS base 0x%X limit 0x%X (0x%08X%08X)", (unsigned int) base, (unsigned int) limit, (unsigned int) gdt_kernel_tss, (unsigned int) *(&gdt_kernel_tss+1));

	// Load the TSS
	uint32_t gdt_number = 0x28 | 0x03;
	__asm__ volatile("ltr %0" :: "m" (gdt_number));
}