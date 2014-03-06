#import <types.h>

typedef void (*kern_timer_callback_t)();

void kern_timer_tick_init(void);
void kern_timer_tick(void);

uint32_t kern_get_ticks(void);

void kern_timer_register_handler(kern_timer_callback_t callback);