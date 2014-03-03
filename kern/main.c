#import <types.h>

void test_pagefault(void);

void kernel_main() {
	// Initialise modules
	modules_load();

/*	klog(kLogLevelDebug, "Debug 0x%X", 0xDEADCACA);
	klog(kLogLevelInfo, "Info 0x%X", 0xDEADBEEF);
	klog(kLogLevelSuccess, "Success 0x%X", 0xCAFEBABE);
	klog(kLogLevelWarning, "Warning 0x%X", 0x80808080);
	klog(kLogLevelError, "Error 0x%X", 0x12345678);
	klog(kLogLevelCritical, "Critical 0x%X", 0xD00D);*/

	// test_pagefault();

	klog(kLogLevelDebug, "Begin memory test.\n");

	int numChunks = 4;
	int sizes[8] = {63, 64, 64, 1024, 4096, 8192, 16384, 17000};
	void *memories[numChunks];

	int currentSize = 0;

	for(int i = 0; i < 8; i++) {
		currentSize = sizes[i];
		klog(kLogLevelDebug, "Allocating %u %u byte chunks", numChunks, currentSize);

		for(int x = 0; x < numChunks; x++) {
			memories[x] = (void *) kmalloc(currentSize);
		}

		klog(kLogLevelDebug, "Freeing chunks");

		for(int x = 0; x < numChunks-1; x++) {
			kfree(memories[x]);
		}
	}
	

	while(1);
}

void test_pagefault(void) {
	uint32_t *ptr = (uint32_t *) 0xA0000000;
	uint32_t do_page_fault = *ptr;

	kprintf("deather 0x%X\n", do_page_fault);
}