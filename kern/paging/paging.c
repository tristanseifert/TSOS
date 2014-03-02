#import <types.h>

#import "kheap.h"
#import "paging.h"
 
extern uint32_t __kern_size, __kern_bss_start, __kern_bss_size;

uint32_t pages_total, pages_wired;
static uint32_t previous_directory;

// Maps a memory section enum entry to a range
static uint32_t section_to_memrange[6][2] = {
	{0x00000000, 0x00000000}, // kMemorySectionNone
	{0x00000000, 0x7FFFFFFF}, // kMemorySectionProcess
	{0x80000000, 0xBFFFFFFF}, // kMemorySectionSharedLibraries
	{0xC0000000, 0xC7FFFFFF}, // kMemorySectionKernel
	{0xC8000000, 0xCFFFFFFF}, // kMemorySectionKernelHeap
	{0xD0000000, 0xFFFFFFFF}  // kMemorySectionHardware
};

// The kernel's page directory
page_directory_t *kernel_directory = 0;
uint32_t kern_dir_phys;

// The current page directory;
page_directory_t *current_directory = 0;

// A bitset of frames - used or free.
static uint32_t* frames;
static uint32_t nframes;

extern uint32_t __kern_end;

// Defined in kheap.c
extern uint32_t kheap_placement_address;

// Macros used in the bitset algorithms.
#define INDEX_FROM_BIT(a) (a/(8*4))
#define OFFSET_FROM_BIT(a) (a%(8*4))

/*
 * Static function to set a bit in the frames bitset
 */
static void set_frame(uint32_t frame_addr) {
	uint32_t frame = frame_addr / 0x1000;
	uint32_t idx = INDEX_FROM_BIT(frame);
	uint32_t off = OFFSET_FROM_BIT(frame);
	frames[idx] |= (0x1 << off);
}

/*
 * Static function to clear a bit in the frames bitset
 */
static void clear_frame(uint32_t frame_addr) {
	uint32_t frame = frame_addr / 0x1000;
	uint32_t idx = INDEX_FROM_BIT(frame);
	uint32_t off = OFFSET_FROM_BIT(frame);
	frames[idx] &= ~(0x1 << off);
}

/*
 * Static function to test if a bit is set.
 */
static uint32_t test_frame(uint32_t frame_addr) {
	uint32_t frame = frame_addr / 0x1000;
	uint32_t idx = INDEX_FROM_BIT(frame);
	uint32_t off = OFFSET_FROM_BIT(frame);
	return (frames[idx] & (0x1 << off));
}

/*
 * Static function to find the first free frame.
 */
static uint32_t first_frame() {
	uint32_t i, j;
	for (i = 0; i < INDEX_FROM_BIT(nframes); i++) {
		if (frames[i] != 0xFFFFFFFF) { // nothing free, check next
			// At least one bit is free here
			for (j = 0; j < 32; j++) {
				uint32_t toTest = 0x1 << j;

				// If this frame is free, return
				if (!(frames[i] & toTest)) {
					return i*4*8+j;
				}
			}
		}
	}

	return -1;
}

/*
 * Function to allocate a frame.
 */
void alloc_frame(page_t* page, bool is_kernel, bool is_writeable) {
	if (page->frame != 0) {
		return;
	} else {
		uint32_t idx = first_frame();

		if (idx == (uint32_t) -1) {
			PANIC("No Free Frames");
		}

		set_frame(idx * 0x1000);

		// Clear the page's memory!
		memclr(page, sizeof(page_t));

		page->present = 1;
		page->rw = (is_writeable) ? 1 : 0;
		page->user = (is_kernel) ? 0 : 1;
		page->frame = idx;

		// kprintf("Mapped page at phys 0x%X\n", idx * 0x1000);
	}
}

/*
 * Function to deallocate a frame.
 */
void free_frame(page_t* page) {
	uint32_t frame;
	if (!(frame=page->frame)) {
		return;
	} else {
		clear_frame(frame);
		page->frame = 0x0;
	}
}

