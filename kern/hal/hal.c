#import <types.h>
#import <hal.h>

#import "x86_pc/interrupts.h"

// Init functions
static list_t *hal_init_functions;

/*
 * Registers an IRQ handler.
 */
int hal_register_irq_handler(uint8_t irq, hal_irq_callback_t callback, void* ctx) {
	return irq_register_handler(irq, callback, ctx);
}

/*
 * Registers an init handler to be run when the init process (task 0) is
 * spawned by the operating system.
 */
int hal_register_init_handler(hal_init_function_t callback) {
	// Allocate list if it doesn't already exist
	if(!hal_init_functions) {
		hal_init_functions = list_allocate();
	}

	list_add(hal_init_functions, callback);
	return 0;
}

/*
 * Executes all callbacks registered using hal_register_init_handler.
 */
void hal_run_init_handlers(void) {
	if(hal_init_functions) {
		KDEBUG("Running %u init handlers", hal_init_functions->num_entries);

		for(unsigned int i = 0; i < hal_init_functions->num_entries; i++) {
			hal_init_function_t f = list_get(hal_init_functions, i);
			f();
		}
	}
}