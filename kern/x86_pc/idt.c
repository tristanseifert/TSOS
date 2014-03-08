#import <types.h>
#import "idt.h"
#import "x86_pc.h"

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

#define MAX_IRQ 16

// Assembly IRQ handlers
extern void irq_0(void); 
extern void irq_1(void); 
extern void irq_2(void); 
extern void irq_3(void); 
extern void irq_4(void); 
extern void irq_5(void); 
extern void irq_6(void); 
extern void irq_7(void); 
extern void irq_8(void); 
extern void irq_9(void); 
extern void irq_10(void); 
extern void irq_11(void); 
extern void irq_12(void); 
extern void irq_13(void); 
extern void irq_14(void); 
extern void irq_15(void); 

// Assembly IRQ handlers
static const uint32_t asm_irq_handlers[MAX_IRQ] = {
	(uint32_t) irq_0, (uint32_t) irq_1,
	(uint32_t) irq_2, (uint32_t) irq_3,
	(uint32_t) irq_4, (uint32_t) irq_5,
	(uint32_t) irq_6, (uint32_t) irq_7,
	(uint32_t) irq_8, (uint32_t) irq_9,
	(uint32_t) irq_10, (uint32_t) irq_11,
	(uint32_t) irq_12, (uint32_t) irq_13,
	(uint32_t) irq_14, (uint32_t) irq_15,
};

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
		idt_set_gate(i, (uint32_t) irq_dummy, GDT_KERNEL_CODE, 0x8E);
	}

	// Install exception handlers
	idt_set_gate(0, (uint32_t) isr0, GDT_KERNEL_CODE, 0x8E);
	idt_set_gate(1, (uint32_t) isr1, GDT_KERNEL_CODE, 0x8E);
	idt_set_gate(2, (uint32_t) isr2, GDT_KERNEL_CODE, 0x8E);
	idt_set_gate(3, (uint32_t) isr3, GDT_KERNEL_CODE, 0x8E);
	idt_set_gate(4, (uint32_t) isr4, GDT_KERNEL_CODE, 0x8E);
	idt_set_gate(5, (uint32_t) isr5, GDT_KERNEL_CODE, 0x8E);
	idt_set_gate(6, (uint32_t) isr6, GDT_KERNEL_CODE, 0x8E);
	idt_set_gate(7, (uint32_t) isr7, GDT_KERNEL_CODE, 0x8E);
	idt_set_gate(8, (uint32_t) isr8, GDT_KERNEL_CODE, 0x8E);
	idt_set_gate(9, (uint32_t) isr9, GDT_KERNEL_CODE, 0x8E);
	idt_set_gate(10, (uint32_t) isr10, GDT_KERNEL_CODE, 0x8E);
	idt_set_gate(11, (uint32_t) isr11, GDT_KERNEL_CODE, 0x8E);
	idt_set_gate(12, (uint32_t) isr12, GDT_KERNEL_CODE, 0x8E);
	idt_set_gate(13, (uint32_t) isr13, GDT_KERNEL_CODE, 0x8E);
	idt_set_gate(14, (uint32_t) isr14, GDT_KERNEL_CODE, 0x8E); // page fault
	idt_set_gate(15, (uint32_t) isr15, GDT_KERNEL_CODE, 0x8E);
	idt_set_gate(16, (uint32_t) isr16, GDT_KERNEL_CODE, 0x8E);
	idt_set_gate(17, (uint32_t) isr17, GDT_KERNEL_CODE, 0x8E);
	idt_set_gate(18, (uint32_t) isr18, GDT_KERNEL_CODE, 0x8E);

	// Set up IRQ gates
	for(int irq = 0; irq < 16; irq++) {
		idt_set_gate(irq+0x20, asm_irq_handlers[irq], GDT_KERNEL_CODE, 0x8E);
	}

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