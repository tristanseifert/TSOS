#import <types.h>
#import "kheap.h"
#import "paging.h"
#import "liballoc_1_1.h"

#define DEBUG_NULL_FREE 0

// Bitmap macros
#define INDEX_FROM_BIT(a) (a/(8*4))
#define OFFSET_FROM_BIT(a) (a%(8*4))

// Internal functions
void *kheap_smart_alloc(size_t size, bool aligned, uint32_t *phys);
static void free(void *address);

// Kernel heap and pagetables
heap_t *kernel_heap = NULL;
extern page_directory_t *kernel_directory;
static uint32_t nframes;

/*
 * Set a bit in the kernel_heap->bitmap bitset
 */
static void set_frame(uint32_t frame_addr) {
	uint32_t frame = frame_addr / 0x1000;
	uint32_t idx = INDEX_FROM_BIT(frame);
	uint32_t off = OFFSET_FROM_BIT(frame);
	kernel_heap->bitmap[idx] |= (0x1 << off);
}

/*
 * Clear a bit in the kernel_heap->bitmap bitset
 */
static void clear_frame(uint32_t frame_addr) {
	uint32_t frame = frame_addr / 0x1000;
	uint32_t idx = INDEX_FROM_BIT(frame);
	uint32_t off = OFFSET_FROM_BIT(frame);
	kernel_heap->bitmap[idx] &= ~(0x1 << off);
}

/*
 * Check if a certain page is allocated.
 */
static uint32_t test_frame(uint32_t frame_addr) {
	uint32_t frame = frame_addr / 0x1000;
	uint32_t idx = INDEX_FROM_BIT(frame);
	uint32_t off = OFFSET_FROM_BIT(frame);
	return (kernel_heap->bitmap[idx] & (0x1 << off));
}

/*
 * Creates the kernel heap.
 *
 * @param start Starting address
 * @param end End address of the heap
 * @param supervisor When set, mapped pages are only accessible by supervisor.
 * @param readonly Causes mapped pages to be readonly to lower priv levels.
 */
void kheap_install(uint32_t start, uint32_t end, bool supervisor, bool readonly) {
	// Allocate memory for pages
	for(int i = start; i < end; i += 0x1000) {
		paging_get_page(i, true, kernel_directory);
	}

	// Allocate memory and set it to zero.
	heap_t *heap = (heap_t *) kmalloc(sizeof(heap_t));
	ASSERT(heap);

	memclr(heap, sizeof(heap_t));

	// Allocate memory for the bitmap
	uint32_t size = end - start;
	nframes = size / 0x1000;

	heap->bitmap = kmalloc(INDEX_FROM_BIT(nframes));
	memclr(heap->bitmap, INDEX_FROM_BIT(nframes));

	// Start address
	heap->start_address = start;

	// End address
	heap->end_address = end;

	// Set up protection attributes
	heap->is_supervisor = supervisor;
	heap->is_readonly = readonly;

	// Finish.
	kernel_heap = heap;
}

/*
 * Destroys an allocated heap, and relinquishes all memory it allocated.
 *
 * @param heap A valid heap object to destroy.
 */
void heap_dealloc(heap_t *heap) {
	// Don't deallocate the kernel heap!
	if(unlikely(heap == kernel_heap)) {
		PANIC("Tried to deallocate kernel heap");
		return;
	}
}

/*
 * Allocates a continuous block of memory on the specified heap.
 *
 * @param size Number of bytes to allocate
 * @param aligned Whether the allocation should be aligned to page boundaries
 * @param phys Pointer to memory to store the physical address in
 * @return Pointer to memory, or NULL if error.
 */
void *kheap_smart_alloc(size_t size, bool aligned, uint32_t *phys) {
	uint32_t ptr = (uint32_t) lalloc_malloc(size);

	// klog(kLogLevelWarning, "SCHREIBKUGEL ALLOC sized 0x%08X at 0x%08X", size, ptr);

	return (void *) ptr;
}

/*
 * Frees a previously allocated block of memory on the specified heap.
 *
 * @Param address The address to deallocate
 */
static void free(void *address) {
#if DEBUG_NULL_FREE
	if(!address) {
		klog(kLogLevelError, "Tried to deallocate NULL address");
		dump_stack_here();
		return;
	}
#endif

	// liballoc
	lalloc_free(address);
	// klog(kLogLevelError, "SCHREIBKUGEL DEALLOC at 0x%08X", address);
}

/*
 * Deallocates a chunk of previously-allocated memory.
 *
 * @param address Address of memory on kernel heap
 */
void kfree(void* address) {
	if(kernel_heap) {
		free(address);
	} else {
		klog(kLogLevelWarning, "Tried to free 0x%X on dumb heap", address);
	}
}

/*
 * Locking functions for liballoc
 */
int liballoc_lock() {
	return 0;
}

int liballoc_unlock() {
	return 0;
}

/*
 * Allocate pages pages of memory
 */
void* liballoc_alloc(size_t pages) {
	bool pagesFound = false;
	uint32_t first_free_page = 0;
	void *start = NULL;

	// Check all free frames for a section of pages
	uint32_t i, j, k;
	for (i = 0; i < INDEX_FROM_BIT(nframes); i++) {
		// Skip if all 32 frames are filled
		if (kernel_heap->bitmap[i] != 0xFFFFFFFF) {
			// Check which one of the 32 frames are filled
			for (j = 0; j < 32; j++) {
				uint32_t toTest = 0x1 << j;

				// If this frame is free, return
				if (!(kernel_heap->bitmap[i] & toTest)) {
					first_free_page = i*4*8+j;

					// Check if there's pages-1 free pages after this page
					for(k = 1; k < pages; k++) {
						// Test the page
						if(test_frame(first_free_page + k)) {
							// If not free, end the for loop
							pagesFound = false;
							goto notFree;
						}
					}

					// We found enough free pages.
					pagesFound = true;
					goto pagesFound;

					notFree: ;
				}
			}
		}
	}

	// We drop down here if there wasn't enough pages
	klog(kLogLevelError, "Could not allocate 0x%X pages (last checked page is 0x%X)", pages, first_free_page);
	return NULL;

	// Enough free pages were found
	pagesFound:;
	klog(kLogLevelDebug, "Allocated 0x%X pages (page 0x%X)", pages, first_free_page);
	start = (void *) (first_free_page * 0x1000) + kernel_heap->start_address;

	uint32_t first_addr = (first_free_page * 0x1000) + kernel_heap->start_address;

	page_t *page;

	// Allocate requested pages some physical memory
	for(int p = 0; p < pages; p++) {
		page = paging_get_page(first_addr, false, kernel_directory);
		alloc_frame(page, kernel_heap->is_supervisor, !kernel_heap->is_readonly);

		// Mark this frame as set
		set_frame(first_addr - kernel_heap->start_address);

		// Advance allocation pointer
		first_addr += 0x1000;
	}

	// Increment allocation counter
	kernel_heap->size += pages;
	klog(kLogLevelDebug, "Heap size: 0x%X", kernel_heap->size);

	return start;
}

/*
 * Frees pages number of pages of consecutive memory, starting at mem.
 */
int liballoc_free(void *mem, size_t pages) {
	page_t *page;
	uint32_t start = (uint32_t) mem;

	// Loop through all the pages
	for(int i = 0; i < pages; i++) {
		page = paging_get_page(start + (i * 0x1000), false, kernel_directory);
		free_frame(page);
	}

	// Stats
	kernel_heap->size -= pages;
	klog(kLogLevelDebug, "Heap size: 0x%X", kernel_heap->size);

	return 0;
}