/*
 * Kernel heap!
 */
#import <types.h>

// Data types
typedef struct heap {
	/*
	 * The virtual starting address fo the heap. It is allocated free physical
	 * memory pages by the paging physical memory allocator.
	 */
	unsigned int start_address;

	/*
	 * Number of bytes currently occupied by the heap. This will usually be a
	 * multiple of the page size, since memory is allocated in pages.
	 */
	size_t size;

	/*
	 * Maximum address that the heap may grow to. If size+start_address is
	 * equal to this, any further memory allocations will fail, and an error
	 * code of ENOMEM will be set.
	 */
	unsigned int end_address;
} heap_t;

/*
 * Creates the kernel heap.
 */
void kheap_install();


// !Heap accessing functions
/*
 * Allocates a page-aligned chunk of memory.
 *
 * @param sz Size of memory to allocate
 */
void *kmalloc_a(size_t sz);

/*
 * Allocates a chunk of memory and gets physical address.
 *
 * @param sz Size of memory to allocate
 * @param phys Pointer to memory to place physical address in
 */
void *kmalloc_p(size_t sz, unsigned int *phys);

/*
 * Allocates a page-aligned chunk of memory and gets physical address.
 *
 * @param sz Size of memory to allocate
 * @param phys Pointer to memory to place physical address in
 */
void *kmalloc_ap(size_t sz, unsigned int *phys);

/*
 * Allocates a chunk of memory.
 *
 * @param sz Size of memory to allocate
 */
void *kmalloc(size_t sz);

/*
 * Deallocates a chunk of previously-allocated memory.
 *
 * @param address Address of memory on kernel heap
 */
void kfree(void* address);

/*
 * Resizes an allocated block of memory.
 *
 * @param addr Address of block
 * @param size New size to change to.
 */
void *krealloc(void *addr, size_t size);

/*
 * Allocates memory for count items with size bytes per item.
 *
 * @param count Number of items
 * @param size Size of a single item
 */
void *kcalloc(size_t count, size_t size);