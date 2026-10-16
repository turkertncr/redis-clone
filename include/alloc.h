
#ifndef REDIS_CLONE_ALLOC_H
#define REDIS_CLONE_ALLOC_H

#include <stddef.h>

void* zalloc(size_t size);
void* zcalloc(size_t nmemb, size_t size);
void* zrealloc(void* ptr, size_t size);
void zfree(void* ptr);

#endif //REDIS_CLONE_ALLOC_H
