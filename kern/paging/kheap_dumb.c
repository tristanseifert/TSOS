/*
 * Memory allocation algorithm used by the dumb kernel heap before the real
 * intelligent heap has been set up. Memory allocated through this way cannot
 * be deallocated again.
 */

#import <types.h>
#import "kheap.h"

// end is defined in the linker script.
extern uint32_t __kern_end;
uint32_t placement_address = (uint32_t) &__kern_end;

uint32_t kmalloc_int(size_t sz, bool align, uint32_t *phys) {
	if(align && (placement_address & 0xFFFFF000)) {
		placement_address &= 0xFFFFF000;
		placement_address += 0x1000;
	}
	
	// Get physical address, if requested
	if (phys) {
		*phys = placement_address & 0x0FFFFFFF;
	}

	uint32_t tmp = placement_address;
	placement_address += sz;
	return tmp;
}

uint32_t kmalloc_a(size_t sz) {
	return kmalloc_int(sz, 1, 0);
}

uint32_t kmalloc_p(size_t sz, uint32_t *phys) {
	return kmalloc_int(sz, 0, phys);
}

uint32_t kmalloc_ap(size_t sz, uint32_t *phys) {
	return kmalloc_int(sz, 1, phys);
}

uint32_t kmalloc(size_t sz) {
	return kmalloc_int(sz, 0, 0);
}