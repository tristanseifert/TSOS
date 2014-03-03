#ifndef TYPES_H
#define TYPES_H

#if !defined(__cplusplus)
#import <stdbool.h>
#endif
#import <stddef.h>
#import <stdint.h>
#import <stdlib.h>

#import <errno.h>

// Console support (printing, panic, etc)
#import "console/console.h"
#import "console/panic.h"
#import "paging/kheap.h"
#import "driver_support/module.h"

// X86 intrinsic macros (SSE)
// #import <x86intrin.h>

// These tell gcc how to optimise branches since it's stupid
#define likely(x)    __builtin_expect(!!(x), 1)
#define unlikely(x)  __builtin_expect(!!(x), 0)

#define __used	__attribute__((__used__))

#define PANIC(msg) panic(msg, __FILE__, __LINE__);
#define ASSERT(b) ((b) ? (void)0 : panic_assert(__FILE__, __LINE__, #b))

#define IRQ_OFF __asm__ volatile("cli");
#define IRQ_RES __asm__ volatile("sti");

#define ENDIAN_DWORD_SWAP(x) ((x >> 24) & 0xFF) | ((x << 8) & 0xFF0000) | ((x >> 8) & 0xFF00) | ((x << 24) & 0xFF000000)
#define ENDIAN_WORD_SWAP(x) ((x & 0xFF) << 0x08) | ((x & 0xFF00) >> 0x08)

/*
 * Write a byte to system IO port
 */
static inline void io_outb(uint16_t port, uint8_t val) {
	__asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

/*
 * Read a byte from a system IO port
 */
static inline uint8_t io_inb(uint16_t port) {
	uint8_t ret;
	__asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
	return ret;
}

/*
 * Write a word to system IO port
 */
static inline void io_outw(uint16_t port, uint16_t val) {
	__asm__ volatile("outw %0, %1" : : "a"(val), "Nd"(port));
}

/*
 * Read a word from a system IO port
 */
static inline uint16_t io_inw(uint16_t port) {
	uint16_t ret;
	__asm__ volatile("inw %1, %0" : "=a"(ret) : "Nd"(port));
	return ret;
}

/*
 * Write a dword to system IO port
 */
static inline void io_outl(uint16_t port, uint32_t val) {
	__asm__ volatile("outl %0, %1" : : "a"(val), "Nd"(port));
}

/*
 * Read a dword from a system IO port
 */
static inline uint32_t io_inl(uint16_t port) {
	uint32_t ret;
	__asm__ volatile("inl %1, %0" : "=a"(ret) : "Nd"(port));
	return ret;
}

/*
 * Wait for a system IO operation to complete.
 */
static inline void io_wait(void) {
	// port 0x80 is used for 'checkpoints' during POST.
	// The Linux kernel seems to think it is free for use
	__asm__ volatile("outb %%al, $0x80" : : "a"(0));
}

#endif