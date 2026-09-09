
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <include/data_types/hash_table.h>
#include <include/data_types/redis_string.h>

#define HASH_TABLE_LOAD_FACTOR 0.75

hash_table* ht_create(ht_value_free_fn free_fn) {
    hash_table *table = malloc(sizeof(*table));
    if (table == NULL) return NULL;

    table->size = 0;
    table->capacity = 1024;
    table->free_fn = free_fn;

    table->table = calloc(table->capacity, sizeof(ht_node*));
    if (table->table == NULL) {
        free(table);
        return NULL;
    }

    return table;
}

ht_node* ht_node_create(sds key, void *value) {
    ht_node *new_node = malloc(sizeof(*new_node));
    if (new_node == NULL) return NULL;

    new_node->key = sdsdup(key);
    if (new_node->key == NULL) {
        free(new_node);
        return NULL;
    }
    new_node->value = value;
    new_node->next = NULL;

    return new_node;
}

ht_node* ht_node_get(hash_table *ht, sds key) {
    if (ht == NULL || ht->size == 0) return NULL;

    unsigned long idx = ht_hash(key) % ht->capacity;
    ht_node *current = ht->table[idx];

    while (current != NULL) {
        if (sdscmp(current->key, key) == 0) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

int ht_delete(hash_table *ht, sds key){

    unsigned long idx = ht_hash(key) % ht->capacity;
    ht_node *current = ht->table[idx];
    ht_node *previous = NULL;

    while (current != NULL) {
        if (sdscmp(current->key, key) == 0) {
            if (previous == NULL) {
                ht->table[idx] = current->next;
            } else {
                previous->next = current->next;
            }
            ht->size--;
            sdsfree(current->key);
            if (ht->free_fn)
                ht->free_fn(current->value);
            else {
                sdsfree(current->value);
            }
            free(current);
            return 1;
        }
        previous = current;
        current = current->next;
    }

    return 0;
}

void* ht_get(hash_table *ht, sds key) {
    ht_node *node = ht_node_get(ht, key);
    if (node != NULL) {
        return node->value;
    }
    return NULL;
}

int ht_put(hash_table *ht, sds key, void *value) {
    if (ht == NULL) return 0;

    ht_node *entry = ht_node_get(ht, key);
    if (entry != NULL) {
        if (ht->free_fn)
            ht->free_fn(entry->value);
        else
            sdsfree(entry->value);
        entry->value = value;
        return 1;
    }

    if ((double)(ht->size + 1) / ht->capacity > HASH_TABLE_LOAD_FACTOR) {
        ht_extend(ht);
    }

    unsigned long idx = ht_hash(key) % ht->capacity;
    ht_node* inserted = ht_node_create(key, value);
    if (inserted == NULL) return 0;

    inserted->next = ht->table[idx];
    ht->table[idx] = inserted;
    ht->size++;
    return 1;
}

void ht_extend(hash_table *ht) {
    int old_capacity = ht->capacity;
    ht_node** old_table = ht->table;

    ht_node** new_table = calloc(old_capacity * 2, sizeof(ht_node*));
    if (new_table == NULL) return;

    ht->table = new_table;
    ht->capacity = old_capacity * 2;
    ht->size = 0;

    for (int i = 0; i < old_capacity; i++) {
        ht_node* current = old_table[i];
        while (current != NULL) {
            ht_node* next = current->next;
            ht_put(ht, current->key, current->value);
            sdsfree(current->key);
            free(current);
            current = next;
        }
    }

    free(old_table);
}

unsigned long ht_hash(sds key) {
    const unsigned char *bytes = (const unsigned char *)key;
    unsigned long hash = 5381;
    size_t length = sdslen(key);

    for (size_t i = 0; i < length; i++) {
        hash = ((hash << 5) + hash) + bytes[i];
    }

    return hash;
}

void ht_free(hash_table *ht) {

    for (int i = 0; i < ht->capacity; i++) {
        ht_node *current = ht->table[i];
        while (current != NULL) {
            ht_node *next = current->next;
            sdsfree(current->key);
            ht->free_fn(current->value);
            free(current);
            current = next;
        }
    }
    free(ht->table);
    free(ht);
}

int ht_exists(hash_table *ht, sds key) {
    return ht_node_get(ht, key) != NULL;
}

void ht_foreach(hash_table *ht, ht_iter_fn fn, void *user_data) {
    if (ht == NULL || fn == NULL) return;

    for (int i = 0; i < ht->capacity; i++) {
        ht_node *current = ht->table[i];
        while (current != NULL) {
            fn(current->key, current->value, user_data);
            current = current->next;
        }
    }
}