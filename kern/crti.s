.section .init
.global _init
.type _init, @function
_init:
	push %ebp
	movl %esp, %ebp
	# Contents of crtbegin.o's .init section go here

.section .fini
.global _fini
.type _fini, @function
_fini:
	push %ebp
	movl %esp, %ebp
	# Contents of crtbegin's .fini section go here
