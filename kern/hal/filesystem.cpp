#import "filesystem.hpp"

/*
 * Initialises a basic filesystem
 */
hal_fs::hal_fs(hal_disk_partition_t *p, hal_disk_t *d) {
	partition = p;
	disk = d;
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