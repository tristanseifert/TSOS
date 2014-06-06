#import <types.h>
#import "apic.h"
#import "x86_pc.h"

#import "paging/paging.h"

#define IA32_APIC_BASE_MSR				0x1B
#define IA32_APIC_BASE_MSR_BSP			0x100
#define IA32_APIC_BASE_MSR_ENABLE		0x800

#define	CPUID_FLAG_APIC					(1 << 9)

extern bool pic_enabled;

// State of the LAPIC
unsigned int lapic_virt_addr;

// State of the IOAPIC
unsigned int iolapic_virt_addr;

// Private functions
static void apic_set_base(unsigned int apic);
static unsigned int apic_get_base();

static uint32_t lapic_read(uint32_t reg);
static void lapic_write(uint32_t reg, uint32_t val);

static uint32_t ioapic_read(uint32_t reg);
static void ioapic_write(uint32_t reg, uint32_t value);

/*
 * Determine if this processor supports APIC.
 */
bool apic_supported(void) {
	unsigned int eax, ebx, ecx, edx;
	__get_cpuid(1, &eax, &ebx, &ecx, &edx);

	return edx & CPUID_FLAG_APIC;
}

/*
 * Initialises the APIC for interrupt routing.
 */
void apic_init(void) {
	pic_enabled = false;

	// Set up the base of the APIC
	apic_set_base(apic_get_base());

	// Map to virtual addr
	unsigned int addr = apic_get_base();
	lapic_virt_addr = paging_map_section(addr, 0x4000, kernel_directory, kMemorySectionHardware);

	KDEBUG("LAPIC: 0x%08X => 0x%08X", addr, lapic_virt_addr);

	// Enable receiving of interrupts
	lapic_write(0xF0, lapic_read(0xF0) | 0x100);

	PANIC("tit");
}

/*
 * Set the base of the APIC registers.
 */
static void apic_set_base(unsigned int apic) {
	uint32_t edx = 0;
	uint32_t eax = (apic & 0xfffff100) | IA32_APIC_BASE_MSR_ENABLE;
 
	x86_pc_write_msr(IA32_APIC_BASE_MSR, eax, edx);
}
 
/*
 * Get the PHYSICAL location in which the APIC regs are mapped. 
 */
static unsigned int apic_get_base() {
	uint32_t eax, edx;
	x86_pc_read_msr(IA32_APIC_BASE_MSR, &eax, &edx);
 
	return (eax & 0xfffff100);
}


/*
 * Reads from a LAPIC register
 */
static uint32_t lapic_read(uint32_t reg) {
	uint32_t *ptr = (uint32_t *) (lapic_virt_addr + reg);
	return *ptr;
}

/*
 * Writes to a LAPIC register
 */
static void lapic_write(uint32_t reg, uint32_t val) {
	uint32_t *ptr = (uint32_t *) (lapic_virt_addr + reg);
	*ptr = val;
}

/*
 * Reads from an IOAPIC register
 */
static uint32_t ioapic_read(uint32_t reg) {
	uint32_t volatile *ioapic = (uint32_t volatile *) iolapic_virt_addr;
	ioapic[0] = (reg & 0xff);
	return ioapic[4];
}

/*
 * Writes to an IOAPIC register.
 */
static void ioapic_write(uint32_t reg, uint32_t value) {
	uint32_t volatile *ioapic = (uint32_t volatile *) iolapic_virt_addr;
	ioapic[0] = (reg & 0xff);
	ioapic[4] = value;
}
