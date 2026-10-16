
#include <alloc.h>

#if defined(USE_JEMALLOC) && USE_JEMALLOC == 1
#define JEMALLOC_NO_DEMANGLE
#include <jemalloc/jemalloc.h>
    #define alloc_fn je_malloc
    #define realloc_fn je_realloc
    #define free_fn je_free
    #define calloc_fn je_calloc
#else
#include <stdlib.h>
    #define alloc_fn malloc
    #define realloc_fn realloc
    #define free_fn free
    #define calloc_fn calloc
#endif

void* zalloc(size_t size) {
    return alloc_fn(size);
}

void* zcalloc(size_t nmemb, size_t size) {
    return calloc_fn(nmemb, size);
}

void* zrealloc(void* ptr, size_t size) {
    return realloc_fn(ptr, size);
}

void zfree(void* ptr) {
    free_fn(ptr);
}
