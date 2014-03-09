/*
 * Implementation for the ramdisk filesystem.
 */
#import <types.h>

#define QRFS_MAGIC 'QRFS'
#define QRFS_VERSION 0x00010000

typedef struct qrfs_header qrfs_header_t;
typedef struct qrfs_file_entry qrfs_file_entry_t;

// Header of the filesystem
struct qrfs_header {
	uint32_t magic;
	uint32_t version;
	uint32_t numFiles;
	bool compressed;

	uint32_t reserved[8];
} __attribute__((packed));

// Entry in a filesystem (first one is directly after header)
struct qrfs_file_entry {
	const char name[64];

	uint32_t file_start;
	uint32_t length;
	uint32_t attributes;
} __attribute__((packed));

// Called when the multiboot loader finds the RAM disk (copy to kernel mem)
void ramdisk_found(uint32_t addr, uint32_t size);
void *ramdisk_fopen(char *name);