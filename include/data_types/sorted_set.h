//
// Created by turker on 17.08.2026.
//

#ifndef REDIS_CLONE_SORTED_SET_H
#define REDIS_CLONE_SORTED_SET_H

#include <include/data_types/skip_list.h>
#include <include/data_types/hash_table.h>

typedef struct {
    sl* sl;
    hash_table* ht;
    int size;
} zset;

zset* zset_create();

int zset_add(zset *zset, double score, sds mem);
int zset_rem(zset* zset, sds mem);
double zset_find(zset* zset, sds mem);
void zset_incr(zset* zset, sds mem, double incr);
int zset_size(zset* zset);
void zset_free_fn(void* ptr);
void zset_free(zset* zset);
int zset_range_by_score(zset *zset, double min, double max, sl_iter_range_fn fn, void *user_data);


#endif //REDIS_CLONE_SORTED_SET_H
