MODULE=driver_support
SOURCES=module.c ramdisk.c ramdisk_compression.c
OBJECTS=$(sort $(filter-out %.c %.s,$(SOURCES:.c=.o) $(SOURCES:.s=.o)))
CFLAGS+=-Wno-multichar

all: $(OBJECTS)

.c.o:
	@echo "[CC] $<"
	@$(CC) $(CFLAGS) $< -o $@

.s.o:
	@echo "[AS] $<"
	@$(AS) $(ASFLAGS) $< -o $@

clean:
	@rm -rf $(OBJECTS)