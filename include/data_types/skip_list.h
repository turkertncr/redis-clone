//
// Created by turker on 16.08.2026.
//

#ifndef REDIS_CLONE_SKIPL_LIST_H
#define REDIS_CLONE_SKIPL_LIST_H

#include <include/data_types/redis_string.h>

#define P 0.25
#define MAX_LEVEL 32

typedef struct sl_node {
    sds member;
    double score;
    struct sl_node* prev;
    int level;
    struct {
        struct sl_node* next;
    } level_array[];
} sl_node;

typedef struct {
    int top_level;
    uint32_t size;
    sl_node* head;
} sl;

typedef void sl_iter_range_fn(sl_node *node, void *user_data);

sl *sl_create(void);
void sl_free(sl *skipl);

sl_node *sl_node_create(double score, sds member, int level);
void sl_node_free(sl_node *node);

int sl_rand(void);
int sl_cmp(double score1, sds member1, double score2, sds member2);

sl_node *sl_insert(sl *skipl, double score, sds member);
int sl_delete(sl *skipl, double score, sds member);
sl_node *sl_search(sl *skipl, double score, sds member);

int sl_range(sl *skipl, double min, double max, sl_iter_range_fn fn, void *user_data);

#endif //REDIS_CLONE_SKIPL_LIST_H
