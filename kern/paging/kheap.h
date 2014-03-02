#import <types.h>

typedef struct heap heap_t;

struct heap {
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
};

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
heap_t *heap_alloc(uint32_t start, size_t size, uint32_t end, bool supervisor, bool readonly);

/*
 * Destroys an allocated heap, and relinquishes all memory it allocated.
 *
 * @param heap A valid heap object to destroy.
 */
void heap_dealloc(heap_t *heap);

/*
 * Allocates a continuous block of memory on the specified heap.
 *
 * @param heap Valid heap object to allocate on
 * @param size Number of bytes to allocate
 * @param aligned Whether the allocation should be aligned to page boundaries
 * @return Pointer to memory, or NULL if error.
 */
void *alloc(heap_t *heap, size_t size, bool aligned);

/*
 * Frees a previously allocated block of memory on the specified heap.
 *
 * @param heap Valid heap object that the address was allocated on
 * @Param address The address to deallocate
 */
void free(heap_t *heap, void *address);


// !Heap convenience functions
/*
 * Allocates a chunk of memory.
 *
 * @param sz Size of memory to allocati
 * @param align When set, allocation is
 * @param phys Pointer to memory to place physical address in
 */
uint32_t kmalloc_int(size_t sz, bool align, uint32_t *phys);

/*
 * Allocates a page-aligned chunk of memory.
 *
 * @param sz Size of memory to allocate
 */
uint32_t kmalloc_a(size_t sz);

/*
 * Allocates a chunk of memory and gets physical address.
 *
 * @param sz Size of memory to allocate
 * @param phys Pointer to memory to place physical address in
 */
uint32_t kmalloc_p(size_t sz, uint32_t *phys);

/*
 * Allocates a page-aligned chunk of memory and gets physical address.
 *
 * @param sz Size of memory to allocate
 * @param phys Pointer to memory to place physical address in
 */
uint32_t kmalloc_ap(size_t sz, uint32_t *phys);

/*
 * Allocates a chunk of memory.
 *
 * @param sz Size of memory to allocate
 */
uint32_t kmalloc(size_t sz);