/*
 * PRNG algorithm: Uses a Linear congruential generator to generate numbers,
 * with the following properties:
 *
 * m = 2^32, a = 1103515245, c = 12345
 */

#import <types.h>

uint32_t rand_32(void);
void* rand_bytes(void*, size_t);

void srand(uint32_t);