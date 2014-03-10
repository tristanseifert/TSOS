/*
 * This file's sole purpose is to hold strings and other miscellaneous info
 * that identifies this kernel module to the kernel module loader.
 */
#ifdef __cplusplus
extern "C" {
#endif

#import <module.h>

// Change MODULE_NAME in Makefile
const __attribute__((section (".info") visibility("default"))) char name[] = MODULE_NAME;
const __attribute__((section (".info") visibility("default"))) char version[] = "1.0";
const __attribute__((section (".info") visibility("default"))) char license[] = "BSD";
const __attribute__((section (".info") visibility("default"))) char note[] = "Support for reading and writing FAT32 filesystems.";

const __attribute__((section (".info") visibility("default"))) char compiler[] = "GNU GCC " __VERSION__;
const __attribute__((section (".info") visibility("default"))) char supported_kernel[] = "0.1";

#ifdef __cplusplus
}
#endif