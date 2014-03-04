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
# Called when a task wishes to yield its CPU slice.
###############################################################################
sys_yield:
	pushl	%edx				# %eip to resume with
	pushl	%ecx				# %esp to resume with
	call	task_yield			# Run scheduler

###############################################################################
# Locations of the various syscall routines
###############################################################################
.section .rodata
syscall_table:
	.long	.invalid_syscall	# Syscall 0 is invalid

#	.long	sys_exit			# Exit with return code
#	.long	sys_yield			# Cause this task to yield its CPU slicen

#	.long	sys_fork			# Create new proccess with current %eip
#	.long	sys_read			# Read from specified file handle
#	.long	sys_write			# Write to specified file handle
#	.long	sys_open			# Open a file handle
#	.long	sys_close			# Close a file handle
#	.long	sys_creat			# Create a file and return file handle
#	.long	sys_link			# Create symlink
#	.long	sys_unlink			# Deletes a file

#	.long	sys_execve			# Replace this program with a new binary

#	.long	sys_chdir			# Change current directory

#	.long	sys_time			# seconds since Jan 1, 1970

#	.long	sys_mknod
#	.long	sys_chmod
#	.long	sys_lchown
#	.long	sys_stat

#	.long	sys_lseek
#	.long	sys_getpid

syscall_table_end:
