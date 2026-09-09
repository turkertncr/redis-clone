//
// Created by turker on 11.08.2026.
//

#ifndef REDIS_CLONE_QUICK_LIST_H
#define REDIS_CLONE_QUICK_LIST_H

#include <stdint.h>

#include "zip_list.h"

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

ql* quicklist_create();
ql_node* create_quicklist_node();
void quicklist_push_tail(ql *ql, const char *value, uint32_t value_len);
void quicklist_push_head(ql *ql, const char *value, uint32_t value_len);
void quicklist_pop_tail(ql *ql);
void quicklist_pop_head(ql *ql);
void quick_list_free(ql *ql);

#endif //REDIS_CLONE_QUICK_LIST_H
