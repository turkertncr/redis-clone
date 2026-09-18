#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include <include/command_handler.h>
#include <include/redis_object.h>
#include <include/data_types/set.h>
#include <include/data_types/sorted_set.h>
#include <include/data_types/quick_list.h>

#if defined(_WIN32) || defined(_WIN64)
#define strcasecmp _stricmp
#else
#include <strings.h>
#endif

size_t append_fmt(char *response, size_t response_size, size_t offset, const char *fmt, ...) {
    if (offset >= response_size) return offset;

    va_list args;
    va_start(args, fmt);
    int written = vsnprintf(response + offset, response_size - offset, fmt, args);
    va_end(args);

    if (written < 0) return offset;
    if ((size_t) written >= response_size - offset) return response_size;

    return offset + (size_t) written;
}

size_t append_b(char *buf, size_t capacity, size_t len, sds val) {
    if (val == NULL) return append_fmt(buf, capacity, len, "$-1\r\n");
    return append_fmt(buf, capacity, len, "$%u\r\n%s\r\n", sdslen(val), val);
}

static size_t append_bulk(char *buf, size_t capacity, size_t len, const redis_object *redis_obj) {
    if (redis_obj == NULL || redis_obj->type != OBJ_STRING) {
        return append_fmt(buf, capacity, len, "$-1\r\n");
    }
    return append_b(buf, capacity, len, (sds) redis_obj->ptr);
}

static void err_wrong_args(char *response, size_t response_size, const char *cmd_name) {
    snprintf(response, response_size, "-ERR wrong number of arguments for '%s' command\r\n", cmd_name);
}

static void err_wrongtype(char *response, size_t response_size) {
    snprintf(response, response_size, "-WRONGTYPE Operation against a key holding the wrong kind of value\r\n");
}

static int parse_double(sds s, double *out) {
    char *end;
    errno = 0;
    double v = strtod(s, &end);
    if (end == s || *end != '\0' || errno == ERANGE || isnan(v)) return 0;
    *out = v;
    return 1;
}

static redis_object *lookup_typed(hash_table *db, sds key, redis_type type, int create_if_missing,
                                  const char *missing_reply, char *response, size_t response_size) {
    redis_object *result = lookup_redis_object(db, key);
    if (result == NULL) {
        if (!create_if_missing) {
            snprintf(response, response_size, "%s", missing_reply);
            return NULL;
        }
        result = create_redis_object(type, NULL, -1);
        if (result == NULL) {
            snprintf(response, response_size, "-ERR out of memory\r\n");
            return NULL;
        }
        ht_put(db, key, result);
        return result;
    }
    if (result->type != type) {
        err_wrongtype(response, response_size);
        return NULL;
    }
    return result;
}

static void hgetall_append(sds field, void *value, void *user_data) {
    getall_ctx *ctx = (getall_ctx *) user_data;
    sds field_value = (sds) value;
    ctx->len = append_fmt(ctx->buf, ctx->capacity, ctx->len,
        "$%u\r\n%s\r\n$%u\r\n%s\r\n", sdslen(field), field, sdslen(field_value), field_value);
}

static void sgetall_append(sds member, void *value, void *user_data) {
    (void) value;
    getall_ctx *ctx = (getall_ctx *) user_data;
    ctx->len = append_fmt(ctx->buf, ctx->capacity, ctx->len,
        "$%u\r\n%s\r\n", sdslen(member), member);
}

static void zrange_append(sl_node *node, void *user_data) {
    getall_ctx *ctx = (getall_ctx *) user_data;
    ctx->len = append_fmt(ctx->buf, ctx->capacity, ctx->len,
        "$%u\r\n%s\r\n", sdslen(node->member), node->member);
}

static void zrange_count(sl_node *node, void *user_data) {
    (void) node;
    (void) user_data;
}

static void lrange_count(struct zlentry *entry, void *user_data) {
    (void) entry;
    (void) user_data;
}

