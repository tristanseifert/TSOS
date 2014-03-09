/*
 * These strings are used by the kernel module loader to determine if a certain
 * module is compatible with the running kernel. In addition, it is used to
 * build a pretty database of the loaded modules.
 */
#define MODULE_NAME "fs_fat32"

const __attribute__((section (".info") visibility("default"))) char name[] = MODULE_NAME;
const __attribute__((section (".info") visibility("default"))) char version[] = "1.0";
const __attribute__((section (".info") visibility("default"))) char license[] = "BSD";
const __attribute__((section (".info") visibility("default"))) char note[] = "Support for reading and writing to FAT32 filesystems.";
const __attribute__((section (".info") visibility("default"))) char compiler[] = "GNU GCC " __VERSION__;
const __attribute__((section (".info") visibility("default"))) char supported_kernel[] = "0.1";

#import <module.h>

// Module definition
static const module_t mod = {
	.name = MODULE_NAME
};

/*
 * Entry function called by the kernel to initialise the driver
 */
__attribute__((visibility("default"))) module_t *module_entry() {
	KDEBUG("Note: %s", note);

	return (module_t *) &mod;
}