#import <types.h>
#import "systimer.h"

static uint32_t ticks;

/*
 * Initialises the system tick functionality.
 */
void kern_timer_tick_init(void) {
	ticks = 0;
}

/*
 * Timer callback that should be called every 10 ms: used to preempt tasks and
 * derive other timers from.
 */
void kern_timer_tick(void) {
	ticks++;
}

/*
 * Returns the number of ticks since bootup.
 */
uint32_t kern_get_ticks(void) {
	return ticks;
}