#ifndef REDIS_CLONE_SKIPL_LIST_H
#define REDIS_CLONE_SKIPL_LIST_H

#include <data_types/redis_string.h>

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
} skiplist_node;

typedef struct {
    int top_level;
    uint32_t size;
    skiplist_node* head;
} skiplist;

typedef void sl_iter_range_fn(skiplist_node *node, void *user_data);

skiplist *skiplist_create(void);
void skiplist_free(skiplist *skipl);

skiplist_node *skiplist_node_create(double score, sds member, int level);
void skiplist_node_free(skiplist_node *node);

int skiplist_rand(void);
int skiplist_cmp(double score1, sds member1, double score2, sds member2);

skiplist_node *skiplist_insert(skiplist *skipl, double score, sds member);
int skiplist_delete(skiplist *skipl, double score, sds member);
skiplist_node *skiplist_search(skiplist *skipl, double score, sds member);

int skiplist_range(skiplist *skipl, double min, double max, sl_iter_range_fn fn, void *user_data);

#endif //REDIS_CLONE_SKIPL_LIST_H
