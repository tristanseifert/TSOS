/*
 * Memory allocation algorithm used by the dumb kernel heap before the real
 * intelligent heap has been set up. Memory allocated through this way cannot
 * be deallocated again.
 */

#import <types.h>
#import "kheap.h"

// Smart heap functions
extern void *kheap_smart_alloc(size_t size, bool aligned, unsigned int *phys);

// end is defined in the linker script.
extern unsigned int __kern_end;
unsigned int dumb_heap_address = (unsigned int) &__kern_end;

// Smart kernel heap
extern heap_t *kernel_heap;

/*
 * Allocates memory.
 */
static void *kmalloc_int(size_t sz, bool align, unsigned int *phys) {
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

		unsigned int tmp = dumb_heap_address;
		dumb_heap_address += sz;
		return (void *) tmp;
	} else {
		return kheap_smart_alloc(sz, align, phys);
	}
}

void *kmalloc_a(size_t sz) {
	return kmalloc_int(sz, true, NULL);
}

void *kmalloc_p(size_t sz, unsigned int *phys) {
	return kmalloc_int(sz, false, phys);
}

void *kmalloc_ap(size_t sz, unsigned int *phys) {
	return kmalloc_int(sz, true, phys);
}

void *kmalloc(size_t sz) {
	return kmalloc_int(sz, false, NULL);
}