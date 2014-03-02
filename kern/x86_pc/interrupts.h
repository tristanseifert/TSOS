#import <types.h>

typedef void (*irq_callback_t)();

int irq_register_handler(uint8_t irq, irq_callback_t callback);