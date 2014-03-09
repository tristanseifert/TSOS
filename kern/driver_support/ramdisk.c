#import <types.h>
#import "ramdisk.h"
#import "ramdisk_compression.h"

// Internal state
static void *ramdisk;
static qrfs_header_t *header;
static qrfs_file_entry_t *files;

/*
 * Called during early preboot to allow the ramdisk to be verified, and its
 * contents to be copied to kernel space.
 */
void ramdisk_found(uint32_t addr, uint32_t size) {
	// Verify magic
	if(((qrfs_header_t *) addr)->magic == QRFS_MAGIC) {
		// KDEBUG("Found ramdisk at 0x%08X, %u bytes", (unsigned int) addr, (unsigned int) size);

		// Copy to kernel space
		ramdisk = kmalloc_a(size);
		memcpy(ramdisk, (void *) addr, size);

		// Update state to reference copied location
		header = (qrfs_header_t *) ramdisk;
		files = (qrfs_file_entry_t *) (ramdisk+sizeof(qrfs_header_t));
	} else {
		KWARNING("Module at 0x%08X is NOT ramdisk (magic 0x%08X, expected 0x%08X)", (unsigned int) addr, (unsigned int) header->magic, QRFS_MAGIC);
		return;
	}
}

/*
 * Indicates if a ramdisk is loaded.
 */
bool ramdisk_loaded(void) {
	if(ramdisk) return true;
	else return false;
}

/*
 * Searches for the specified filename, and if found, returns a pointer to it.
 */
void *ramdisk_fopen(const char *name) {
	if(!ramdisk) {
		KERROR("Tried to read '%s' without valid ramdisk", name);
		return NULL;
	}

	// Loop through all the files on the disk
	for(int f = 0; f < header->numFiles; f++) {
		qrfs_file_entry_t *ent = &files[f];
//		KDEBUG("File '%s', file offset %u", ent->name, (unsigned int) ent->file_start);

		// Does the name match?
		if(strcmp(ent->name, name) == 0) {
			// KDEBUG("Found %s, file offset %u", name, (unsigned int) ent->file_start);
			return (void *) (ramdisk + ent->file_start);
		}

		ent += sizeof(qrfs_file_entry_t);
	}

	KERROR("File '%s' not found in ramdisk", name);
	return NULL;
}