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

		// Filename functions
		/*
		 * Extracts the 8.3 filename for a directory entry, and formats it as a 14-byte
		 * character string.
		 */
		static char* dirent_get_8_3_name(fat_dirent_t *d) {
			char *buf = (char *) kmalloc(16);
			memclr(buf, 16);

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
			if(!(d->attributes & FAT_ATTR_DIRECTORY)) {
				buf[c++] = '.';
			}

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

		// Gets the sector that the offset into the file is contained in.
		unsigned int sector_for_file(char *path, unsigned int offset);

		// Lists a directory, the Fun Way™.
		fs_directory_t* list_directory(char* dirname, bool cache);

	private:
		// Small cache for directory handles
		hashmap_t *dirHandleCache;

		// Structures read from disk
		fat_fs_bpb32_t bpb;
		fat_fs_fsinfo32_t fs_info;

		// Information about the filesystem
		unsigned int root_dir_sectors;

		unsigned int num_data_clusters;
		unsigned int num_data_sectors;
		unsigned int first_data_sector;

		unsigned int cluster_size;

		// Buffer for a single sector of FAT data
		uint32_t *fatBuffer;

		void read_root_dir(void);

		// Calcualtes FAT entry location for a cluster
		fat32_secoff_t fatEntryOffsetForCluster(unsigned int cluster);

		// Follows a cluster chain
		unsigned int *clusterChainForCluster(unsigned int cluster);

		// Reads a single cluster from the filesystem
		void *readCluster(unsigned int cluster, void* buffer, unsigned int* error);

		// Reads an entire directory table into memory.
		fat_dirent_t *read_dir_file(fs_directory_t *parent, char *childName, unsigned int *entries);

		// Reads a directory and converts it
		fs_directory_t *read_directory(fs_directory_t *dir, char *name, bool cache, char *fullpath);

		// Takes an input buffer of FAT directory entries and "prettifies" them.
		void processFATDirEnt(fat_dirent_t *entries, unsigned int number, fs_directory_t *dir);

		// Compute checksum for long filenames
		uint8_t lfnCheckSum(unsigned char *shortName);

		// Converts a FAT timestamp to a UNIX timestamp
		time_t convert_timestamp(uint16_t date, uint16_t time, uint8_t millis);
};