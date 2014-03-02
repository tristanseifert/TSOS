#import <types.h>

typedef enum {
	i8254_mode_single_shot = 0,
	i8254_mode_rate_generator = 2,
	i8254_mode_square = 3
} i8254_mode_t;

void i8254_set_mode(uint8_t channel, i8254_mode_t mode);
void i8254_set_ticks(uint8_t channel, uint16_t ticks);