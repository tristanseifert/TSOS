.globl task_switch

# OR with 0x03 for ring 3
.set GDT_USER_CODE, 0x18
.set GDT_USER_DATA, 0x20

###############################################################################
# Performs a context switch, assuming that a task_state_t struct has been
# pushed onto the stack.
###############################################################################
task_switch:
	mov		task_new_tempstack, %ecx

	# Set the ring this program will execute in into %ebx
	movl	$0x03, %ebx

	# set userspace %esp
	pop		%eax
	mov		%eax, 12(%ecx)

	# Set data segment (with correct ring mask applied)
	movl	$GDT_USER_DATA, %eax
	or		%ebx, %eax
	movl	%eax, 16(%ecx)

	# set userspace %eip
	pop		%eax
	mov		%eax, 0(%ecx)

	# Set code segment
	movl	$GDT_USER_CODE, %eax
	or		%ebx, %eax
	movl	%eax, 4(%ecx)

	# Get current flags
	pushf
	pop		%eax
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
	mov		task_new_tempstack, %esp

	# Return to ring 3
	iret

.section .bss
task_new_tempstack:
	.skip	4*6
