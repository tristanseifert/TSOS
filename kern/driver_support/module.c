#import <types.h>
#import "module.h"
#import "ramdisk.h"

#import "hal/config.h"
#import "x86_pc/binfmt_elf.h"

// Compiler used to compile the kernel (used for kernel module compatibility checks)
static const char compiler[] = "GNU GCC " __VERSION__;

// Addresses of initcall addresses
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

	KSUCCESS("Static modules initialised");
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
	if(!ramdisk_loaded()) return;

	char *modulesToLoad = hal_config_get("modules");
	char *moduleName = strtok(modulesToLoad, " ");
	void *elf;

	// Find all modules
	while(moduleName) {
		// Attempt to load module from ramdisk
		if((elf = ramdisk_fopen(moduleName))) {
			elf_header_t *header = (elf_header_t *) elf;

			// Verify header
			if(ELF_CHECK_MAGIC(header->ident.magic)) {
				KWARNING("Module '%s' has invalid ELF magic of 0x%X%X%X%X\n", moduleName, header->ident.magic[0], header->ident.magic[1], header->ident.magic[2], header->ident.magic[3]);
				goto nextModule;
			}
		}

		nextModule: ;
		moduleName = strtok(NULL, " ");
	}


	KSUCCESS("Dynamically loaded modules initialised");
}