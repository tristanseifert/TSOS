/*
 * The definition of MODULE_NAME and the "name", version", "license", "note" and
 * "compiler", strings is needed so the kernel can get an idea of module
 * compatibility and whatnot.
 */
#define MODULE_NAME "fs_fat32"

const __attribute__((section (".info"))) char name[] = MODULE_NAME;
const __attribute__((section (".info"))) char version[] = "1.0";
const __attribute__((section (".info"))) char license[] = "BSD";
const __attribute__((section (".info"))) char note[] = "Support for reading and writing to FAT32 filesystems.";

#if defined(__GNUC__) || defined(__GNUG__)
const __attribute__((section (".info"))) char compiler[] = "GNU GCC " __VERSION__;
#endif

#import <types.h>
#import <module.h>

const char test[] = "obama";

/*
 * Entry function called by the kernel to initialise the driver
 */
__attribute__((visibility("default"))) module_t *module_entry() {
	KDEBUG("Initialising %s", test);

	return NULL;
}