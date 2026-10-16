
#include <alloc.h>
#include <data_types/redis_string.h>
#include <string.h>

sds sdsnewlen(const void *init, uint32_t initlen) {
    size_t struct_size = sizeof(struct sdshdr);
    size_t total_size = struct_size + initlen + 1;

    struct sdshdr *sh = zalloc(total_size);
    if (sh == NULL) { return NULL; }

    sh->len = initlen;
    sh->alloc = initlen;
    sh->flags = 0;

    if (initlen > 0 && init != NULL) {
        memcpy(sh->data, init, initlen);
    }

    sh->data[initlen] = '\0';
    return sh->data;
}

sds sdsnew(const char *init) {
    uint32_t initlen = (init == NULL) ? 0 : strlen(init);
    return sdsnewlen(init, initlen);
}

sds sdsempty() {
    return sdsnewlen("", 0);
}

sds sdsdup(const sds s) {
    if (s == NULL) return NULL;
    return sdsnewlen(s, sdslen(s));
}

void sdsfree(sds s) {
    if (s == NULL) return;
    struct sdshdr *sh = (struct sdshdr *) (s - sizeof(struct sdshdr));
    zfree(sh);
}

uint32_t sdslen(const sds s) {
    if (s == NULL) return -1;
    struct sdshdr *sh = (struct sdshdr *) (s - sizeof(struct sdshdr));
    return sh->len;
}

int sdscmp(const sds s1, const sds s2) {
    if (s1 == NULL || s2 == NULL) return s1 == s2 ? 0 : (s1 == NULL ? -1 : 1);

    size_t l1 = sdslen(s1);
    size_t l2 = sdslen(s2);
    size_t min = l1 < l2 ? l1 : l2;

    int res = memcmp(s1, s2, min);
    if (res == 0) return l1 > l2 ? 1 : (l1 < l2 ? -1 : 0);
    return res;
}

struct sdshdr *sdsgethdr(sds s) {
    if (s == NULL) return NULL;
    struct sdshdr *sh = (struct sdshdr *) (s - sizeof(struct sdshdr));
    if (sh == NULL) return NULL;
    return sh;
}

sds sdscat(sds s1, sds s2) {

    uint32_t l1 = sdslen(s1);
    uint32_t l2 = sdslen(s2);

    sds res = sdsnewlen(NULL, l1 + l2);
    memcpy(res, s1, l1);
    memcpy(res + l1, s2, l2);

    return res;
}

sds sdsappendlen(sds s1, const void *s2, size_t len) {

    struct sdshdr *sh = sdsgethdr(s1);
    if (sh == NULL) return NULL;

    size_t newlen = sh->len + len;
    size_t alloc = sh->alloc;
    if (alloc == 0) alloc = newlen;
    else while (alloc < newlen) alloc *= 2;

    struct sdshdr *tmp = zrealloc(sh, sizeof(struct sdshdr) + alloc + 1);
    if (tmp == NULL) return NULL;
    sh = tmp;
    sh->alloc = alloc;

    if (len > 0 && s2 != NULL) memcpy(sh->data + sh->len, s2, len);

    sh->len = newlen;
    sh->data[newlen] = '\0';

    return sh->data;
}

sds sdsappend(sds s1, const char *s2) {
    return sdsappendlen(s1, s2, strlen(s2));
}
