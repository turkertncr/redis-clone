
#include <stdlib.h>
#include <string.h>

#include <include/data_types/zip_list.h>

ziplist ziplist_create() {

    size_t total = hdrsize + 1;
    struct zlhdr *zlhdr = malloc(total);
    if (zlhdr == NULL) { return NULL; }

    zlhdr->zllen = 0;
    zlhdr->zlbytes = total;
    zlhdr->zltail = hdrsize;

    zlhdr->entries[0] = ZL_END;

    return zlhdr->entries;
}

struct zlhdr *getzlhdr(ziplist zl) {
    if (zl == NULL) { return NULL; }
    return (struct zlhdr *)(zl - hdrsize);
}

ziplist ziplist_head_push(ziplist zl, const char* data, uint32_t data_len) {

    struct zlhdr *hdr = getzlhdr(zl);
    if (hdr == NULL) { return NULL; }

    size_t new_entry_size = zlentry_hdrsize + data_len;
    size_t old_bytes = hdr->zlbytes;
    size_t new_bytes = old_bytes + new_entry_size;
    uint32_t old_tail_offset = hdr->zltail;
    uint16_t old_len = hdr->zllen;

    struct zlhdr *tmp = realloc(hdr, new_bytes);
    if (tmp == NULL) { return NULL; }
    hdr = tmp;

    unsigned char *first = hdr->entries;
    memmove(first + new_entry_size, first, old_bytes - hdrsize);

    struct zlentry *entry = (struct zlentry *)first;
    entry->prevlen = 0;
    entry->currlen = data_len;
    memcpy(entry->data, data, data_len);

    if (old_len > 0) {
        struct zlentry *next = (struct zlentry *)(first + new_entry_size);
        next->prevlen = new_entry_size;
    }

    hdr->zlbytes = new_bytes;
    hdr->zltail = (old_len == 0) ? hdrsize : old_tail_offset + new_entry_size;
    hdr->zllen++;

    return hdr->entries;
}

ziplist ziplist_tail_push(ziplist zl, const char* data, uint32_t data_len) {

    struct zlhdr *hdr = getzlhdr(zl);
    if (hdr == NULL) { return NULL; }

    size_t old_bytes = hdr->zlbytes;
    uint32_t tail_offset = hdr->zltail;

    uint32_t prev_len = 0;
    if (hdr->zllen > 0) {
        struct zlentry *last = (struct zlentry *)((unsigned char *)hdr + tail_offset);
        prev_len = zlentry_hdrsize + last->currlen;
    }

    size_t new_entry_size = zlentry_hdrsize + data_len;
    size_t new_bytes = old_bytes + new_entry_size;

    struct zlhdr *tmp = realloc(hdr, new_bytes);
    if (tmp == NULL) { return NULL; }
    hdr = tmp;

    struct zlentry *entry = (struct zlentry *)((unsigned char *)hdr + old_bytes - ZL_END_SIZE);
    entry->prevlen = prev_len;
    entry->currlen = data_len;
    memcpy(entry->data, data, data_len);

    hdr->zlbytes = new_bytes;
    hdr->zltail = old_bytes - ZL_END_SIZE;
    hdr->zllen++;

    ((unsigned char *)hdr)[new_bytes - 1] = ZL_END;

    return hdr->entries;
}

ziplist ziplist_head_pop(ziplist zl) {
    struct zlhdr *hdr = getzlhdr(zl);
    if (hdr == NULL || hdr->zllen == 0) { return zl; }

    struct zlentry *first = (struct zlentry *)hdr->entries;
    uint32_t entry_size = zlentry_hdrsize + first->currlen;

    unsigned char *next = hdr->entries + entry_size;
    uint32_t remaining = hdr->zlbytes - hdrsize - entry_size;

    if (hdr->zllen > 1) {
        ((struct zlentry *)next)->prevlen = 0;
    }

    memmove(hdr->entries, next, remaining);

    uint32_t new_bytes = hdr->zlbytes - entry_size;
    uint16_t old_len = hdr->zllen;
    uint32_t old_tail = hdr->zltail;

    struct zlhdr *tmp = realloc(hdr, new_bytes);
    if (tmp == NULL) { return NULL; }
    hdr = tmp;

    hdr->zlbytes = new_bytes;
    hdr->zltail = (old_len == 1) ? hdrsize : old_tail - entry_size;
    hdr->zllen--;

    return hdr->entries;
}

ziplist ziplist_tail_pop(ziplist zl) {
    struct zlhdr *hdr = getzlhdr(zl);
    if (hdr == NULL || hdr->zllen == 0) { return zl; }

    uint32_t tail_offset = hdr->zltail;
    struct zlentry *last = (struct zlentry *)((unsigned char *)hdr + tail_offset);

    uint32_t prev_len = last->prevlen;
    uint32_t entry_size = zlentry_hdrsize + last->currlen;

    uint32_t new_bytes = hdr->zlbytes - entry_size;
    uint16_t old_len = hdr->zllen;

    struct zlhdr *tmp = realloc(hdr, new_bytes);
    if (tmp == NULL) { return NULL; }
    hdr = tmp;

    ((unsigned char *)hdr)[new_bytes - 1] = ZL_END;

    hdr->zlbytes = new_bytes;
    hdr->zltail = (old_len == 1) ? hdrsize : tail_offset - prev_len;
    hdr->zllen--;

    return hdr->entries;
}

void ziplist_free(ziplist zl) {
    struct zlhdr *hdr = getzlhdr(zl);
    if (hdr == NULL) { return; }
    free(hdr);
}