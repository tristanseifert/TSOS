#import <hal/vfs.h>

class fs_fat32 : public hal_fs {
	public:
		fs_fat32(hal_disk_partition_t *, hal_disk_t *);

		unsigned int sector_for_file(char *path, unsigned int offset);
		list_t *contents_of_directory(char *directory);
};