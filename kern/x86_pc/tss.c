#import <types.h>
#import "tss.h"
#import "x86_pc.h"

// TSS
static tss_entry_t kern_tss;

// Stack to use (shared with syscalls)
extern uint8_t syscall_stack[1024 * 32];

// GDT entry pointing to the TSS
extern uint8_t gdt_kernel_tss;

/*
 * Initialises the memory that was already allocated to the TSS, and sets it
 * up to support kernel interrupts.
 */
void tss_init() {
	// Set up the stack segment and stack address
	kern_tss.ss0 = GDT_KERNEL_DATA;
	kern_tss.esp0 = (uint32_t) &syscall_stack;
	kern_tss.iomap_base = sizeof(tss_entry_t);

	// Get address and size of TSS entry
	uint32_t base = (uint32_t) &kern_tss;
	uint32_t limit = sizeof(tss_entry_t);

	uint8_t *gdt = (uint8_t *) &gdt_kernel_tss;

	// Size bit set
	gdt[6] = 0x40;
 
	// Limit (size of TSS)
	gdt[0] = limit & 0xFF;
	gdt[1] = (limit >> 8) & 0xFF;
	gdt[6] |= (limit >> 16) & 0xF;
 
	// Base address (physical)
	gdt[2] = base & 0xFF;
	gdt[3] = (base >> 8) & 0xFF;
	gdt[4] = (base >> 16) & 0xFF;
	gdt[7] = (base >> 24) & 0xFF;

	// Flags
	gdt[5] = 0x89;

	// Print
	kprintf("TSS entry: 0x");
	for(int i = 0; i < 8; i++) {
		kprintf("%02X", gdt[i]);
	}
	kprintf("\n");

	// Flush GDT
	__asm__ volatile("lgdt gdt_table");

	// Debug
	klog(kLogLevelDebug, "Set up TSS base 0x%X limit 0x%X (0x%08X%08X)", (unsigned int) base, (unsigned int) limit, (unsigned int) gdt_kernel_tss, (unsigned int) *(&gdt_kernel_tss+1));

	// Load the TSS (entry 0x28, CPL3, LDT mode)
	uint32_t gdt_number = GDT_KERNEL_TSS | 0x03;
	__asm__ volatile("ltr %%ax" :: "a" (gdt_number));
}