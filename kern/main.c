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
extern uint32_t stack_top;

// Idle task
task_t *idle_task;

/*
 * Kernel entry point
 */
void main(void) {
	// Copy multiboot
	x86_pc_init_multiboot();

	// Paging and VM
	paging_init();

	// Set up platform
	x86_pc_init();

	// Console
	vga_init();
	KINFO("TSOS Version 0.1 build %u", (unsigned int) &BUILD_NUMBER);

	// Seed the rng
	srand(0xDEADBEEF);

	// Set up stack guard
	__stack_chk_guard_setup();

	// Initialise modules
	modules_load();
	KSUCCESS("Modules initialised");

	// Allocate idle task
	idle_task = task_new(kTaskPriorityIdle, true);
	strncpy((char *) &idle_task->name, "Kernel Idle Task", 64);

	// Set up initial state (%esp and %eip)
	idle_task->cpu_state.eip = (uint32_t) &kern_idle;
	idle_task->cpu_state.usersp = (uint32_t) stack_top;
	idle_task->cpu_state.eax = 0xDEADBEEF;

	// Switch to the idle task.
	task_switch(idle_task);
}

/*
 * Idle thread: runs in kernel space.
 */
void kern_idle(void) {
	uint32_t temp;
	__asm__ volatile("mov %%esp, %0" : "=r" (temp));
	KWARNING("I am the kernel idle task! (%%esp = 0x%08X)", temp);
	
	// Sleeping until an IRQ comes in lets the CPU possibly sleep
	for(;;) {
		__asm__ volatile("hlt");
	}
}