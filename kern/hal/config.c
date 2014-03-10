#import <types.h>
#import "config.h"

// Internal state
hashmap_t *keys;

// Private functions
static void hal_config_parseline(char *line);

/*
 * Attempts to parse the config file in the buffer pointed to by buf, loading
 * the key/value pairs into the internal hashmap.
 */
void hal_config_parse(void *buf) {
	if(!buf) return;

	// Allocate hashmap
	keys = hashmap_allocate();

	// Read the config file line by line
	char *token = strtok(buf, "\n");

	// Temporary token buffer
	char str[1024];

	while(token) {
		// Locate hash so we can ignore comments
		char *startOfComment = strchr(token, '#');

		// If a comment was found, copy the line up to the comment
		if(startOfComment) {
			size_t size = startOfComment - token;

			// Ignore comments (first character == hash)
			if(size == 0) {
				goto findNext;
			} else {
				memcpy(&str, token, size);
			}
		} else { // This line has no comment
			if(strlen(token) > 2) {
				strncpy((char *) &str, token, 1024-1);
			} else {
				goto findNext;
			}
		}

		// Process the line
		hal_config_parseline(str);

		// Find next line
		findNext: ;
		token = strtok(NULL, "\n");
	}
}

/*
 * Parse a single line from the config file.
 */
static void hal_config_parseline(char *str) {
	// Buffers
	char keyBuf[64];
	char *valBuf = kmalloc(1024-64);
	memclr(&keyBuf, 64);

	// Split the string at the colon
	char *value = strchr(str, ':');
	size_t keyLength = value - str;

	// Copy key and value
	memcpy((void *) &keyBuf, str, keyLength);
	strncpy(valBuf, str+keyLength+1, 1024-65);

	// Strip leading spaces from the value string
	while(true) {
		if(valBuf[0] == ' ') {
			for(int i = 0; i < 1024-65; i++) {
				valBuf[i] = valBuf[i+1];
			}
		} else {
			break;
		}
	}

	// Insert into hashmap
	hashmap_insert(keys, &keyBuf, valBuf);
}

/*
 * Gets the value for a certain key from the internal hashmap.
 */
char *hal_config_get(const char *key) {
	if(!keys) return NULL;

	return hashmap_get(keys, (void *) key);
}

/*
 * Gets the key, and formats it as a signed integer.
 */
int hal_config_get_int(const char *key) {
	char *value = hal_config_get(key);

	if(value) {
		return strtol(value, NULL, 10);
	} else {
		return 0;
	}
}

/*
 * Gets the key, and formats it as an unsigned integer.
 */
unsigned int hal_config_get_uint(const char *key) {
	char *value = hal_config_get(key);

	if(value) {
		return strtoul(value, NULL, 10);
	} else {
		return 0;
	}
}

/*
 * Gets the key, and formats it as a boolean.
 *
 * Expected value: true/false
 */
bool hal_config_get_bool(const char *key) {
	char *value = hal_config_get(key);

	if(value) {
		if(!strcmp(value, "true")) {
			return true;
		} else {
			return false;
		}
	} else {
		return false;
	}
}

/*
 * Sets a value in the internal hashmap.
 */
void hal_config_set(const char *key, char *value) {
	if(!keys) return;

	hashmap_insert(keys, (void *) key, value);
}