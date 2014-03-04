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