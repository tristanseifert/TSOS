#import <types.h>
#import "8259_pic.h"
#import "interrupts.h"
#import "runtime/error.h"
#import "idt.h"

#define MAX_IRQ 16
#define MAX_IRQ_CALLBACK 16

// IRQs that have gotten a handler associated
static bool irqs_handled[MAX_IRQ];

// IRQ counters
static uint32_t irq_counter[MAX_IRQ];
static uint32_t irqs_spurious;

// IRQ callbacks
static irq_callback_t irq_callbacks[MAX_IRQ][MAX_IRQ_CALLBACK];

// Whether the old PIC is used, or if the APIC is used
extern bool pic_enabled;

// Private functions
static void irq_eoi(uint8_t irq);

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

	irq_eoi(irq);
}

/*
 * Registers an IRQ handler.
 */
int irq_register_handler(uint8_t irq, irq_callback_t callback) {
	// Unmask IRQ
	irq_unmask(irq);
	irqs_handled[irq] = true;

	// Find an IRQ callback slot
	for(int i = 0; i < MAX_IRQ_CALLBACK; i++) {
		// Is this IRQ slot empty?
		if(!irq_callbacks[irq][i]) {
			irq_callbacks[irq][i] = callback;

			klog(kLogLevelDebug, "Added IRQ %u (Cb %u) *0x%X", (unsigned int) irq, (unsigned int) i, (unsigned int) callback);

			return 0;
		}
	}

	// No IRQs free
	return -1;
}

/*
 * Masks an IRQ.
 */
void irq_mask(uint8_t irq) {
	if(pic_enabled) {
		i8259_set_mask(irq);
	} else {

	}
}

/*
 * Unmask an IRQ.
 */
void irq_unmask(uint8_t irq) {
	if(pic_enabled) {
		i8259_clear_mask(irq);
	} else {

	}
}

/*
 * Signal the end of an interrupt.
 */
static void irq_eoi(uint8_t irq) {
	if(pic_enabled) {
		i8259_eoi(irq);
	} else {

	}
}