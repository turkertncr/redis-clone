#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <command_handler.h>
#include <redis_object.h>
#include <data_types/set.h>
#include <data_types/sorted_set.h>
#include <data_types/quick_list.h>
#include <aof_handler.h>
#include <util.h>

#if defined(_WIN32) || defined(_WIN64)
#define strcasecmp _stricmp
#else
#include <strings.h>
#endif

static sds required_password = NULL;

static void append_bulk(sds *out, const redis_object *redis_obj) {
    if (redis_obj == NULL || redis_obj->type != OBJ_STRING) {
        append_fmt(out, "$-1\r\n");
        return;
    }
    append_b(out, (sds) redis_obj->ptr);
}

static void err_wrong_args(sds *out, const char *cmd_name) {
    append_fmt(out, "-ERR wrong number of arguments for '%s' command\r\n", cmd_name);
}

static void err_wrongtype(sds *out) {
    append_fmt(out, "-WRONGTYPE Operation against a key holding the wrong kind of value\r\n");
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
                                  const char *missing_reply, sds *out) {
    redis_object *result = lookup_redis_object(db, key);
    if (result == NULL) {
        if (!create_if_missing) {
           append_fmt(out, "%s", missing_reply);
            return NULL;
        }
        result = create_redis_object(type, NULL, -1);
        if (result == NULL) {
            append_fmt(out, "-ERR out of memory\r\n");
            return NULL;
        }
        ht_put(db, key, result);
        return result;
    }
    if (result->type != type) {
        err_wrongtype(out);
        return NULL;
    }
    return result;
}

static void hgetall_append(sds field, void *value, void *user_data) {
    getall_ctx *ctx = (getall_ctx *) user_data;
    sds field_value = (sds) value;
    append_fmt(&ctx->out,"$%u\r\n%s\r\n$%u\r\n%s\r\n", sdslen(field), field, sdslen(field_value), field_value);
}

static void sgetall_append(sds member, void *value, void *user_data) {
    (void) value;
    getall_ctx *ctx = (getall_ctx *) user_data;
    append_fmt(&ctx->out,"$%u\r\n%s\r\n", sdslen(member), member);
}

static void zrange_append(skiplist_node *node, void *user_data) {
    getall_ctx *ctx = (getall_ctx *) user_data;
    append_fmt(&ctx->out,"$%u\r\n%s\r\n", sdslen(node->member), node->member);
}

static void zrange_count(skiplist_node *node, void *user_data) {
    (void) node;
    (void) user_data;
}

static void lrange_count(struct zlentry *entry, void *user_data) {
    (void) entry;
    (void) user_data;
}

static void lrange_append(struct zlentry *entry, void *user_data) {
    getall_ctx *ctx = (getall_ctx *) user_data;
    append_fmt(&ctx->out, "$%u\r\n%.*s\r\n", entry->currlen, entry->currlen, entry->data);
}

static void ping_command(resp_object *args, hash_table *db, sds *out) {
    (void) db;
    if (args->array.len >= 2) {
        sds msg = get_bulk_at(args, 1);
        append_fmt(out, "$%u\r\n%s\r\n", sdslen(msg), msg);
    } else {
        append_fmt(out, "+PONG\r\n");
    }
}

static void echo_command(resp_object *args, hash_table *db, sds *out) {
    (void) db;
    sds msg = get_bulk_at(args, 1);
    append_fmt(out, "$%u\r\n%s\r\n", sdslen(msg), msg);
}

static void exists_command(resp_object *args, hash_table *db, sds *out) {
    int count = 0;
    for (int i = 1; i < args->array.len; i++) {
        if (lookup_redis_object(db, get_bulk_at(args, i)) != NULL) count++;
    }
    append_fmt(out, ":%d\r\n", count);
}

static void del_command(resp_object *args, hash_table *db, sds *out) {
    int deleted = 0;
    for (int i = 1; i < args->array.len; i++) {
        deleted += ht_delete(db, get_bulk_at(args, i));
    }
    append_fmt(out, ":%d\r\n", deleted);
}

static void expire_command(resp_object *args, hash_table *db, sds *out) {
    redis_object *redis_obj = lookup_redis_object(db, get_bulk_at(args, 1));
    if (redis_obj == NULL) {
        append_fmt(out, ":0\r\n");
        return;
    }
    long long sec = strtoll(get_bulk_at(args, 2), NULL, 10);
    redis_obj->expires_at = current_time_ms() + sec * 1000;
    append_fmt(out, ":1\r\n");
}

