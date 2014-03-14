MODULE=console
SOURCES=printf.c vga_console.c console.c panic.c
OBJECTS=$(sort $(filter-out %.c %.s,$(SOURCES:.c=.o) $(SOURCES:.s=.o)))

all: $(OBJECTS)

.c.o:
	@echo "[CC] $<"
	@$(CC) $(CFLAGS) $< -o $@

.s.o:
	@echo "[AS] $<"
	@$(AS) $(ASFLAGS) $< -o $@

clean:
	@rm -rf $(OBJECTS)