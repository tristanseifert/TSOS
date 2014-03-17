#import "filesystem.hpp"
#import "x86_pc/cmos_rtc.h"

/*
 * Initialises the filesystem
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

/*
 * Writes sectors to the drive. Returns NULL if there was an error, or the
 * start of the original buffer if success.
 */
void *hal_fs::write_sectors(unsigned int start, unsigned int numSectors, void *buffer, unsigned int *error) {
	start += partition->lba_start;
	unsigned int err = hal_disk_write(disk, start, numSectors, buffer, NULL, NULL, NULL);

	// Error writing to disk?
	if(err) {
		// If the caller wants an error code, pass it.
		if(error) {
			*error = err;
		}

		return NULL;
	}

	return buffer;
}

/*
 * Splits a string with the slash ("/") character, and returns a string array
 * containing the individual pieces.
 *
 * Note: This function makes a copy of the path internally, as it is modified.
 * Therefore, to release its memory, you must free the first string, then the
 * array itself to relinquish all memory.
 */
list_t *hal_fs::split_path(char *in) {
	// Copy string
	size_t length = strlen(in) + 1;
	char *path = (char *) kmalloc(length);
	strncpy(path, in, length);

	// Allocate list
	list_t *list = list_allocate();
	ASSERT(list);

	// Perform separation by token
	char *component = strtok(path, "/");
	unsigned int i = 0;

	while(component) {
		// Allocate a buffer to copy this into
		size_t bufSize = strlen(component) + 4;
		char *newBuf = (char *) kmalloc(bufSize);

		// Clear buffer
		memclr(newBuf, bufSize);

		// Copy to buffer and add
		memcpy(newBuf, component, strlen(component));
		list_add(list, newBuf);

		component = strtok(NULL, "/");
	}

	// Clean up
	kfree(path);

	return list;
}

/*
 * Return current time.
 */
time_components_t hal_fs::get_current_fs_time(void) {
	return rtc_get_time();
}