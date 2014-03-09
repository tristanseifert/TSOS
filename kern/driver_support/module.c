#include "module.h"
#include <types.h>

extern uint32_t __kern_initcalls, __kern_exitcalls, __kern_callsend;

/*
 * Runs the init functions of all modules compiled statically into the kernel.
 */
void modules_load() {
	module_initcall_t *initcallArray = (module_initcall_t *) &__kern_initcalls;

	int i = 0;
	while(initcallArray[i] != NULL) {
		int returnValue = (initcallArray[i]());

		i++;
	}
}

/*
 * Loads the modules specified in the ramdisk. Loading follows this general
 * procedure:
 *
 * 1. Ensure the file is a valid ELF.
 * 2. Dynamically link with kernel functions from kernel symtab.
 * 3. Call module entry point
 */
void modules_ramdisk_load() {

}