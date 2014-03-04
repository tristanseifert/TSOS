#import <types.h>
#import "8259_pic.h"
#import "interrupts.h"
#import "runtime/error.h"
#import "idt.h"

#define MAX_IRQ 16
#define MAX_IRQ_CALLBACK 16

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

// IRQs that have gotten a handler associated
static bool irqs_handled[MAX_IRQ];

// IRQ counters
static uint32_t irq_counter[MAX_IRQ];
static uint32_t irqs_spurious;

// IRQ callbacks
static irq_callback_t irq_callbacks[MAX_IRQ][MAX_IRQ_CALLBACK];

/*
 * Called by IRQ handlers when an IRQ is called to service it by calling the
 * appropriate callbacks.
 */
void irq_handler(uint32_t irq) {
	irq &= 0x1F;

	// IRQs 7 and 15 can be spurious
	if(irq == 7 || irq == 15) {
		uint16_t isr = i8259_get_isr();

		// If the in-service bit for the IRQ isn't set, the IRQ was spurious
		if(!(isr & (1 << irq))) {
			irqs_spurious++;
			return;
		}
	}
	
	// If the IRQ isn't spurious, handle it
	irq_counter[irq]++;

	// Run IRQ handlers if they exist
	if(irqs_handled[irq]) {
		for(int i = 0; i < MAX_IRQ_CALLBACK; i++) {
			// If this slot has an IRQ handler, run it
			if(irq_callbacks[irq][i]) {
				irq_callbacks[irq][i]();
			}
		}
	} else {
		klog(kLogLevelError, "Unhandled IRQ Level %u", (unsigned int) irq);
	}

	i8259_eoi(irq);
}

/*
 * Registers an IRQ handler.
 */
int irq_register_handler(uint8_t irq, irq_callback_t callback) {
	// Unmask IRQ in the PIC
	i8259_clear_mask(irq);
	irqs_handled[irq] = true;

	// Find an IRQ callback slot
	for(int i = 0; i < MAX_IRQ_CALLBACK; i++) {
		// Is this IRQ slot empty?
		if(!irq_callbacks[irq][i]) {
			irq_callbacks[irq][i] = callback;

			// Set up an IDT entry, if needed
			if(i == 0) {
				idt_set_gate(irq+0x20, asm_irq_handlers[irq], 0x08, 0x8E);
				idt_flush_cache();
			}

			klog(kLogLevelDebug, "Added IRQ %u (Cb %u) *0x%X", (unsigned int) irq, (unsigned int) i, (unsigned int) callback);

			return 0;
		}
	}

	// No IRQs free
	return -1;
}