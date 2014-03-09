#include <types.h>

enum log_type {
	kLogLevelDebug = 0,
	kLogLevelInfo,
	kLogLevelSuccess,
	kLogLevelWarning,
	kLogLevelError,
	kLogLevelCritical
};

#define CONSOLE_MIN_LOG_LEVEL kLogLevelDebug

int kprintf(const char* format, ...) __attribute__ ((format (printf, 1, 2)));
int klog(enum log_type type, const char* format, ...) __attribute__ ((format (printf, 2, 3)));

// don't define for kernel modules
#ifndef KMODULE
#define KDEBUG(...) klog(kLogLevelDebug, __VA_ARGS__)
#define KINFO(...) klog(kLogLevelInfo, __VA_ARGS__)
#define KSUCCESS(...) klog(kLogLevelSuccess, __VA_ARGS__)
#define KWARNING(...) klog(kLogLevelWarning, __VA_ARGS__)
#define KERROR(...) klog(kLogLevelError, __VA_ARGS__)
#define KCRITICAL(...) klog(kLogLevelCritical, __VA_ARGS__)
#endif