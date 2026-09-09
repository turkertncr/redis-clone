//
// Created by turker on 10.08.2026.
//

#include <stdlib.h>
#include <sys/time.h>

#include "include/redis_object.h"

#include "include/data_types/quick_list.h"
#include "include/data_types/redis_string.h"
#include "include/data_types/set.h"
#include "include/data_types/sorted_set.h"

long long current_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return ((long long)tv.tv_sec * 1000) + (tv.tv_usec / 1000);
}

redis_object* create_redis_object(redis_type type, void* ptr, long long ttl_seconds) {
    redis_object* obj = malloc(sizeof(*obj));
    if (!obj) return NULL;

    obj->type = type;

    switch (type) {
        case OBJ_STRING: obj->ptr = sdsdup((sds)ptr); break;
        case OBJ_HASH: obj->ptr = ht_create((ht_value_free_fn) sdsfree); break;
        case OBJ_SET: obj->ptr = set_create(); break;
        case OBJ_LIST: obj->ptr = quicklist_create(); break;
        case OBJ_ZSET: obj->ptr = zset_create(); break;
        default: free(obj); return NULL;
    }

    if (obj->ptr == NULL) {
        free(obj);
        return NULL;
    }

    if (ttl_seconds > 0) {
        obj->expires_at = current_time_ms() + (ttl_seconds * 1000);
    } else {
        obj->expires_at = -1;
    }
    return obj;
}

redis_object* lookup_redis_object(hash_table* ht, sds key) {
    ht_node* hn = ht_node_get(ht, key);
    if (hn == NULL) { return NULL; }

    redis_object* obj = (redis_object*)hn->value;
    if (obj == NULL) return NULL;

    if (obj->expires_at == -1 || obj->expires_at > current_time_ms()) {
        return obj;
    }

    ht_delete(ht, key);
    return NULL;
}

void redis_object_free(redis_object *obj) {
    if (obj == NULL) return;

    switch (obj->type) {
        case OBJ_STRING: sdsfree(obj->ptr); break;
        case OBJ_ZSET: zset_free(obj->ptr); break;
        case OBJ_LIST: quick_list_free(obj->ptr); break;
        case OBJ_SET: set_free(obj->ptr); break;
        case OBJ_HASH: ht_free(obj->ptr); break;
    }
    free(obj);
}

