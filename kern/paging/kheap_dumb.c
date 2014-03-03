/*
 * Memory allocation algorithm used by the dumb kernel heap before the real
 * intelligent heap has been set up. Memory allocated through this way cannot
 * be deallocated again.
 */

#import <types.h>
#import "kheap.h"

// Smart heap functions
extern void *kheap_smart_alloc(size_t size, bool aligned, uint32_t *phys);

// end is defined in the linker script.
extern uint32_t __kern_end;
uint32_t dumb_heap_address = (uint32_t) &__kern_end;

// Smart kernel heap
extern heap_t *kernel_heap;

/*
 * Allocates memory.
 */
static void *kmalloc_int(size_t sz, bool align, uint32_t *phys) {
	// Use the dumb allocator if needed
	if(!kernel_heap) {
		if(align && (dumb_heap_address & 0xFFFFF000)) {
			dumb_heap_address &= 0xFFFFF000;
			dumb_heap_address += 0x1000;
		}
		
		// Get physical address, if requested
		if (phys) {
			*phys = dumb_heap_address & 0x0FFFFFFF;
		}

		uint32_t tmp = dumb_heap_address;
		dumb_heap_address += sz;
		return (void *) tmp;
	} else {
		return kheap_smart_alloc(sz, align, phys);
	}
}

void *kmalloc_a(size_t sz) {
	return kmalloc_int(sz, 1, 0);
}

void *kmalloc_p(size_t sz, uint32_t *phys) {
	return kmalloc_int(sz, 0, phys);
}

void *kmalloc_ap(size_t sz, uint32_t *phys) {
	return kmalloc_int(sz, 1, phys);
}

void *kmalloc(size_t sz) {
	return kmalloc_int(sz, 0, 0);
}