/*
 * Initialises paging and sets up page tables.
 */
void paging_init() {
	unsigned int i = 0;

	// TODO: Make highmem address appear here
	uint32_t mem_end_page = 0x20000000;
	nframes = mem_end_page / 0x1000;

	pages_total = nframes;

	frames = (uint32_t *) kmalloc(INDEX_FROM_BIT(nframes));
	memclr(frames, INDEX_FROM_BIT(nframes));

	// Allocate mem for a page directory.
	kernel_directory = (page_directory_t *) kmalloc_a(sizeof(page_directory_t));
	ASSERT(kernel_directory != NULL);
	memclr(kernel_directory, sizeof(page_directory_t));
	current_directory = kernel_directory;

	// This step serves to map the kernel itself
	// We don't allocate frames here, as that's done below.
	for(i = 0xC0000000; i < 0xC7FFF000; i += 0x1000) {
		page_t* page = paging_get_page(i, true, kernel_directory);
		memclr(page, sizeof(page_t));

		page->present = 1;
		page->rw = 1;
		page->user = 0;
		page->frame = ((i & 0x0FFFF000) >> 12);
	}

	// Convert kernel directory address to physical and save it
	kern_dir_phys = (uint32_t) &kernel_directory->tablesPhysical;
	kern_dir_phys -= 0xC0000000;
	kernel_directory->physicalAddr = kern_dir_phys;

	// Enable paging
	paging_switch_directory(kernel_directory);
}

/*
 * Switches the currently-used page directory.
 */
void paging_switch_directory(page_directory_t* new) {
	__asm__ volatile("mov %%cr3, %0" : "=r" (previous_directory));

	uint32_t tables_phys_ptr = (uint32_t) new->physicalAddr;
	current_directory = new;
	__asm__ volatile("mov %0, %%cr3" : : "r"(tables_phys_ptr));
}

/*
 * Creates a new page directory with 0xC0000000 to 0xC7FFFFFF mapped as kernel
 * data. Heap and friends are not mapped as this pagetable merely needs to map
 * code so IRQ handlers won't triple-fault the machine.
 */
page_directory_t *paging_new_directory() {
	uint32_t phys_loc;

	// Allocate a page-aligned block of memory, put physical address in phys_loc
	page_directory_t* directory = (page_directory_t *) kmalloc_int(sizeof(page_directory_t), true, &phys_loc);
	ASSERT(directory != NULL);
	memclr(directory, sizeof(page_directory_t));

	// We need to copy entries 0x300 to 0x31F in the directory.
	for(int i = 0x300; i < 0x320; i++) {
		directory->tables[i] = kernel_directory->tables[i];
		directory->tablesPhysical[i] = kernel_directory->tablesPhysical[i];
	}

	directory->physicalAddr = phys_loc + offsetof(page_directory_t, tablesPhysical);
	
	return directory;
}

/*
 * Maps length bytes starting at physicalAddress anywhere in the specified memory
 * region.
 */
