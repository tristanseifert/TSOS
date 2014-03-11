#import <hal/hal.h>

/*
 * Base class that filesystems should inherit from. Provides extra fun benefits
 * like caching and whatnot.
 */
class hal_fs {
	public:
		hal_fs(hal_disk_partition_t*, hal_disk_t *);

		// Directory functions
		bool directory_exists(char *directory);
		list_t* list_directory(char* dirname);

		// File opening
		fs_file_t *file_open(char *path);
		void file_close(fs_file_t *);

		void *file_read(void *buffer, unsigned int offset, unsigned int bytes, fs_file_t *file);
		void *file_write(void *buffer, unsigned int offset, unsigned int bytes, fs_file_t *file);

	// Various driver state to be exported
	public:
		bool fs_clealyUnmounted;

	// These should have to be overridden by the superclass for functionality
	protected:
		virtual unsigned int sector_for_file(char *path, unsigned int offset) =0;

		/*
		 * Reads the contents of a directory from the filesystem and creates
		 * a VFS directory structure to represent it.
		 *
		 * Note: path is relative to the root of THIS filesystem, not the root
		 * filesystem.
		 */
		virtual fs_directory_t *contents_of_directory(char *path) =0;

	protected:
		// Sector read/write functions
		void *read_sectors(unsigned int start, unsigned int numSectors, void *buffer, unsigned int *error);

		// Split path
		list_t *split_path(char *in);

	protected:
		hal_disk_partition_t *partition;
		hal_disk_t *disk;

		fs_directory_t *root_directory;

		// Filesystem label
		char *volumeLabel;
};
