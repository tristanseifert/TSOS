#import <types.h>
#import "x86_pc/x86_pc.h"

void __stack_chk_guard_setup(void);

void smash_stack(char *input) {
	char buf[16];

	strcpy(buf, input);
}

void kernel_main(void) {
	// Initialise modules
	modules_load();

/*	klog(kLogLevelDebug, "Debug 0x%X", 0xDEADCACA);
	klog(kLogLevelInfo, "Info 0x%X", 0xDEADBEEF);
	klog(kLogLevelSuccess, "Success 0x%X", 0xCAFEBABE);
	klog(kLogLevelWarning, "Warning 0x%X", 0x80808080);
	klog(kLogLevelError, "Error 0x%X", 0x12345678);
	klog(kLogLevelCritical, "Critical 0x%X", 0xD00D);*/
	
/*	klog(kLogLevelDebug, "Begin memory test.\n");

	int numChunks = 32;
	int sizes[8] = {16, 32, 64, 128, 256, 512, 1024, 2048};
	void *memories[numChunks];
	int currentSize = 0;
	uint32_t phys;

	for(int i = 0; i < 8; i++) {
		currentSize = sizes[i];
		klog(kLogLevelDebug, "Allocating %u %u byte chunks", numChunks, currentSize);

		for(int x = 0; x < numChunks; x++) {
			memories[x] = (void *) kmalloc_p(currentSize, &phys);
		}

		klog(kLogLevelWarning, "NIEDERKLATSCHEN SCHREIBKUGELN 0x%X", phys);

		klog(kLogLevelDebug, "Freeing chunks");

		for(int x = 0; x < numChunks; x++) {
			kfree(memories[x]);
		}
	} */

	char *doom = "smash the stack !!!!!!!!";
	smash_stack(doom);

	while(1);
}

/*
 * Performs initialisation before the actual kernel runs.
 */
void kernel_init(void) {
	// Set up stack guards
	__stack_chk_guard_setup();

	// Set up platform
	x86_pc_init();
}

void test_pagefault(void) {
	uint32_t *ptr = (uint32_t *) 0xA0000000;
	uint32_t do_page_fault = *ptr;

	kprintf("deather 0x%X\n", (unsigned int) do_page_fault);
}