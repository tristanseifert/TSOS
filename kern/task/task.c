#import <types.h>
#import "task.h"
#import "x86_pc/x86_pc.h"

// Task to execute next
static task_t *next_task;

// Currently-running task
static task_t *current_task;

// TSC value when this task started
static uint64_t task_start_ticks;

// PID of the last task
static unsigned int last_pid;

// Function to perform a context switch
extern void task_switch(task_state_t state);

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

	// Switch next task to current task
	current_task = next_task;

	// Restore FPU state, if needed
	if(current_task->uses_fpu) {
		__asm__ volatile("fxrstor %0" : "=m" (current_task->fpu_state));
	}

	// Store current tick time
	task_start_ticks = x86_pc_read_tsc();

	// Perform context switch to the next task
	task_switch(next_task->cpu_state);
}