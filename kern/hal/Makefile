MODULE=hal
SOURCES=hal.c keyboard.c disk.cpp bus.c config.c vfs.cpp filesystem.cpp handle.cpp
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