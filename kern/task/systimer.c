#import <types.h>
#import "systimer.h"

#define NUM_TIMER_CALLBACKS 32

static uint32_t ticks;
static kern_timer_callback_t callbacks[NUM_TIMER_CALLBACKS];

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

	// Execute callbacks
	for(int i = 0; i < NUM_TIMER_CALLBACKS; i++) {
		if(callbacks[i]) {
			callbacks[i]();
		}
	}
}

/*
 * Returns the number of ticks since bootup.
 */
uint32_t kern_get_ticks(void) {
	return ticks;
}

/*
 * Registers a timer handler.
 */
void kern_timer_register_handler(kern_timer_callback_t callback) {
	// Ensure we don't register the same callback twice
	for(int i = 0; i < NUM_TIMER_CALLBACKS; i++) {
		if(callbacks[i] == callback) {
			klog(kLogLevelError, "Attempted to register timer handler 0x%X again", (unsigned int) callback);
		}
	}

	// Try to find an empty slot
	for(int i = 0; i < NUM_TIMER_CALLBACKS; i++) {
		if(!callbacks[i]) {
			callbacks[i] = callback;
			return;
		}
	}

	// No timer slots available :(
	klog(kLogLevelError, "No timer handler slots available!");
}