#import <types.h>

// Logging
#define KDEBUG(...) klog(kLogLevelDebug, MODULE_NAME ": " __VA_ARGS__)
#define KINFO(...) klog(kLogLevelInfo, MODULE_NAME ": " __VA_ARGS__)
#define KSUCCESS(...) klog(kLogLevelSuccess, MODULE_NAME ": " __VA_ARGS__)
#define KWARNING(...) klog(kLogLevelWarning, MODULE_NAME ": " __VA_ARGS__)
#define KERROR(...) klog(kLogLevelError, MODULE_NAME ": " __VA_ARGS__)
#define KCRITICAL(...) klog(kLogLevelCritical, MODULE_NAME ": " __VA_ARGS__)

// Kernel module structure
typedef struct kern_module module_t;
struct kern_module {
	const char name[64];
};