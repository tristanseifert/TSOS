MODULE=debugger
SOURCES=debugger.c udis86_decode.c udis86_itab.c udis86_syn-att.c udis86_syn-intel.c udis86_syn.c udis86.c
OBJECTS=$(sort $(filter-out %.c %.s %.cpp,$(SOURCES:.c=.o) $(SOURCES:.s=.o) $(SOURCES:.cpp=.o)))

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

.cpp.o:
	@echo "[G++] $<"
	@$(CPP) $(CPPFLAGS) $< -o $@

clean:
	@rm -rf $(OBJECTS)