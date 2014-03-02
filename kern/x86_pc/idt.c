#import <types.h>
#import "idt.h"

// Exception handlers
extern void isr0(void);
extern void isr1(void);
extern void isr2(void);
extern void isr3(void);
extern void isr4(void);
extern void isr5(void);
extern void isr6(void);
extern void isr7(void);
extern void isr8(void);
extern void isr9(void);
extern void isr10(void);
extern void isr11(void);
extern void isr12(void);
extern void isr13(void);
extern void isr14(void);
extern void isr15(void);
extern void isr16(void);
extern void isr17(void);
extern void isr18(void);

extern void irq_dummy(void);

// IDT
static idt_entry_t sys_idt[256];

// Internal private functions
static void idt_install(void* base, uint16_t size);

/*
 * Initialises the IDT.
 */
void idt_init(void) {
	idt_entry_t* idt = (idt_entry_t *) &sys_idt;

	// Clear IDT
	memclr(idt, sizeof(idt_entry_t) * 256);

	for(int i = 0; i < 256; i++) {
		idt_set_gate(i, (uint32_t) irq_dummy, 0x08, 0x8E);
	}

	// Install exception handlers (BSOD)
	idt_set_gate(0, (uint32_t) isr0, 0x08, 0x8E);
	idt_set_gate(1, (uint32_t) isr1, 0x08, 0x8E);
	idt_set_gate(2, (uint32_t) isr2, 0x08, 0x8E);
	idt_set_gate(3, (uint32_t) isr3, 0x08, 0x8E);
	idt_set_gate(4, (uint32_t) isr4, 0x08, 0x8E);
	idt_set_gate(5, (uint32_t) isr5, 0x08, 0x8E);
	idt_set_gate(6, (uint32_t) isr6, 0x08, 0x8E);
	idt_set_gate(7, (uint32_t) isr7, 0x08, 0x8E);
	idt_set_gate(8, (uint32_t) isr8, 0x08, 0x8E);
	idt_set_gate(9, (uint32_t) isr9, 0x08, 0x8E);
	idt_set_gate(10, (uint32_t) isr10, 0x08, 0x8E);
	idt_set_gate(11, (uint32_t) isr11, 0x08, 0x8E);
	idt_set_gate(12, (uint32_t) isr12, 0x08, 0x8E);
	idt_set_gate(13, (uint32_t) isr13, 0x08, 0x8E);
	idt_set_gate(14, (uint32_t) isr14, 0x08, 0x8E); // page fault
	idt_set_gate(15, (uint32_t) isr15, 0x08, 0x8E);
	idt_set_gate(16, (uint32_t) isr16, 0x08, 0x8E);
	idt_set_gate(17, (uint32_t) isr17, 0x08, 0x8E);
	idt_set_gate(18, (uint32_t) isr18, 0x08, 0x8E);

	// Install IDT (LIDT instruction)
	idt_install((void *) idt, sizeof(idt_entry_t) * 256);
}

/*
 * Sets an IDT gate
 */
void idt_set_gate(uint8_t entry, uint32_t function, uint8_t segment, uint8_t flags) {
	idt_entry_t *ptr = (idt_entry_t *) &sys_idt;

	ptr[entry].offset_1 = function & 0xFFFF;
	ptr[entry].offset_2 = (function >> 0x10) & 0xFFFF;
	ptr[entry].selector = segment;
	ptr[entry].flags = flags; // OR with 0x60 for user level
	ptr[entry].zero = 0x00;
}

/*
 * Loads the IDTR register.
 */
static void idt_install(void* base, uint16_t size) {
	struct {
		uint16_t length;
		uint32_t base;
	} __attribute__((__packed__)) IDTR;
 
	IDTR.length = size;
	IDTR.base = (uint32_t) base;
	
	__asm__ volatile("lidt (%0)": : "p"(&IDTR));
}

/*
 * Reloads the IDT, therefore flushing its caches
 */
void idt_flush_cache(void) {
	idt_install((void *) &sys_idt, sizeof(idt_entry_t) * 256);
}