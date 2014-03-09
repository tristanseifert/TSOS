#import <types.h>

typedef enum {
	kMemorySectionNone = 0,
	kMemorySectionProcess = 1,
	kMemorySectionSharedLibraries = 2,
	kMemorySectionKernel = 3,			// kernel code and data
	kMemorySectionDrivers = 4,			// dynamically loaded drivers
	kMemorySectionKernelHeap = 5,		// heap (duh)
	kMemorySectionHardware = 6			// MMIO and such
} paging_memory_section_t;

/*
 * Contains functions to set up and deal with paging.
 */
typedef struct page {
	int present:1;		// Page present in memory
	int rw:1;			// Read-only if clear, readwrite if set
	int user:1;			// Supervisor level only if clear
	int writethrough:1; // When set, writethrough caching is enabled
	int cache:1;		// Disables caching of the page when set
	int accessed:1;		// Has the page been accessed since last refresh?
	int dirty:1;		// Has the page been written to since last refresh?
	int unused:1;		// Ignored bits
	int global:1;		// When set, not evicted from TLB on pagetable switch
	int unused2:3;		// More ignored bits
	int frame:20;		// Frame address (shifted right 12 bits)
} page_t;

typedef struct page_table {
	page_t pages[1024];
} page_table_t;

typedef struct page_directory {
	// Array of ptrs to page_table structs.
	page_table_t* tables[1024];
	// Array of pointers to page tables above, but giving their physical location
	unsigned int tablesPhysical[1024];
	// Physical address of tablesPhysical.
	unsigned int physicalAddr;
} page_directory_t;

typedef struct paging_stats {
	unsigned int total_pages;
	unsigned int pages_mapped;
	unsigned int pages_free;
	unsigned int pages_wired;
} paging_stats_t;

void alloc_frame(page_t*, bool, bool);
void free_frame(page_t*);

paging_stats_t paging_get_stats();

void paging_init();
void paging_switch_directory(page_directory_t*);

page_t* paging_get_page(unsigned int, bool, page_directory_t*);
page_t* paging_get_user_page(unsigned int address, bool make, page_directory_t* dir);

unsigned int paging_map_section(unsigned int, unsigned int, page_directory_t*, paging_memory_section_t);
void paging_unmap_section(unsigned int, unsigned int, page_directory_t*);

void paging_page_fault_handler();
void paging_flush_tlb(unsigned int);

unsigned int *paging_get_memrange(paging_memory_section_t section);
