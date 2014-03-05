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
	uint32_t usersp, eip;

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
	volatile int state; // 0 = runnable, >0 unrunnable, <0 stopped
	uint64_t ticks; // number of TSC ticks this task got
	uint8_t priority; // 0-15, where 0 is least and 15 is most

	// Task state
	task_state_t cpu_state;

	// FPU state
	bool uses_fpu;
	uint8_t fpu_state[512] __attribute__((aligned(16)));

	// General info
	unsigned int pid;
	char name[64];

	// Signal to be sent to this process
	unsigned int next_signal;
};

/*
 * Called to cause the current task to yield, saving its register and FPU state
 * so it can be resumed later.
 */
void task_yield(task_state_t state);