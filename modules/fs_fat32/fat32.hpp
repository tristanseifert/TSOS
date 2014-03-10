#import <hal/vfs.h>
#import "fat.hpp"

typedef struct fat32_secoff {
	unsigned int sector;
	unsigned int offset;
} fat32_secoff_t;

class fs_fat32 : public hal_fs {
	public:
		fs_fat32(hal_disk_partition_t *, hal_disk_t *);
		~fs_fat32();

		unsigned int sector_for_file(char *path, unsigned int offset);
		list_t *contents_of_directory(char *directory);

	protected:

	private:
		fat_fs_bpb32_t *bpb;

		unsigned int root_dir_sectors;
		unsigned int data_sectors;
		unsigned int cluster_count;

		fat32_secoff_t fatEntryOffsetForCluster(unsigned int cluster);
};