#import <types.h>
#import "module.h"
#import "ramdisk.h"

#import "paging/paging.h"
#import "hal/config.h"
#import "x86_pc/binfmt_elf.h"

#define DEBUG_MODULE_MAPPING	0
#define DEBUG_MOBULE_RELOC		1

// Compiler used to compile the kernel (used for kernel module compatibility checks)
static const char kernel_compiler[] = "GNU GCC " __VERSION__;

// Addresses of initcall addresses
extern uint32_t __kern_initcalls, __kern_exitcalls, __kern_callsend;

// Address at which new modules may be placed.
static unsigned int module_placement_addr, module_placement_end;

// Paging info
extern page_directory_t *kernel_directory;

// Private functions
static unsigned int find_symbol_in_kernel(char *name);

/*
 * Runs the init functions of all modules compiled statically into the kernel.
 */
void modules_load() {
	module_initcall_t *initcallArray = (module_initcall_t *) &__kern_initcalls;

	int i = 0;
	while(initcallArray[i] != NULL) {
		int returnValue = (initcallArray[i]());

		i++;
	}

	KSUCCESS("Static modules initialised");
}

/*
 * Loads the modules specified in the ramdisk. Loading follows this general
 * procedure:
 *
 * 1. Ensure the file is a valid ELF.
 * 2. Dynamically link with kernel functions from kernel symtab.
 * 3. Call module entry point
 */
