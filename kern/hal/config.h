#import <types.h>

void hal_config_parse(void *buf);

char *hal_config_get(const char *key);
void hal_config_set(const char *key, char *value);