/*
 * Tool to generate a compressed TSOS ramdisk.
 *
 * This automagically aligns the files within the ramdisk to page (4K) bounds,
 * as to make the processing within the kernel easier. This can lead to some
 * wasted space, so it is recommended that the files are minified, if possible,
 * and superfluous files are not included.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include <dirent.h>
#include <unistd.h>

#include <zlib.h>

 __attribute__((used)) const char *info = "mkramdisk 1.0 by Tristan Seifert";

#define QRFS_VERSION 0x00010000
#define QRFS_MAGIC 'QRFS'

typedef struct qrfs_header qrfs_header_t;
typedef struct qrfs_file_entry qrfs_file_entry_t;

// Header of the filesystem
struct qrfs_header {
	uint32_t magic;
	uint32_t version;
	uint32_t numFiles;
	bool compressed;
} __attribute__((packed));

// Entry in a filesystem
struct qrfs_file_entry {
	const char name[32];

	uint32_t file_start;
	uint32_t length;
	uint32_t attributes;
} __attribute__((packed));

// Array to hold the file entries
static qrfs_file_entry_t files[256];
static qrfs_header_t header;
static int placement_offset;

/*
 * Main entry point
 */
int main(int argc, char *argv[]) {
	if(argc != 2) {
		printf("usage: %s directory\n", argv[0]);
		return -1;
	}

	// List files in directory
	char *directory = argv[1];

	DIR *dir;
	struct dirent *ent;

	if((dir = opendir(directory)) != NULL) {
		while ((ent = readdir (dir)) != NULL) {
			// Ignore "." and ".."
			if(strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
			} else {
				// Create a file entry for the files
				qrfs_file_entry_t *entry = &files[header.numFiles++];
				strncpy((char *) &entry->name, ent->d_name, 64);
			}
		}

		closedir (dir);
	} else {
		perror("Couldn't open directory specified");
		return -1;
	}

	printf("%s\n\n", info);

	// Initialise header
	header.magic = QRFS_MAGIC;
	header.version = QRFS_VERSION;

	// Open initrd file and write header
	FILE *initrd = fopen("initrd.bin", "wb+");
	if(!initrd) {
		perror("Couldn't open output file");
		return -1;
	}

	fwrite(&header, sizeof(qrfs_header_t), 1, initrd);

	// Now, write the individual files' entries
	for(int i = 0; i < header.numFiles; i++) {
		qrfs_file_entry_t *file = &files[i];
		fwrite(file, sizeof(qrfs_file_entry_t), 1, initrd);
	}

	// Get the current location of the file pointer
	placement_offset = ftell(initrd);
	printf("Writing files...\n");

	// We should now have info for files
	chdir(directory);

	// Iterate through all the files
	for(int i = 0; i < header.numFiles; i++) {
		qrfs_file_entry_t *file = &files[i];

		// Read file
		FILE *fp = fopen(file->name, "rb");
		if(fp) {
			uint32_t zero = 0;
			printf("\tWriting '%s'...\n", file->name);

			// Pad the placement address out to a page boundary
			int bytes_until_pagealigned = 4096 - (placement_offset % 4096);
			printf("\t\tInserting %u bytes of padding\n", bytes_until_pagealigned);

			// Write the correct amount of padding
			for(int x = 0; x < bytes_until_pagealigned; x++) {
				fputc(0x00, initrd);
			}

			// Place the file at the page offset
			placement_offset += bytes_until_pagealigned;

			// Determine filesize
			fseek(fp, 0L, SEEK_END);
			unsigned int sz = ftell(fp);
			fseek(fp, 0L, SEEK_SET);

			// Read file
			void *mem = malloc(sz);
			fread(mem, sz, 1, fp);
			fclose(fp);

			// Write to the output file
			fwrite(mem, sz, 1, initrd);
			free(mem);

			// Write a dword of zeros
			fwrite(&zero, sizeof(unsigned int), 1, initrd);
			sz += 4;

			// Update offset
			file->file_start = placement_offset;
			file->length = sz;
			file->attributes = 0;
			placement_offset += sz;
		} else {
			perror("Couldn't open an input file");
			return -1;
		}
	}

	// Pad the file out to a page
	int bytes_until_pagealigned = 4096 - (placement_offset % 4096);
	printf("\tWriting %u bytes of padding to end of file\n", bytes_until_pagealigned);

	// Write the correct amount of padding
	for(int x = 0; x < bytes_until_pagealigned; x++) {
		fputc(0x00, initrd);
	}

	// Re-write headers in the output file
	printf("\nContents of ramdisk:\n");
	fseek(initrd, sizeof(qrfs_header_t), SEEK_SET);

	for(int i = 0; i < header.numFiles; i++) {
		qrfs_file_entry_t *file = &files[i];
		fwrite(file, sizeof(qrfs_file_entry_t), 1, initrd);

		printf("\t'%s' at offset 0x%08X, %u bytes length\n", file->name, file->file_start, file->length);
	}

	// Get final ramdisk size
	fseek(initrd, 0L, SEEK_END);
	unsigned int initrd_sz = ftell(initrd);
	fseek(initrd, 0L, SEEK_SET);

	void *initrd_mem = malloc(initrd_sz);
	fread(initrd_mem, initrd_sz, 1, initrd);

	// Close file
	fclose(initrd);
	chdir("..");
	initrd = fopen("initrd.gz", "wb+");

	// Compression output buffer
	unsigned char out[0x4000];

	z_stream strm;
	deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 | 16, 8, Z_DEFAULT_STRATEGY);
	strm.next_in = initrd_mem;
	strm.avail_in = initrd_sz;

	// Perform compression
	do {
		int have;
		strm.avail_out = 0x4000;
		strm.next_out = out;
		
		deflate(&strm, Z_FINISH);
		have = 0x4000 - strm.avail_out;
		fwrite(out, 1, have, initrd);
	} while (strm.avail_out == 0);

	deflateEnd(&strm);
	fclose(initrd);
}