static void ttl_command(resp_object *args, hash_table *db, sds *out) {
    redis_object *redis_obj = lookup_redis_object(db, get_bulk_at(args, 1));
    if (redis_obj == NULL) {
        append_fmt(out, ":-2\r\n");
        return;
    }
    if (redis_obj->expires_at == -1) {
        append_fmt(out, ":-1\r\n");
        return;
    }
    long long ttl = (redis_obj->expires_at - current_time_ms()) / 1000;
    append_fmt(out, ":%lld\r\n", ttl);
}

static void pexpireat_command(resp_object *args, hash_table *db, sds *out) {
    redis_object *redis_obj = lookup_redis_object(db, get_bulk_at(args, 1));
    if (redis_obj == NULL) {
        append_fmt(out, ":0\r\n");
        return;
    }
    long long timestamp = strtoll(get_bulk_at(args, 2), NULL, 10);
    redis_obj->expires_at = timestamp;
    append_fmt(out, ":1\r\n");
}

static int parse_set_options(resp_object *args, sds *out, long long *ttl, int *flags) {
    *flags = 0;
    long long expire_ms = -1;

    for (int i = 3; i < args->array.len; i++) {
        sds tok = get_bulk_at(args, i);

        if (strcasecmp(tok, "NX") == 0) {
            if (*flags & SET_EXISTENCE_MASK) goto syntax_err;
            *flags |= SET_NX;
        } else if (strcasecmp(tok, "XX") == 0) {
            if (*flags & SET_EXISTENCE_MASK) goto syntax_err;
            *flags |= SET_XX;
        } else if (strcasecmp(tok, "GET") == 0) {
            *flags |= SET_GET;
        } else if (strcasecmp(tok, "KEEPTTL") == 0) {
            if (*flags & SET_EXPIRE_MASK) goto syntax_err;
            *flags |= SET_KEEPTTL;
        } else if (strcasecmp(tok, "EX") == 0) {
            if (*flags & SET_EXPIRE_MASK) goto syntax_err;
            sds val = get_bulk_at(args, ++i);
            if (val == NULL) { return 0; }
            expire_ms = current_time_ms() + strtoll(val, NULL, 10) * 1000;
        } else if (strcasecmp(tok, "PX") == 0) {
            if (*flags & SET_EXPIRE_MASK) goto syntax_err;
            sds val = get_bulk_at(args, ++i);
            if (val == NULL) { return 0;}
            expire_ms = current_time_ms() + strtoll(val, NULL, 10);
        } else {
            goto syntax_err;
        }
    }

    *ttl = expire_ms;
    return 1;

    syntax_err:
        append_fmt(out, "-ERR syntax error\r\n");
        return 0;
}

static void set_command(resp_object *args, hash_table *db, sds *out) {
    long long ttl;
    int flags;
    if (!parse_set_options(args, out, &ttl, &flags)) return;

    sds key = get_bulk_at(args, 1);
    sds value = get_bulk_at(args, 2);

    redis_object *existing = lookup_redis_object(db, key);
    sds old_value = (existing != NULL) ? sdsdup(existing->ptr) : NULL;

    int write = 1;
    if (flags & SET_EXISTENCE_MASK)
        write = ((flags & SET_NX) && existing == NULL) || ((flags & SET_XX) && existing != NULL);

    if (write) {
        if ((flags & SET_KEEPTTL) && existing != NULL) ttl = existing->expires_at;
        redis_object *redis_obj = create_redis_object(OBJ_STRING, value, ttl);
        ht_put(db, key, redis_obj);
    }

    if (flags & SET_GET) {
        if (old_value != NULL) append_b(out, old_value);
        else append_fmt(out, "$-1\r\n");
    } else if (write) append_fmt(out, "+OK\r\n");
    else append_fmt(out, "$-1\r\n");

    sdsfree(old_value);
}

static void mset_command(resp_object *args, hash_table *db, sds *out) {
    int argc = args->array.len;
    if (argc % 2 == 0) {
        err_wrong_args(out, "mset");
        return;
    }

    for (int i = 1; i < argc; i += 2) {
        redis_object *redis_obj = create_redis_object(OBJ_STRING, get_bulk_at(args, i + 1), -1);
        ht_put(db, get_bulk_at(args, i), redis_obj);
    }
    append_fmt(out, "+OK\r\n");
}

