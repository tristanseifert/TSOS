#import <types.h>
#import "paging/paging.h"

typedef struct task task_t;
typedef struct task_state task_state_t;

/*
 * Task priority: These map to one of 16 "queues" within the scheduler, so that
 * processes will not be starved. Obviously, "Normal" processes will come
 * before "Idle" processes, but can be preempted by "High" ones.
 */
typedef enum {
	kTaskPriorityIdle,
	kTaskPriorityLow,
	kTaskPriorityNormal,
	kTaskPriorityInteractive,
	kTaskPriorityHigh,
	kTaskPriorityVeryHigh,
} task_priority_t;

/*
 * State of a task
 */
struct task_state {	
	// When not 0, indicates that the task runs in kernel mode.
	uint32_t kernel_mode;

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

	task_priority_t orig_priority;
	uint8_t priority; // 0-15, where 0 is least and 15 is most

	// Task state
	task_state_t cpu_state;

	// FPU state
	bool uses_fpu;
	uint8_t fpu_state[512] __attribute__((aligned(16)));

	// Paging map
	page_directory_t *pagetable;

	// General info
	unsigned int pid;
	char name[64];
	bool is_v8086;

	// Signal to be sent to this process
	unsigned int next_signal;
};

/*
 * Creates a new task. This only sets up the struct and configures priority and
 * the page table.
 *
 * @param pri Base priority class
 * @param isKernel Whether the task executes in ring 0 (when set) or ring 3
 */
task_t *task_new(task_priority_t pri, bool isKernel);

/*
 * Maps a kernel-mode memory range into a specific task's memory.
 *
 * @param task Task in whose memory space the mapping should occurr
 * @param kern_addr Kernel mode address
 * @param length Size of memory range, in pages
 * @param user_addr Userspace address to begin the mapping
 */
void task_map(task_t *task, uint32_t kern_addr, size_t length, uint32_t user_addr);

/*
 * Causes a switch to the specified task.
 *
 * @param task Task to switch to
 */
void task_switch(task_t *task);

/*
 * Called to cause the current task to yield, saving its register and FPU state
 * so it can be resumed later.
 */
void task_yield(task_state_t state);