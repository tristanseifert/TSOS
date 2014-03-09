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
/*
 * A structure found at the top of all system allocated memory blocks. It 
 * details the usage of the memory block.
 */
struct allocator_major {
	struct allocator_major *prev;
	struct allocator_major *next;
	unsigned int pages;	
	unsigned int size;
	unsigned int usage;
	struct allocator_minor *first;
};

/*
 * This is a structure found at the beginning of all sections in a major block
 * which were allocated by a malloc, calloc, realloc call.
 */
struct	allocator_minor {
	struct allocator_minor *prev;
	struct allocator_minor *next;
	struct allocator_major *block;
	unsigned int magic;
	unsigned int size;
	unsigned int req_size;
};

// Allocator state
static struct allocator_major *l_memRoot = NULL; // root memory vlock from system
static struct allocator_major *l_bestBet = NULL; // Major block with most free memory

static unsigned int l_pageSize = 4096; // size of a page
static unsigned int l_pageCount = 16; // pages to request per chunk
static unsigned long long l_allocated = 0; // total allocated memory
static unsigned long long l_inuse = 0; // total used memory

static long long l_warningCount = 0; // warnings
static long long l_errorCount = 0; // errors
static long long l_possibleOverruns = 0; // possible overruns

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
static uint32_t *heap_frames;

/*
 * Set a bit in the heap_frames bitset
 */
static void set_frame(unsigned int frame_addr) {
	// kprintf("set_frame 0x%08X ", frame_addr);

	unsigned int frame = frame_addr / 0x1000;
	unsigned int idx = INDEX_FROM_BIT(frame);
	unsigned int off = OFFSET_FROM_BIT(frame);
	heap_frames[idx] |= (0x1 << off);
}

/*
 * Clear a bit in the heap_frames bitset
 */
static void clear_frame(unsigned int frame_addr) {
	unsigned int frame = frame_addr / 0x1000;
	unsigned int idx = INDEX_FROM_BIT(frame);
	unsigned int off = OFFSET_FROM_BIT(frame);
	heap_frames[idx] &= ~(0x1 << off);
}

/*
 * Creates the kernel heap.
 *
 * @param start Starting address
 * @param end End address of the heap
 * @param supervisor When set, mapped pages are only accessible by supervisor.
 * @param readonly Causes mapped pages to be readonly to lower priv levels.
 */
void kheap_install() {
	// Allocate memory and set it to zero.
	heap_t *heap = (heap_t *) kmalloc(sizeof(heap_t));
	ASSERT(heap);
	memclr(heap, sizeof(heap_t));

	// Allocate memory for the bitmap
	unsigned int size = 0x8000000;
	nframes = size / 0x1000;

	heap_frames = kmalloc(INDEX_FROM_BIT(nframes));
	memclr(heap_frames, INDEX_FROM_BIT(nframes));

	// Start address
	heap->start_address = paging_get_memrange(kMemorySectionKernelHeap)[0];
	heap->end_address = paging_get_memrange(kMemorySectionKernelHeap)[1];

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
	unsigned int ptr;

	// Handle unaligned memory accesses
	if(likely(!aligned)) {
		ptr = (unsigned int) lalloc_malloc(size);
		memclr((void *) ptr, size);

		// Handle an out of memory condition
		if(!ptr) {
			errno = ENOMEM;
			return NULL;
		}

		// KWARNING("SCHREIBKUGEL ALLOC sized 0x%08X at 0x%08X", size, ptr);
	} else { // This allocation must be aligned
		// First, try to see if the allocation we get back is already page aligned
		ptr = (unsigned int) lalloc_malloc(size);
		memclr((void *) ptr, size);

		// If it isn't, re-allocate it plus 0x1000
		if(likely(ptr & 0x00000FFF)) {
			krealloc((void *) ptr, size + 0x1000);
			ptr += 0x1000;
			ptr &= 0xFFFFF000;
		}
	}

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
		KERROR("Tried to deallocate NULL address");
		dump_stack_here();
		return;
	}
#endif

	// liballoc
	lalloc_free(address);
	// KERROR("SCHREIBKUGEL DEALLOC at 0x%08X", address);
}

