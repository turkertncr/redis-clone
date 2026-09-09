//
// Created by turker on 4.08.2026.
//

#ifndef REDIS_CLONE_HASH_TABLE_H
#define REDIS_CLONE_HASH_TABLE_H

#include <include/data_types/redis_string.h>

typedef void (*ht_value_free_fn)(void*);
typedef void (*ht_iter_fn)(sds key, void* value, void* user_data);

typedef struct ht_node {
    sds key;
    void* value;
    struct ht_node* next;
} ht_node;

typedef struct ht {
    int size;
    int capacity;
    ht_node** table;
    ht_value_free_fn free_fn;
} hash_table;

hash_table* ht_create(ht_value_free_fn free_fn);
ht_node* ht_node_create(sds key, void* value);
ht_node* ht_node_get(hash_table *ht, sds key);
void ht_extend(hash_table *ht);
void* ht_get(hash_table* ht, sds key);
int ht_put(hash_table* ht, sds key, void* value);
int ht_delete(hash_table* ht, sds key);
unsigned long ht_hash(sds key);
void ht_free(hash_table* ht);
int ht_exists(hash_table* ht, sds key);
void ht_foreach(hash_table* ht, ht_iter_fn fn, void* user_data);

#endif //REDIS_CLONE_HASH_TABLE_H
