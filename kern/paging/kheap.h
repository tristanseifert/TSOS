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
	uint32_t start_address;

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
	uint32_t end_address;

	/*
	 * Whether allocated pages are supervisor only or readonly.
	 */
	bool is_supervisor, is_readonly;

	/*
	 * A bitmap of pages that have been allocated physical memory throughout
	 * the memory range allocated to the kernel heap. Used to find places
	 * where to map stuff.
	 */
	uint32_t *bitmap;
} heap_t;

/*
 * Creates the kernel heap.
 *
 * @param start Starting address
 * @param end End address of the heap
 * @param supervisor When set, mapped pages are only accessible by supervisor.
 * @param readonly Causes mapped pages to be readonly to lower priv levels.
 */
void kheap_install(uint32_t start, uint32_t end, bool supervisor, bool readonly);


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
void *kmalloc_p(size_t sz, uint32_t *phys);

/*
 * Allocates a page-aligned chunk of memory and gets physical address.
 *
 * @param sz Size of memory to allocate
 * @param phys Pointer to memory to place physical address in
 */
void *kmalloc_ap(size_t sz, uint32_t *phys);

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