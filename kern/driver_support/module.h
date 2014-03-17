#import <types.h>

typedef int (*module_initcall_t)(void);
typedef void (*module_exitcall_t)(void);

#define module_early_init(fn)	__define_initcall(fn, 0)
#define module_bus_init(fn)	__define_initcall(fn, 1)
#define module_init(fn)	__define_initcall(fn, 4)
#define module_driver_init(fn)	__define_initcall(fn, 5)
#define module_post_driver_init(fn)	__define_initcall(fn, 6)
#define module_post_dynload(fn)	__define_initcall(fn, 8)
#define module_exit(fn)	__exitcall(fn)

// Plops pointers to a module's init function into the appropraite section
#define __define_initcall(fn, id) \
	static module_initcall_t __initcall_##fn##id __used \
	__attribute__((__section__(".initcall" #id ".init"))) = fn

#define __exitcall(fn) \
	static module_exitcall_t __exitcall_##fn __used \
	__attribute__((__section__(".exitcall.exit"))) = fn

void modules_load();
void modules_ramdisk_load();

/*
 * Loads a module from the specified memory address: it must NOT be deallocated
 * but may be unmapped from kernel space, as it is mapped again in the driver
 * address space.
 *
 * @param elf Pointer to an ELF file
 * @param moduleName Name of this module: used only for display purposes
 */
void module_load(void *elf, char *moduleName);

/*
 * Reads the file from the path into a temporary buffer, and then attempt to
 * load it as a kernel module.
 *
 * @param path Path to the kernel module
 * @param err Pointer to an integer in which to store a more detailed error
 * @return true if success, false if error.
 */
bool module_load_from_file(char *path, int *err);