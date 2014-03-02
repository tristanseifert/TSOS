#import <types.h>
#import "kheap.h"
#import "paging.h"

heap_t *kernel_heap = NULL;

/*
 * Creates a heap.
 *
 * @param start Starting address
 * @param size Memory to allocate. If not specified, 4 pages are assumed.
 * @param end End address of the heap
 * @param supervisor When set, mapped pages are only accessible by supervisor.
 * @param readonly Causes mapped pages to be readonly to lower priv levels.
 * @return An allocated heap object.
 */
heap_t *heap_alloc(uint32_t start, size_t size, uint32_t end, bool supervisor, bool readonly) {
	// Allocate memory and set it to zero.
	heap_t *heap = (heap_t *) kmalloc(sizeof(heap_t));
	if(!heap) return NULL;

	memclr(heap, sizeof(heap_t));

	// Set up protection attributes
	heap->is_supervisor = supervisor;
	heap->is_readonly = readonly;
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
 * @param heap Valid heap object to allocate on
 * @param size Number of bytes to allocate
 * @param aligned Whether the allocation should be aligned to page boundaries
 * @return Pointer to memory, or NULL if error.
 */
void *alloc(heap_t *heap, size_t size, bool aligned) {

}

/*
 * Frees a previously allocated block of memory on the specified heap.
 *
 * @param heap Valid heap object that the address was allocated on
 * @Param address The address to deallocate
 */
void free(heap_t *heap, void *address) {

}
