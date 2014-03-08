/*
 * Implementation for the ramdisk filesystem.
 */
#import <types.h>

typedef struct qrfs_header qrfs_header_t;
typedef struct qrfs_file_entry qrfs_file_entry_t;

// Header of the filesystem
struct qrfs_header {
	uint32_t magic;
	uint32_t version;
	bool compressed;

	qrfs_file_entry_t *file_table;
} __attribute__((packed));

// Entry in a filesystem
struct qrfs_file_entry {
	const char name[64];

	uint32_t file_start;
	uint32_t length;
	uint32_t attributes;

	qrfs_file_entry_t *next;
	qrfs_file_entry_t *prev;
} __attribute__((packed));