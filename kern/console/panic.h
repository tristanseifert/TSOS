void panic(const char *message, const char *file, unsigned int line);
void panic_assert(const char *file, unsigned int line, const char *desc);
void dump_stack_here(void);