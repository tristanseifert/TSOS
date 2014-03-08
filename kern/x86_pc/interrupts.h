#import <types.h>

typedef void (*irq_callback_t)(void*);

int irq_register_handler(uint8_t irq, irq_callback_t callback, void* ctx);

// Mask or unmask an IRQ
void irq_mask(uint8_t irq);
void irq_unmask(uint8_t irq);

uint32_t irq_count(void);