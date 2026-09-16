//
// Created by turker on 11.08.2026.
//

#ifndef REDIS_CLONE_QUICK_LIST_H
#define REDIS_CLONE_QUICK_LIST_H

#include <stdint.h>

#include "zip_list.h"
#include "redis_string.h"

#define QL_MAX_BYTES 8192

typedef struct ql_node {
    struct ql_node *prev;
    struct ql_node *next;
    ziplist zl;
    uint32_t sz;
    uint32_t count;
} ql_node;

typedef struct ql {
    ql_node* head;
    ql_node* tail;
    unsigned long count;
    unsigned long len;
} ql;

typedef void (*quicklist_foreach_fn)(struct zlentry*, void *user_data);
typedef int (*quicklist_push_fn)(ql*, sds);
typedef int (*quicklist_pop_fn)(ql*);

ql* quicklist_create();
ql_node* create_quicklist_node();
int quicklist_push_tail(ql *ql, sds value);
int quicklist_push_head(ql *ql, sds value);
int quicklist_pop_tail(ql *ql);
int quicklist_pop_head(ql *ql);
void quicklist_free(ql *ql);
int quicklist_insert(ql *ql, const char *pivot, size_t pivot_size, sds value, int before);
struct zlentry* quicklist_get_at(ql *ql, int index);
int quicklist_range(ql *ql, int start, int stop, quicklist_foreach_fn fn, void *user_data);
int quicklist_remove(ql *ql, sds value, int count);
struct zlentry* ziplist_find(ziplist zl, const char *data, size_t data_len);

#endif //REDIS_CLONE_QUICK_LIST_H
