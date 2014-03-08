#import <types.h>
#import "task.h"
#import "x86_pc/x86_pc.h"

// Scheduler tasks
static task_t *next_task;
static task_t *current_task;

task_state_t *task_switchto_state;

// TSC value when this task started
static uint64_t task_start_ticks;

// PID of the last task
static unsigned int last_pid;

// Kernel pagetable
extern page_directory_t *kernel_directory;

// Function to perform a context switch
extern void task_context_switch(task_state_t state);

/*
 * Creates a new task. This only sets up the struct and configures priority and
 * the page table.
 *
 * @param pri Base priority class
 * @param isKernel Whether the task executes in ring 0 (when set) or ring 3
 */
task_t *task_new(task_priority_t pri, bool isKernel) {
	task_t *task = kmalloc(sizeof(task_t));
	ASSERT(task);

	// Initialise task struct
	memclr(task, sizeof(task_t));

	// Kernel state
	if(isKernel) {
		task->cpu_state.kernel_mode = 1;
		task->pagetable = kernel_directory;
	} else {
		// Create pagetable
		unsigned int phys;
		task->pagetable = kmalloc_ap(sizeof(page_directory_t), &phys);
		task->pagetable->physicalAddr = phys;

		// Map 0xC0000000 to 0xFFFFFFFF in userspace (but inaccessible to users)
		for(int i = 0x300; i < 0x400; i++) {
			task->pagetable->tables[i] = kernel_directory->tables[i];
			task->pagetable->tablesPhysical[i] = kernel_directory->tablesPhysical[i];
		}
	}

	// Set up priority
	task->orig_priority = pri;
	task->pid = last_pid++;

	// Final task setup
	return task;
}

/*
 * Maps a kernel-mode memory range into a specific task's memory.
 *
 * @param task Task in whose memory space the mapping should occurr
 * @param kern_addr Kernel mode address
 * @param length Size of memory range, in pages
 * @param user_addr Userspace address to begin the mapping
 */
void task_map(task_t *task, uint32_t kern_addr, size_t length, uint32_t user_addr) {
	page_t *kern_page, *user_page;

	// End address in kernelspace
	uint32_t kern_end = kern_addr + (length * 0x1000);

	// Do the mapping
	for(uint32_t addr = kern_addr; addr < kern_end; addr += 0x1000) {
		// Get kernel page map (it must exist)
		kern_page = paging_get_page(addr, false, kernel_directory);
		// Create page in the task's page table
		user_page = paging_get_user_page(user_addr, false, task->pagetable);

		// Copy physical address
		user_page->present = 1;
		user_page->rw = 1;
		user_page->user = 1;
		user_page->frame = kern_page->frame;

		// Increment user address
		user_addr += 0x1000;
	}
}

/*
 * Causes a switch to the specified task.
 *
 * @param task Task to switch to
 */
void task_switch(task_t *task) {
	IRQ_OFF();
	current_task = task;

	// Switch pagetable, if needed
	if(!task->cpu_state.kernel_mode) {
		klog(kLogLevelDebug, "doomers");
		paging_switch_directory(task->pagetable);
	}

	// Restore FPU state, if needed
	if(task->uses_fpu) {
		__asm__ volatile("fxrstor %0" : "=m" (task->fpu_state));
	}

	// Store current tick time
	task_start_ticks = x86_pc_read_tsc();

	// Perform context switch to the next task
	task_context_switch(task->cpu_state);
}

/*
 * Called to cause the current task to yield, saving its register and FPU state
 * so it can be resumed later.
 */
void task_yield(task_state_t state) {
	IRQ_OFF();

	// Copy task state
	memcpy(&current_task->cpu_state, &state, sizeof(task_state_t));

	// Save FPU state, if neccesary
	if(current_task->uses_fpu) {
		__asm__ volatile("fxsave %0" : "=m" (current_task->fpu_state));
	}

	// Calculate ticks number of ticks this task got
	current_task->ticks += (task_start_ticks - x86_pc_read_tsc());

	// Perform task switch
	task_switch(next_task);
}