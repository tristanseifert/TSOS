MODULE=x86_pc
SOURCES=x86_pc.c idt.c 8259_pic.c 8254_pit.c interrupts.c isr.s tss.c 8042_ps2.c ps2_kbd.c cmos_rtc.c
OBJECTS=$(sort $(filter-out %.c %.s,$(SOURCES:.c=.o) $(SOURCES:.s=.o)))

all: $(OBJECTS)

.c.o:
	@echo "[CC] $<"
	@$(CC) $(filter-out -O2,$(CFLAGS)) $< -o $@

.s.o:
	@echo "[AS] $<"
	@$(AS) $(ASFLAGS) $< -o $@

clean:
	@rm -rf $(OBJECTS)