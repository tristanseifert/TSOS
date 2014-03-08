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

	# Userspace context switch
	mov		$task_new_tempstack, %ecx

	# Set the ring this program will execute in into %ebx
	movl	$0x03, %ebx

	# set userspace %esp
	movl	4(%edi), %eax
	mov		%eax, 12(%ecx)

	# Set data segment (with correct ring mask applied)
	movl	$GDT_USER_DATA, %eax
	or		%ebx, %eax
	movl	%eax, 16(%ecx)

	# set userspace %eip
	movl	8(%edi), %eax
	mov		%eax, 0(%ecx)

	# Set code segment
	movl	$GDT_USER_CODE, %eax
	or		%ebx, %eax
	movl	%eax, 4(%ecx)

	# Get current flags (and re-enable IRQs in the flags)
	pushf
	pop		%eax
	or		$0x200, %eax
	mov		%eax, 8(%ecx)

	# Reset data segments
	mov		$GDT_USER_DATA, %ax
	mov		%ax, %ds
	mov		%ax, %es
	mov		%ax, %fs
	mov		%ax, %gs

	# Pop the remaining userspace registers
	popa

	# Set the stack pointer to a special area we prepared so IRET works
	mov		$task_new_tempstack, %esp

	# Return to ring 3
	iret

###############################################################################
# Performs a task switch, but without leaving ring 0.
###############################################################################
task_switch_kernel:
	mov		$task_new_tempstack, %ecx

	# set userspace %esp
	movl	4(%edi), %eax
	mov		%eax, 0x0C(%ecx)

	# Set data segment (with correct ring mask applied)
	movl	$GDT_KERN_DATA, %eax
	movl	%eax, 0x10(%ecx)

	# set %eip
	movl	8(%edi), %eax
	mov		%eax, 0x00(%ecx)

	# Set code segment
	movl	$GDT_KERN_CODE, %eax
	movl	%eax, 0x04(%ecx)

	# Get current flags (and re-enable IRQs in the flags)
	pushf
	pop		%eax
	or		$0x200, %eax
	mov		%eax, 0x08(%ecx)

	# Reset data segments
	mov		$GDT_KERN_DATA, %ax
	mov		%ax, %ds
	mov		%ax, %es
	mov		%ax, %fs
	mov		%ax, %gs

	# Pop the remaining userspace registers
	lea		0xC(%edi), %esp
	popa

	# Set the stack pointer to a special area we prepared so IRET works
	mov		$task_new_tempstack, %esp
	xchg	%bx, %bx

	# Continue execution of the task
	iret

.section .bss
	.align	0x10
task_new_tempstack:
	.skip	4*8