/*
 * Deallocates a chunk of previously-allocated memory.
 *
 * @param address Address of memory on kernel heap
 */
void kfree(void* address) {
	if(likely(kernel_heap)) {
		free(address);
	} else {
		KWARNING("Tried to free 0x%X on dumb heap", (unsigned int) address);
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
		if (likely(heap_frames[i] != 0xFFFFFFFF)) {
			// Check which one of the 32 frames are filled
			for (j = 0; j < 32; j++) {
				unsigned int toTest = 0x1 << j;

				// If this frame is free, increment a counter
				if (!(heap_frames[i] & toTest)) {
					// If no previous pages match, set starting one
					if(!l) {
						first_free_page = i*4*8+j;
					}

					// Increment page counter
					l++;

					// If the count matches the number of pages we want, allocate pages
					if(unlikely(l == pages)) {
						goto pagesFound;
					}
				} else {
					l = 0;
				}
			}
		}
	}

	// We drop down here if there wasn't enough pages
	KERROR("Could not allocate 0x%X pages (last checked page is 0x%X)", (unsigned int) pages, first_free_page);
	return NULL;

	// Enough free pages were found
	pagesFound:;
	// KDEBUG("Allocated 0x%X pages (page 0x%X)", (unsigned int) pages, first_free_page);
	start = (void *) (first_free_page * 0x1000) + kernel_heap->start_address;

	// Starting address
	unsigned int address = (first_free_page * 0x1000) + kernel_heap->start_address;
	page_t *page;

#if DEBUG_PAGE_ALLOCATION
	KDEBUG("Allocated 0x%X pages (virt 0x%X)", (unsigned int) pages, address);
#endif

	// Allocate requested pages some physical memory
	for(int p = 0; p < pages; p++) {
		page = paging_get_page(address, false, kernel_directory);
		alloc_frame(page, true, true);

		// Mark this frame as set
		set_frame(address - kernel_heap->start_address);
		// kprintf("virt 0x%08X\n", address);

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
	KDEBUG("Freed 0x%X pages (virt 0x%X)", pages, address);
#endif

	// Loop through all the pages
	for(int i = 0; i < pages; i++) {
		page = paging_get_page(address, false, kernel_directory);
		if(page) {
			free_frame(page);

			// Mark this page as unused
			clear_frame(address - kernel_heap->start_address);

			// Advance pointer
			address += 0x1000;
		}
	}

	// Stats
	kernel_heap->size -= pages;

	return 0;
}

/*
 * Allocate a new memory page.
 */
static struct allocator_major *allocate_new_page(unsigned int size) {
	unsigned int st;
	struct allocator_major *maj;

	// This is how much space is required.
	st  = size + sizeof(struct allocator_major);
	st += sizeof(struct allocator_minor);

	// Perfect amount of space?
	if ((st % l_pageSize) == 0) {
		st  = st / (l_pageSize);
	} else {
		// No, we need more space
		st  = st / (l_pageSize) + 1;
	}
	
	// Make sure it's  the minimum size.
	if (st < l_pageCount) st = l_pageCount;
	
	maj = (struct allocator_major *) allocator_alloc(st);

	if (maj == NULL)  {
		l_warningCount += 1;
		#if defined DEBUG || defined INFO
		KDEBUG("liballoc: WARNING: allocator_alloc(%i) return NULL", st);
		#endif

		// We ran out of memory
		return NULL;
	}
	
	maj->prev = NULL;
	maj->next = NULL;
	maj->pages = st;
	maj->size = st * l_pageSize;
	maj->usage = sizeof(struct allocator_major);
	maj->first = NULL;

	l_allocated += maj->size;

	#ifdef DEBUG
	KDEBUG("liballoc: Resource allocated %x of %i pages (%i bytes) for %i size.", maj, st, maj->size, size);
	KDEBUG("liballoc: Total memory usage = %i KB",  (int)((l_allocated / (1024))));
	#endif
	
	return maj;
}

/*
 * Allocates a memory block of the requested size.
 */
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
		KDEBUG("liballoc: WARNING: alloc(0) called from %x", __builtin_return_address(0));
		#endif
		allocator_unlock();
		return NULL;
	}
	

	if (l_memRoot == NULL) {
		#ifdef DEBUG
		KDEBUG("liballoc: initialization of liballoc " VERSION "");
		#endif
			
		// First run of allocator
		l_memRoot = allocate_new_page(size);
		if (l_memRoot == NULL) {
			allocator_unlock();

			#ifdef DEBUG
			KDEBUG("liballoc: initial l_memRoot initialization failed", p); 
			FLUSH();
			#endif

			return NULL;
		}

		#ifdef DEBUG
		KDEBUG("liballoc: set up first memory major %x", l_memRoot);
		FLUSH();
		#endif
	}


	#ifdef DEBUG
	KDEBUG("liballoc: %x lalloc_malloc(%i): ", __builtin_return_address(0), size);
	FLUSH();
	#endif

	// Now we need to bounce through every major and find enough space....
	maj = l_memRoot;
	startedBet = 0;
	
	// Start at largest free major block
	if (l_bestBet != NULL) {
		bestSize = l_bestBet->size - l_bestBet->usage;

		if (bestSize > (size + sizeof(struct allocator_minor))) {
			maj = l_bestBet;
			startedBet = 1;
		}
	}
	
	// Loop until we have no more majors
	while (maj != NULL) {
		diff  = maj->size - maj->usage;	// free memory in the block

		// If it's bigger than the best bet, remember that
		if (bestSize < diff) {
			l_bestBet = maj;
			bestSize = diff;
		}
		
		// CASE 1:  There is not enough space in this major block.
		if (diff < (size + sizeof(struct allocator_minor))) {
			#ifdef DEBUG
			KDEBUG("Insufficient space in block %x", maj);
			#endif
				
			// Another major block next to this one?
			if (maj->next != NULL)  {
				// Go to it
				maj = maj->next;
				continue;
			}

			// If we started at the best bet, let's start all over again
			if (startedBet == 1) {
				maj = l_memRoot;
				startedBet = 0;
				continue;
			}

			// Create a new major block next to this one and...
			maj->next = allocate_new_page(size);
			if (maj->next == NULL) break;

			maj->next->prev = maj;
			maj = maj->next;
		}

		// It's a brand new block.
		if (maj->first == NULL) {
			maj->first = (struct allocator_minor*)((uintptr_t)maj + sizeof(struct allocator_major));

			
			maj->first->magic = LIBALLOC_MAGIC;
			maj->first->prev = NULL;
			maj->first->next = NULL;
			maj->first->block = maj;
			maj->first->size = size;
			maj->first->req_size = req_size;
			maj->usage 	+= size + sizeof(struct allocator_minor);

			l_inuse += size;
			
			p = (void*)((uintptr_t)(maj->first) + sizeof(struct allocator_minor));

			ALIGN(p);
			
			#ifdef DEBUG
			KDEBUG("returning %x", p); 
			#endif

			allocator_unlock();		// release the lock
			return p;
		}

		// Block in use and enough space at the start of the block.
		diff =  (uintptr_t)(maj->first);
		diff -= (uintptr_t)maj;
		diff -= sizeof(struct allocator_major);

		if (diff >= (size + sizeof(struct allocator_minor))) {
			// Yes, space in front. Squeeze in.
			maj->first->prev = (struct allocator_minor*)((uintptr_t)maj + sizeof(struct allocator_major));
			maj->first->prev->next = maj->first;
			maj->first = maj->first->prev;
				
			maj->first->magic = LIBALLOC_MAGIC;
			maj->first->prev = NULL;
			maj->first->block = maj;
			maj->first->size = size;
			maj->first->req_size = req_size;
			maj->usage += size + sizeof(struct allocator_minor);

			l_inuse += size;

			p = (void*)((uintptr_t)(maj->first) + sizeof(struct allocator_minor));
			ALIGN(p);

			#ifdef DEBUG
			KDEBUG("returning %x", p); 
			#endif

			allocator_unlock();		// release the lock
			return p;
		}
		
		// There is enough space in this block. But is it contiguous?
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
					// Subtract already existing usage..

					if (diff >= (size + sizeof(struct allocator_minor))) {
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
						KDEBUG("returning %x", p); 
						#endif

						allocator_unlock();		// release the lock
						return p;
					}
				}

				// Is there space between two minors?
				if (min->next != NULL) {
					// is the difference between here and next big enough?
					diff  = (uintptr_t)(min->next);
					diff -= (uintptr_t)min;
					diff -= sizeof(struct allocator_minor);
					diff -= min->size;
					// Subtract our existing usage.

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
						KDEBUG("returning %x", p); 
						#endif
						
						allocator_unlock();
						return p;
					}
				}

				min = min->next;
		} // while min != NULL ...

		// Block full! Ensure next block and loop.
		if (maj->next == NULL) {
			#ifdef DEBUG
			KDEBUG("block full");
			#endif

			if (startedBet == 1) {
				maj = l_memRoot;
				startedBet = 0;
				continue;
			}
				
			// we've run out. we need more...
			maj->next = allocate_new_page(size);
			if (maj->next == NULL) break;
			maj->next->prev = maj;

		}

		maj = maj->next;
	}

	allocator_unlock();

	#ifdef DEBUG
	KDEBUG("All cases exhausted. No memory available.");
	#endif

	#if defined DEBUG || defined INFO
	KDEBUG("liballoc: WARNING: lalloc_malloc(%i) returning NULL.", size);
	#endif

	return NULL;
}

