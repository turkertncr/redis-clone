#ifndef REDIS_CLONE_QUICK_LIST_H
#define REDIS_CLONE_QUICK_LIST_H

#include <stdint.h>

#include <data_types/zip_list.h>
#include <data_types/redis_string.h>

#define QL_MAX_BYTES 8192

typedef struct quicklist_node {
    struct quicklist_node *prev;
    struct quicklist_node *next;
    ziplist zl;
    uint32_t sz;
    uint32_t count;
} quicklist_node;

typedef struct ql {
    quicklist_node* head;
    quicklist_node* tail;
    unsigned long count;
    unsigned long len;
} quicklist;

typedef void (*quicklist_foreach_fn)(struct zlentry*, void *user_data);
typedef int (*quicklist_push_fn)(quicklist*, sds);
typedef int (*quicklist_pop_fn)(quicklist*);

quicklist* quicklist_create();
quicklist_node* create_quicklist_node();
int quicklist_push_tail(quicklist *ql, sds value);
int quicklist_push_head(quicklist *ql, sds value);
int quicklist_pop_tail(quicklist *ql);
int quicklist_pop_head(quicklist *ql);
void quicklist_free(quicklist *ql);
int quicklist_insert(quicklist *ql, const char *pivot, size_t pivot_size, sds value, int before);
struct zlentry* quicklist_get_at(quicklist *ql, int index);
int quicklist_range(quicklist *ql, int start, int stop, quicklist_foreach_fn fn, void *user_data);
int quicklist_remove(quicklist *ql, sds value, int count);
struct zlentry* ziplist_find(ziplist zl, const char *data, size_t data_len);

#endif //REDIS_CLONE_QUICK_LIST_H
