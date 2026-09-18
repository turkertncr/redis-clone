
#include <stdio.h>
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

struct zlentry* ziplist_find(ziplist zl, const char *data, size_t data_len) {
    struct zlhdr* hdr = getzlhdr(zl);
    struct zlentry *head = (struct zlentry *)hdr->entries;

    for (int i = 0; i < hdr->zllen; i++) {
        if (head->currlen == data_len && memcmp(head->data, data, data_len) == 0) {
            return head;
        }
        head = (struct zlentry*)((unsigned char*)head + zlentry_hdrsize + head->currlen);
    }
    return NULL;
}

struct zlentry* ziplist_get_at(ziplist zl, int index) {
    struct zlhdr *hdr = getzlhdr(zl);
    if (hdr == NULL || hdr->zllen == 0) { return NULL; }

    if (index < 0) { index += hdr->zllen; }
    if (index < 0 || index >= hdr->zllen) { return NULL; }

    struct zlentry *cur = zlhead(zl);
    for (int i = 0; i < index; i++) {
        cur = zlnext(cur);
    }
    return cur;
}

struct zlentry* zltail(ziplist zl) {
    struct zlhdr *hdr = getzlhdr(zl);
    return (struct zlentry *)((unsigned char *)hdr + hdr->zltail);
}

struct zlentry * zlhead(ziplist zl) {
    struct zlhdr *hdr = getzlhdr(zl);
    if (hdr == NULL) { return NULL; }
    return (struct zlentry *)hdr->entries;
}

struct zlentry* zlnext(struct zlentry *entry) {
    struct zlentry *next = (struct zlentry *)((unsigned char *)entry + entry->currlen + zlentry_hdrsize);
    if (next == NULL) { return NULL; }
    return next;
}

struct zlentry* zlprev(struct zlentry *entry) {
    struct zlentry *next = (struct zlentry *)((unsigned char *)entry - entry->prevlen);
    if (next == NULL) { return NULL; }
    return next;
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

ziplist ziplist_insert(ziplist zl, const char *pivot, uint32_t pivot_size, const char* data, uint32_t data_len, int before) {
    struct zlhdr *hdr = getzlhdr(zl);
    if (hdr == NULL || hdr->zllen == 0) { return zl; }

    struct zlentry *piv = ziplist_find(zl, pivot, pivot_size);
    if (piv == NULL) { return zl; }

    size_t piv_offset = (unsigned char *)piv - (unsigned char *)hdr;
    uint32_t piv_size = zlentry_hdrsize + piv->currlen;
    uint32_t piv_prevlen = piv->prevlen;
    int piv_is_tail = (piv_offset == hdr->zltail);

    size_t entry_size = zlentry_hdrsize + data_len;
    size_t old_bytes = hdr->zlbytes;
    size_t new_bytes = old_bytes + entry_size;

    struct zlhdr *tmp = realloc(hdr, new_bytes);
    if (tmp == NULL) { return NULL; }
    hdr = tmp;

    piv = (struct zlentry *)((unsigned char *)hdr + piv_offset);
    unsigned char *end = (unsigned char *)hdr + old_bytes - ZL_END_SIZE;

    unsigned char *pos = before ? (unsigned char *)piv : (unsigned char *)piv + piv_size;
    size_t move_bytes = end - pos;
    memmove(pos + entry_size, pos, move_bytes);

    struct zlentry *entry = (struct zlentry *)pos;
    entry->currlen = data_len;
    memcpy(entry->data, data, data_len);

    if (before) {
        entry->prevlen = piv_prevlen;
        struct zlentry *new_piv = (struct zlentry *)((unsigned char *)piv + entry_size);
        new_piv->prevlen = entry_size;
    } else {
        entry->prevlen = piv_size;
        if (move_bytes > 0) {
            struct zlentry *next = (struct zlentry *)((unsigned char *)entry + entry_size);
            next->prevlen = entry_size;
        }
    }

    hdr->zlbytes = new_bytes;
    hdr->zltail = (!before && piv_is_tail) ? (size_t)(pos - (unsigned char *)hdr) : hdr->zltail + entry_size;
    hdr->zllen++;
    hdr->entries[hdr->zlbytes - 1 - hdrsize] = ZL_END;

    return hdr->entries;
}

static struct zlentry* ziplist_find_last(ziplist zl, const char *data, size_t data_len) {
    struct zlhdr *hdr = getzlhdr(zl);
    if (hdr == NULL || hdr->zllen == 0) { return NULL; }

    struct zlentry *cur = zltail(zl);
    for (int i = 0; i < hdr->zllen; i++) {
        if (cur->currlen == data_len && memcmp(cur->data, data, data_len) == 0) {
            return cur;
        }
        if (cur->prevlen == 0) { break; }
        cur = (struct zlentry *)((unsigned char *)cur - cur->prevlen);
    }
    return NULL;
}

static struct zlhdr* ziplist_remove_at(struct zlhdr *hdr, struct zlentry *entry) {
    size_t offset = (unsigned char *)entry - (unsigned char *)hdr;
    size_t entry_size = zlentry_hdrsize + entry->currlen;
    uint32_t entry_prevlen = entry->prevlen;
    int is_tail = (offset == hdr->zltail);

    unsigned char *pos = (unsigned char *)hdr + offset;
    unsigned char *end = (unsigned char *)hdr + hdr->zlbytes - ZL_END_SIZE;
    size_t bytes = (end - pos) - entry_size;

    memmove(pos, pos + entry_size, bytes);
    if (!is_tail) {
        ((struct zlentry *)pos)->prevlen = entry_prevlen;
    }

    size_t new_bytes = hdr->zlbytes - entry_size;
    uint32_t old_tail = hdr->zltail;
    uint16_t new_len = hdr->zllen - 1;

    struct zlhdr *tmp = realloc(hdr, new_bytes);
    if (tmp == NULL) { return NULL; }
    hdr = tmp;

    hdr->zlbytes = new_bytes;
    hdr->zllen = new_len;
    hdr->zltail = (new_len == 0) ? hdrsize : (is_tail ? offset - entry_prevlen : old_tail - entry_size);
    hdr->entries[hdr->zlbytes - 1 - hdrsize] = ZL_END;

    return hdr;
}

ziplist ziplist_remove(ziplist zl, const char *data, uint32_t data_len, int count) {

    struct zlhdr *hdr = getzlhdr(zl);
    if (hdr == NULL || hdr->zllen == 0) { return zl; }

    if (count == 0) {
        struct zlentry *entry;
        while ((entry = ziplist_find(hdr->entries, data, data_len)) != NULL) {
            hdr = ziplist_remove_at(hdr, entry);
            if (hdr == NULL) { return NULL; }
            if (hdr->zllen == 0) { break; }
        }
        return hdr->entries;
    }

    int remaining = (count > 0) ? count : -count;
    while (remaining-- > 0 && hdr->zllen > 0) {
        struct zlentry *entry = (count > 0)
            ? ziplist_find(hdr->entries, data, data_len)
            : ziplist_find_last(hdr->entries, data, data_len);
        if (entry == NULL) { break; }
        hdr = ziplist_remove_at(hdr, entry);
        if (hdr == NULL) { return NULL; }
    }

    return hdr->entries;
}

void ziplist_free(ziplist zl) {
    struct zlhdr *hdr = getzlhdr(zl);
    if (hdr == NULL) { return; }
    free(hdr);
}