#import <types.h>
#import "x86_pc/x86_pc.h"
#import "task/task.h"
#import "paging/paging.h"
#import "console/vga_console.h"

#import "driver_support/ramdisk.h"

#import "hal/hal.h"

#import "kconfig.h"

// External functions
extern void __stack_chk_guard_setup(void);
extern void srand(uint32_t);
extern uint32_t kern_get_ticks(void);
extern void _init(void);

void kern_idle(void);
extern void infinite_loop(void);

// Linker defines
extern uint32_t BUILD_NUMBER;
const char KERNEL_VERSION[] = "0.1";

extern uint32_t stack_top;

// Idle task
task_t *idle_thread = NULL;
static task_t *gInitTask = NULL;

/*
 * Kernel entry point
 */
void main(void) {
	// Console
	vga_init();
	KINFO("TSOS Version %s build %u", KERNEL_VERSION, (unsigned int) &BUILD_NUMBER);

	// Seed the rng
	srand(0xDEADBEEF);

	// Set up stack guard
	__stack_chk_guard_setup();

	// Copy multiboot
	x86_pc_init_multiboot();

	// Paging and VM
	paging_init();

	// Init C library
	_init();

	// Set up platform
	x86_pc_init();

	// Parse kernel config
	hal_config_parse(ramdisk_fopen("kernel.cfg"));

	// Initialise modules
	modules_load();
	modules_ramdisk_load();

	// Allocate idle task
	idle_thread = task_new(kTaskPriorityIdle, true);
	strncpy((char *) &idle_thread->name, "Kernel Idle Task", sizeof(idle_thread->name));

	// Set up initial state (%esp and %eip)
	idle_thread->cpu_state.eip = (uint32_t) &kern_idle;
	idle_thread->cpu_state.usersp = (uint32_t) &stack_top;
	idle_thread->cpu_state.eax = 0xDEADBEEF;

	// Switch to the idle task.
	task_switch(idle_thread);
}

/*
 * Idle thread: runs in kernel space.
 */
void kern_idle(void) {
	uint32_t temp;
	__asm__ volatile("mov %%esp, %0" : "=r" (temp));
	KSUCCESS("Kernel idle task started");

	hal_run_init_handlers();

	// Ensure IRQs are on
	IRQ_RES();

	// Initialise ACPI
	// acpi_init();

	// We can't do anything unless the root fs is mounted
	if(!hal_vfs_root_mounted()) PANIC("No root fs mounted");

	// Load the drivers specified in the file at /etc/modules.cfg
	fs_file_handle_t *modules = hal_vfs_fopen((char *) __kcfg_modules_list_path, kFSFileModeReadOnly);
	if(modules) {
		fs_file_t *file = hal_vfs_handle_to_file(modules);
		// Read file to memory
		char *buffer = (char *) kmalloc(file->size + 2);
		memclr(buffer, file->size + 2);
		hal_vfs_fread(buffer, file->size, modules);

		// Parse list
		list_t *modulesToLoad = parse_list(buffer, "\n");

		char *modulePathBuf = (char *) kmalloc(512);

		// Iterate over each module
		for(unsigned int i = 0; i < modulesToLoad->num_entries; i++) {
			// Initialise modules buffer
			memclr(modulePathBuf, 512);
			strncpy(modulePathBuf, __kcfg_modules_path, 512);

			// Get name of module
			char *moduleName = (char *) list_get(modulesToLoad, i);

			// Concatenate to form a full path
			strcat(modulePathBuf, moduleName);
			KDEBUG("Loading '%s'...", moduleName);

			module_load_from_file(modulePathBuf, NULL);
		}

		// Release memory
		kfree(modulePathBuf);

		// Close handle
		hal_vfs_fclose(modules);
	} else {
		KWARNING("Couldn't open %s", __kcfg_modules_list_path);
	}

	// Run late initcalls
	modules_late_init();

	// Load drivers for devices that still need them
	hal_bus_match_devices();

	// Start "init" process in userland
	KINFO("Starting \"init\" now...");

	// Load "/bin/init" from disk
	fs_file_handle_t *init = hal_vfs_fopen((char *) "/bin/init", kFSFileModeReadOnly);
	if(init) {
		gInitTask = task_new(kTaskPriorityHigh, false);
	} else {
		PANIC("couldn't start init");
	}

	/*
	 * Use of the "hlt" instruction allows the CPU to enter lower P states and
	 * conserve power, as well as not heating up as much.
	 */
	for(;;) {
		__asm__ volatile("hlt");
	}
}