static void lrange_append(struct zlentry *entry, void *user_data) {
    getall_ctx *ctx = (getall_ctx *) user_data;
    ctx->len = append_fmt(ctx->buf, ctx->capacity, ctx->len,
        "$%u\r\n%.*s\r\n", entry->currlen, entry->currlen, entry->data);
}

static void ping_command(resp_object *args, hash_table *db, char *response, size_t response_size) {
    (void) db;
    if (args->array.len >= 2) {
        sds msg = get_bulk_at(args, 1);
        snprintf(response, response_size, "$%u\r\n%s\r\n", sdslen(msg), msg);
    } else {
        snprintf(response, response_size, "+PONG\r\n");
    }
}

static void echo_command(resp_object *args, hash_table *db, char *response, size_t response_size) {
    (void) db;
    sds msg = get_bulk_at(args, 1);
    snprintf(response, response_size, "$%u\r\n%s\r\n", sdslen(msg), msg);
}

static void exists_command(resp_object *args, hash_table *db, char *response, size_t response_size) {
    int count = 0;
    for (int i = 1; i < args->array.len; i++) {
        if (lookup_redis_object(db, get_bulk_at(args, i)) != NULL) count++;
    }
    snprintf(response, response_size, ":%d\r\n", count);
}

static void del_command(resp_object *args, hash_table *db, char *response, size_t response_size) {
    int deleted = 0;
    for (int i = 1; i < args->array.len; i++) {
        deleted += ht_delete(db, get_bulk_at(args, i));
    }
    snprintf(response, response_size, ":%d\r\n", deleted);
}

static void expire_command(resp_object *args, hash_table *db, char *response, size_t response_size) {
    redis_object *redis_obj = lookup_redis_object(db, get_bulk_at(args, 1));
    if (redis_obj == NULL) {
        snprintf(response, response_size, ":0\r\n");
        return;
    }
    long long sec = strtoll(get_bulk_at(args, 2), NULL, 10);
    redis_obj->expires_at = current_time_ms() + sec * 1000;
    snprintf(response, response_size, ":1\r\n");
}

static void ttl_command(resp_object *args, hash_table *db, char *response, size_t response_size) {
    redis_object *redis_obj = lookup_redis_object(db, get_bulk_at(args, 1));
    if (redis_obj == NULL) {
        snprintf(response, response_size, ":-2\r\n");
        return;
    }
    if (redis_obj->expires_at == -1) {
        snprintf(response, response_size, ":-1\r\n");
        return;
    }
    long long ttl = (redis_obj->expires_at - current_time_ms()) / 1000;
    snprintf(response, response_size, ":%lld\r\n", ttl);
}

static void set_command(resp_object *args, hash_table *db, char *response, size_t response_size) {
    redis_object *redis_obj = create_redis_object(OBJ_STRING, get_bulk_at(args, 2), -1);
    ht_put(db, get_bulk_at(args, 1), redis_obj);
    snprintf(response, response_size, "+OK\r\n");
}

static void mset_command(resp_object *args, hash_table *db, char *response, size_t response_size) {
    int argc = args->array.len;
    if (argc % 2 == 0) {
        err_wrong_args(response, response_size, "mset");
        return;
    }

    for (int i = 1; i < argc; i += 2) {
        redis_object *redis_obj = create_redis_object(OBJ_STRING, get_bulk_at(args, i + 1), -1);
        ht_put(db, get_bulk_at(args, i), redis_obj);
    }
    snprintf(response, response_size, "+OK\r\n");
}

static void get_command(resp_object *args, hash_table *db, char *response, size_t response_size) {
    redis_object *redis_obj = lookup_typed(db, get_bulk_at(args, 1), OBJ_STRING, 0, "$-1\r\n", response, response_size);
    if (redis_obj == NULL) return;
    append_bulk(response, response_size, 0, redis_obj);
}

