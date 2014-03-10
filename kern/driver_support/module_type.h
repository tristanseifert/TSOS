#ifdef __cplusplus
extern "C" {
#endif

#import <types.h>

// Kernel module structure
typedef struct kern_module module_t;
struct kern_module {
	const char name[64];

	// Virtual location the module is loaded at
	unsigned int progbits_start;
	unsigned int nobits_start;
	unsigned int map_end;
};

#ifdef __cplusplus
}
#endif