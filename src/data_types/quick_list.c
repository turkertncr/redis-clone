//
// Created by turker on 13.08.2026.
//

#include "../../include/data_types/quick_list.h"
#include "../../include/data_types/zip_list.h"

#include <stdlib.h>

ql* quicklist_create() {
    ql *ql = malloc(sizeof(*ql));
    if (ql == NULL) return NULL;

    ql->head = ql->tail = NULL;
    ql->count = 0;
    ql->len = 0;
    return ql;
}

ql_node* create_quicklist_node() {
    ql_node *node = malloc(sizeof(*node));
    if (node == NULL) return NULL;

    node->prev = node->next = NULL;
    node->zl = ziplist_create();
    node->sz = ZL_BYTES(node->zl);
    node->count = 0;
    return node;
}

void quicklist_push_tail(ql *ql, const char *value, uint32_t value_len) {
    ql_node *node = ql->tail;

    if (!node || (node->sz + value_len) > QL_MAX_BYTES) {
        ql_node* new = create_quicklist_node();

        if (ql->tail == NULL) {
            ql->tail = new;
            ql->head = new;
        } else {
            ql->tail->next = new;
            new->prev = ql->tail;
            ql->tail = new;
        }

        node = new;
        ql->len++;
    }

    node->zl = ziplist_tail_push(node->zl, value, value_len);

    node->sz = ZL_BYTES(node->zl);
    node->count++;
    ql->count++;
}

void quicklist_push_head(ql *ql, const char *value, uint32_t value_len) {
    ql_node *node = ql->head;

    if (!node || (node->sz + value_len) > QL_MAX_BYTES) {
        ql_node* new = create_quicklist_node();

        if (ql->head == NULL) {
            ql->head = new;
        } else {
            ql->head->prev = new;
            new->next = ql->head;
            ql->head = new;
        }

        node = new;
        ql->len++;
    }

    node->zl = ziplist_head_push(node->zl, value, value_len);

    node->sz = ZL_BYTES(node->zl);
    node->count++;
    ql->count++;
}

void quicklist_pop_tail(ql *ql) {
    ql_node *node = ql->tail;

    if (node && node->count > 0) {
        node->zl = ziplist_tail_pop(node->zl);
        node->count--;
        ql->count--;

        if (node->count == 0) {
            ql->tail = node->prev;
            if (ql->tail) {
                ql->tail->next = NULL;
            } else {
                ql->head = NULL;
            }
            ql->len--;
            ziplist_free(node->zl);
            free(node);
        }
    }
}

void quicklist_pop_head(ql *ql) {
    ql_node *node = ql->head;

    if (node && node->count > 0) {
        node->zl = ziplist_head_pop(node->zl);
        node->count--;
        ql->count--;

        if (node->count == 0) {
            ql->head = node->next;
            if (ql->head) {
                ql->head->prev = NULL;
            } else {
                ql->tail = NULL;
            }
            ql->len--;
            ziplist_free(node->zl);
            free(node);
        }
    }
}

void quick_list_free(ql *ql) {
    ql_node *head = ql->head;
    while (head) {
        ql_node *next = head->next;
        ziplist_free(head->zl);
        free(head);
        head = next;
    }
    free(ql);
}
