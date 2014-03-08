.globl task_context_switch

# OR with 0x03 for ring 3
.set GDT_KERN_CODE,	0x08
.set GDT_KERN_DATA,	0x10
.set GDT_USER_CODE, 0x18
.set GDT_USER_DATA, 0x20

###############################################################################
# Performs a context switch, assuming that a task_state_t struct has been
# pushed onto the stack.
###############################################################################
task_context_switch:
	# First entry in struct is in edi
	lea		4(%esp), %edi

	# If the task will execute in kernel mode, jump
	movl	(%edi), %eax
	test	%eax, %eax
	jnz		task_switch_kernel

	# Reserve 16 bytes on the process stack frame
	movl	4(%edi), %ecx
	sub		$0x10, %ecx

	# Set the ring this program will execute in into %ebx
	movl	$0x03, %ebx

	# set userspace %esp
	movl	4(%edi), %eax
	movl	%eax, 12(%ecx)

	# Set data segment (with correct ring mask applied)
	movl	$GDT_USER_DATA, %eax
	or		%ebx, %eax
	movl	%eax, 16(%ecx)

	# set userspace %eip
	movl	8(%edi), %eax
	movl	%eax, 0(%ecx)

	# Set code segment
	movl	$GDT_USER_CODE, %eax
	or		%ebx, %eax
	movl	%eax, 4(%ecx)

	# Get current flags (and re-enable IRQs in the flags)
	pushf
	pop		%eax
	or		$0x200, %eax
	movl	%eax, 8(%ecx)

	# Reset data segments
	movw	$GDT_USER_DATA, %ax
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %fs
	movw	%ax, %gs

	# Pop the remaining userspace registers
	popa

	# Return to ring 3
	iret

###############################################################################
# Performs a task switch, but without leaving ring 0.
###############################################################################
task_switch_kernel:
	# Reserve 12 bytes on the process stack frame
	xchg	%bx, %bx
	movl	4(%edi), %ecx
	lea		-0xC(%ecx), %ecx
	movl	%ecx, task_iretd_stack_ptr

	# set %eip
	movl	8(%edi), %eax
	movl	%eax, 0x00(%ecx)

	# Set code segment
	movl	$GDT_KERN_CODE, %eax
	movl	%eax, 0x04(%ecx)

	# Get current flags (and re-enable IRQs in the flags)
	pushf
	popl	%eax
	or		$0x200, %eax
	movl	%eax, 0x08(%ecx)

	# popal should restore that fabricated stack pointer
	movl	%ecx, 0x18(%edi)

	# Reset data segments
	movw	$GDT_KERN_DATA, %ax
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %fs
	movw	%ax, %gs

	# Pop the remaining userspace registers
	lea		0xC(%edi), %esp
	popal

	# Continue execution of the task
	movl	task_iretd_stack_ptr, %esp
	iret

.section .bss
task_iretd_stack_ptr:
	.long	0
