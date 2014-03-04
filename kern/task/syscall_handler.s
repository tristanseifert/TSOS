.section .text
.global syscall_enter

.extern syscall_invalid

.set max_syscall, (syscall_table_end-syscall_table)/4

###############################################################################
# Location jumped to when a syscall is executed.
# This backs up the state of the CPU general purpose registers to the stack.
#
# We expect the syscall number to be in %eax, and %ebx should contain a pointer
# to the syscall info structure, in the process' own memory space. This works
# because this routine does not have to switch the page table, as the kernel is
# mapped in the same location in all usermode code.
#
# In addition, sysexit will require that %ecx contains the stack pointer before
# the call, and %edx the instruction to resume userspace execution at.
###############################################################################
syscall_enter:
	pushal

	# Ensure the syscall is in range
	cmpl	$max_syscall, %eax
	jge		.invalid_syscall

	# Push address of syscall info struct onto stack
	pushl	%ebx

	# Jump to syscall address
	call	*syscall_table(, %eax, 4)

	popal
	sysexit

###############################################################################
# Called when an invalid syscall number is passed to the syscall handler
###############################################################################
.invalid_syscall:
	pushl	%eax
	call	syscall_invalid

	popal
	sysexit

###############################################################################
# Locations of the various syscall routines
###############################################################################
.section .rodata
syscall_table:

syscall_table_end:
