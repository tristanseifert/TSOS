#import <types.h>

#import "paging.h"
#import "x86_pc/multiboot.h"
#import "runtime/error.h"
#import "console/vga_console.h"
 
extern unsigned int __kern_size, __kern_bss_start, __kern_bss_size;

unsigned int pages_total, pages_wired, pages_mapped;
static unsigned int previous_directory;

// Multiboot struct: Used to get memory info
extern multiboot_info_t *x86_multiboot_info;

// Maps a memory section enum entry to a range
static unsigned int section_to_memrange[7][2] = {
	{0x00000000, 0x00000000}, // kMemorySectionNone
	
	{0x00800000, 0x7FFFFFFF}, // kMemorySectionProcess
	{0x80000000, 0xBFFFFFFF}, // kMemorySectionSharedLibraries

	{0xC0000000, 0xC7FFFFFF}, // kMemorySectionKernel
	{0xC8000000, 0xCFFFFFFF}, // kMemorySectionDrivers
	
	{0xD0000000, 0xDFFFFFFF}, // kMemorySectionKernelHeap
	
	{0xE0000000, 0xFFFFFFFF}  // kMemorySectionHardware
};

static const char* section_name_table[7] = {
	"kMemorySectionNone",

	"Process Memory",
	"Shared Library Memory",

	"Kernel Code/Data",
	"Driver Code/Data",

	"Kernel Heap",
	"Hardware I/O"
};

// The kernel's page directory
page_directory_t *kernel_directory = NULL;
unsigned int kern_dir_phys;

// The current page directory;
page_directory_t *current_directory = NULL;

// A bitset of frames - used or free.
static unsigned int* frames;
static unsigned int nframes;

extern unsigned int __kern_end;

// Defined in kheap.c
extern unsigned int dumb_heap_address;

// Macros used in the bitset algorithms.
#define INDEX_FROM_BIT(a) (a/(8*4))
#define OFFSET_FROM_BIT(a) (a%(8*4))

/*
 * Set a bit in the frames bitset
 *
 * @param frame_addr Physical memory address
 */
static void set_frame(unsigned int frame_addr) {
	unsigned int frame = frame_addr / 0x1000;
	unsigned int idx = INDEX_FROM_BIT(frame);
	unsigned int off = OFFSET_FROM_BIT(frame);
	frames[idx] |= (0x1 << off);

	pages_mapped++;
}

/*
 * Clear a bit in the frames bitset
 *
 * @param frame_addr Physical memory address
 */
static void clear_frame(unsigned int frame_addr) {
	unsigned int frame = frame_addr / 0x1000;
	unsigned int idx = INDEX_FROM_BIT(frame);
	unsigned int off = OFFSET_FROM_BIT(frame);
	frames[idx] &= ~(0x1 << off);

	pages_mapped--;
}

/*
 * Check if a certain page is allocated.
 *
 * @param frame_addr Physical memory address
 */
static unsigned int test_frame(unsigned int frame_addr) {
	unsigned int frame = frame_addr / 0x1000;
	unsigned int idx = INDEX_FROM_BIT(frame);
	unsigned int off = OFFSET_FROM_BIT(frame);
	return (frames[idx] & (0x1 << off));
}

/*
 * Find the first free frame that can be allocated
 */
static unsigned int first_frame() {
	unsigned int i, j;
	for (i = 0; i < INDEX_FROM_BIT(nframes); i++) {
		if (frames[i] != 0xFFFFFFFF) { // nothing free, check next
			// At least one bit is free here
			for (j = 0; j < 32; j++) {
				unsigned int toTest = 0x1 << j;

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
		unsigned int idx = first_frame();

		if (idx == (unsigned int) -1) {
			PANIC("Out of memory");
		}

		set_frame(idx * 0x1000);

		// Clear the page's memory!
		memclr(page, sizeof(page_t));

		page->present = 1;
		page->rw = (is_writeable) ? 1 : 0;
		page->user = (is_kernel) ? 0 : 1;
		page->frame = idx;

		// KDEBUG("Mapped page at phys 0x%08X", idx * 0x1000);
	}
}

/*
 * Function to deallocate a frame.
 */
void free_frame(page_t* page) {
	unsigned int frame;
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

	// Highmem is allocated only, so we ignore lowmem
	unsigned int mem_end_page = (x86_multiboot_info->mem_upper * 1024);
	nframes = mem_end_page / 0x1000;
	nframes += 0x100; // take lowmem into account
	pages_total = nframes;

	// Allocate page frame table
	frames = (unsigned int *) kmalloc(INDEX_FROM_BIT(nframes));
	memclr(frames, INDEX_FROM_BIT(nframes));

	// Allocate page directory
	kernel_directory = (page_directory_t *) kmalloc_a(sizeof(page_directory_t));
	ASSERT(kernel_directory != NULL);
	memclr(kernel_directory, sizeof(page_directory_t));
	current_directory = kernel_directory;


	// Identity map from 0x00000000 to 0x000FF000 (legacy lowmem)
	for(i = 0; i < 0x00100000; i += 0x1000) {
		page_t* page = paging_get_page(i, true, kernel_directory);

		page->present = 1;
		page->rw = 1;
		page->user = 0;
		page->frame = ((i & 0x0FFFF000) >> 12);
	}


	// Create pages for the kernel
	unsigned int kern_start = paging_get_memrange(kMemorySectionKernel)[0];
	unsigned int kern_end = paging_get_memrange(kMemorySectionKernel)[1];

	for(i = kern_start; i < kern_end; i += 0x1000) {
		paging_get_page(i, true, kernel_directory);
	}


	// Create pages for loaded drivers
	unsigned int driver_start = paging_get_memrange(kMemorySectionDrivers)[0];
	unsigned int driver_end = paging_get_memrange(kMemorySectionDrivers)[1];
	for(i = driver_start; i < driver_end; i += 0x1000) {
		paging_get_page(i, true, kernel_directory);
	}

	// Create pages for kernel heap
	unsigned int kheap_start = paging_get_memrange(kMemorySectionKernelHeap)[0];
	unsigned int kheap_end = paging_get_memrange(kMemorySectionKernelHeap)[1];
	for(i = kheap_start; i < kheap_end; i += 0x1000) {
		paging_get_page(i, true, kernel_directory);
	}

	/*
	 * GIGANTIC FUCKING HACK ALERT!
	 * 
	 * Remap VGA memory from physical 0xB8000 to somewhere in the hardware
	 * address range.
	 */
	unsigned int vga_newaddr = paging_map_section(0xB8000, 0x10000, kernel_directory, kMemorySectionHardware);

	// Create the kernel heap
	kheap_install();


	// Mark frames as in-use from 0x00000000 to the end of the dumb heap
	unsigned int kern_end_phys = ((dumb_heap_address - 0xC0000000) & 0xFFFFF000) + 0x2000;
	for(i = 0; i < kern_end_phys; i += 0x1000) {
		set_frame(i);
	}
	KDEBUG("Memory from 0x00000000 to 0x%08X marked as used", kern_end_phys);


	// Mark kernel data as present
	unsigned int kern_heap_end = (dumb_heap_address & 0xFFFFF000) + 0x2000;
	for(i = kern_start; i < (kern_end & 0xFFFFF000); i += 0x1000) {
		page_t* page = paging_get_page(i, false, kernel_directory);

		page->present = 1;
		page->rw = 1;
		page->user = 0;
		page->frame = ((i & 0x0FFFF000) >> 12);
	}


	// Convert kernel directory address to physical
	kern_dir_phys = (unsigned int) &kernel_directory->tablesPhysical;
	kern_dir_phys -= 0xC0000000;
	kernel_directory->physicalAddr = kern_dir_phys;


	/*
	 * Enable global addresses. This helps with minimising the TLB flush
	 * overhead when performing a context switch, as kernel pages can stay in
	 * the TLB.
	 */
	uint32_t cr4;
	__asm__ volatile("mov %%cr4, %0" : "=r" (cr4));
	cr4 |= (1 << 7);
	__asm__ volatile("mov %0, %%cr4" : : "r"(cr4));

	// Enable paging
	paging_switch_directory(kernel_directory);
	vga_textmem_remap(vga_newaddr);
}

/*
 * Switches to a page directory.
 */
void paging_switch_directory(page_directory_t* new) {
	__asm__ volatile("mov %%cr3, %0" : "=r" (previous_directory));

	unsigned int tables_phys_ptr = (unsigned int) new->physicalAddr;
	current_directory = new;
	__asm__ volatile("mov %0, %%cr3" : : "r"(tables_phys_ptr));
}

/*
 * Maps length bytes starting at physicalAddress anywhere in the specified memory
 * region.
 */
unsigned int paging_map_section(unsigned int physAddress, unsigned int length, page_directory_t* dir, paging_memory_section_t sec) {
	unsigned int section_start = section_to_memrange[sec][0];
	unsigned int section_end = section_to_memrange[sec][1];

	// Round up length to a multiple of a page
	if(length & 0x00000FFF) {
		length &= 0xFFFFF000;
		length += 0x1000;
	}

	// Align physical address to a page boundary.
	unsigned int phys_transformed = physAddress & 0xFFFFF000;

	unsigned int found_length = 0;
	unsigned int mapping_start = 0;

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
		// Don't call alloc_frame as this doesn't allocate any physical RAM
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
		KERROR("Could not map %u bytes, from 0x%08X in section %s", length, physAddress, section_name_table[sec]);
		return 0;
	}
}

/*
 * Basically performs the exact opposite of the above, unmapping a section of
 * memory.
 */
void paging_unmap_section(unsigned int virtAddr, unsigned int length, page_directory_t* dir) {
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

	unsigned int i;
	volatile unsigned int x; // GCC likes optimising this away

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

	unsigned int i;
	volatile unsigned int x; // GCC likes optimising this away

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
page_t* paging_get_page(unsigned int address, bool make, page_directory_t* dir) {
	// Turn the address into an index.
	address /= 0x1000;

	// Find the page table containing this address.
	unsigned int table_idx = address / 1024;

	if (dir->tables[table_idx]) { // If this table is already assigned
		return &dir->tables[table_idx]->pages[address % 0x400];
	} else if(make == true) { // Table does not exist
		unsigned int tmp;

		// Create table, and zero its memory
		dir->tables[table_idx] = (page_table_t *) kmalloc_ap(sizeof(page_table_t), &tmp);
		memclr(dir->tables[table_idx], sizeof(page_table_t));

		// update physical address
		unsigned int phys_ptr = tmp | 0x3;

		// Ensure that kernel code and data is global
		if(unlikely(table_idx >= 0x300 && table_idx < 0x320)) {
			phys_ptr |= 0x100;
			dir->tables[table_idx]->pages[address % 0x400].global = 1;
		}

		dir->tablesPhysical[table_idx] = phys_ptr;
		dir->tables[table_idx]->pages[address % 0x400].present = 0;

		return &dir->tables[table_idx]->pages[address % 0x400];
	} else {
		return NULL;
	}
}

/*
 * Returns pointer to the specified page, and if not present and make = true,
 * creates it. When a pagetable is created through this mechanism, its entry in
 * the page directory marks it as a user page.
 *
 * Note that address = the logical address we wish to map.
 */
page_t* paging_get_user_page(unsigned int address, bool make, page_directory_t* dir) {
	// Turn the address into an index.
	address /= 0x1000;

	// Find the page table containing this address.
	unsigned int table_idx = address / 1024;

	if (dir->tables[table_idx]) { // If this table is already assigned
		return &dir->tables[table_idx]->pages[address % 0x400];
	} else if(make == true) {
		unsigned int tmp;
		dir->tables[table_idx] = (page_table_t *) kmalloc_ap(sizeof(page_table_t), &tmp);

		// update physical address
		unsigned int phys_ptr = tmp | 0x7;

		// Ensure that kernel code and data is global
		if(unlikely(table_idx >= 0x300 && table_idx < 0x320)) {
			phys_ptr |= 0x100;
			dir->tables[table_idx]->pages[address % 0x400].global = 1;
		}

		// Update page
		dir->tablesPhysical[table_idx] = phys_ptr;
		dir->tables[table_idx]->pages[address % 0x400].present = 0;

		return &dir->tables[table_idx]->pages[address % 0x400];
	} else {
		return 0;
	}
}

/*
 * Page fault handler
 */
void paging_page_fault_handler(err_registers_t regs) {
	// A page fault has occurred.
	// The faulting address is stored in the CR2 register.
	unsigned int faulting_address;
	__asm__ volatile("mov %%cr2, %0" : "=r" (faulting_address));

	// The error code gives us details of what happened.
	int present	= !(regs.err_code & 0x1); // Page not present
	int rw = regs.err_code & 0x2; // Write operation?
	int us = regs.err_code & 0x4; // Processor was in user-mode?
	int reserved = regs.err_code & 0x8; // Overwritten CPU-reserved bits of page entry?
	int id = regs.err_code & 0x10; // Caused by an instruction fetch?

	kprintf("Page fault exception ( ");
	if (present) kprintf("present ");
	if (rw) kprintf("read-only ");
	if (us) kprintf("user-mode ");
	if (reserved) kprintf("reserved ");
	if(id) kprintf("instruction fetch");
	kprintf(") at 0x%X (regs 0x%X)\n", faulting_address, (unsigned int) regs.err_code);

	// Dump registers
	error_dump_regs(regs);

	for(;;);
}

/*
 * Flushes an address out of the MMU's cache.
 */
void paging_flush_tlb(unsigned int addr) {
	__asm__ volatile("invlpg (%0)" : : "r" (addr) : "memory");
}

/*
 * Returns the memory range in which a specific type of mapping will go.
 */
unsigned int *paging_get_memrange(paging_memory_section_t section) {
	return section_to_memrange[section];
}