void modules_ramdisk_load() {
	if(!ramdisk_loaded()) return;

	// Acquire initial placement address
	module_placement_addr = paging_get_memrange(kMemorySectionDrivers)[0];
	module_placement_end = paging_get_memrange(kMemorySectionDrivers)[1];

	// Get modules
	char *modulesToLoad = hal_config_get("modules");
	char *moduleName = strtok(modulesToLoad, " ");
	void *elf;

	// Find all modules
	while(moduleName) {
		// Attempt to load module from ramdisk
		if((elf = ramdisk_fopen(moduleName))) {
			elf_header_t *header = (elf_header_t *) elf;
			elf_section_entry_t *sections = elf + header->sh_offset;

			// Verify header
			if(ELF_CHECK_MAGIC(header->ident.magic)) {
				KWARNING("Module '%s' has invalid ELF magic of 0x%X%X%X%X\n", moduleName, header->ident.magic[0], header->ident.magic[1], header->ident.magic[2], header->ident.magic[3]);
				goto nextModule;
			}

			// Variables used for mapping of sections
			unsigned int progbits_start = 0, progbits_size = 0, progbits_offset = 0, progbits_size_raw = 0;
			unsigned int nobits_start = 0, nobits_size = 0;

			// Symbol table
			elf_symbol_entry_t *symtab = NULL;
			unsigned int symtab_entries = 0;

			// String and section string tables
			char *strtab = NULL;
			char *shstrtab = NULL;

			elf_section_entry_t *shstrtab_sec = &sections[header->sh_str_index];
			shstrtab = elf + shstrtab_sec->sh_offset;

			// Relocation table
			elf_program_relocation_t *rtab = NULL;
			unsigned int rtab_entries = 0;

			// Read the section table
			for(unsigned int s = 0; s < header->sh_entry_count; s++) {
				elf_section_entry_t *section = &sections[s];

				// Does this section have physical memory associated with it?
				if(section->sh_type == SHT_PROGBITS) {
					progbits_size += section->sh_size;

					if(!progbits_offset) {
						progbits_offset = section->sh_offset;
					}
				} else if(section->sh_type == SHT_NOBITS) { // NOBITS?
					nobits_size += section->sh_size;

					// Ensure consecutive NOBITS sections are properly handled
					if(!nobits_start) {
						nobits_start = section->sh_addr;
					}
				} else if(section->sh_type == SHT_REL) { // relocation
					rtab = elf + section->sh_offset;
					rtab_entries = section->sh_size / sizeof(elf_program_relocation_t);
				} else if(section->sh_type == SHT_SYMTAB) { // symbol table
					symtab = elf + section->sh_offset;
					symtab_entries = section->sh_size / sizeof(elf_symbol_entry_t);
				} else if(section->sh_type == SHT_STRTAB) { // string table
					if((elf + section->sh_offset) != shstrtab) {
						strtab = elf + section->sh_offset;
					}
				}
			}

			// Sane-ify section addresses and sizes
			progbits_size_raw = progbits_size;
			progbits_size += 0x1000;
			progbits_size &= 0xFFFFF000;
			progbits_start = module_placement_addr;

			// Traverse symbol table to find "module_entry" and "compiler"
			unsigned int init_addr = 0;
			char *compiler = NULL;
			bool entry_found = false;

			for(unsigned int s = 0; s < symtab_entries; s++) {
				elf_symbol_entry_t *symbol = &symtab[s];

				// Function of interest
				if(symbol->st_info & STT_FUNC) {
					char *name = strtab + symbol->st_name;

					if(!strcmp(name, "module_entry")) {
						elf_section_entry_t *section = &sections[symbol->st_shndx];
						init_addr = section->sh_addr + symbol->st_address;
						entry_found = true;
					}
				} 
				// Objects of interest
				else if(symbol->st_info & STT_OBJECT) {
					char *name = strtab + symbol->st_name;

					// Note how sh_offset is used, as we read out of the loaded elf
					if(!strcmp(name, "compiler")) {
						elf_section_entry_t *section = &sections[symbol->st_shndx];
						compiler = elf + section->sh_offset + symbol->st_address;
					}
				}
			}

			// Check if we're using compatible compiler versions
			if(strcmp(compiler, kernel_compiler)) {
				KERROR("'%s' has incompatible compiler of '%s', expected '%s'", moduleName, compiler, kernel_compiler);
				goto nextModule;
			}

			// Calculate logical address of the entry point
			unsigned int init_function_addr = 0;

			if(entry_found) {
				init_function_addr = progbits_start + init_addr;
				KDEBUG("Init function at 0x%08X", init_function_addr);
			} else {
				KERROR("'%s' has no entry function", moduleName);
				goto nextModule;
			}

			/*
 			 * In order for the module to be able to properly call into kernel
 			 * functions, relocation needs to be performed so it has the proper
 			 * addresses for kernel functions.
 			 *
 			 * To do this, the relocation table is searched for entries whose
 			 * type is R_386_PC32, and who are marked as "undefined" in the
 			 * symbol table.
 			 *
 			 * If all of the above conditions are met for a symbol, it's looked
 			 * up in the kernel symbol table. If this lookup fails, module
 			 * loading is aborted, as the module may crash later in hard to
 			 * debug ways if a function is simply left unrelocated.
			 */
			for(unsigned int r = 0; r < rtab_entries; r++) {
				elf_program_relocation_t *ent = &rtab[r];
				unsigned int symtab_index = ELF32_R_SYM(ent->r_info);

				// Function call relocations?
				if(ELF32_R_TYPE(ent->r_info) == R_386_PC32) {
					/*
					 * The ELF spec says that R_386_PC32 relocation entries must
					 * add the value at the offset to the symbol address, and
					 * subtract the section base address.
					 */

					// Look up only non-NULL relocations
					if(symtab_index != STN_UNDEF) {
						// Get symbol in question
						elf_symbol_entry_t *symbol = &symtab[symtab_index];
						char *name = strtab + symbol->st_name;

						unsigned int kern_symbol_loc = find_symbol_in_kernel(name);

						if(kern_symbol_loc) {
							// Perform the relocation.
							unsigned int *ptr = elf + progbits_offset + ent->r_offset;
							unsigned int newOffset = kern_symbol_loc;
							
							*ptr = newOffset + *ptr - module_placement_addr;

							#if DEBUG_MOBULE_RELOC
							KDEBUG("0x%08X -> 0x%08X (%s, kern)", (unsigned int) ent->r_offset, *ptr, name);
							#endif
						} else {
							KERROR("Module %s references '%s', but symbol does not exist in kernel", moduleName, name);
							goto nextModule;
						}
					} else {

					} 
				} else if(ELF32_R_TYPE(ent->r_info) == R_386_32) {
					/*
					 * The ELF spec says that R_386_32 relocation entries must
					 * add the value at the offset to the symbol address.
					 */
					elf_symbol_entry_t *symbol = &symtab[symtab_index];

					// If name = 0, relocating section
					if(symbol->st_name == 0) {
						// Get the section requested
						unsigned int sectionIndex = symbol->st_shndx;
						elf_section_entry_t *section = &sections[sectionIndex];
						char *name = shstrtab + section->sh_name;

						// Get virtual address of the section
						unsigned int addr = section->sh_addr + module_placement_addr;

						// Perform relocation
						unsigned int *ptr = elf + progbits_offset + ent->r_offset;
						*ptr = addr + *ptr;

						#if DEBUG_MOBULE_RELOC
						KDEBUG("0x%08X -> 0x%08X (section: %s+0x%X)", (unsigned int) ent->r_offset, *ptr, name, *ptr - addr);
						#endif
					} else {
						// Get symbol name and a placeholder address
						char *name = strtab + symbol->st_name;
						unsigned int addr = 0xDEADBEEF;
						bool inKernel = false;

						// Search through the module's symbols first
						for(unsigned int i = 0; i < symtab_entries; i++) {
							elf_symbol_entry_t *entry = &symtab[i];
							char *symbol_name = strtab + entry->st_name;

							if(!strcmp(name, symbol_name) && entry->st_shndx != STN_UNDEF) {
								addr = entry->st_address + module_placement_addr;
								inKernel = false;
								goto R_386_32_reloc_good;
							}
						}

						// See if the kernel has the symbol
						if(!(addr = find_symbol_in_kernel(name))) {
							KERROR("Module %s references '%s', but symbol does not exist", moduleName, name);
							goto nextModule;					
						}

						inKernel = true;

						// Perform relocation
						R_386_32_reloc_good: ;
						unsigned int *ptr = elf + progbits_offset + ent->r_offset;
						*ptr = addr + *ptr;

						#if DEBUG_MOBULE_RELOC
						KDEBUG("0x%08X -> 0x%08X (%s, %s)", (unsigned int) ent->r_offset, addr, name, inKernel ? "kernel" : "module");
						#endif
					}
				}
			}

			// Move PROGBITS from the file forward however many bits the offset is
			memmove(elf, elf+progbits_offset, progbits_size_raw);

			// Perform mapping for the PROGBITS section
#if DEBUG_MODULE_MAPPING
			KDEBUG("Mapping PROGBITS from 0x%08X to 0x%08X", module_placement_addr, module_placement_addr+progbits_size);
#endif

			unsigned int progbits_end = module_placement_addr + progbits_size;

			for(unsigned int a = module_placement_addr; a < progbits_end; a += 0x1000) {
				unsigned int progbits_offset = (a - module_placement_addr);
				unsigned int progbits_virt_addr = ((unsigned int) elf) + progbits_offset;

				// Get the page whose physical address we want
				page_t *elf_page = paging_get_page(progbits_virt_addr, false, kernel_directory);

				// Create a page in the proper virtual space, and assign physical address of above
				page_t *new_page = paging_get_page(a, true, kernel_directory);

				new_page->rw = 1;
				new_page->present = 1;
				new_page->frame = elf_page->frame;

				// KDEBUG("0x%08X -> 0x%08X (0x%08X)", a, progbits_offset, progbits_virt_addr);
			}

			module_placement_addr += progbits_size;

			// Perform mapping for NOBITS section, if needed
			if(nobits_size) {
				nobits_size += 0x1000;
				nobits_size &= 0xFFFFF000;

				unsigned int nobits_end = module_placement_addr+nobits_size;

#if DEBUG_MODULE_MAPPING
				KDEBUG("Mapping NOBITS from 0x%08X to 0x%08X", module_placement_addr, nobits_end);
#endif

				// Map the pages, and allocate memory to them
				for(unsigned a = module_placement_addr; a < nobits_end; a += 0x1000) {
					page_t *page = paging_get_page(a, true, kernel_directory);
					alloc_frame(page, true, true);

					// Zero the memory
					memclr((void *) a, 4096);
				}

				nobits_start = module_placement_addr;
				module_placement_addr += nobits_size;
			} else {
				KDEBUG("NOBITS section not required");
			}

			// Jump into the initialiser function
			void *driver = ((void* (*)(void)) init_function_addr)();
			KWARNING("Driver: 0x%08X", (unsigned int) driver);
		}

		nextModule: ;
		moduleName = strtok(NULL, " ");
	}


	KSUCCESS("Dynamically loaded modules initialised");
}

/*
 * Iterates through the kernel's symbol table to find the virtual address for
 * a certain symbol.
 */
static unsigned int find_symbol_in_kernel(char *name) {
	extern char *kern_elf_strtab;
	extern elf_symbol_entry_t *kern_elf_symtab;
	extern unsigned int kern_elf_symtab_entries;

	// Loop through symbols
	for(unsigned int i = 0; i < kern_elf_symtab_entries; i++) {
		elf_symbol_entry_t *entry = &kern_elf_symtab[i];
		char *symbol_name = kern_elf_strtab + entry->st_name;

		// Return address, if found
		if(!strcmp(name, symbol_name)) return entry->st_address;
	}

	// Symbol not found
	return 0;
}

void testStuff(void *ptr) {
	KDEBUG("Ptr: 0x%08X", (unsigned int) ptr);
}

void testStuff2(unsigned int d) {
	KDEBUG("Data: 0x%08X", d);
}