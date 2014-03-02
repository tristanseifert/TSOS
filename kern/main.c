#import <types.h>

void test_pagefault(void);

void kernel_main() {
	// Initialise modules
	modules_load();

	// test_pagefault();

	kprintf("Doom.");

	while(1);
}

void test_pagefault(void) {
	uint32_t *ptr = (uint32_t *) 0xA0000000;
	uint32_t do_page_fault = *ptr;

	kprintf("deather 0x%X\n", do_page_fault);
}