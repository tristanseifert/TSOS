#ifdef __cplusplus
extern "C" {
#endif

// General types
#import <types.h>

// Hardware interfacing and kernel functions
#import <hal/hal.h>
#import <driver_support/module_type.h>

// Logging
#define KDEBUG(...) klog(kLogLevelDebug, MODULE_NAME ": " __VA_ARGS__)
#define KINFO(...) klog(kLogLevelInfo, MODULE_NAME ": " __VA_ARGS__)
#define KSUCCESS(...) klog(kLogLevelSuccess, MODULE_NAME ": " __VA_ARGS__)
#define KWARNING(...) klog(kLogLevelWarning, MODULE_NAME ": " __VA_ARGS__)
#define KERROR(...) klog(kLogLevelError, MODULE_NAME ": " __VA_ARGS__)
#define KCRITICAL(...) klog(kLogLevelCritical, MODULE_NAME ": " __VA_ARGS__)

#ifdef __cplusplus
}
#endif