/*
 * General Hardware Abstraction Layer include file: you should only include this
 * file, rather than the individual files in this folder, as this prevents
 * problems with C++ code, and defines additional functions.
 */
#ifdef __cplusplus
extern "C" {
#endif

#import <hal/config.h>
#import <hal/handle.h>

#import <hal/keyboard.h>
#import <hal/disk.h>
#import <hal/bus.h>
#import <hal/vfs.h>

#import <acpi/acpi.h>

// Interrupt handling
typedef void (*hal_irq_callback_t)(void*);
C_FUNCTION int hal_register_irq_handler(uint8_t irq, hal_irq_callback_t callback, void* ctx);

// Bootup handlers
typedef void (*hal_init_function_t)(void);
C_FUNCTION int hal_register_init_handler(hal_init_function_t callback);

C_FUNCTION void hal_run_init_handlers(void);

#ifdef __cplusplus
}
#endif