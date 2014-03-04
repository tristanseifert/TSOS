#import <types.h>
#import "x86_pc.h"
#import "binfmt_elf.h"
#import "multiboot.h"
#import "interrupts.h"
#import "8259_pic.h"
#import "8254_pit.h"

#import "task/systimer.h"

// Some info from the ELF
char *kern_elf_strtab;
elf_symbol_entry_t *kern_elf_symtab;
unsigned int kern_elf_symtab_entries;

// Multiboot header
multiboot_info_t *x86_multiboot_info;

// Private functions
static void x86_pc_init_timer(void);

void x86_pc_init_multiboot(void) {
	// Is the multiboot info set up?
	if(x86_multiboot_info) {
		multiboot_info_t *lowmemStruct = x86_multiboot_info;
		multiboot_info_t *himemStruct = (multiboot_info_t *) kmalloc(sizeof(multiboot_info_t));

		memmove(himemStruct, lowmemStruct, sizeof(multiboot_info_t));

		x86_multiboot_info = himemStruct;

		// Copy ELF headers
		if (MULTIBOOT_CHECK_FLAG(lowmemStruct->flags, 5)) {
			multiboot_elf_section_header_table_t *multiboot_elf_sec = &(lowmemStruct->u.elf_sec);
			memmove(&himemStruct->u.elf_sec, multiboot_elf_sec, sizeof(multiboot_elf_section_header_table_t));

			// Physical address of the ELF headers
			elf_section_entry_t *entries = (elf_section_entry_t *) multiboot_elf_sec->addr;

			// Get string table
			elf_section_entry_t *strtab = &entries[multiboot_elf_sec->shndx];
			char *secstrtab = (char *) strtab->sh_addr;

			// First, find the symbol table
			int symbol_string_table = -1;

			for(int i = 0; i < multiboot_elf_sec->num; i++) {
				elf_section_entry_t *ent = &entries[i];

				// Ignore NULL entries
				if(ent->sh_type != SHT_NULL) {
					char* name = secstrtab+ent->sh_name;

					// Did we find the symbol table?
					if(ent->sh_type == SHT_SYMTAB) {
						kern_elf_symtab = (elf_symbol_entry_t *) kmalloc(ent->sh_size);
						memmove(kern_elf_symtab, (void *) ent->sh_addr, ent->sh_size);

						kern_elf_symtab_entries = ent->sh_size / sizeof(elf_symbol_entry_t);

						symbol_string_table = ent->sh_link;

						// klog(kLogLevelDebug, "Symbol table copied from 0x%X to 0x%X, link 0x%X", ent->sh_addr, kern_elf_symtab, ent->sh_link);
					} /*else { // other kind of section
						klog(kLogLevelWarning, "Section %s, addr 0x%x, sz 0x%X, type 0x%X, idx 0x%X\n", name, ent->sh_addr, ent->sh_size, ent->sh_type, i);
					}*/
				}
			}

			// Get symbol strings table and copy
			if(symbol_string_table != -1) {
				elf_section_entry_t *ent = &entries[symbol_string_table];
				kern_elf_strtab = (char *) kmalloc(ent->sh_size);
				memmove(kern_elf_strtab, (void *) ent->sh_addr, ent->sh_size);
				// klog(kLogLevelDebug, "String table copied from 0x%X to 0x%X, size 0x%X", ent->sh_addr, kern_elf_strtab, ent->sh_size);
			} else {
				klog(kLogLevelWarning, "Couldn't find symbol string table");
			}

			// klog(kLogLevelWarning, "'%s' at 0x%X\n", kern_elf_strtab + kern_elf_symtab[96].st_name, kern_elf_symtab[96].st_address);
		}

		// Copy memory map
		if (MULTIBOOT_CHECK_FLAG(lowmemStruct->flags, 6)) {
			multiboot_memory_map_t *mmap = (multiboot_memory_map_t *) lowmemStruct->mmap_addr;
			multiboot_memory_map_t *new = (multiboot_memory_map_t *) kmalloc(lowmemStruct->mmap_length);
			memmove(new, mmap, lowmemStruct->mmap_length);
			himemStruct->mmap_addr = (uint32_t) new;
		}

		klog(kLogLevelInfo, "%uKB low memory, %uKB high memory", (unsigned int) x86_multiboot_info->mem_lower, (unsigned int) x86_multiboot_info->mem_upper);
	} else {
		klog(kLogLevelWarning, "No multiboot info!");
	}	
}

void x86_pc_init(void) {
	// Set up IDT
	idt_init();

	// Remap PICs
	i8259_remap(0x20, 0x28);

	// Set up system timer
	x86_pc_init_timer();

	__asm__ volatile("sti");
}

/*
 * Sets up the periodic 1ms timer.
 */
static void x86_pc_init_timer(void) {
	// Initialise timer tick facilities
	kern_timer_tick_init();

	// Set up PIT to generate an interrupt at 100Hz
	i8254_set_mode(0, i8254_mode_square);
	i8254_set_ticks(0, 11932);

	// Set up the system timer
	irq_register_handler(0, kern_timer_tick);
}

/*
 * Reads a quadword from a specific MSR to memory pointed to by lo and hi.
 */
void x86_pc_read_msr(uint32_t msr, uint32_t *lo, uint32_t *hi) {
	__asm__ volatile("rdmsr" : "=a"(*lo), "=d"(*hi) : "c"(msr));
}

/*
 * Writes a quadword of data (split into low and high words) to a specific MSR.
 */
void x86_pc_write_msr(uint32_t msr, uint32_t lo, uint32_t hi) {
	__asm__ volatile("wrmsr" : : "a"(lo), "d"(hi), "c"(msr));
}