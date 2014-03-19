#define GDT_KERNEL_CODE 0x08
#define GDT_KERNEL_DATA 0x10
#define GDT_USER_CODE 0x18
#define GDT_USER_DATA 0x20

#define GDT_KERNEL_TSS (GDT_USER_DATA + 8)

// include below only from C code
#ifndef __ASSEMBLY__

#import <types.h>
#import "idt.h"

void x86_pc_init_multiboot(void);
void x86_pc_init(void);

void x86_pc_read_msr(uint32_t msr, uint32_t *lo, uint32_t *hi);
void x86_pc_write_msr(uint32_t msr, uint32_t lo, uint32_t hi);

uint64_t x86_pc_read_tsc(void);

void x86_flush_cpu_caches(void);

#endif