static void get_command(resp_object *args, hash_table *db, sds *out) {
    redis_object *redis_obj = lookup_typed(db, get_bulk_at(args, 1), OBJ_STRING, 0, "$-1\r\n", out);
    if (redis_obj == NULL) return;
    append_bulk(out, redis_obj);
}

static void mget_command(resp_object *args, hash_table *db, sds *out) {
    append_fmt(out, "*%d\r\n", args->array.len - 1);
    for (int i = 1; i < args->array.len; i++) {
        redis_object *redis_obj = lookup_redis_object(db, get_bulk_at(args, i));
        append_bulk(out, redis_obj);
    }
}

static void incr_decr(hash_table *db, sds key, long long delta, sds *out) {
    redis_object *redis_obj = lookup_redis_object(db, key);
    if (redis_obj != NULL && redis_obj->type != OBJ_STRING) {
        append_fmt(out, "-WRONGTYPE value is not a string\r\n");
        return;
    }

    long long current = 0;
    if (redis_obj != NULL) {
        char *end;
        current = strtoll(redis_obj->ptr, &end, 10);
        if (end == (sds) redis_obj->ptr || *end != '\0') {
            append_fmt(out, "-ERR value is not an integer or out of range\r\n");
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
        sdsfree(redis_obj->ptr);
        redis_obj->ptr = sdsnew(buf);
    }

    append_fmt(out, ":%lld\r\n", new_value);
}

static void incr_command(resp_object *args, hash_table *db, sds *out) {
    incr_decr(db, get_bulk_at(args, 1), 1, out);
}

static void decr_command(resp_object *args, hash_table *db, sds *out) {
    incr_decr(db, get_bulk_at(args, 1), -1, out);
}

static void incrby_command(resp_object *args, hash_table *db, sds *out) {
    long long delta = strtoll(get_bulk_at(args, 2), NULL, 10);
    incr_decr(db, get_bulk_at(args, 1), delta, out);
}

static void append_command(resp_object *args, hash_table *db, sds *out) {
    sds key = get_bulk_at(args, 1);
    sds value = get_bulk_at(args, 2);

    redis_object *redis_obj = lookup_redis_object(db, key);
    if (redis_obj == NULL) {
        redis_obj = create_redis_object(OBJ_STRING, value, -1);
        ht_put(db, key, redis_obj);
        append_fmt(out, ":%u\r\n", sdslen((sds) redis_obj->ptr));
        return;
    }
    if (redis_obj->type != OBJ_STRING) {
        err_wrongtype(out);
        return;
    }

    sds existing_value = redis_obj->ptr;
    sds new_value = sdscat(existing_value, value);
    redis_obj->ptr = new_value;
    sdsfree(existing_value);

    append_fmt(out, ":%u\r\n", sdslen(new_value));
}

static void hset_command(resp_object *args, hash_table *db, sds *out) {
    int argc = args->array.len;
    if (argc % 2 != 0) {
        err_wrong_args(out, "hset");
        return;
    }

    redis_object *redis_obj = lookup_typed(db, get_bulk_at(args, 1), OBJ_HASH, 1, NULL, out);
    if (redis_obj == NULL) return;

    hash_table *hash = redis_obj->ptr;
    int added = 0;
    for (int i = 2; i < argc; i += 2) {
        sds field = get_bulk_at(args, i);
        if (ht_put(hash, field, sdsdup(get_bulk_at(args, i + 1))) == 2) added++;
    }
    append_fmt(out, ":%d\r\n", added);
}

static void hget_command(resp_object *args, hash_table *db, sds *out) {
    redis_object *redis_obj = lookup_typed(db, get_bulk_at(args, 1), OBJ_HASH, 0, "$-1\r\n", out);
    if (redis_obj == NULL) return;

    sds field_value = ht_get(redis_obj->ptr, get_bulk_at(args, 2));
    append_b(out, field_value);
}

static void hgetall_command(resp_object *args, hash_table *db, sds *out) {
    redis_object *redis_obj = lookup_typed(db, get_bulk_at(args, 1), OBJ_HASH, 0, "*0\r\n", out);
    if (redis_obj == NULL) return;

    hash_table *hash = redis_obj->ptr;
    getall_ctx ctx = { .out = *out };
    append_fmt(&ctx.out, "*%d\r\n", hash->size * 2);
    ht_foreach(hash, hgetall_append, &ctx);
    *out = ctx.out;
}

static void hdel_command(resp_object *args, hash_table *db, sds *out) {
    sds key = get_bulk_at(args, 1);
    redis_object *redis_obj = lookup_typed(db, key, OBJ_HASH, 0, ":0\r\n", out);
    if (redis_obj == NULL) return;

    hash_table *hash = redis_obj->ptr;
    int del = 0;
    for (int i = 2; i < args->array.len; i++) {
        if (ht_delete(hash, get_bulk_at(args, i))) del++;
    }
    append_fmt(out, ":%d\r\n", del);
    if (hash->size == 0) ht_delete(db, key);
}

static void sadd_command(resp_object *args, hash_table *db, sds *out) {
    redis_object *redis_obj = lookup_typed(db, get_bulk_at(args, 1), OBJ_SET, 1, NULL, out);
    if (redis_obj == NULL) return;

    set *s = redis_obj->ptr;
    int count = 0;
    for (int i = 2; i < args->array.len; i++) {
        if (set_add(s, get_bulk_at(args, i))) count++;
    }
    append_fmt(out, ":%d\r\n", count);
}

static void srem_command(resp_object *args, hash_table *db, sds *out) {
    sds key = get_bulk_at(args, 1);
    redis_object *redis_obj = lookup_typed(db, key, OBJ_SET, 0, ":0\r\n", out);
    if (redis_obj == NULL) return;

    set *s = redis_obj->ptr;
    int del = 0;
    for (int i = 2; i < args->array.len; i++) {
        if (set_remove(s, get_bulk_at(args, i))) del++;
    }
    append_fmt(out, ":%d\r\n", del);

    if (s->size == 0) ht_delete(db, key);
}

static void sismember_command(resp_object *args, hash_table *db, sds *out) {
    redis_object *redis_obj = lookup_typed(db, get_bulk_at(args, 1), OBJ_SET, 0, ":0\r\n", out);
    if (redis_obj == NULL) return;

    int is_member = set_contains(redis_obj->ptr, get_bulk_at(args, 2));
    append_fmt(out, ":%d\r\n", is_member);
}

static void smembers_command(resp_object *args, hash_table *db, sds *out) {
    redis_object *redis_obj = lookup_typed(db, get_bulk_at(args, 1), OBJ_SET, 0, "*0\r\n", out);
    if (redis_obj == NULL) return;

    set *s = redis_obj->ptr;
    getall_ctx ctx = { .out = *out };
    append_fmt(&ctx.out, "*%d\r\n", s->size);
    set_foreach(s, sgetall_append, &ctx);
    *out = ctx.out;
}

/** - ZADD key [NX | XX] [GT | LT] [CH] [INCR] score member [score member ...]
    - NX/XX — only add new / only update existing
    - GT/LT — only update if new score greater/less than current
    - CH — return count of changed elements (added + updated) instead of just
      added
    - INCR — increment the member's score instead of setting it (like ZINCRBY);
      only works with a single score/member pair
*/
static void zadd_command(resp_object *args, hash_table *db, sds *out) {
    int argc = args->array.len;
    if (argc % 2 != 0) {
        err_wrong_args(out, "zadd");
        return;
    }

    for (int i = 2; i < argc; i += 2) {
        double tmp;
        if (!parse_double(get_bulk_at(args, i), &tmp)) {
            append_fmt(out, "-ERR value is not a valid float\r\n");
            return;
        }
    }

    redis_object *redis_obj = lookup_typed(db, get_bulk_at(args, 1), OBJ_ZSET, 1, NULL, out);
    if (redis_obj == NULL) return;

    zset *zs = redis_obj->ptr;
    int count = 0;
    for (int i = 2; i < argc; i += 2) {
        double score;
        parse_double(get_bulk_at(args, i), &score);
        if (zset_add(zs, score, get_bulk_at(args, i + 1))) count++;
    }
    append_fmt(out, ":%d\r\n", count);
}

static void zrem_command(resp_object *args, hash_table *db, sds *out) {
    sds key = get_bulk_at(args, 1);
    redis_object *redis_obj = lookup_typed(db, get_bulk_at(args, 1), OBJ_ZSET, 0, ":0\r\n", out);
    if (redis_obj == NULL) return;

    zset *zs = redis_obj->ptr;
    int count = 0;
    for (int i = 2; i < args->array.len; i++) {
        if (zset_rem(zs, get_bulk_at(args, i))) count++;
    }
    append_fmt(out, ":%d\r\n", count);

    if (zs->size == 0) {
        ht_delete(db, key);
    }
}

static void zcount_command(resp_object *args, hash_table *db, sds *out) {
    sds key = get_bulk_at(args, 1);
    redis_object *redis_obj = lookup_typed(db, key, OBJ_ZSET, 0, ":0\r\n", out);
    if (redis_obj == NULL) return;

    zset *zs = redis_obj->ptr;
    append_fmt(out, ":%d\r\n", zs->size);
}

static void zrangebyscore_command(resp_object *args, hash_table *db, sds *out) {
    redis_object *redis_obj = lookup_typed(db, get_bulk_at(args, 1), OBJ_ZSET, 0, "*0\r\n", out);
    if (redis_obj == NULL) return;

    double min, max;
    if (!parse_double(get_bulk_at(args, 2), &min) || !parse_double(get_bulk_at(args, 3), &max)) {
        append_fmt(out, "-ERR min or max is not a float\r\n");
        return;
    }

    zset *zs = redis_obj->ptr;
    int count = zset_range_by_score(zs, min, max, zrange_count, NULL);

    getall_ctx ctx = { .out = *out };
    append_fmt(&ctx.out, "*%d\r\n", count);
    zset_range_by_score(zs, min, max, zrange_append, &ctx);
    *out = ctx.out;
}

static void zscore_command(resp_object *args, hash_table *db, sds *out) {
    redis_object *redis_obj = lookup_typed(db, get_bulk_at(args, 1), OBJ_ZSET, 0, "$-1\r\n", out);
    if (redis_obj == NULL) return;

    double score;
    if (!zset_find(redis_obj->ptr, get_bulk_at(args, 2), &score)) {
        append_fmt(out, "$-1\r\n");
        return;
    }
    char buf[32];
    int len = snprintf(buf, sizeof(buf), "%.17g", score);
    append_fmt(out, "$%d\r\n%s\r\n", len, buf);
}

static void list_push(resp_object *args, hash_table *db, sds *out,
                      quicklist_push_fn fn, int create_if_missing) {
    redis_object *redis_obj = lookup_typed(db, get_bulk_at(args, 1), OBJ_LIST, create_if_missing,
                                           "-The list doesnt exist\r\n", out);
    if (redis_obj == NULL) return;

    quicklist *ql = redis_obj->ptr;
    int count = 0;
    for (int i = 2; i < args->array.len; i++) {
        if (fn(ql, get_bulk_at(args, i))) count++;
    }
    append_fmt(out, ":%d\r\n", count);
}

static void list_pop(resp_object *args, hash_table *db, sds *out, quicklist_pop_fn fn) {
    sds key = get_bulk_at(args, 1);
    redis_object *redis_obj = lookup_typed(db, key, OBJ_LIST, 0, ":0\r\n", out);
    if (redis_obj == NULL) return;

    quicklist *ql = redis_obj->ptr;
    sds str = get_bulk_at(args, 2);
    int count = (str) ? (int) strtoll(str, NULL, 10) : 1;
    int deleted = 0;
    for (int i = 0; i < count; i++) {
        if (fn(ql)) deleted++;
    }
    append_fmt(out, ":%d\r\n", deleted);

    if (ql->count == 0) ht_delete(db, key);
}

static void lpush_command(resp_object *args, hash_table *db, sds *out) {
    list_push(args, db, out, quicklist_push_head, 1);
}

static void rpush_command(resp_object *args, hash_table *db, sds *out) {
    list_push(args, db, out, quicklist_push_tail, 1);
}

static void lpushx_command(resp_object *args, hash_table *db, sds *out) {
    list_push(args, db, out, quicklist_push_head, 0);
}

static void rpushx_command(resp_object *args, hash_table *db, sds *out) {
    list_push(args, db, out, quicklist_push_tail, 0);
}

static void lpop_command(resp_object *args, hash_table *db, sds *out) {
    list_pop(args, db, out, quicklist_pop_head);
}

static void rpop_command(resp_object *args, hash_table *db, sds *out) {
    list_pop(args, db, out, quicklist_pop_tail);
}

static void linsert_command(resp_object *args, hash_table *db, sds *out) {
    redis_object *redis_obj = lookup_typed(db, get_bulk_at(args, 1), OBJ_LIST, 0, ":0\r\n", out);
    if (redis_obj == NULL) return;

    quicklist *ql = redis_obj->ptr;
    int before = strcasecmp(get_bulk_at(args, 2), "BEFORE") == 0;
    sds pivot = get_bulk_at(args, 3);
    sds value = get_bulk_at(args, 4);

    if (!quicklist_insert(ql, pivot, sdslen(pivot), value, before)) {
        append_fmt(out, ":-1\r\n");
    } else {
        append_fmt(out, ":%lu\r\n", ql->count);
    }
}

static void llen_command(resp_object *args, hash_table *db, sds *out) {
    redis_object *redis_obj = lookup_typed(db, get_bulk_at(args, 1), OBJ_LIST, 0, ":0\r\n", out);
    if (redis_obj == NULL) return;

    append_fmt(out, ":%lu\r\n", ((quicklist *) redis_obj->ptr)->count);
}

static void lrem_command(resp_object *args, hash_table *db, sds *out) {
    sds key = get_bulk_at(args, 1);
    redis_object *redis_obj = lookup_typed(db, key, OBJ_LIST, 0, ":0\r\n", out);
    if (redis_obj == NULL) return;

    sds value = get_bulk_at(args, 2);
    quicklist *ql = redis_obj->ptr;
    int count = (int) strtoll(get_bulk_at(args, 3), NULL, 10);
    int del = quicklist_remove(ql, value, count);
    append_fmt(out, ":%d\r\n", del);

    if (ql->count == 0) ht_delete(db, key);
}

static void lrange_command(resp_object *args, hash_table *db, sds *out) {
    redis_object *redis_obj = lookup_typed(db, get_bulk_at(args, 1), OBJ_LIST, 0, "*0\r\n", out);
    if (redis_obj == NULL) return;

    int start = (int) strtoll(get_bulk_at(args, 2), NULL, 10);
    int stop = (int) strtoll(get_bulk_at(args, 3), NULL, 10);
    quicklist *ql = redis_obj->ptr;

    int count = quicklist_range(ql, start, stop, lrange_count, NULL);

    getall_ctx ctx = { .out = sdsempty() };
    append_fmt(&ctx.out, "*%d\r\n", count);
    quicklist_range(ql, start, stop, lrange_append, &ctx);
    *out = ctx.out;
}

static void lindex_command(resp_object *args, hash_table *db, sds *out) {
    redis_object *redis_obj = lookup_typed(db, get_bulk_at(args, 1), OBJ_LIST, 0, "$-1\r\n", out);
    if (redis_obj == NULL) return;

    int index = (int) strtoll(get_bulk_at(args, 2), NULL, 10);
    struct zlentry *entry = quicklist_get_at(redis_obj->ptr, index);
    if (entry == NULL) {
        append_fmt(out, "-Index out of bounds\r\n");
        return;
    }
    append_fmt(out, "$%u\r\n", entry->currlen);
    *out = sdsappendlen(*out, entry->data, entry->currlen);
    *out = sdsappendlen(*out, "\r\n", 2);
}

static command commands[] = {
    {"PING", -1, ping_command, CMD_NOAUTH},
    {"ECHO", 2, echo_command},

    {"EXISTS", -2, exists_command},
    {"DEL", -2, del_command, CMD_WRITE},
    {"EXPIRE", 3, expire_command, CMD_WRITE | CMD_TTL},
    {"TTL", 2, ttl_command},
    {"PEXPIREAT", 3, pexpireat_command, CMD_WRITE},

    {"SET", -3, set_command, CMD_WRITE | CMD_SET_TTL},
    {"MSET", -3, mset_command, CMD_WRITE},
    {"GET", 2, get_command},
    {"MGET", -2, mget_command},
    {"INCR", 2, incr_command, CMD_WRITE},
    {"DECR", 2, decr_command, CMD_WRITE},
    {"INCRBY", 3, incrby_command, CMD_WRITE},
    {"APPEND", 3, append_command, CMD_WRITE},

    {"HSET", -4, hset_command, CMD_WRITE},
    {"HGET", 3, hget_command},
    {"HGETALL", 2, hgetall_command},
    {"HDEL", -3, hdel_command, CMD_WRITE},

    {"SADD", -3, sadd_command, CMD_WRITE},
    {"SREM", -3, srem_command, CMD_WRITE},
    {"SISMEMBER", 3, sismember_command},
    {"SMEMBERS", 2, smembers_command},

    {"ZADD", -4, zadd_command, CMD_WRITE},
    {"ZREM", -3, zrem_command, CMD_WRITE},
    {"ZRANGEBYSCORE", 4, zrangebyscore_command},
    {"ZSCORE", 3, zscore_command},
    {"ZCOUNT", 2, zcount_command},

    {"LPUSH", -3, lpush_command, CMD_WRITE},
    {"RPUSH", -3, rpush_command, CMD_WRITE},
    {"LPUSHX", -3, lpushx_command, CMD_WRITE},
    {"RPUSHX", -3, rpushx_command, CMD_WRITE},
    {"LPOP", -2, lpop_command, CMD_WRITE},
    {"RPOP", -2, rpop_command, CMD_WRITE},
    {"LINSERT", 5, linsert_command, CMD_WRITE},
    {"LLEN", 2, llen_command},
    {"LREM", 4, lrem_command, CMD_WRITE},
    {"LRANGE", 4, lrange_command},
    {"LINDEX", 3, lindex_command},
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

void set_required_password(const char *password) {
    sdsfree(required_password);
    required_password = (password != NULL && password[0] != '\0') ? sdsnew(password) : NULL;
}

int auth_required(void) {
    return required_password != NULL;
}

static int secure_eq(sds a, sds b) {
    uint32_t alen = sdslen(a), blen = sdslen(b);
    unsigned char diff = alen != blen;
    for (uint32_t i = 0; i < alen && i < blen; i++) diff |= (unsigned char) (a[i] ^ b[i]);
    return diff == 0;
}

static void auth_command(resp_object *args, client_ctx *client, sds *out) {
    int argc = args->array.len;
    if (argc != 2 && argc != 3) {
        err_wrong_args(out, "auth");
        return;
    }
    if (required_password == NULL) {
        append_fmt(out,
            "-ERR AUTH <password> called without any password configured for the default user. "
            "Are you sure your configuration is correct?\r\n");
        return;
    }

    sds password = get_bulk_at(args, argc - 1);
    int user_ok = argc == 2 || strcmp(get_bulk_at(args, 1), "default") == 0;
    if (user_ok && secure_eq(password, required_password)) {
        client->authenticated = 1;
        append_fmt(out, "+OK\r\n");
    } else {
        append_fmt(out, "-WRONGPASS invalid username-password pair or user is disabled.\r\n");
    }
}

void execute(resp_object *args, hash_table *db, sds *out) {
    const command *cmd = lookup_command(get_bulk_at(args, 0));
    if (cmd == NULL) return;
    cmd->fun(args, db, out);
}

const command* execute_command(resp_object *args, hash_table *db, client_ctx *client, sds *out) {
    if (args->type != RESP_ARRAY || args->array.len == 0) return NULL;
    if (!validate_cmd_args(args)) {
        append_fmt(out, "-ERR Protocol error: expected bulk string arguments\r\n");
        return NULL;
    }

    sds name = get_bulk_at(args, 0);
    if (strcasecmp(name, "AUTH") == 0) {
        auth_command(args, client, out);
        return NULL;
    }

    const command *cmd = lookup_command(name);
    if (required_password != NULL && !client->authenticated && (cmd == NULL || !(cmd->flags & CMD_NOAUTH))) {
        append_fmt(out, "-NOAUTH Authentication required.\r\n");
        return NULL;
    }
    if (cmd == NULL) {
        append_fmt(out, "-ERR unknown command '%s'\r\n", name);
        return NULL;
    }

    int argc = args->array.len;
    if ((cmd->argc > 0 && argc != cmd->argc) || (cmd->argc < 0 && argc < -cmd->argc)) {
        err_wrong_args(out, cmd->name);
        return NULL;
    }

    cmd->fun(args, db, out);
    return cmd;
}
