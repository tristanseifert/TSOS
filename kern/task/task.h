#import <types.h>

typedef struct task task_t;
typedef struct task_state task_state_t;

/*
 * State of a task
 */
struct task_state {	
	/*
	 * Set manually either from the stack image or syscall
	 * We needn't wory about SS or CS as it's always the same and eflags will
	 * only apply if the task was pre-empted
	 */
	uint32_t eip, usersp;

	// Pushed on the stack by the PUSHA instruction
	uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;

	// Automatically pushed by CPU during interrupts
	// uint32_t eip, cs, eflags, useresp, ss;
} __attribute__((__packed__));

/*
 * Structure describing a scheduled task, including its state at the last
 * context switch.
 */
struct task {
	// General task state
	task_state_t state;

	// FPU state
	bool uses_fpu;
	uint8_t fpu_state[512] __attribute__((aligned(16)));

	// Scheduler info
	uint64_t ticks;
};

/*
 * Called to cause the current task to yield, saving its register and FPU state
 * so it can be resumed later.
 */
void task_yield(task_state_t state);