uint32_t paging_map_section(uint32_t physAddress, uint32_t length, page_directory_t* dir, paging_memory_section_t sec) {
	uint32_t section_start = section_to_memrange[sec][0];
	uint32_t section_end = section_to_memrange[sec][1];

	// Round up length to a multiple of a page
	length &= 0xFFFFF000;
	length += 0x1000;

	// Align physical address to a page boundary.
	uint32_t phys_transformed = physAddress & 0xFFFFF000;

	uint32_t found_length = 0;
	uint32_t mapping_start = 0;

	for(int i = section_start; i < section_end; i+= 0x1000) {
		// Try to get the page, but do not allocate it
		page_t* page = paging_get_page(i, false, dir);

		// Store pointer to the start of the free block.
		if(found_length == 0) {
			mapping_start = i;
		}

		// If page doesn't exist, we can allocate some memory here.
		if(page == NULL) {
			found_length += 0x1000;
		} else {
			// Is page not present or mapped to 0?
			if(page->present == 0 || page->frame == 0) {
				found_length += 0x1000;
			} else {
				found_length = 0x0000;
			}
		}

		// Check if we found enough memory.
		if(found_length == length) {
			break;
		}
	}

	// We found enough memory, so map it
	if(mapping_start != 0) {
		// Note we don't call alloc_frame as this doesn't allocate any of our
		// physical RAM.
		// kprintf("Mapping 0x%X to 0x%X -> 0x%X phys\n", mapping_start, mapping_start+length, phys_transformed);

		for(int i = mapping_start; i < mapping_start+length; i+= 0x1000) {
			// Get pagetable and allocate if needed
			page_t* page = paging_get_page(i, true, dir);
			memclr(page, sizeof(page_t));

			// Present, RW, supervisor only
			page->present = 1;
			page->rw = 1;
			page->user = 0;
			page->frame = (((phys_transformed + (i - mapping_start)) & 0xFFFFF000) >> 12);
		}

		// Add the offset into the page we were requested to map
		return mapping_start + (physAddress & 0x00000FFF);
	} else {
		return 0;
	}
}

/*
 * Basically performs the exact opposite of the above, unmapping a section of
 * memory.
 */
void paging_unmap_section(uint32_t virtAddr, uint32_t length, page_directory_t* dir) {
	// Round up length to a multiple of a page
	length &= 0xFFFFF000;
	length += 0x1000;

	for(int i = virtAddr; i < virtAddr+length; i+= 0x1000) {
		page_t* page = paging_get_page(i, false, dir);
		memclr(page, sizeof(page_t));
	}
}

/*
 * Returns the number of free pages.
 */
unsigned int paging_get_free_pages() {
	unsigned int frames_allocated = 0;

	uint32_t i;
	volatile uint32_t x; // GCC likes optimising this away

	for (i = 0; i < INDEX_FROM_BIT(nframes); i++) {
		// Count bits free
		if ((x = frames[i]) != 0xFFFFFFFF) {
			frames_allocated += std_popCnt(x);
		}
	}

	return pages_total - frames_allocated;
}

/*
 * Gathers some info about paging.
 */
paging_stats_t paging_get_stats() {
	unsigned int frames_allocated = 0;

	uint32_t i;
	volatile uint32_t x; // GCC likes optimising this away

	for (i = 0; i < INDEX_FROM_BIT(nframes); i++) {
		// Count bits free
		if ((x = frames[i]) != 0xFFFFFFFF) {
			frames_allocated += std_popCnt(x);
		}
	}

	paging_stats_t stats;

	stats.total_pages = pages_total;
	stats.pages_mapped = frames_allocated;
	stats.pages_free = pages_total - frames_allocated;
	stats.pages_wired = pages_wired;

	return stats;
}

/*
 * Returns pointer to the specified page, and if not present and make = true,
 * creates it.
 *
 * Note that address = the logical address we wish to map.
 */
page_t* paging_get_page(uint32_t address, bool make, page_directory_t* dir) {
	// Turn the address into an index.
	address /= 0x1000;

	// Find the page table containing this address.
	uint32_t table_idx = address / 1024;

	if (dir->tables[table_idx]) { // If this table is already assigned
		return &dir->tables[table_idx]->pages[address % 0x400];
	} else if(make == true) {
		uint32_t tmp;
		dir->tables[table_idx] = (page_table_t *) kmalloc_ap(sizeof(page_table_t), &tmp);

		// update physical address
		uint32_t phys_ptr = tmp | 0x7;
		phys_ptr &= 0x0FFFFFFF; // get rid of high nybble
		dir->tablesPhysical[table_idx] = phys_ptr;
		dir->tables[table_idx]->pages[address % 0x400].present = 0;

		return &dir->tables[table_idx]->pages[address % 0x400];
	} else {
		return 0;
	}
}

/*
 * Flushes an address out of the MMU's cache.
 */
void paging_flush_tlb(uint32_t addr) {
	__asm__ volatile("invlpg (%0)" : : "r" (addr) : "memory");
}