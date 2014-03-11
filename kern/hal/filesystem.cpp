#import "filesystem.hpp"

/*
 * Initialises a basic filesystem
 */
hal_fs::hal_fs(hal_disk_partition_t *p, hal_disk_t *d) {
	partition = p;
	disk = d;
}

/*
 * Reads the specified amount of sectors into the buffer. Note that sector
 * numbers are relative to the start of the partition: Sector 0 would be the
 * first sector of the partition, not the drive.
 *
 * @return NULL if there was an error, start of the original buffer otherwise.
 */
void *hal_fs::read_sectors(unsigned int start, unsigned int numSectors, void *buffer, unsigned int *error) {
	start += partition->lba_start;
	unsigned int err = hal_disk_read(disk, start, numSectors, buffer, NULL, NULL, NULL);

	// Error reading disk?
	if(err) {
		// If the caller wants an error code, pass it.
		if(error) {
			*error = err;
		}

		return NULL;
	}

	return buffer;
}

// Directory functions
bool hal_fs::directory_exists(char *directory) {
	return false;
}

list_t* hal_fs::list_directory(char* dirname) {
	// Does directory exist?
	if(this->directory_exists(dirname)) {

	}

	return NULL;
}

// File opening
FILE *hal_fs::file_open(char *path) {
	return NULL;
}

void hal_fs::file_close(FILE *) {

}

void *hal_fs::file_read(void *buffer, unsigned int offset, unsigned int bytes, FILE *file) {
	return NULL;
}

void *hal_fs::file_write(void *buffer, unsigned int offset, unsigned int bytes, FILE *file) {
	return NULL;
}