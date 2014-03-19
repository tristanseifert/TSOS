MODULE=acpi
SOURCES=acpi.c
OBJECTS=$(sort $(filter-out %.c %.s,$(SOURCES:.c=.o) $(SOURCES:.s=.o)))

all: acpica $(OBJECTS)

acpica: ./acpica/*.c
	@echo "[CC] $<"
	@$(CC) $(CFLAGS) -I./acpica/include/ -Wno-format -fno-strict-aliasing ./acpica/*.c

.c.o:
	@echo "[CC] $<"
	@$(CC) $(CFLAGS) -I./acpica/include/ $< -o $@

.s.o:
	@echo "[AS] $<"
	@$(AS) $(ASFLAGS) $< -o $@

clean:
	@rm -rf $(OBJECTS)
	@rm -rf *.o