#import <types.h>

/*
 * Takes a character string, and parses it into a list_t object, using the
 * specified delimiter. Lines starting with a hash ("#") are ignored.
 *
 * @param in Input buffer
 * @param delimiter Character string to use as a delimiter
 * @return A list containing copies of the strings.
 */
list_t *parse_list(char *in, const char *delimiter);
