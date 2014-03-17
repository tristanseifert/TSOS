#import <types.h>
#import "list_parser.h"

list_t *parse_list(char *in, const char *delimiter) {
	if(!in) return NULL;

	// Copy input buffer
	size_t in_len = strlen(in);
	char *string = (char *) kmalloc(in_len + 4);
	memcpy(string, in, in_len);

	// Allocate list
	list_t *list = list_allocate();

	// Read the file, separated by the specified delimiter
	char *token = strtok(string, delimiter);

	// Temporary string buffer
	char *tokBuf = (char *) kmalloc(1024);

	while(token) {
		memclr(tokBuf, 1024);

		// Locate hash so we can ignore comments
		char *startOfComment = strchr(token, '#');

		// If a comment was found, copy the line up to the comment
		if(startOfComment) {
			size_t size = startOfComment - token;

			// Ignore comments (first character == hash)
			if(size == 0) {
				goto findNext;
			} else {
				memcpy(tokBuf, token, size);
			}
		} else { // This line has no comment
			if(strlen(token) > 2) {
				strncpy(tokBuf, token, 1024-1);
			} else {
				goto findNext;
			}
		}

		// Ignore zero-length lines
		if(strlen(tokBuf) > 0) {
			// Allocate a buffer for the line
			size_t bufSize = strlen(tokBuf) + 4;
			char *copy = (char *) kmalloc(bufSize);
			memclr(copy, bufSize);

			// If success, copy the line and add to list
			if(copy) {
				strncpy(copy, tokBuf, bufSize - 1);
				list_add(list, copy);
			}
		}

		// Find next line
		findNext: ;
		token = strtok(NULL, delimiter);
	}

	// Clean up memory
	kfree(tokBuf);
	kfree(string);

	return list;	
}