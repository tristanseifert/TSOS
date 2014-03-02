#import <types.h>
#import "8259_pic.h"

#define PIC1_CMD		0x20 // IO base of master PIC
#define PIC1_DATA		0x21
#define PIC2_CMD		0xA0 // IO base of slave PIC
#define PIC2_DATA		0xA1
#define PIC_READ_IRR	0x0A // OCW3 irq ready next CMD read
#define PIC_READ_ISR	0x0B // OCW3 irq service next CMD read

#define PIC_EOI			0x20 // End of Interrupt

#define ICW1_ICW4		0x01 // ICW4 (not) needed
#define ICW1_SINGLE		0x02 // Single (cascade) mode
#define ICW1_INTERVAL4	0x04 // Call address interval 4 (8)
#define ICW1_LEVEL		0x08 // Level triggered (edge) mode
#define ICW1_INIT		0x10 // Initialization - required!
 
#define ICW4_8086		0x01 // 8086/88 (MCS-80/85) mode
#define ICW4_AUTO		0x02 // Auto (normal) EOI
#define ICW4_BUF_SLAVE	0x08 // Buffered mode/slave
#define ICW4_BUF_MASTER	0x0C // Buffered mode/master
#define ICW4_SFNM		0x10 // Special fully nested (not)

/*
 * Signals the end of an interrupt.
 */
void i8259_eoi(uint8_t irq) {
	if(irq >= 8) {
		io_outb(PIC2_CMD, PIC_EOI);
	}
 
	io_outb(PIC1_CMD, PIC_EOI);
}

/*
 * Remaps the PICs to the specified IRQ offsets.
 */
void i8259_remap(uint8_t offset1, uint8_t offset2) {
	uint8_t a1, a2;

	// Save current interrupt masks
	a1 = io_inb(PIC1_DATA);
	a2 = io_inb(PIC2_DATA);
 
 	// Begin PIC init in cascade mode
	io_outb(PIC1_CMD, ICW1_INIT+ICW1_ICW4);
	io_wait();
	io_outb(PIC2_CMD, ICW1_INIT+ICW1_ICW4);
	io_wait();

	// Set master PIC vector offset
	io_outb(PIC1_DATA, offset1);
	io_wait();

	// Set slave PIC vector offset
	io_outb(PIC2_DATA, offset2);
	io_wait();

	// Slave PIC at master IRQ 1
	io_outb(PIC1_DATA, 4);
	io_wait();

	// Slave PIC cascade identity of 0000 0010
	io_outb(PIC2_DATA, 2);
	io_wait();
 
	io_outb(PIC1_DATA, ICW4_8086);
	io_wait();
	io_outb(PIC2_DATA, ICW4_8086);
	io_wait();
 
	// Restore masks
	io_outb(PIC1_DATA, a1);
	io_outb(PIC2_DATA, a2);
}

/*
 * Masks a certain IRQ
 */
void i8259_set_mask(uint8_t irq) {
	uint16_t port;
	uint8_t value;
 
	if(irq < 8) {
		port = PIC1_DATA;
	} else {
		port = PIC2_DATA;
		irq -= 8;
	}
	value = io_inb(port) | (1 << irq);
	io_outb(port, value);
}

/*
 * Unmasks an IRQ, allowing it to be raised
 */
void i8259_clear_mask(uint8_t irq) {
	uint16_t port;
	uint8_t value;
 
	if(irq < 8) {
		port = PIC1_DATA;
	} else {
		port = PIC2_DATA;
		irq -= 8;
	}
	value = io_inb(port) & ~(1 << irq);
	io_outb(port, value);
}

/*
 * Reads a certain PIC IRQ register from both PICs
 */
static uint16_t __pic_get_irq_reg(uint8_t ocw3) {
	/* 
	 * OCW3 to PIC CMD to get the register values.  PIC2 is chained, and
	 * represents IRQs 8-15.  PIC1 is IRQs 0-7, with 2 being the chain 
	 */
	io_outb(PIC1_CMD, ocw3);
	io_outb(PIC2_CMD, ocw3);
	return (io_inb(PIC2_CMD) << 8) | io_inb(PIC1_CMD);
}

/*
 * Returns the combined value of the PIC IRQ request register.
 */
uint16_t i8259_get_irr(void) {
	return __pic_get_irq_reg(PIC_READ_IRR);
}

/*
 * Returns the combiend value of the cascaded PICs in-service register.
 */
uint16_t i8259_get_isr(void) {
	return __pic_get_irq_reg(PIC_READ_ISR);
}