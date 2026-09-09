//
// Created by turker on 18.08.2026.
//

#ifndef REDIS_CLONE_SET_H
#define REDIS_CLONE_SET_H

#include <include/data_types/hash_table.h>

typedef struct {
    hash_table* ht;
    int size;
} set;

set* set_create();
int set_add(set* set, sds value);
int set_remove(set* set, sds value);
int set_contains(set* set, sds value);
int set_size(set* set);
void set_free(set* set);
void set_foreach(set *set, ht_iter_fn func, void *data);

#endif //REDIS_CLONE_SET_H
