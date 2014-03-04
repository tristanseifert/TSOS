#import <types.h>
#import "kheap.h"
#import "paging.h"

#define DEBUG_NULL_FREE 0
#define DEBUG_PAGE_ALLOCATION 0

// Config options for allocator

// Alignment enforced for memory
#define ALIGNMENT		16ul
#define ALIGN_TYPE		char
// Number of bytes each chunk needs extra for alignment info
#define ALIGN_INFO		sizeof(ALIGN_TYPE)*16

// Pointer alignment macros
#define ALIGN(ptr)													\
		if (ALIGNMENT > 1)											\
		{																\
			uintptr_t diff;												\
			ptr = (void*)((uintptr_t)ptr + ALIGN_INFO);					\
			diff = (uintptr_t)ptr & (ALIGNMENT-1);						\
			if (diff != 0)											\
			{															\
				diff = ALIGNMENT - diff;								\
				ptr = (void*)((uintptr_t)ptr + diff);					\
			}															\
			*((ALIGN_TYPE*)((uintptr_t)ptr - ALIGN_INFO)) = 			\
				diff + ALIGN_INFO;										\
		}															


#define UNALIGN(ptr)													\
		if (ALIGNMENT > 1)											\
		{																\
			uintptr_t diff = *((ALIGN_TYPE*)((uintptr_t)ptr - ALIGN_INFO));	\
			if (diff < (ALIGNMENT + ALIGN_INFO))						\
			{															\
				ptr = (void*)((uintptr_t)ptr - diff);					\
			}															\
		}

#define LIBALLOC_MAGIC	0xc001c0de
#define LIBALLOC_DEAD	0xdeaddead

// Allocator types
/** A structure found at the top of all system allocated 
 * memory blocks. It details the usage of the memory block.
 */
struct allocator_major {
	struct allocator_major *prev;		///< Linked list information.
	struct allocator_major *next;		///< Linked list information.
	unsigned int pages;					///< The number of pages in the block.
	unsigned int size;					///< The number of pages in the block.
	unsigned int usage;					///< The number of bytes used in the block.
	struct allocator_minor *first;		///< A pointer to the first allocated memory in the block.	
};

/** This is a structure found at the beginning of all
 * sections in a major block which were allocated by a
 * malloc, calloc, realloc call.
 */
struct	allocator_minor {
	struct allocator_minor *prev;		///< Linked list information.
	struct allocator_minor *next;		///< Linked list information.
	struct allocator_major *block;		///< The owning block. A pointer to the major structure.
	unsigned int magic;					///< A magic number to idenfity correctness.
	unsigned int size; 					///< The size of the memory allocated. Could be 1 byte or more.
	unsigned int req_size;				///< The size of memory requested.
};

// Allocator state
static struct allocator_major *l_memRoot = NULL;	///< The root memory block acquired from the system.
static struct allocator_major *l_bestBet = NULL; ///< The major with the most free memory.

static unsigned int l_pageSize  = 4096;			///< The size of an individual page. Set up in allocator_init.
static unsigned int l_pageCount = 16;			///< The number of pages to request per chunk. Set up in allocator_init.
static unsigned long long l_allocated = 0;		///< Running total of allocated memory.
static unsigned long long l_inuse	 = 0;		///< Running total of used memory.

static long long l_warningCount = 0;		///< Number of warnings encountered
static long long l_errorCount = 0;			///< Number of actual errors
static long long l_possibleOverruns = 0;	///< Number of possible overruns

// Bitmap macros
#define INDEX_FROM_BIT(a) (a/(8*4))
#define OFFSET_FROM_BIT(a) (a%(8*4))

// Internal functions
void *kheap_smart_alloc(size_t size, bool aligned, unsigned int *phys);
static void free(void *address);

// Memory allocator
static void *lalloc_malloc(size_t);
static void *lalloc_realloc(void *, size_t);
static void *lalloc_calloc(size_t, size_t);
static void lalloc_free(void *);

// Kernel heap and pagetables
heap_t *kernel_heap = NULL;
extern page_directory_t *kernel_directory;
static unsigned int nframes;

/*
 * Set a bit in the kernel_heap->bitmap bitset
 */
static void set_frame(unsigned int frame_addr) {
	unsigned int frame = frame_addr / 0x1000;
	unsigned int idx = INDEX_FROM_BIT(frame);
	unsigned int off = OFFSET_FROM_BIT(frame);
	kernel_heap->bitmap[idx] |= (0x1 << off);
}

