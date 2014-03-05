#import <types.h>
#import "error.h"
#import "x86_pc/binfmt_elf.h"

// ELF sections useful for stack dumps
extern char *kern_elf_strtab;
extern elf_symbol_entry_t *kern_elf_symtab;
extern unsigned int kern_elf_symtab_entries;

void error_dump_regs(err_registers_t regs);

uint32_t error_cr0, error_cr1, error_cr2, error_cr3;

// Strings
static const char err_names[19][28] = {
	"Division by Zero",
	"Debug Exception",
	"Non-Maskable Interrupt",
	"Breakpoint Exception",
	"Into Detected Overflow",
	"Out of Bounds Exception",
	"Invalid Opcode Exception",
	"No Coprocessor Exception",
	"Double Fault",
	"Coprocessor Segment Overrun",
	"Bad TSS",
	"Segment not Present",
	"Stack Fault",
	"General Protection Fault",
	"Page Fault",
	"Unknown Interrupt Exception",
	"Coprocessor Fault",
	"Alignment Check Exception",
	"Machine Check Exception"
};

static const char reg_names[18][4] = {
	" DS",
	"EDI",
	"ESI",
	"EBP",
	"ESP",
	"EBX",
	"EDX",
	"ECX",
	"EAX",
	"EIP",
	" CS",
	"EFG",
	"USP",
	" SS",
	"CR0",
	"CR1",
	"CR2",
	"CR3",
};

/*
 * Dumps registers and exception name.
 */
void error_dump_regs(err_registers_t regs) {
	kprintf("\n");
	klog(kLogLevelCritical, "%s (0x%X)", (char *) &err_names[regs.int_no], (unsigned int) regs.err_code);

	// Dump the registers now.
	uint32_t registers[18] = {
		regs.ds, regs.edi, regs.esi, regs.ebp, regs.esp, regs.ebx, regs.edx, regs.ecx,
		regs.eax, regs.eip, regs.cs, regs.eflags, regs.useresp, regs.ss, error_cr0, error_cr1,
		error_cr2, error_cr3
	};

	for(uint8_t i = 0; i < 18; i+=2) {
		klog(kLogLevelCritical, "%s: 0x%08X %s: 0x%08X", (char *) &reg_names[i], (unsigned int) registers[i], (char *) &reg_names[i+1], (unsigned int) registers[i+1]);
	}

	error_dump_stack_trace(256, regs.ebp);
}

/*
 * Called by ISRs when an error occurrs.
 */
void error_handler(err_registers_t regs) {
	uint32_t obama;
	__asm__ volatile("mov %%cr2, %0" : "=r" (obama));
	error_cr2 = obama;
	__asm__ volatile("mov %%cr0, %0" : "=r" (obama));
	error_cr0 = obama;
	__asm__ volatile("mov %%cr3, %0" : "=r" (obama));
	error_cr3 = obama;

	error_dump_regs(regs);

	for(;;);
}

/*
 * Dumps a stack trace, up to maxFrames deep.
 */
void error_dump_stack_trace(unsigned int maxFrames, unsigned int address) {
	klog(kLogLevelCritical, "Call trace:");

	// Stack contains:
	//  Second function argument
	//  First function argument (MaxFrames)
	//  Return address in calling function
	//  ebp of calling function (pointed to by current ebp)

	// Read current EBP
	unsigned int *ebp = (unsigned int *) address;

	// If 0 is specified for address, use current
	if(!address) {
		uint32_t temp;
		__asm__ volatile("mov %%ebp, %0" : "=r" (temp));
		ebp = (unsigned int *) temp;
	}

	unsigned int eip = 0;

	// Iterate through each stack frame
	for(unsigned int frame = 0; frame < maxFrames; ++frame) {
		// Get instruction pointer
		eip = ebp[1];
		// Find closest symbol name
		char *closest_symbol = error_get_closest_symbol(eip);
		// Print
		klog(kLogLevelCritical, " [%.8X] %s", eip, closest_symbol);
		
		// Unwind to previous stack frame
		ebp = (unsigned int *) ebp[0];

		// Is there a stack frame before this one? (EBP != 0)
		if(!ebp) {
			eip = 0;
			klog(kLogLevelCritical, "-- [End of trace] --");
			break;
		}
	}

	// Print an end label if needed
	if(eip) {
		klog(kLogLevelCritical, "-- [Trace cut short] --");
	}
}

/*
 * Finds the name of the symbol closest to the specified address.
 */
char *error_get_closest_symbol(unsigned int address) {
	static char outBuffer[512];

	unsigned int index = 0xFFFFFFFF; // index of closest
	unsigned int abs_offset = 0xFFFFFFFF; // distance from it (absolute)
	elf_symbol_entry_t *entry;

	for(unsigned int i = 0; i < kern_elf_symtab_entries; i++) {
		entry = &kern_elf_symtab[i];

		// Calculate offset
		int offset = address - entry->st_address;

		// Ignore negative offsets
		if(offset > 0) {
			// Is it closer than the last?
			if(offset < abs_offset) {
				// If so, save offset
				abs_offset = (unsigned int) offset;
				index = i;
			}
		}
	}

	// If there is nothing, just print the address
	if(index == 0xFFFFFFFF) {
		sprintf(outBuffer, "??? 0x%X", address);
		return outBuffer;
	}

	// Get closest entry
	entry = &kern_elf_symtab[index];
	char *name = kern_elf_strtab + entry->st_name;
	int offset = address - entry->st_address;

	if(offset == 0) {
		// The address is the start of the symbol
		return name;
	} else { // Offset
		if(offset > 0) { // positive offset
			sprintf(outBuffer, "%s+0x%X", name, (unsigned int) offset);
		} else if(offset < 0) { // negative offset
			sprintf(outBuffer, "%s-0x%X", name, (unsigned int) -offset);
		}

		return outBuffer;
	}
}

/*
 * Functions for dealing with stack guards. This will cause the kernel to panic
 * if a stack frame becomes corrupted.
 */
// unsigned int __stack_chk_guard[4] = {0xDEADBEEF, 0xCAFEBABE, 0x80808080, 0xC001C0DE};
void *__stack_chk_guard = NULL;

void __stack_chk_guard_setup(void) {
	unsigned int *p = (unsigned int *) __stack_chk_guard;
	*p = 0xDEADBEEF;
	
	klog(kLogLevelDebug, "Stack guards initialised");
}
 
void __attribute__((noreturn)) __stack_chk_fail() { 
	IRQ_OFF();

	klog(kLogLevelCritical, "Kernel stack memory corruption detected!");

	uint32_t ebp;
	__asm__("mov %%ebp, %0" : "=r" (ebp));
	error_dump_stack_trace(256, ebp);

	// Halt by going into an infinite loop.
	for(;;);
}