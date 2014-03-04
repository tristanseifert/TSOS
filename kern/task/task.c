#import <types.h>
#import "task.h"
#import "x86_pc/x86_pc.h"

// Task to execute next
static task_t *next_task;

// State of the currently-running task
static task_t *current_task;
static uint8_t fxsave_area[512] __attribute__((aligned(16)));

// TSC value when this task started
static uint64_t task_start_ticks;

// Function to perform a context switch
extern void task_switch(task_state_t state);

/*
 * Called to cause the current task to yield, saving its register and FPU state
 * so it can be resumed later.
 */
void task_yield(task_state_t state) {
	IRQ_OFF();

	// Copy task state
	memcpy(&current_task->state, &state, sizeof(task_state_t));

	// Save FPU state, if neccesary
	if(current_task->uses_fpu) {
		__asm__ volatile("fxsave %0" : "=m" (fxsave_area));
		memcpy(&current_task->fpu_state, &fxsave_area, 512);
	}

	// Calculate ticks
	current_task->ticks += (task_start_ticks - x86_pc_read_tsc());

	// Perform context switch to the next task
	task_switch(next_task->state);
}