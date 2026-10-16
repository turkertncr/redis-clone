
#include <stdlib.h>
#include <sys/time.h>

#include "alloc.h"
#include "redis_object.h"

#include "util.h"
#include "data_types/quick_list.h"
#include "data_types/redis_string.h"
#include "data_types/set.h"
#include "data_types/sorted_set.h"

redis_object* create_redis_object(redis_type type, void* ptr, long long expires_at) {
    redis_object* obj = zalloc(sizeof(*obj));
    if (!obj) return NULL;

    obj->type = type;

    switch (type) {
        case OBJ_STRING: obj->ptr = sdsdup((sds)ptr); break;
        case OBJ_HASH: obj->ptr = ht_create((ht_value_free_fn) sdsfree, 0, HT_SMALL_CAPACITY); break;
        case OBJ_SET: obj->ptr = set_create(); break;
        case OBJ_LIST: obj->ptr = quicklist_create(); break;
        case OBJ_ZSET: obj->ptr = zset_create(); break;
        default: zfree(obj); return NULL;
    }

    if (obj->ptr == NULL) {
        zfree(obj);
        return NULL;
    }

    obj->expires_at = (expires_at > 0) ? expires_at : -1;
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
        case OBJ_LIST: quicklist_free(obj->ptr); break;
        case OBJ_SET: set_free(obj->ptr); break;
        case OBJ_HASH: ht_free(obj->ptr); break;
    }
    zfree(obj);
}

