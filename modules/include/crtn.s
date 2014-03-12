.section .init
	# Contents of crtend.o's .init section go here
	popl %ebp
	ret

.section .fini
	# Contents of crtend.o's .fini section go here
	popl %ebp
	ret
