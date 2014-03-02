#import <types.h>

void test_pagefault(void);

void kernel_main() {
	// Initialise modules
	modules_load();

	// test_pagefault();

	klog(kLogLevelDebug, "Debug 0x%X", 0xDEADCACA);
	klog(kLogLevelInfo, "Info 0x%X", 0xDEADBEEF);
	klog(kLogLevelSuccess, "Success 0x%X", 0xCAFEBABE);
	klog(kLogLevelWarning, "Warning 0x%X", 0x80808080);
	klog(kLogLevelError, "Error 0x%X", 0x12345678);
	klog(kLogLevelCritical, "Critical 0x%X", 0xD00D);

	// test_pagefault();

	while(1);
}

void test_pagefault(void) {
	uint32_t *ptr = (uint32_t *) 0xA0000000;
	uint32_t do_page_fault = *ptr;

	kprintf("deather 0x%X\n", do_page_fault);
}