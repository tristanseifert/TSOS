#import <hal/hal.h>

/*
 * Base class that filesystems should inherit from, which provides some common
 * functions for tasks such as reading sectors and splitting paths.
 */
class hal_fs {
	public:
		hal_fs(hal_disk_partition_t*, hal_disk_t *);

		// Sector read/write functions
		void *read_sectors(unsigned int start, unsigned int numSectors, void *buffer, unsigned int *error);
		void *write_sectors(unsigned int start, unsigned int numSectors, void *buffer, unsigned int *error);

		// Split path
		list_t *split_path(char *in);

	protected:
		hal_disk_partition_t *partition;
		hal_disk_t *disk;

		fs_directory_t *root_directory;

		// Filesystem label
		char *volumeLabel;

		bool fs_clealyUnmounted;
};