static void mget_command(resp_object *args, hash_table *db, char *response, size_t response_size) {
    size_t offset = append_fmt(response, response_size, 0, "*%d\r\n", args->array.len - 1);
    for (int i = 1; i < args->array.len; i++) {
        redis_object *redis_obj = lookup_redis_object(db, get_bulk_at(args, i));
        offset = append_bulk(response, response_size, offset, redis_obj);
    }
}

static void incr_decr(hash_table *db, sds key, long long delta, char *response, size_t response_size) {
    redis_object *redis_obj = lookup_redis_object(db, key);
    if (redis_obj != NULL && redis_obj->type != OBJ_STRING) {
        snprintf(response, response_size, "-WRONGTYPE value is not a string\r\n");
        return;
    }

    long long current = 0;
    if (redis_obj != NULL) {
        char *end;
        current = strtoll((sds) redis_obj->ptr, &end, 10);
        if (end == (sds) redis_obj->ptr || *end != '\0') {
            snprintf(response, response_size, "-ERR value is not an integer or out of range\r\n");
            return;
        }
    }

    long long new_value = current + delta;

    char buf[32];
    snprintf(buf, sizeof(buf), "%lld", new_value);

    if (redis_obj == NULL) {
        sds value = sdsnew(buf);
        redis_obj = create_redis_object(OBJ_STRING, value, -1);
        sdsfree(value);
        ht_put(db, key, redis_obj);
    } else {
        sdsfree((sds) redis_obj->ptr);
        redis_obj->ptr = sdsnew(buf);
    }

    snprintf(response, response_size, ":%lld\r\n", new_value);
}

static void incr_command(resp_object *args, hash_table *db, char *response, size_t response_size) {
    incr_decr(db, get_bulk_at(args, 1), 1, response, response_size);
}

static void decr_command(resp_object *args, hash_table *db, char *response, size_t response_size) {
    incr_decr(db, get_bulk_at(args, 1), -1, response, response_size);
}

static void incrby_command(resp_object *args, hash_table *db, char *response, size_t response_size) {
    long long delta = strtoll(get_bulk_at(args, 2), NULL, 10);
    incr_decr(db, get_bulk_at(args, 1), delta, response, response_size);
}

static void append_command(resp_object *args, hash_table *db, char *response, size_t response_size) {
    sds key = get_bulk_at(args, 1);
    sds value = get_bulk_at(args, 2);

    redis_object *redis_obj = lookup_redis_object(db, key);
    if (redis_obj == NULL) {
        redis_obj = create_redis_object(OBJ_STRING, value, -1);
        ht_put(db, key, redis_obj);
        snprintf(response, response_size, ":%u\r\n", sdslen((sds) redis_obj->ptr));
        return;
    }
    if (redis_obj->type != OBJ_STRING) {
        err_wrongtype(response, response_size);
        return;
    }

    sds existing_value = redis_obj->ptr;
    sds new_value = sdscat(existing_value, value);
    redis_obj->ptr = new_value;
    sdsfree(existing_value);

    snprintf(response, response_size, ":%u\r\n", sdslen(new_value));
}

static void hset_command(resp_object *args, hash_table *db, char *response, size_t response_size) {
    int argc = args->array.len;
    if (argc % 2 != 0) {
        err_wrong_args(response, response_size, "hset");
        return;
    }

    redis_object *redis_obj = lookup_typed(db, get_bulk_at(args, 1), OBJ_HASH, 1, NULL, response, response_size);
    if (redis_obj == NULL) return;

    hash_table *hash = redis_obj->ptr;
    int added = 0;
    for (int i = 2; i < argc; i += 2) {
        sds field = get_bulk_at(args, i);
        if (!ht_exists(hash, field)) added++;
        ht_put(hash, field, sdsdup(get_bulk_at(args, i + 1)));
    }
    snprintf(response, response_size, ":%d\r\n", added);
}

