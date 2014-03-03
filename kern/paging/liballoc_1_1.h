#import <types.h>

void *lalloc_malloc(size_t);
void *lalloc_realloc(void *, size_t);
void *lalloc_calloc(size_t, size_t);
void lalloc_free(void *);
