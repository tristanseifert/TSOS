#import <types.h>

typedef struct sys_idt_descriptor {
   uint16_t offset_1;	// offset bits 0..15
   uint16_t selector;	// a code segment selector in GDT or LDT
   uint8_t zero;		// unused, set to 0
   uint8_t flags;		// type and attributes
   uint16_t offset_2;	// offset bits 16..31
} __attribute__((packed)) idt_entry_t;

void idt_init(void);
void idt_set_gate(uint8_t entry, uint32_t function, uint8_t segment, uint8_t flags);
void idt_flush_cache(void);