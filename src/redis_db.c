
#include <redis_db.h>
#include <util.h>

static void dummy_free(void *ptr) {
    (void) ptr;
}

void db_init(redis_db *db) {
    db->dict = ht_create((ht_value_free_fn) robj_free, REF_MODE, HT_MEDIUM_CAPACITY);
    db->expires = ht_create(dummy_free, REF_MODE, HT_MEDIUM_CAPACITY);
}

robj* db_lookup(redis_db *db, sds key) {
    ht_node* hn = ht_node_get(db->dict, key);
    if (hn == NULL) { return NULL; }

    robj* obj = hn->value;
    if (obj == NULL) return NULL;

    if (obj->expires_at == -1 || obj->expires_at > current_time_ms()) {
        return obj;
    }

    db_del(db, key);
    return NULL;
}

void db_put(redis_db *db, sds key, robj *obj) {
    if (!obj) return;

    long long expires_at = obj->expires_at;
    if (expires_at < current_time_ms() && expires_at != -1) {
        robj_free(obj);
        db_del(db, key);
        return;
    }

    sds common_key;
    ht_node* hn = ht_node_get(db->dict, key);
    if (!hn) { common_key = sdsdup(key); }
    else { common_key = hn->key; }

    ht_put(db->dict, common_key, obj);
    if (expires_at != -1) {
        ht_put(db->expires, common_key, obj);
    } else {
        ht_delete(db->expires, common_key);
    }
}

int db_del(redis_db *db, sds key) {
    if (!key) return 0;

    ht_node* hn = ht_node_get(db->dict, key);
    if (hn == NULL) return 0;
    sds stored = hn->key;

    ht_delete(db->expires, key);
    ht_delete(db->dict, key);
    sdsfree(stored);
    return 1;
}

int db_set_expire(redis_db *db, sds key, long long when) {
    robj* obj = db_lookup(db, key);
    if (obj == NULL) return 0;

    obj->expires_at = when;
    if (obj->expires_at < current_time_ms() && obj->expires_at != -1) {
        db_del(db, key);
        return 0;
    }
    sds real_key = ht_node_get(db->dict, key)->key;
    if (obj->expires_at != -1) ht_put(db->expires, real_key, obj);
    else ht_delete(db->expires, real_key);

    return 1;
}

int db_remove_expire(redis_db *db, sds key) {
    robj* obj = db_lookup(db, key);
    if (obj == NULL) return 0;

    obj->expires_at = -1;
    ht_delete(db->expires, key);
    return 1;
}