static void hget_command(resp_object *args, hash_table *db, char *response, size_t response_size) {
    redis_object *redis_obj = lookup_typed(db, get_bulk_at(args, 1), OBJ_HASH, 0, "$-1\r\n", response, response_size);
    if (redis_obj == NULL) return;

    sds field_value = ht_get(redis_obj->ptr, get_bulk_at(args, 2));
    append_b(response, response_size, 0, field_value);
}

static void hgetall_command(resp_object *args, hash_table *db, char *response, size_t response_size) {
    redis_object *redis_obj = lookup_typed(db, get_bulk_at(args, 1), OBJ_HASH, 0, "*0\r\n", response, response_size);
    if (redis_obj == NULL) return;

    hash_table *hash = redis_obj->ptr;
    getall_ctx ctx = { .buf = response, .capacity = response_size, .len = 0 };
    ctx.len = append_fmt(response, response_size, ctx.len, "*%d\r\n", hash->size * 2);
    ht_foreach(hash, hgetall_append, &ctx);
}

static void hdel_command(resp_object *args, hash_table *db, char *response, size_t response_size) {
    redis_object *redis_obj = lookup_typed(db, get_bulk_at(args, 1), OBJ_HASH, 0, ":0\r\n", response, response_size);
    if (redis_obj == NULL) return;

    hash_table *hash = redis_obj->ptr;
    int del = 0;
    for (int i = 2; i < args->array.len; i++) {
        if (ht_delete(hash, get_bulk_at(args, i))) del++;
    }
    snprintf(response, response_size, ":%d\r\n", del);
}

static void sadd_command(resp_object *args, hash_table *db, char *response, size_t response_size) {
    redis_object *redis_obj = lookup_typed(db, get_bulk_at(args, 1), OBJ_SET, 1, NULL, response, response_size);
    if (redis_obj == NULL) return;

    set *s = redis_obj->ptr;
    int count = 0;
    for (int i = 2; i < args->array.len; i++) {
        if (set_add(s, get_bulk_at(args, i))) count++;
    }
    snprintf(response, response_size, ":%d\r\n", count);
}

static void srem_command(resp_object *args, hash_table *db, char *response, size_t response_size) {
    redis_object *redis_obj = lookup_typed(db, get_bulk_at(args, 1), OBJ_SET, 0, ":0\r\n", response, response_size);
    if (redis_obj == NULL) return;

    set *s = redis_obj->ptr;
    int del = 0;
    for (int i = 2; i < args->array.len; i++) {
        if (set_remove(s, get_bulk_at(args, i))) del++;
    }
    snprintf(response, response_size, ":%d\r\n", del);
}

static void sismember_command(resp_object *args, hash_table *db, char *response, size_t response_size) {
    redis_object *redis_obj = lookup_typed(db, get_bulk_at(args, 1), OBJ_SET, 0, ":0\r\n", response, response_size);
    if (redis_obj == NULL) return;

    int is_member = set_contains(redis_obj->ptr, get_bulk_at(args, 2));
    snprintf(response, response_size, ":%d\r\n", is_member);
}

static void smembers_command(resp_object *args, hash_table *db, char *response, size_t response_size) {
    redis_object *redis_obj = lookup_typed(db, get_bulk_at(args, 1), OBJ_SET, 0, "*0\r\n", response, response_size);
    if (redis_obj == NULL) return;

    set *s = redis_obj->ptr;
    getall_ctx ctx = { .buf = response, .capacity = response_size, .len = 0 };
    ctx.len = append_fmt(response, response_size, ctx.len, "*%d\r\n", s->size);
    set_foreach(s, sgetall_append, &ctx);
}

