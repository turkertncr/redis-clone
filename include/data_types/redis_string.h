#ifndef REDIS_CLONE_REDIS_STRING_H
#define REDIS_CLONE_REDIS_STRING_H

#include <stdint.h>

typedef char* sds;

struct __attribute__((__packed__)) sdshdr {
    uint32_t len;
    uint32_t alloc;
    unsigned char flags;
    char data[];
};

sds sdsnewlen(const void *init, uint32_t initlen);
sds sdsnew(const char *init);
void sdsfree(sds s);
int sdscmp(sds s1, sds s2);
sds sdsdup(sds s);
uint32_t sdslen(sds s);
struct sdshdr *sdsgethdr(sds s);
sds sdscat(sds s1, sds s2);

#endif //REDIS_CLONE_REDIS_STRING_H
