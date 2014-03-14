#import <types.h>

// Debugging
#define	DEBUG_DIRECTORY_CACHING	0
#define	DEBUG_READ				0
#define DEBUG_FILE_CREATE		0

#define DEBUG_FILE_NOT_FOUND	1

#define	PRINT_ERROR				1

// FAT equates
#define FAT32_MASK					0x0FFFFFFF
#define FAT32_BAD_CLUSTER			0x0FFFFFF7
#define FAT32_END_CHAIN				0x0FFFFFF8
#define FAT32_VOLUME_DIRTY_MASK		0x08000000
#define FAT32_VOLUME_IO_ERROR		0x04000000

#define FAT16_BAD_CLUSTER			0xFFF7
#define FAT16_END_CHAIN				0xFFF8

#define FAT_ATTR_READ_ONLY			0x01
#define FAT_ATTR_HIDDEN				0x02
#define FAT_ATTR_SYSTEM				0x04
#define FAT_ATTR_VOLUME_ID			0x08
#define FAT_ATTR_DIRECTORY			0x10
#define FAT_ATTR_ARCHIVE			0x20
#define FAT_ATTR_LFN				(FAT_ATTR_READ_ONLY | FAT_ATTR_HIDDEN | FAT_ATTR_SYSTEM | FAT_ATTR_VOLUME_ID)

// FAT32 FSInfo sector
typedef struct fat_fs_fsinfo32 {
	uint32_t signature; // 0x41615252

	uint8_t reserved[480];

	uint32_t signature2; // 0x61417272
	uint32_t last_known_free_sec_cnt;
	uint32_t free_cluster_search_start;

	uint8_t reserved2[12];

	uint32_t trailSig; // 0xAA550000
} __attribute__((packed)) fat_fs_fsinfo32_t;

// Various BPB types
typedef struct fat_extBS_32 {
	uint8_t bootjmp[3];
	unsigned char oem_name[8];
	uint16_t bytes_per_sector;
	uint8_t sectors_per_cluster;
	uint16_t reserved_sector_count;
	uint8_t table_count;
	uint16_t root_entry_count;
	uint16_t total_sectors_16;
	uint8_t media_type;
	uint16_t table_size_16;
	uint16_t sectors_per_track;
	uint16_t head_side_count;
	uint32_t hidden_sector_count;
	uint32_t total_sectors_32;

	uint32_t table_size_32;
	uint16_t extended_flags;
	uint16_t fat_version;
	uint32_t root_cluster;
	uint16_t fat_info;
	uint16_t backup_BS_sector;
	uint8_t reserved_0[12];
	uint8_t drive_number;
	uint8_t reserved_1;
	uint8_t boot_signature;
	uint32_t volume_id;
	uint8_t  volume_label[11];
	uint8_t fat_type_label[8];
} __attribute__((packed)) fat_fs_bpb32_t;

typedef struct fat_extBS_16 {
	uint8_t bootjmp[3];
	unsigned char oem_name[8];
	uint16_t bytes_per_sector;
	uint8_t sectors_per_cluster;
	uint16_t reserved_sector_count;
	uint8_t table_count;
	uint16_t root_entry_count;
	uint16_t total_sectors_16;
	uint8_t media_type;
	uint16_t table_size_16;
	uint16_t sectors_per_track;
	uint16_t head_side_count;
	uint32_t hidden_sector_count;
	uint32_t total_sectors_32;

	uint8_t bios_drive_num;
	uint8_t reserved1;
	uint8_t boot_signature;
	uint32_t volume_id;
	uint8_t volume_label[11];
	uint8_t fat_type_label[8];
 
} __attribute__((packed)) fat_fs_bpb16_t;

typedef struct {
	uint8_t bootjmp[3];
	unsigned char oem_name[8];
	uint16_t bytes_per_sector;
	uint8_t sectors_per_cluster;
	uint16_t reserved_sector_count;
	uint8_t table_count;
	uint16_t root_entry_count;
	uint16_t total_sectors_16;
	uint8_t media_type;
	uint16_t table_size_16;
	uint16_t sectors_per_track;
	uint16_t head_side_count;
	uint32_t hidden_sector_count;
	uint32_t total_sectors_32;
} __attribute__((packed)) fat_fs_bpb_t;

typedef struct {
	unsigned char name[8];
	unsigned char ext[3];
	
	uint8_t attributes;
	
	uint8_t nt_reserved; // all uppercase/lowercase state

	uint8_t time_created_seconds; // tenths of seconds, 0-199
	uint16_t created_time;
	
	uint16_t created_date;
	uint16_t accessed_date;

	uint16_t cluster_high;

	uint16_t write_time;
	uint16_t write_date;

	uint16_t cluster_low;

	uint32_t filesize; // in bytes
} __attribute__((packed)) fat_dirent_t;

typedef struct {
	uint8_t order;
	uint16_t name1[5]; // characters 1-5

	uint8_t attributes;
	uint8_t type; // should be zero
	uint8_t checksum; // checksum of name of short dir

	uint16_t name2[6]; // characters 6-11
	uint16_t zero;
	uint16_t name3[2]; // characters 12-13

} __attribute__((packed)) fat_longname_dirent_t;