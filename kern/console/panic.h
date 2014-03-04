void panic(char *message, char *file, unsigned int line);
void panic_assert(char *file, unsigned int line, char *desc);
void dump_stack_here(void);