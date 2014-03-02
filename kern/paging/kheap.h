#import <types.h>

/*
 * Allocates a chunk of memory.
 *
 * @param sz Size of memory to allocati
 * @param align When set, allocation is
 * @param phys Pointer to memory to place physical address in
 */
uint32_t kmalloc_int(uint32_t sz, bool align, uint32_t *phys);

/*
 * Allocates a page-aligned chunk of memory.
 *
 * @param sz Size of memory to allocate
 */
uint32_t kmalloc_a(uint32_t sz);

/*
 * Allocates a chunk of memory and gets physical address.
 *
 * @param sz Size of memory to allocate
 * @param phys Pointer to memory to place physical address in
 */
uint32_t kmalloc_p(uint32_t sz, uint32_t *phys);

/*
 * Allocates a page-aligned chunk of memory and gets physical address.
 *
 * @param sz Size of memory to allocate
 * @param phys Pointer to memory to place physical address in
 */
uint32_t kmalloc_ap(uint32_t sz, uint32_t *phys);

/*
 * Allocates a chunk of memory.
 *
 * @param sz Size of memory to allocate
 */
uint32_t kmalloc(uint32_t sz);