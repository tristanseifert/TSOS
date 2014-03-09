#import <types.h>
#import "config.h"

// Internal state
hashmap_t *keys;

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
			if(strlen(token) > 8) {
				strncpy((char *) &str, token, 1024);
			} else {
				goto findNext;
			}
		}

		// Process the line
		KDEBUG("Line: %s", str);

		// Find next line
		findNext: ;
		token = strtok(NULL, "\n");
	}
}