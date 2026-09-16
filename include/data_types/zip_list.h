#ifndef REDIS_CLONE_ZIP_LIST_H
#define REDIS_CLONE_ZIP_LIST_H

#include <stdint.h>

typedef unsigned char* ziplist;


/*
 * Ziplist layout
 * <zlbytes(4)> <zltail(4)> <zllen(2)> <entries...> <zlend(1)>
 *
 * Entry layout:
 * <prevlen(4)> <currlen(4)> <data(currlen)>
 */

struct __attribute__((__packed__)) zlhdr {
    uint32_t zlbytes;
    uint32_t zltail;
    uint16_t zllen;
    unsigned char entries[];
};

struct __attribute__((__packed__)) zlentry {
    uint32_t prevlen;
    uint32_t currlen;
    unsigned char data[];
};

static const int hdrsize = sizeof(struct zlhdr);
static const int zlentry_hdrsize = sizeof(struct zlentry);

#define ZL_HEADER_SIZE (sizeof(struct zlhdr))
#define ZL_END_SIZE 1
#define ZL_END 0xFF

#define ZL_BYTES(zl) (((struct zlhdr*)(zl - hdrsize))->zlbytes)

ziplist ziplist_create();
ziplist ziplist_head_push(ziplist zl, const char* data, uint32_t data_len);
ziplist ziplist_tail_push(ziplist zl, const char* data, uint32_t data_len);
ziplist ziplist_head_pop(ziplist zl);
ziplist ziplist_tail_pop(ziplist zl);
ziplist ziplist_insert(ziplist zl, const char *pivot, uint32_t pivot_size, const char* data, uint32_t data_len, int before);
ziplist ziplist_remove(ziplist zl, const char *data, uint32_t data_len, int count);
void ziplist_free(ziplist zl);
struct zlentry* zltail(ziplist zl);
struct zlentry* zlhead(ziplist zl);
struct zlentry* ziplist_get_at(ziplist zl, int index);
struct zlentry* zlnext(struct zlentry *entry);
struct zlentry* zlprev(struct zlentry *entry);
struct zlhdr* getzlhdr(ziplist zl);

#endif //REDIS_CLONE_ZIP_LIST_H
