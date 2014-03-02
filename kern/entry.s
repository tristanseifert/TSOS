#########################################################################################
# Multiboot header
#########################################################################################
.set ALIGN,    1 << 0						# align loaded modules on page boundaries
.set MEMINFO,  1 << 1						# provide memory map
.set VIDINFO,  1 << 2						# OS wants video mode set
.set FLAGS,    ALIGN | MEMINFO# | VIDINFO	# this is the multiboot 'flag' field
.set MAGIC,    0x1BADB002					# 'magic number' lets bootloader find the header
.set CHECKSUM, -(MAGIC + FLAGS)				# checksum required

#########################################################################################
# This function is responsible for taking control from the bootloader, re-locating the
# kernel to a more sane address, and then starting the kernel.
#########################################################################################
.section .multiboot
.long MAGIC
.long FLAGS
.long CHECKSUM
.long 0, 0, 0, 0, 0							# This is load address stuff we don't care for
.long 0										# Set graphics mode
.long 1024, 768, 32							# Width, height, depth

.section .entry

.globl _osentry
.type _osentry, @function
.set osentry, (_osentry - 0xC0000000)
.globl osentry

_osentry:
	cli

	# Enable protected mode
	mov		%cr0, %ecx
	or		$0x00000001, %ecx
	mov		%ecx, %cr0	

	# Load page directory
	mov		$(boot_page_directory - 0xC0000000), %ecx
	mov		%ecx, %cr3

	# Enable paging
	mov		%cr0, %ecx
	or		$0x80000000, %ecx
	mov		%ecx, %cr0

	# jump to the higher half kernel
	lea		loader, %ecx
	jmp		*%ecx
 

# We have rudimentary paging down here.
loader:
	# Use virtual address for stack
	mov		$stack_top, %esp

	# Set up GDT
	lgdt	gdt_table

	# Reload GDT entries
	jmp		$0x08, $.gdtFlush

.gdtFlush:
	mov		$0x10, %cx
	mov		%cx, %ds
	mov		%cx, %es
	mov		%cx, %fs
	mov		%cx, %gs
	mov		%cx, %ss

	# Push multiboot info (EBX) and magic number (EAX)
	#push 	%ebx
	#push	%eax

	# Write multiboot info pointer to memory
	#mov		%ebx, sys_multiboot_info

	# Check for SSE, and if it exists, enable it
	mov		$0x1, %eax
	cpuid
	test	$(1 << 25), %edx
	jz 		.+5
	
	call	sse_init

	# Initialise paging
	call	paging_init

	# Jump into the kernel's main function
	call	kernel_main

	# In case the kernel returns, hang forever
	cli

.Lhang:
	hlt
	jmp .Lhang

/*
 * Enables the SSE features of the processor.
 */
sse_init:
	mov		%cr0, %eax

	# clear coprocessor emulation CR0.EM
	and		$0xFFFB, %ax

	# set coprocessor monitoring CR0.MP
	or		$0x2, %ax
	mov		%eax, %cr0
	mov		%cr4, %eax

	# set CR4.OSFXSR and CR4.OSXMMEXCPT at the same time
	or		$(3 << 9), %ax
	mov		%eax, %cr4
	ret

# GDT
.section .data
	.align	0x10

gdt_start:
	.quad	0x0000000000000000									# Null Descriptor
	.quad	0x00CF9A000000FFFF									# Kernel code
	.quad	0x00CF92000000FFFF									# Kernel data
	.quad	0x00CFFA000000FFFF									# User code
	.quad	0x00CFF2000000FFFF									# User data

gdt_table:
	.word	0x27												# Length
	.long	gdt_start											# Linear address to GDT	


# Reserve a stack of 128K
.section .bss
stack_bottom:
.skip 0x10000
stack_top:


.section .bss

# Bootup page tables
.section .entry.data

# Map first 4MB of space.
.align 0x1000
boot_page_table:
	.set addr, 0

	.rept 1024
	.long addr + 0x03
	.set addr, addr+0x1000
	.endr

# Map second 4MB of space.
.align 0x1000
boot_page_table2:
	.set addr, 0x00400000

	.rept 1024
	.long addr + 0x03
	.set addr, addr+0x1000
	.endr

# Mirror the first 4 MB throughout the address space.
.align 0x1000
boot_page_directory:
	.rept 512
	.long (boot_page_table - 0xC0000000) + 0x07
	.long (boot_page_table2 - 0xC0000000) + 0x07
	.endr
