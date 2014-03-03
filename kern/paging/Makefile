MODULE=paging
SOURCES=paging.c kheap.c kheap_dumb.c liballoc_1_1.c
OBJECTS=$(sort $(filter-out %.c %.s,$(SOURCES:.c=.o) $(SOURCES:.s=.o)))

all: $(OBJECTS)
	@rm -f $(MODULE).oa
	@$(LD) -r *.o -o $(MODULE).oa
	@rm -f *.o

.c.o:
	@echo "[CC] $<"
	@$(CC) $(CFLAGS) $< -o $@

.s.o:
	@echo "[AS] $<"
	@$(AS) $(ASFLAGS) $< -o $@

clean:
	@rm -rf $(OBJECTS)