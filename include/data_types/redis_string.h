#ifndef REDIS_CLONE_REDIS_STRING_H
#define REDIS_CLONE_REDIS_STRING_H

#include <stddef.h>
#include <stdint.h>

typedef char *sds;

struct __attribute__((__packed__)) sdshdr {
    uint32_t len;
    uint32_t alloc;
    unsigned char flags;
    char data[];
};

sds sdsnewlen(const void *init, uint32_t initlen);
sds sdsnew(const char *init);
sds sdsempty();
void sdsfree(sds s);
int sdscmp(sds s1, sds s2);
sds sdsdup(sds s);
uint32_t sdslen(sds s);
struct sdshdr *sdsgethdr(sds s);
sds sdscat(sds s1, sds s2);
sds sdsappend(sds s1, const char *s2);
sds sdsappendlen(sds s1, const void *s2, size_t len);

#endif //REDIS_CLONE_REDIS_STRING_H