static void zadd_command(resp_object *args, hash_table *db, char *response, size_t response_size) {
    int argc = args->array.len;
    if (argc % 2 != 0) {
        err_wrong_args(response, response_size, "zadd");
        return;
    }

    for (int i = 2; i < argc; i += 2) {
        double tmp;
        if (!parse_double(get_bulk_at(args, i), &tmp)) {
            snprintf(response, response_size, "-ERR value is not a valid float\r\n");
            return;
        }
    }

    redis_object *redis_obj = lookup_typed(db, get_bulk_at(args, 1), OBJ_ZSET, 1, NULL, response, response_size);
    if (redis_obj == NULL) return;

    zset *zs = redis_obj->ptr;
    int count = 0;
    for (int i = 2; i < argc; i += 2) {
        double score;
        parse_double(get_bulk_at(args, i), &score);
        if (zset_add(zs, score, get_bulk_at(args, i + 1))) count++;
    }
    snprintf(response, response_size, ":%d\r\n", count);
}

static void zrem_command(resp_object *args, hash_table *db, char *response, size_t response_size) {
    redis_object *redis_obj = lookup_typed(db, get_bulk_at(args, 1), OBJ_ZSET, 0, ":0\r\n", response, response_size);
    if (redis_obj == NULL) return;

    zset *zs = redis_obj->ptr;
    int count = 0;
    for (int i = 2; i < args->array.len; i++) {
        if (zset_rem(zs, get_bulk_at(args, i))) count++;
    }
    snprintf(response, response_size, ":%d\r\n", count);
}

static void zrangebyscore_command(resp_object *args, hash_table *db, char *response, size_t response_size) {
    redis_object *redis_obj = lookup_typed(db, get_bulk_at(args, 1), OBJ_ZSET, 0, "*0\r\n", response, response_size);
    if (redis_obj == NULL) return;

    double min, max;
    if (!parse_double(get_bulk_at(args, 2), &min) || !parse_double(get_bulk_at(args, 3), &max)) {
        snprintf(response, response_size, "-ERR min or max is not a float\r\n");
        return;
    }

    zset *zs = redis_obj->ptr;
    int count = zset_range_by_score(zs, min, max, zrange_count, NULL);

    getall_ctx ctx = { .buf = response, .capacity = response_size, .len = 0 };
    ctx.len = append_fmt(response, response_size, ctx.len, "*%d\r\n", count);
    zset_range_by_score(zs, min, max, zrange_append, &ctx);
}

static void zscore_command(resp_object *args, hash_table *db, char *response, size_t response_size) {
    redis_object *redis_obj = lookup_typed(db, get_bulk_at(args, 1), OBJ_ZSET, 0, "$-1\r\n", response, response_size);
    if (redis_obj == NULL) return;

    double score;
    if (!zset_find(redis_obj->ptr, get_bulk_at(args, 2), &score)) {
        snprintf(response, response_size, "$-1\r\n");
        return;
    }
    char buf[32];
    int len = snprintf(buf, sizeof(buf), "%.17g", score);
    snprintf(response, response_size, "$%d\r\n%s\r\n", len, buf);
}

static void list_push(resp_object *args, hash_table *db, char *response, size_t response_size,
                      quicklist_push_fn fn, int create_if_missing) {
    redis_object *redis_obj = lookup_typed(db, get_bulk_at(args, 1), OBJ_LIST, create_if_missing,
                                           "-The list doesnt exist\r\n", response, response_size);
    if (redis_obj == NULL) return;

    ql *ql = redis_obj->ptr;
    int count = 0;
    for (int i = 2; i < args->array.len; i++) {
        if (fn(ql, get_bulk_at(args, i))) count++;
    }
    snprintf(response, response_size, ":%d\r\n", count);
}

static void list_pop(resp_object *args, hash_table *db, char *response, size_t response_size, quicklist_pop_fn fn) {
    redis_object *redis_obj = lookup_typed(db, get_bulk_at(args, 1), OBJ_LIST, 0, ":0\r\n", response, response_size);
    if (redis_obj == NULL) return;

    ql *ql = redis_obj->ptr;
    int count = (int) strtoll(get_bulk_at(args, 2), NULL, 10);
    int deleted = 0;
    for (int i = 0; i < count; i++) {
        if (fn(ql)) deleted++;
    }
    snprintf(response, response_size, ":%d\r\n", deleted);
}

