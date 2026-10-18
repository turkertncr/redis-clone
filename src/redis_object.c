
#include <stdlib.h>
#include "alloc.h"
#include "redis_object.h"

#include "server.h"
#include "util.h"
#include "data_types/quick_list.h"
#include "data_types/redis_string.h"
#include "data_types/set.h"
#include "data_types/sorted_set.h"

robj* create_robj(robj_type type, void* ptr, long long expires_at) {
    robj* obj = zalloc(sizeof(*obj));
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

void robj_free(robj *obj) {
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

