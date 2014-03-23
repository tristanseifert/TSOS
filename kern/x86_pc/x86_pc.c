#undef __ASSEMBLY__

#import <types.h>
#import "x86_pc.h"
#import "binfmt_elf.h"
#import "multiboot.h"
#import "interrupts.h"
#import "8259_pic.h"
#import "8254_pit.h"
#import "tss.h"
#import "apic.h"

#import "task/systimer.h"

#import "driver_support/ramdisk.h"

#define MAX_IRQ 16

// Assembly IRQ handlers
extern void irq_0(void); 
extern void irq_1(void); 
extern void irq_2(void); 
extern void irq_3(void); 
extern void irq_4(void); 
extern void irq_5(void); 
extern void irq_6(void); 
extern void irq_7(void); 
extern void irq_8(void); 
extern void irq_9(void); 
extern void irq_10(void); 
extern void irq_11(void); 
extern void irq_12(void); 
extern void irq_13(void); 
extern void irq_14(void); 
extern void irq_15(void); 

// Assembly IRQ handlers
static const uint32_t asm_irq_handlers[MAX_IRQ] = {
	(uint32_t) irq_0, (uint32_t) irq_1,
	(uint32_t) irq_2, (uint32_t) irq_3,
	(uint32_t) irq_4, (uint32_t) irq_5,
	(uint32_t) irq_6, (uint32_t) irq_7,
	(uint32_t) irq_8, (uint32_t) irq_9,
	(uint32_t) irq_10, (uint32_t) irq_11,
	(uint32_t) irq_12, (uint32_t) irq_13,
	(uint32_t) irq_14, (uint32_t) irq_15,
};

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

						// KDEBUG("Symbol table copied from 0x%X to 0x%X, link 0x%X", ent->sh_addr, kern_elf_symtab, ent->sh_link);
					} /*else { // other kind of section
						KWARNING("Section %s, addr 0x%x, sz 0x%X, type 0x%X, idx 0x%X\n", name, ent->sh_addr, ent->sh_size, ent->sh_type, i);
					}*/
				}
			}

			// Get symbol strings table and copy
			if(symbol_string_table != -1) {
				elf_section_entry_t *ent = &entries[symbol_string_table];
				kern_elf_strtab = (char *) kmalloc(ent->sh_size);
				memmove(kern_elf_strtab, (void *) ent->sh_addr, ent->sh_size);
				// KDEBUG("String table copied from 0x%X to 0x%X, size 0x%X", ent->sh_addr, kern_elf_strtab, ent->sh_size);
			} else {
				KWARNING("Couldn't find symbol string table");
			}

			// KWARNING("'%s' at 0x%X\n", kern_elf_strtab + kern_elf_symtab[96].st_name, kern_elf_symtab[96].st_address);
		}

		// Copy memory map
		if (MULTIBOOT_CHECK_FLAG(lowmemStruct->flags, 6)) {
			multiboot_memory_map_t *mmap = (multiboot_memory_map_t *) lowmemStruct->mmap_addr;
			multiboot_memory_map_t *new = (multiboot_memory_map_t *) kmalloc(lowmemStruct->mmap_length);
			memmove(new, mmap, lowmemStruct->mmap_length);
			himemStruct->mmap_addr = (uint32_t) new;
		}

		// Modules (only one, the ramdisk) 
		if (MULTIBOOT_CHECK_FLAG(lowmemStruct->flags, 3)) {
			multiboot_module_t *module = (multiboot_module_t *) lowmemStruct->mods_addr;
			uint32_t size = module->mod_end - module->mod_start;
			ramdisk_found(module->mod_start, size);
		}

		KINFO("%uKB low memory, %uKB high memory", (unsigned int) x86_multiboot_info->mem_lower, (unsigned int) x86_multiboot_info->mem_upper);
	} else {
		KERROR("No multiboot info!");
	}	
}

void x86_pc_init(void) {
	// Set up IDT
	idt_init();

	// Set up a TSS (for CR3 => CR0 ints)
	tss_init();

	// Determine if we can use APIC
	if(apic_supported()) {
		apic_init();
	} else {
		// Set up IRQ gates
		for(int irq = 0; irq < 16; irq++) {
			idt_set_gate(irq+0x20, asm_irq_handlers[irq], GDT_KERNEL_CODE, 0x8E);
		}

		// Remap PICs
		i8259_remap(0x20, 0x28);
	}

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
	irq_register_handler(0, kern_timer_tick, NULL);
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

/*
 * Reads the timestamp counter.
 */
uint64_t x86_pc_read_tsc(void) {
	unsigned int lo, hi;

	__asm__ volatile("rdtsc" : "=a" (lo), "=d" (hi));
	return (uint64_t) hi << 32 | lo;
}

/*
 * Flush CPU caches?
 */
void x86_flush_cpu_caches(void) {

}