/*
 * Clear a bit in the kernel_heap->bitmap bitset
 */
static void clear_frame(unsigned int frame_addr) {
	unsigned int frame = frame_addr / 0x1000;
	unsigned int idx = INDEX_FROM_BIT(frame);
	unsigned int off = OFFSET_FROM_BIT(frame);
	kernel_heap->bitmap[idx] &= ~(0x1 << off);
}

/*
 * Check if a certain page is allocated.
 */
static unsigned int test_frame(unsigned int frame_addr) {
	unsigned int frame = frame_addr / 0x1000;
	unsigned int idx = INDEX_FROM_BIT(frame);
	unsigned int off = OFFSET_FROM_BIT(frame);
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
void kheap_install(unsigned int start, unsigned int end, bool supervisor, bool readonly) {
	// Allocate memory for pages
	for(int i = start; i < end; i += 0x1000) {
		paging_get_page(i, true, kernel_directory);
	}

	// Allocate memory and set it to zero.
	heap_t *heap = (heap_t *) kmalloc(sizeof(heap_t));
	ASSERT(heap);

	memclr(heap, sizeof(heap_t));

	// Allocate memory for the bitmap
	unsigned int size = end - start;
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
 * Allocates a continuous block of memory on the specified heap.
 *
 * @param size Number of bytes to allocate
 * @param aligned Whether the allocation should be aligned to page boundaries
 * @param phys Pointer to memory to store the physical address in
 * @return Pointer to memory, or NULL if error.
 */
void *kheap_smart_alloc(size_t size, bool aligned, unsigned int *phys) {
	unsigned int ptr = (unsigned int) lalloc_malloc(size);

	// Handle an out of memory condition
	if(!ptr) {
		errno = ENOMEM;
		return NULL;
	}

	// klog(kLogLevelWarning, "SCHREIBKUGEL ALLOC sized 0x%08X at 0x%08X", size, ptr);

	// Do we want the physical address?
	if(phys) {
		page_t *page = paging_get_page(ptr & 0xFFFFF000, false, kernel_directory);
		*phys = (page->frame << 12) | (ptr & 0x00000FFF);
	}

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
		klog(kLogLevelWarning, "Tried to free 0x%X on dumb heap", (unsigned int) address);
	}
}

/*
 * Resizes an allocated block of memory.
 *
 * @param addr Address of block
 * @param size New size to change to.
 */
void *krealloc(void *addr, size_t size) {
	return lalloc_realloc(addr, size);
}

/*
 * Allocates memory for count items with size bytes per item.
 *
 * @param count Number of items
 * @param size Size of a single item
 */
void *kcalloc(size_t count, size_t size) {
	return lalloc_calloc(count, size);
}

/*
 * Locking functions for liballoc
 */
static int allocator_lock() {
	return 0;
}

static int allocator_unlock() {
	return 0;
}

/*
 * Allocate pages pages of memory
 */
static void* allocator_alloc(size_t pages) {
	bool pagesFound = false;
	unsigned int first_free_page = 0;
	void *start = NULL;

	// Check all free frames for a section of pages
	unsigned int i, j, k, l = 0;

	for (i = 0; i < INDEX_FROM_BIT(nframes); i++) {
		// Skip if all 32 frames are filled
		if (kernel_heap->bitmap[i] != 0xFFFFFFFF) {
			// Check which one of the 32 frames are filled
			for (j = 0; j < 32; j++) {
				unsigned int toTest = 0x1 << j;

				// If this frame is free, increment a counter
				if (!(kernel_heap->bitmap[i] & toTest)) {
					// If no previous pages match, set starting one
					if(!l) {
						first_free_page = i*4*8+j;
					}

					// Increment page counter
					l++;

					// If the count matches the number of pages we want, allocate pages
					if(l == pages) {
						goto pagesFound;
					}
				} else {
					l = 0;
				}
			}
		}
	}

	// We drop down here if there wasn't enough pages
	klog(kLogLevelError, "Could not allocate 0x%X pages (last checked page is 0x%X)", (unsigned int) pages, first_free_page);
	return NULL;

	// Enough free pages were found
	pagesFound:;
	// klog(kLogLevelDebug, "Allocated 0x%X pages (page 0x%X)", pages, first_free_page);
	start = (void *) (first_free_page * 0x1000) + kernel_heap->start_address;

	unsigned int address = (first_free_page * 0x1000) + kernel_heap->start_address;

	page_t *page;

#if DEBUG_PAGE_ALLOCATION
	klog(kLogLevelDebug, "Allocated 0x%X pages (virt 0x%X)", pages, address);
#endif

	// Allocate requested pages some physical memory
	for(int p = 0; p < pages; p++) {
		page = paging_get_page(address, false, kernel_directory);
		alloc_frame(page, kernel_heap->is_supervisor, !kernel_heap->is_readonly);

		// Mark this frame as set
		set_frame(address - kernel_heap->start_address);

		// Advance allocation pointer
		address += 0x1000;
	}

	// Increment allocation counter
	kernel_heap->size += pages;

	return start;
}

/*
 * Frees pages number of pages of consecutive memory, starting at mem.
 */
static int allocator_free(void *mem, size_t pages) {
	page_t *page;
	unsigned int address = (unsigned int) mem;

#if DEBUG_PAGE_ALLOCATION
	klog(kLogLevelDebug, "Freed 0x%X pages (virt 0x%X)", pages, address);
#endif

	// Loop through all the pages
	for(int i = 0; i < pages; i++) {
		page = paging_get_page(address, false, kernel_directory);
		free_frame(page);

		// Mark this page as unused
		clear_frame(address - kernel_heap->start_address);

		// Advance pointer
		address += 0x1000;
	}

	// Stats
	kernel_heap->size -= pages;

	return 0;
}

static struct allocator_major *allocate_new_page(unsigned int size) {
	unsigned int st;
	struct allocator_major *maj;

	// This is how much space is required.
	st  = size + sizeof(struct allocator_major);
	st += sizeof(struct allocator_minor);

			// Perfect amount of space?
	if ((st % l_pageSize) == 0)
		st  = st / (l_pageSize);
	else
		st  = st / (l_pageSize) + 1;
						// No, add the buffer. 

	
	// Make sure it's >= the minimum size.
	if (st < l_pageCount) st = l_pageCount;
	
	maj = (struct allocator_major*)allocator_alloc(st);

	if (maj == NULL) 
	{
		l_warningCount += 1;
		#if defined DEBUG || defined INFO
		printf("liballoc: WARNING: allocator_alloc(%i) return NULL\n", st);
		FLUSH();
		#endif
		return NULL;	// uh oh, we ran out of memory.
	}
	
	maj->prev 	= NULL;
	maj->next 	= NULL;
	maj->pages 	= st;
	maj->size 	= st * l_pageSize;
	maj->usage 	= sizeof(struct allocator_major);
	maj->first 	= NULL;

	l_allocated += maj->size;

	#ifdef DEBUG
	printf("liballoc: Resource allocated %x of %i pages (%i bytes) for %i size.\n", maj, st, maj->size, size);

	printf("liballoc: Total memory usage = %i KB\n",  (int)((l_allocated / (1024))));
	FLUSH();
	#endif
	
	return maj;
}

static void *lalloc_malloc(size_t req_size) {
	int startedBet = 0;
	unsigned long long bestSize = 0;
	void *p = NULL;
	uintptr_t diff;
	struct allocator_major *maj;
	struct allocator_minor *min;
	struct allocator_minor *new_min;
	unsigned long size = req_size;

	// For alignment, we adjust size so there's enough space to align.
	if (ALIGNMENT > 1) {
		size += ALIGNMENT + ALIGN_INFO;
	}

	// Ideally, we really want an alignment of 0 or 1 in order to save space.
	
	allocator_lock();

	if (size == 0) {
		l_warningCount += 1;
		#if defined DEBUG || defined INFO
		printf("liballoc: WARNING: alloc(0) called from %x\n",
							__builtin_return_address(0));
		FLUSH();
		#endif
		allocator_unlock();
		return lalloc_malloc(1);
	}
	

	if (l_memRoot == NULL) {
		#if defined DEBUG || defined INFO
		#ifdef DEBUG
		printf("liballoc: initialization of liballoc " VERSION "\n");
		#endif
		atexit(allocator_dump);
		FLUSH();
		#endif
			
		// This is the first time we are being used.
		l_memRoot = allocate_new_page(size);
		if (l_memRoot == NULL) {
			allocator_unlock();
			#ifdef DEBUG
			printf("liballoc: initial l_memRoot initialization failed\n", p); 
			FLUSH();
			#endif
			return NULL;
		}

		#ifdef DEBUG
		printf("liballoc: set up first memory major %x\n", l_memRoot);
		FLUSH();
		#endif
	}


	#ifdef DEBUG
	printf("liballoc: %x lalloc_malloc(%i): ", 
					__builtin_return_address(0),
					size);
	FLUSH();
	#endif

	// Now we need to bounce through every major and find enough space....

	maj = l_memRoot;
	startedBet = 0;
	
	// Start at the best bet....
	if (l_bestBet != NULL) {
		bestSize = l_bestBet->size - l_bestBet->usage;

		if (bestSize > (size + sizeof(struct allocator_minor))) {
			maj = l_bestBet;
			startedBet = 1;
		}
	}
	
	while (maj != NULL) {
		diff  = maj->size - maj->usage;	
										// free memory in the block

		if (bestSize < diff) {
			// Hmm.. this one has more memory then our bestBet. Remember!
			l_bestBet = maj;
			bestSize = diff;
		}
		
		// CASE 1:  There is not enough space in this major block.
		if (diff < (size + sizeof(struct allocator_minor))) {
			#ifdef DEBUG
			printf("CASE 1: Insufficient space in block %x\n", maj);
			FLUSH();
			#endif
				
			// Another major block next to this one?
			if (maj->next != NULL)  {
				maj = maj->next;		// Hop to that one.
				continue;
			}

			// If we started at the best bet, let's start all over again
			if (startedBet == 1) {
				maj = l_memRoot;
				startedBet = 0;
				continue;
			}

			// Create a new major block next to this one and...
			maj->next = allocate_new_page(size);	// next one will be okay.
			if (maj->next == NULL) break;			// no more memory.
			maj->next->prev = maj;
			maj = maj->next;

			// .. fall through to CASE 2 ..
		}

		// CASE 2: It's a brand new block.
		if (maj->first == NULL) {
			maj->first = (struct allocator_minor*)((uintptr_t)maj + sizeof(struct allocator_major));

			
			maj->first->magic 		= LIBALLOC_MAGIC;
			maj->first->prev 		= NULL;
			maj->first->next 		= NULL;
			maj->first->block 		= maj;
			maj->first->size 		= size;
			maj->first->req_size 	= req_size;
			maj->usage 	+= size + sizeof(struct allocator_minor);

			l_inuse += size;
			
			p = (void*)((uintptr_t)(maj->first) + sizeof(struct allocator_minor));

			ALIGN(p);
			
			#ifdef DEBUG
			printf("CASE 2: returning %x\n", p); 
			FLUSH();
			#endif
			allocator_unlock();		// release the lock
			return p;
		}

		// CASE 3: Block in use and enough space at the start of the block.
		diff =  (uintptr_t)(maj->first);
		diff -= (uintptr_t)maj;
		diff -= sizeof(struct allocator_major);

		if (diff >= (size + sizeof(struct allocator_minor))) {
			// Yes, space in front. Squeeze in.
			maj->first->prev = (struct allocator_minor*)((uintptr_t)maj + sizeof(struct allocator_major));
			maj->first->prev->next = maj->first;
			maj->first = maj->first->prev;
				
			maj->first->magic 	= LIBALLOC_MAGIC;
			maj->first->prev 	= NULL;
			maj->first->block 	= maj;
			maj->first->size 	= size;
			maj->first->req_size 	= req_size;
			maj->usage 			+= size + sizeof(struct allocator_minor);

			l_inuse += size;

			p = (void*)((uintptr_t)(maj->first) + sizeof(struct allocator_minor));
			ALIGN(p);

			#ifdef DEBUG
			printf("CASE 3: returning %x\n", p); 
			FLUSH();
			#endif
			allocator_unlock();		// release the lock
			return p;
		}
		
		// CASE 4: There is enough space in this block. But is it contiguous?
		min = maj->first;
		
		// Looping within the block now...
		while (min != NULL) {
				// CASE 4.1: End of minors in a block. Space from last and end?
				if (min->next == NULL) {
					// the rest of this block is free...  is it big enough?
					diff = (uintptr_t)(maj) + maj->size;
					diff -= (uintptr_t)min;
					diff -= sizeof(struct allocator_minor);
					diff -= min->size; 
						// minus already existing usage..

					if (diff >= (size + sizeof(struct allocator_minor))) {
						// yay....
						min->next = (struct allocator_minor*)((uintptr_t)min + sizeof(struct allocator_minor) + min->size);
						min->next->prev = min;
						min = min->next;
						min->next = NULL;
						min->magic = LIBALLOC_MAGIC;
						min->block = maj;
						min->size = size;
						min->req_size = req_size;
						maj->usage += size + sizeof(struct allocator_minor);

						l_inuse += size;
						
						p = (void*)((uintptr_t)min + sizeof(struct allocator_minor));
						ALIGN(p);

						#ifdef DEBUG
						printf("CASE 4.1: returning %x\n", p); 
						FLUSH();
						#endif
						allocator_unlock();		// release the lock
						return p;
					}
				}

				// CASE 4.2: Is there space between two minors?
				if (min->next != NULL) {
					// is the difference between here and next big enough?
					diff  = (uintptr_t)(min->next);
					diff -= (uintptr_t)min;
					diff -= sizeof(struct allocator_minor);
					diff -= min->size;
										// minus our existing usage.

					if (diff >= (size + sizeof(struct allocator_minor))) {
						new_min = (struct allocator_minor*)((uintptr_t)min + sizeof(struct allocator_minor) + min->size);

						new_min->magic = LIBALLOC_MAGIC;
						new_min->next = min->next;
						new_min->prev = min;
						new_min->size = size;
						new_min->req_size = req_size;
						new_min->block = maj;
						min->next->prev = new_min;
						min->next = new_min;
						maj->usage += size + sizeof(struct allocator_minor);
						
						l_inuse += size;
						
						p = (void*)((uintptr_t)new_min + sizeof(struct allocator_minor));
						ALIGN(p);


						#ifdef DEBUG
						printf("CASE 4.2: returning %x\n", p); 
						FLUSH();
						#endif
						
						allocator_unlock();		// release the lock
						return p;
					}
				}	// min->next != NULL

				min = min->next;
		} // while min != NULL ...

		// CASE 5: Block full! Ensure next block and loop.
		if (maj->next == NULL) {
			#ifdef DEBUG
			printf("CASE 5: block full\n");
			FLUSH();
			#endif

			if (startedBet == 1) {
				maj = l_memRoot;
				startedBet = 0;
				continue;
			}
				
			// we've run out. we need more...
			maj->next = allocate_new_page(size);		// next one guaranteed to be okay
			if (maj->next == NULL) break;			//  uh oh,  no more memory.....
			maj->next->prev = maj;

		}

		maj = maj->next;
	} // while (maj != NULL)

	allocator_unlock();		// release the lock

	#ifdef DEBUG
	printf("All cases exhausted. No memory available.\n");
	FLUSH();
	#endif
	#if defined DEBUG || defined INFO
	printf("liballoc: WARNING: lalloc_malloc(%i) returning NULL.\n", size);
	allocator_dump();
	FLUSH();
	#endif
	return NULL;
}

static void lalloc_free(void *ptr) {
	struct allocator_minor *min;
	struct allocator_major *maj;

	if (ptr == NULL) {
		l_warningCount += 1;
		#if defined DEBUG || defined INFO
		printf("liballoc: WARNING: lalloc_free)(NULL) called from %x\n",
							__builtin_return_address(0));
		FLUSH();
		#endif
		return;
	}

	UNALIGN(ptr);

	allocator_lock();		// lockit


	min = (struct allocator_minor*)((uintptr_t)ptr - sizeof(struct allocator_minor));

	
	if (min->magic != LIBALLOC_MAGIC) {
		l_errorCount += 1;

		// Check for overrun errors. For all bytes of LIBALLOC_MAGIC 
		if (
			((min->magic & 0xFFFFFF) == (LIBALLOC_MAGIC & 0xFFFFFF)) || 
			((min->magic & 0xFFFF) == (LIBALLOC_MAGIC & 0xFFFF)) || 
			((min->magic & 0xFF) == (LIBALLOC_MAGIC & 0xFF)) 
		  ) {
			l_possibleOverruns += 1;
			#if defined DEBUG || defined INFO
			printf("liballoc: ERROR: Possible 1-3 byte overrun for magic %x != %x\n",
								min->magic,
								LIBALLOC_MAGIC);
			FLUSH();
			#endif
		}
						
						
		if (min->magic == LIBALLOC_DEAD) {
			#if defined DEBUG || defined INFO
			printf("liballoc: ERROR: multiple lalloc_free)() attempt on %x from %x.\n", 
									ptr,
									__builtin_return_address(0));
			FLUSH();
			#endif
		} else {
			#if defined DEBUG || defined INFO
			printf("liballoc: ERROR: Bad lalloc_free)(%x) called from %x\n",
								ptr,
								__builtin_return_address(0));
			FLUSH();
			#endif
		}
			
		// being lied to...
		allocator_unlock();		// release the lock
		return;
	}

	#ifdef DEBUG
	printf("liballoc: %x lalloc_free)(%x): ", 
				__builtin_return_address(0),
				ptr);
	FLUSH();
	#endif
	
	maj = min->block;

	l_inuse -= min->size;

	maj->usage -= (min->size + sizeof(struct allocator_minor));
	min->magic  = LIBALLOC_DEAD;		// No mojo.

	if (min->next != NULL) min->next->prev = min->prev;
	if (min->prev != NULL) min->prev->next = min->next;

	if (min->prev == NULL) maj->first = min->next;	
						// Might empty the block. This was the first
						// minor.


	// We need to clean up after the majors now....
	if (maj->first == NULL) { // Block completely unused.
		if (l_memRoot == maj) l_memRoot = maj->next;
		if (l_bestBet == maj) l_bestBet = NULL;
		if (maj->prev != NULL) maj->prev->next = maj->next;
		if (maj->next != NULL) maj->next->prev = maj->prev;
		l_allocated -= maj->size;

		allocator_free(maj, maj->pages);
	} else {
		if (l_bestBet != NULL) {
			int bestSize = l_bestBet->size  - l_bestBet->usage;
			int majSize = maj->size - maj->usage;

			if (majSize > bestSize) {
				l_bestBet = maj;
			}
		}

	}

	#ifdef DEBUG
	printf("OK\n");
	FLUSH();
	#endif
	
	allocator_unlock();		// release the lock
}

static void* lalloc_calloc(size_t nobj, size_t size) {
	int real_size;
	void *p;

	real_size = nobj * size;
	
	p = lalloc_malloc(real_size);

	memset(p, 0, real_size);

	return p;
}

static void* lalloc_realloc(void *p, size_t size) {
	void *ptr;
	struct allocator_minor *min;
	unsigned int real_size;
	
	// Honour the case of size == 0 => free old and return NULL
	if (size == 0) {
		lalloc_free(p);
		return NULL;
	}

	// In the case of a NULL pointer, return a simple malloc.
	if (p == NULL) return lalloc_malloc(size);

	// Unalign the pointer if required.
	ptr = p;
	UNALIGN(ptr);

	allocator_lock();		// lockit

	min = (struct allocator_minor*)((uintptr_t)ptr - sizeof(struct allocator_minor));

	// Ensure it is a valid structure.
	if (min->magic != LIBALLOC_MAGIC) {
		l_errorCount += 1;

		// Check for overrun errors. For all bytes of LIBALLOC_MAGIC 
		if (
			((min->magic & 0xFFFFFF) == (LIBALLOC_MAGIC & 0xFFFFFF)) || 
			((min->magic & 0xFFFF) == (LIBALLOC_MAGIC & 0xFFFF)) || 
			((min->magic & 0xFF) == (LIBALLOC_MAGIC & 0xFF)) 
		  ) {
			l_possibleOverruns += 1;
			#if defined DEBUG || defined INFO
			printf("liballoc: ERROR: Possible 1-3 byte overrun for magic %x != %x\n",
								min->magic,
								LIBALLOC_MAGIC);
			FLUSH();
			#endif
		}
						
						
		if (min->magic == LIBALLOC_DEAD) {
			#if defined DEBUG || defined INFO
			printf("liballoc: ERROR: multiple lalloc_free)() attempt on %x from %x.\n", 
									ptr,
									__builtin_return_address(0));
			FLUSH();
			#endif
		} else {
			#if defined DEBUG || defined INFO
			printf("liballoc: ERROR: Bad lalloc_free)(%x) called from %x\n",
								ptr,
								__builtin_return_address(0));
			FLUSH();
			#endif
		}
		
		// being lied to...
		allocator_unlock();		// release the lock
		return NULL;
	}	
	
	// Definitely a memory block.
	
	real_size = min->req_size;

	if (real_size >= size) {
		min->req_size = size;
		allocator_unlock();
		return p;
	}

	allocator_unlock();

	// If we got here then we're reallocating to a block bigger than us.
	ptr = lalloc_malloc(size);					// We need to allocate new memory
	memcpy(ptr, p, real_size);
	lalloc_free(p);

	return ptr;
}