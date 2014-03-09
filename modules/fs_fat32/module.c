/*
 * The definition of MODULE_NAME and the "name", version", "license", "note" and
 * "compiler", strings is needed so the kernel can get an idea of module
 * compatibility and whatnot.
 */
#define MODULE_NAME "fs_fat32"

const __attribute__((section (".info") visibility("default"))) char name[] = MODULE_NAME;
const __attribute__((section (".info") visibility("default"))) char version[] = "1.0";
const __attribute__((section (".info") visibility("default"))) char license[] = "BSD";
const __attribute__((section (".info") visibility("default"))) char note[] = "Support for reading and writing to FAT32 filesystems.";
const __attribute__((section (".info") visibility("default"))) char compiler[] = "GNU GCC " __VERSION__;

#import <types.h>
#import <module.h>

const char test[] = "test kernel extension";

static uint32_t some_counter;

void testStuff(void *);
void testStuff2(unsigned int);

/*
 * Entry function called by the kernel to initialise the driver
 */
__attribute__((visibility("default"))) module_t *module_entry() {
	KDEBUG("Initialising %s", test);
	
	testStuff2(0xC001C0DE);
	testStuff("pls test string");

	some_counter = 3;

	return (void *) 0xDEADBEEF;
}