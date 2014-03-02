.extern error_handler
.extern irq_handler

# IRQ handlers
.macro IRQ_HANDLER ARG1
	.globl irq_\ARG1
	.align 4
	irq_\ARG1:
		cli
		pushal
		mov		%eax, \ARG1
		call	irq_handler
		popal
		sti
		iretl
.endm

IRQ_HANDLER 0
IRQ_HANDLER 1
IRQ_HANDLER 2
IRQ_HANDLER 3
IRQ_HANDLER 4
IRQ_HANDLER 5
IRQ_HANDLER 6
IRQ_HANDLER 7
IRQ_HANDLER 8
IRQ_HANDLER 9
IRQ_HANDLER 10
IRQ_HANDLER 11
IRQ_HANDLER 12
IRQ_HANDLER 13
IRQ_HANDLER 14
IRQ_HANDLER 15

# Exception handlers
.globl	isr0
.globl	isr1
.globl	isr2
.globl	isr3
.globl	isr4
.globl	isr5
.globl	isr6
.globl	isr7
.globl	isr8
.globl	isr9
.globl	isr10
.globl	isr11
.globl	isr12
.globl	isr13
.globl	isr14
.globl	isr15
.globl	isr16
.globl	isr17
.globl	isr18
isr0:
	cli                 										# Disable interrupts
	pushl	$0x00												# Push a dummy error code
	pushl	$0x00												# Push the interrupt number
	jmp		error_common_stub									# Go to our common handler.
isr1:
	cli                 										# Disable interrupts
	pushl	$0x00												# Push a dummy error code
	pushl	$0x01												# Push the interrupt number
	jmp		error_common_stub									# Go to our common handler.
isr2:
	cli                 										# Disable interrupts
	pushl	$0x00												# Push a dummy error code
	pushl	$0x02												# Push the interrupt number
	jmp		error_common_stub									# Go to our common handler.
isr3:
	cli                 										# Disable interrupts
	pushl	$0x00												# Push a dummy error code
	pushl	$0x03												# Push the interrupt number
	jmp		error_common_stub									# Go to our common handler.
isr4:
	cli                 										# Disable interrupts
	pushl	$0x00												# Push a dummy error code
	pushl	$0x04												# Push the interrupt number
	jmp		error_common_stub									# Go to our common handler.
isr5:
	cli                 										# Disable interrupts
	pushl	$0x00												# Push a dummy error code
	pushl	$0x05												# Push the interrupt number
	jmp		error_common_stub									# Go to our common handler.
isr6:
	cli                 										# Disable interrupts
	pushl	$0x00												# Push a dummy error code
	pushl	$0x06												# Push the interrupt number
	jmp		error_common_stub									# Go to our common handler.
isr7:
	cli                 										# Disable interrupts
	pushl	$0x00												# Push a dummy error code
	pushl	$0x07												# Push the interrupt number
	jmp		error_common_stub									# Go to our common handler.
isr8:
	cli                 										# Disable interrupts
	pushl	$0x08												# Push the interrupt number
	jmp		error_common_stub									# Go to our common handler.
isr9:
	cli                 										# Disable interrupts
	pushl	$0x00												# Push a dummy error code
	pushl	$0x09												# Push the interrupt number
	jmp		error_common_stub									# Go to our common handler.
isr10:
	cli                 										# Disable interrupts
	pushl	$0x0A												# Push the interrupt number
	jmp		error_common_stub									# Go to our common handler.
isr11:
	cli                 										# Disable interrupts
	pushl	$0x0B												# Push the interrupt number
	jmp		error_common_stub									# Go to our common handler.
isr12:
	cli                 										# Disable interrupts
	pushl	$0x0C												# Push the interrupt number
	jmp		error_common_stub									# Go to our common handler.
isr13:
	cli                 										# Disable interrupts
	pushl	$0x0D												# Push the interrupt number
	jmp		error_common_stub									# Go to our common handler.
isr15:
	cli                 										# Disable interrupts
	pushl	$0x00												# Push a dummy error code
	pushl	$0x0F												# Push the interrupt number
	jmp		error_common_stub									# Go to our common handler.
isr16:
	cli                 										# Disable interrupts
	pushl	$0x00												# Push a dummy error code
	pushl	$0x10												# Push the interrupt number
	jmp		error_common_stub									# Go to our common handler.
isr17:
	cli                 										# Disable interrupts
	pushl	$0x00												# Push a dummy error code
	pushl	$0x11												# Push the interrupt number
	jmp		error_common_stub									# Go to our common handler.
isr18:
	cli                 										# Disable interrupts
	pushl	$0x00												# Push a dummy error code
	pushl	$0x12												# Push the interrupt number
	jmp		error_common_stub									# Go to our common handler.

# Page fault handler
.extern paging_page_fault_handler
isr14:
	cli                 										# Disable interrupts
	pushl	$0x0E												# Push the interrupt number
	pusha														# Pushes edi,esi,ebp,esp,ebx,edx,ecx,eax

	mov		%ds, %ax											# Lower 16-bits of eax = ds.
	push	%eax												# save the data segment descriptor

	mov 	$0x10, %ax											# load the kernel data segment descriptor
	mov 	%ax, %ds
	mov 	%ax, %es
	mov 	%ax, %fs
	mov 	%ax, %gs

	call	paging_page_fault_handler							# Go to our page fault handler.
	
	pop 	%eax												# reload the original data segment descriptor
	mov 	%ax, %ds
	mov 	%ax, %es
	mov 	%ax, %fs
	mov 	%ax, %gs

	popa														# Pops edi,esi,ebp...
	add 	$0x8, %esp											# Cleans up the pushed error code and pushed ISR number
	sti
	iret														# pops 5 things at once: CS, EIP, EFLAGS, SS, and ESP

# Stub called by error doohickeys
error_common_stub:
	pusha														# Pushes edi,esi,ebp,esp,ebx,edx,ecx,eax

	mov		%ds, %ax											# Lower 16-bits of eax = ds.
	push	%eax												# save the data segment descriptor

	mov 	$0x10, %ax											# load the kernel data segment descriptor
	mov 	%ax, %ds
	mov 	%ax, %es
	mov 	%ax, %fs
	mov 	%ax, %gs

	call 	error_handler

	pop 	%eax												# reload the original data segment descriptor
	mov 	%ax, %ds
	mov 	%ax, %es
	mov 	%ax, %fs
	mov 	%ax, %gs

	popa														# Pops edi,esi,ebp...
	add 	$0x8, %esp											# Cleans up the pushed error code and pushed ISR number
	sti
	iret														# pops 5 things at once: CS, EIP, EFLAGS, SS, and ESP

.globl irq_dummy
irq_dummy:
	iret
