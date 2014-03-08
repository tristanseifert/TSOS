#import <types.h>
#import "rand.h"

// LCG variables
#define m ((2^32) - 1)
#define a 1103515245
#define c 12345

// Internal state
static uint32_t seed;
static uint32_t last;

/*
 * Generates a random 32-bit random value.
 */
uint32_t rand_32(void) {
	uint32_t new = ((a * last) + c) % m;
	last = new;

	return new;
}

/*
 * Generates size number of 32-bit words of random date into buffer, or 
 * allocates a buffer if it is NULL. Returns start of the buffer.
 */
void* rand_bytes(void* buffer, size_t size) {
	// Allocate buffer if needed
	if(!buffer) {
		buffer = kmalloc(size*4);
	}

	uint32_t *buf = (uint32_t *) buffer;

	// Fill buffer
	for(int i = 0; i < size; i++) {
		*buf++ = rand_32();
	}

	// Return buffer
	return buffer;
}

/*
 * Seeds the random number generator. This should be done every 2^32-1 calls
 * of rand_32 at the most, as the generator repeats after then.
 */
void srand(uint32_t newSeed) {
	last = seed = newSeed;
}