static void lpush_command(resp_object *args, hash_table *db, char *response, size_t response_size) {
    list_push(args, db, response, response_size, quicklist_push_head, 1);
}

static void rpush_command(resp_object *args, hash_table *db, char *response, size_t response_size) {
    list_push(args, db, response, response_size, quicklist_push_tail, 1);
}

static void lpushx_command(resp_object *args, hash_table *db, char *response, size_t response_size) {
    list_push(args, db, response, response_size, quicklist_push_head, 0);
}

static void rpushx_command(resp_object *args, hash_table *db, char *response, size_t response_size) {
    list_push(args, db, response, response_size, quicklist_push_tail, 0);
}

static void lpop_command(resp_object *args, hash_table *db, char *response, size_t response_size) {
    list_pop(args, db, response, response_size, quicklist_pop_head);
}

static void rpop_command(resp_object *args, hash_table *db, char *response, size_t response_size) {
    list_pop(args, db, response, response_size, quicklist_pop_tail);
}

static void linsert_command(resp_object *args, hash_table *db, char *response, size_t response_size) {
    redis_object *redis_obj = lookup_typed(db, get_bulk_at(args, 1), OBJ_LIST, 0, ":0\r\n", response, response_size);
    if (redis_obj == NULL) return;

    ql *ql = redis_obj->ptr;
    int before = strcasecmp(get_bulk_at(args, 2), "BEFORE") == 0;
    sds pivot = get_bulk_at(args, 3);
    sds value = get_bulk_at(args, 4);

    if (!quicklist_insert(ql, pivot, sdslen(pivot), value, before)) {
        snprintf(response, response_size, ":-1\r\n");
    } else {
        snprintf(response, response_size, ":%lu\r\n", ql->count);
    }
}

static void llen_command(resp_object *args, hash_table *db, char *response, size_t response_size) {
    redis_object *redis_obj = lookup_typed(db, get_bulk_at(args, 1), OBJ_LIST, 0, ":0\r\n", response, response_size);
    if (redis_obj == NULL) return;

    snprintf(response, response_size, ":%lu\r\n", ((ql *) redis_obj->ptr)->count);
}

static void lrem_command(resp_object *args, hash_table *db, char *response, size_t response_size) {
    redis_object *redis_obj = lookup_typed(db, get_bulk_at(args, 1), OBJ_LIST, 0, ":0\r\n", response, response_size);
    if (redis_obj == NULL) return;

    sds value = get_bulk_at(args, 2);
    int count = (int) strtoll(get_bulk_at(args, 3), NULL, 10);
    int del = quicklist_remove(redis_obj->ptr, value, count);
    snprintf(response, response_size, ":%d\r\n", del);
}

static void lrange_command(resp_object *args, hash_table *db, char *response, size_t response_size) {
    redis_object *redis_obj = lookup_typed(db, get_bulk_at(args, 1), OBJ_LIST, 0, "*0\r\n", response, response_size);
    if (redis_obj == NULL) return;

    int start = (int) strtoll(get_bulk_at(args, 2), NULL, 10);
    int stop = (int) strtoll(get_bulk_at(args, 3), NULL, 10);
    ql *ql = redis_obj->ptr;

    int count = quicklist_range(ql, start, stop, lrange_count, NULL);

    getall_ctx ctx = { .buf = response, .capacity = response_size, .len = 0 };
    ctx.len = append_fmt(response, response_size, ctx.len, "*%d\r\n", count);
    quicklist_range(ql, start, stop, lrange_append, &ctx);
}

