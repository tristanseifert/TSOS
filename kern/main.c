#import <types.h>
#import "x86_pc/x86_pc.h"
#import "task/task.h"
#import "paging/paging.h"
#import "console/vga_console.h"

#import "driver_support/ramdisk.h"

#import "hal/hal.h"

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
task_t *idle_task;

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
	idle_task = task_new(kTaskPriorityIdle, true);
	strncpy((char *) &idle_task->name, "Kernel Idle Task", 64);

	// Set up initial state (%esp and %eip)
	idle_task->cpu_state.eip = (uint32_t) &kern_idle;
	idle_task->cpu_state.usersp = (uint32_t) &stack_top;
	idle_task->cpu_state.eax = 0xDEADBEEF;

	// Switch to the idle task.
	task_switch(idle_task);

	while(1);
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

	/*
	 * Use of the "htl" instruction allows the CPU to enter lower P states and
	 * conserve power, as well as not heating up as much.
	 */
	for(;;) {
		__asm__ volatile("hlt");
	}
}