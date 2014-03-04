#import <types.h>
#import "syscall.h"

// needed for MSR read/write functions
#import "x86_pc/x86_pc.h"

#define IA32_SYSENTER_CS 0x174
#define IA32_SYSENTER_ESP 0x175
#define IA32_SYSENTER_EIP 0x176

// Syscall stack (32KB)
uint8_t syscall_stack[1024 * 32];

// Syscall handler
extern void syscall_enter(void);

/*
 * Initialises the MSRs for the sysenter instruction.
 */
static int syscall_init(void) {
	// Set code segment of kernel
	x86_pc_write_msr(IA32_SYSENTER_CS, GDT_KERNEL_CODE, 0);

	// Stack to use for syscalls
	x86_pc_write_msr(IA32_SYSENTER_ESP, (uint32_t) &syscall_stack, 0);

	// Syscall handler to jump to for syscalls
	x86_pc_write_msr(IA32_SYSENTER_ESP, (uint32_t) &syscall_enter, 0);

	return 0;
}

module_early_init(syscall_init);

/*
 * Called if a process attempts an invalid syscall.
 */
void syscall_invalid(uint32_t num) {
	klog(kLogLevelInfo, "Invalid syscall 0x%X attempted", (unsigned int) num);
}