#import <types.h>
#import "x86_pc/x86_pc.h"
#import "task/task.h"
#import "paging/paging.h"
#import "console/vga_console.h"

// External functions
extern void __stack_chk_guard_setup(void);
extern void srand(uint32_t);

void kern_idle(void);

// Linker defines
extern uint32_t BUILD_NUMBER;

// Idle task
task_t *idle_task;

/*
 * Kernel entry point
 */
void main(void) {
	// Console
	vga_init();
	klog(kLogLevelInfo, "TSOS Version 0.1 build %u", (unsigned int) &BUILD_NUMBER);

	// Seed the rng
	srand(0xDEADBEEF);

	// Copy multiboot
	x86_pc_init_multiboot();

	// Paging and VM
	paging_init();

	// Set up stack guard
	__stack_chk_guard_setup();

	// Set up platform
	x86_pc_init();

	// Initialise modules
	modules_load();
	klog(kLogLevelInfo, "Modules initialised");

	// Allocate idle task
	idle_task = task_new(kTaskPriorityIdle, false);
	idle_task->cpu_state.kernel_mode = 1;
	strncpy((char *) &idle_task->name, "Kernel Idle Task", 64);

	// Allocate stack
	void *idle_stack = kmalloc(1024*32);

	// Set up initial state (%esp and %eip)
	idle_task->cpu_state.eip = (uint32_t) &kern_idle;
	idle_task->cpu_state.usersp = (uint32_t) idle_stack;
	idle_task->cpu_state.eax = 0xDEADBEEF;

	klog(kLogLevelDebug, "much switch, very task");

	// Switch to the idle task.
	// task_switch(idle_task);

	while(1);
}

/*
 * Idle thread: runs in kernel space.
 */
void kern_idle(void) {
	// Infinite idling loop
	int i = 0;
	for(;;) {
		klog(kLogLevelDebug + (i & 0x03), "doom");
		i++;
	}
}