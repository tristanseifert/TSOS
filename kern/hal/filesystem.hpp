#import <hal/hal.h>

class hal_fs {
	public:
		hal_fs(hal_disk_partition_t*, hal_disk_t *);

		// Directory functions
		bool directory_exists(char *directory);
		list_t* list_directory(char* dirname);

		// File opening
		FILE *file_open(char *path);
		void file_close(FILE *);

		void *file_read(void *buffer, unsigned int offset, unsigned int bytes, FILE *file);
		void *file_write(void *buffer, unsigned int offset, unsigned int bytes, FILE *file);

	// These should have to be overridden by the superclass for functionality
	protected:
		virtual unsigned int sector_for_file(char *path, unsigned int offset) =0;
		virtual list_t *contents_of_directory(char *directory) =0;

	// Sector read/write functions
	protected:
		void *read_sectors(unsigned int start, unsigned int numSectors, void *buffer, unsigned int *error);

	protected:
		hal_disk_partition_t *partition;
		hal_disk_t *disk;
};