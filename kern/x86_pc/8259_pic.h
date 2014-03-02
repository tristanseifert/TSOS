#import <types.h>

void i8259_eoi(uint8_t irq);
void i8259_remap(uint8_t offset1, uint8_t offset2);

void i8259_set_mask(uint8_t irq);
void i8259_clear_mask(uint8_t irq);

uint16_t i8259_get_irr(void);
uint16_t i8259_get_isr(void);