static void lindex_command(resp_object *args, hash_table *db, char *response, size_t response_size) {
    redis_object *redis_obj = lookup_typed(db, get_bulk_at(args, 1), OBJ_LIST, 0, "$-1\r\n", response, response_size);
    if (redis_obj == NULL) return;

    int index = (int) strtoll(get_bulk_at(args, 2), NULL, 10);
    struct zlentry *entry = quicklist_get_at(redis_obj->ptr, index);
    if (entry == NULL) {
        snprintf(response, response_size, "-Index out of bounds\r\n");
        return;
    }
    snprintf(response, response_size, "$%u\r\n%.*s\r\n", entry->currlen, (int) entry->currlen, entry->data);
}

static const command commands[] = {
    {"PING",          -1, ping_command},
    {"ECHO",           2, echo_command},

    {"EXISTS",        -2, exists_command},
    {"DEL",           -2, del_command},
    {"EXPIRE",         3, expire_command},
    {"TTL",            2, ttl_command},

    {"SET",            3, set_command},
    {"MSET",          -3, mset_command},
    {"GET",            2, get_command},
    {"MGET",          -2, mget_command},
    {"INCR",           2, incr_command},
    {"DECR",           2, decr_command},
    {"INCRBY",         3, incrby_command},
    {"APPEND",         3, append_command},

    {"HSET",          -4, hset_command},
    {"HGET",           3, hget_command},
    {"HGETALL",        2, hgetall_command},
    {"HDEL",          -3, hdel_command},

    {"SADD",          -3, sadd_command},
    {"SREM",          -3, srem_command},
    {"SISMEMBER",      3, sismember_command},
    {"SMEMBERS",       2, smembers_command},

    {"ZADD",          -4, zadd_command},
    {"ZREM",          -3, zrem_command},
    {"ZRANGEBYSCORE",  4, zrangebyscore_command},
    {"ZSCORE",         3, zscore_command},

    {"LPUSH",         -3, lpush_command},
    {"RPUSH",         -3, rpush_command},
    {"LPUSHX",        -3, lpushx_command},
    {"RPUSHX",        -3, rpushx_command},
    {"LPOP",           3, lpop_command},
    {"RPOP",           3, rpop_command},
    {"LINSERT",        5, linsert_command},
    {"LLEN",           2, llen_command},
    {"LREM",           4, lrem_command},
    {"LRANGE",         4, lrange_command},
    {"LINDEX",         3, lindex_command},
};

static const size_t ncmd = sizeof(commands) / sizeof(commands[0]);

static const command *lookup_command(sds name) {
    for (size_t i = 0; i < ncmd; i++) {
        if (strcasecmp(name, commands[i].name) == 0) {
            return &commands[i];
        }
    }
    return NULL;
}

static int validate_cmd_args(const resp_object *resp_obj) {
    for (int i = 0; i < resp_obj->array.len; i++) {
        const resp_object *arg = resp_obj->array.ptr[i];
        if (arg == NULL || arg->type != RESP_BULK || arg->bulk == NULL) return 0;
    }
    return 1;
}

void execute_command(resp_object *args, hash_table *db, char *response, size_t response_size) {
    response[0] = '\0';
    if (args->type != RESP_ARRAY || args->array.len == 0) return;
    if (!validate_cmd_args(args)) {
        snprintf(response, response_size, "-ERR Protocol error: expected bulk string arguments\r\n");
        return;
    }

    sds name = get_bulk_at(args, 0);
    const command *cmd = lookup_command(name);
    if (cmd == NULL) {
        snprintf(response, response_size, "-ERR unknown command '%s'\r\n", name);
        return;
    }

    int argc = args->array.len;
    if ((cmd->arity > 0 && argc != cmd->arity) || (cmd->arity < 0 && argc < -cmd->arity)) {
        err_wrong_args(response, response_size, cmd->name);
        return;
    }
    cmd->fun(args, db, response, response_size);
}
