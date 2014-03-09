#import <types.h>

void hal_config_parse(void *buf);

char *hal_config_get(const char *key);
int hal_config_get_int(const char *key);
unsigned int hal_config_get_uint(const char *key);
bool hal_config_get_bool(const char *key);

void hal_config_set(const char *key, char *value);