static void lalloc_free(void *ptr) {
	struct allocator_minor *min;
	struct allocator_major *maj;

	if (ptr == NULL) {
		l_warningCount += 1;
		#if defined DEBUG || defined INFO
		KDEBUG("liballoc: WARNING: lalloc_free)(NULL) called from %x", __builtin_return_address(0));
		FLUSH();
		#endif
		return;
	}

	UNALIGN(ptr);

	allocator_lock();

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
			KDEBUG("liballoc: ERROR: Possible 1-3 byte overrun for magic %x != %x", min->magic, LIBALLOC_MAGIC);
			#endif
		}
						
						
		if (min->magic == LIBALLOC_DEAD) {
			#if defined DEBUG || defined INFO
			KDEBUG("liballoc: ERROR: multiple lalloc_free)() attempt on %x from %x.", ptr, __builtin_return_address(0));
			#endif

		} else {
			#if defined DEBUG || defined INFO
			KDEBUG("liballoc: ERROR: Bad lalloc_free)(%x) called from %x", ptr, __builtin_return_address(0));
			#endif
		}
			
		allocator_unlock();
		return;
	}

	#ifdef DEBUG
	KDEBUG("liballoc: %x lalloc_free)(%x): ", __builtin_return_address(0), ptr);
	#endif
	
	maj = min->block;

	l_inuse -= min->size;

	maj->usage -= (min->size + sizeof(struct allocator_minor));
	min->magic  = LIBALLOC_DEAD;

	if (min->next != NULL) min->next->prev = min->prev;
	if (min->prev != NULL) min->prev->next = min->next;

	if (min->prev == NULL) maj->first = min->next;

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
	
	allocator_unlock();
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

	allocator_lock();

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
			KDEBUG("liballoc: ERROR: Possible 1-3 byte overrun for magic %x != %x", min->magic, LIBALLOC_MAGIC);
			#endif
		}
						
						
		if (min->magic == LIBALLOC_DEAD) {
			#if defined DEBUG || defined INFO
			KDEBUG("liballoc: ERROR: multiple lalloc_free() attempt on %x from %x.", ptr, __builtin_return_address(0));
			#endif
		} else {
			#if defined DEBUG || defined INFO
			KDEBUG("liballoc: ERROR: Bad lalloc_free(%x) called from %x", ptr, __builtin_return_address(0));
			#endif
		}
		
		allocator_unlock();
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
	ptr = lalloc_malloc(size);
	memcpy(ptr, p, real_size);
	lalloc_free(p);

	return ptr;
}