typedef struct registers {
	uint32_t ds; // Data segment selector
	uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax; // Pushed by pusha.
	uint32_t int_no, err_code; // Interrupt number and error code (if applicable)
	uint32_t eip, cs, eflags, useresp, ss; // Pushed by the processor automatically.
} err_registers_t;

void error_handler(err_registers_t regs);
void error_dump_regs(err_registers_t regs);
void error_dump_stack_trace(unsigned int maxFrames, unsigned int address);

char *error_get_closest_symbol(unsigned int address);
