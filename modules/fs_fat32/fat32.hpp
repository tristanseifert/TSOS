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

		// Filename functions
		/*
		 * Extracts the 8.3 filename for a directory entry, and formats it as a 14-byte
		 * character string.
		 */
		static char* dirent_get_8_3_name(fat_dirent_t *d) {
			char *buf = (char *) kmalloc(16);
			unsigned int c = 0;

			// Copy filename
			for(int i = 0; i < 8; i++) {
				if(d->name[i] != ' ') {
					buf[c++] = d->name[i];
				} else {
					break;
				}
			}

			// Dot
			buf[c++] = '.';

			// Extension
			for(int i = 0; i < 3; i++) {
				if(d->ext[i] != ' ') {
					buf[c++] = d->ext[i];
				} else {
					buf[c] = 0;
					break;
				}
			}

			return buf;
		}

	protected:

	private:
		// Structures read from disk
		fat_fs_bpb32_t bpb;
		fat_fs_fsinfo32_t fs_info;

		// Information about the filesystem
		unsigned int root_dir_sectors;

		unsigned int num_data_clusters;
		unsigned int num_data_sectors;
		unsigned int first_data_sector;

		unsigned int cluster_size;

		// Root directory
		unsigned int root_dir_num_entries;
		fat_dirent_t *root_dir;

		// Buffer for a single sector of FAT data
		uint32_t *fatBuffer;

		void read_root_dir(void);
		fat32_secoff_t fatEntryOffsetForCluster(unsigned int cluster);
		unsigned int *clusterChainForCluster(unsigned int cluster);

		void *readCluster(unsigned int cluster, void* buffer, unsigned int* error);
};