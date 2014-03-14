MODULE=task
SOURCES=systimer.c syscall.c syscall_handler.s task.c taskswitch.s
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