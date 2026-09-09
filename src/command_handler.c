#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <include/command_handler.h>
#include <include/redis_object.h>
#include <include/data_types/set.h>
#include <include/data_types/sorted_set.h>

#define STRCMP(str1, str2) strcmp(str1, str2) == 0

static sds toupper_case(sds str) {
    if (str == NULL) return NULL;

    size_t len = sdslen(str);
    for (size_t i = 0; i < len; i++) {
        str[i] = (char) toupper((unsigned char) str[i]);
    }
    return str;
}

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

static void err_wrong_args(char *response, size_t response_size, const char *cmd_name) {
    snprintf(response, response_size, "-ERR wrong number of arguments for '%s' command\r\n", cmd_name);
}

static void err_wrongtype(char *response, size_t response_size) {
    snprintf(response, response_size, "-WRONGTYPE Operation against a key holding the wrong kind of value\r\n");
}

size_t append_b(char *buf, size_t capacity, size_t len, sds val) {
    if (val == NULL) return append_fmt(buf, capacity, len, "$-1\r\n");;
    return append_fmt(buf, capacity, len, "$%u\r\n%s\r\n", sdslen(val), val);
}

static size_t append_bulk(char *buf, size_t capacity, size_t len, const redis_object *redis_obj) {
    if (redis_obj == NULL || redis_obj->type != OBJ_STRING) {
        return append_fmt(buf, capacity, len, "$-1\r\n");
    }
    sds value = (sds) redis_obj->ptr;
    return append_b(buf, capacity, len, value);
}

static int check_args_len(int min, const resp_object* resp_obj, const char* cmd, char *response, size_t response_size) {
    if (resp_obj->array.len < min) {
        err_wrong_args(response, response_size, cmd);
        return 0;
    }
    return 1;
}

static void incr_decr(hash_table *ht, sds key, long long delta, char *response, size_t response_size) {
    redis_object *redis_obj = (redis_object *) ht_get(ht, key);

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
        redis_obj = create_redis_object(OBJ_STRING, buf, -1);
        ht_put(ht, key, redis_obj);
    } else {
        sdsfree((sds) redis_obj->ptr);
        redis_obj->ptr = sdsnew(buf);
    }

    snprintf(response, response_size, ":%lld\r\n", new_value);
}

static void hgetall_append(sds field, void *value, void *user_data) {
    getall_ctx *ctx = (getall_ctx *) user_data;
    sds field_value = (sds) value;
    ctx->len = append_fmt(ctx->buf, ctx->capacity, ctx->len,
        "$%u\r\n%s\r\n$%u\r\n%s\r\n", sdslen(field), field, sdslen(field_value), field_value);
}

static void sgetall_append(sds member, void* value, void *user_data) {
    getall_ctx *ctx = (getall_ctx *) user_data;
    ctx->len = append_fmt(ctx->buf, ctx->capacity, ctx->len,
        "$%u\r\n%s\r\n", sdslen(member), member);
}

static void zrange_append(sl_node* node, void *user_data) {
    getall_ctx *ctx = (getall_ctx *) user_data;
    ctx->len = append_fmt(ctx->buf, ctx->capacity, ctx->len,
        "$%u\r\n%s\r\n", sdslen(node->member), node->member);
}

static void zrange_count(sl_node* node, void *user_data) {
    (void) node;
    (void) user_data;
}

void execute_command(resp_object *resp_obj, hash_table *ht, char *response, size_t response_size) {
    response[0] = '\0';

    if (resp_obj->type != RESP_ARRAY || resp_obj->array.len == 0) {
        return;
    }

    sds cmd = toupper_case(get_bulk_at(resp_obj, 0));

    if (handle_connection_commands(cmd, resp_obj, response, response_size)) return;
    if (handle_key_commands(cmd, resp_obj, ht, response, response_size)) return;
    if (handle_string_commands(cmd, resp_obj, ht, response, response_size)) return;
    if (handle_hash_commands(cmd, resp_obj, ht, response, response_size)) return;
    if (handle_set_commands(cmd, resp_obj, ht, response, response_size)) return;
    if (handle_zset_commands(cmd, resp_obj, ht, response, response_size)) return;

    snprintf(response, response_size, "-ERR unknown command '%s'\r\n", cmd == NULL ? "" : cmd);
}

int handle_connection_commands(sds cmd, resp_object *resp_obj, char *response, size_t response_size) {
    if (STRCMP(cmd, "PING")) {
        if (check_args_len(2, resp_obj, "PING", response, response_size)) {
            sds msg = get_bulk_at(resp_obj, 1);
            snprintf(response, response_size, "$%d\r\n%s\r\n", sdslen(msg), msg);
        } else {
            snprintf(response, response_size, "+PONG\r\n");
        }
        return 1;
    }

    if (STRCMP(cmd, "ECHO")) {
        if (!check_args_len(2, resp_obj, "ECHO", response, response_size)) return 1;
        sds msg = get_bulk_at(resp_obj, 1);
        snprintf(response, response_size, "$%d\r\n%s\r\n", sdslen(msg), msg);
        return 1;
    }

    return 0;
}

int handle_key_commands(sds cmd, resp_object *resp_obj, hash_table *ht, char *response, size_t response_size) {

    if (STRCMP(cmd, "EXISTS")) {
        if (!check_args_len(2, resp_obj, "exists", response, response_size)) return 1;
        int count = 0;
        for (int i = 1; i < resp_obj->array.len; i++) {
            sds key_at = get_bulk_at(resp_obj, i);
            if (lookup_redis_object(ht, key_at) != NULL) count++;
        }
        snprintf(response, response_size, ":%d\r\n", count);
        return 1;
    }

    if (STRCMP(cmd, "DEL")) {
        if (!check_args_len(2, resp_obj, "del", response, response_size)) return 1;
        int deleted = 0;
        for (int i = 1; i < resp_obj->array.len; i++) {
            sds key_at = get_bulk_at(resp_obj, i);
            deleted += ht_delete(ht, key_at);
        }
        snprintf(response, response_size, ":%d\r\n", deleted);
        return 1;
    }

    if (STRCMP(cmd, "EXPIRE")) {
        if (!check_args_len(3, resp_obj, "expire", response, response_size)) return 1;
        sds key = get_bulk_at(resp_obj, 1);
        redis_object *redis_obj = lookup_redis_object(ht, key);
        if (redis_obj == NULL) {
            snprintf(response, response_size, ":0\r\n");
            return 1;
        }
        long long sec = strtoll(get_bulk_at(resp_obj, 2), NULL, 10);
        redis_obj->expires_at = current_time_ms() + sec * 1000;
        snprintf(response, response_size, ":1\r\n");
        return 1;
    }

    if (STRCMP(cmd, "TTL")) {
        if (!check_args_len(2, resp_obj, "ttl", response, response_size)) return 1;
        sds key = get_bulk_at(resp_obj, 1);
        redis_object *redis_obj = lookup_redis_object(ht, key);
        if (redis_obj == NULL) {
            snprintf(response, response_size, ":-2\r\n");
            return 1;
        }
        if (redis_obj->expires_at == -1) {
            snprintf(response, response_size, ":-1\r\n");
            return 1;
        }
        long long ttl = (redis_obj->expires_at - current_time_ms()) / 1000;
        snprintf(response, response_size, ":%lld\r\n", ttl);
        return 1;
    }

    return 0;
}

int handle_string_commands(sds cmd, resp_object *resp_obj, hash_table *ht, char *response, size_t response_size) {

    if (STRCMP(cmd, "SET")) {
        if (!check_args_len(3, resp_obj, "set", response, response_size)) return 1;
        sds key = get_bulk_at(resp_obj, 1);
        sds value = get_bulk_at(resp_obj, 2);
        redis_object *redis_obj = create_redis_object(OBJ_STRING, value, -1);

        ht_put(ht, key, redis_obj);
        snprintf(response, response_size, "+OK\r\n");
        return 1;
    }

    if (STRCMP(cmd, "MSET")) {
        int array_len = resp_obj->array.len;
        if (!check_args_len(3, resp_obj, "mset", response, response_size)) return 1;
        if ((array_len & 1) != 1) {
            err_wrong_args(response, response_size, "mset");
            return 1;
        }

        for (int i = 1; i < array_len; i += 2) {
            sds key = get_bulk_at(resp_obj, i);
            sds value = get_bulk_at(resp_obj, i + 1);
            redis_object *redis_obj = create_redis_object(OBJ_STRING, value, -1);
            ht_put(ht, key, redis_obj);
        }

        snprintf(response, response_size, "+OK\r\n");
        return 1;
    }

    if (STRCMP(cmd, "GET")) {
        if (!check_args_len(2, resp_obj, "get", response, response_size)) return 1;
        sds key = get_bulk_at(resp_obj, 1);
        redis_object *redis_obj = lookup_redis_object(ht, key);

        if (redis_obj != NULL && redis_obj->type != OBJ_STRING) {
            err_wrongtype(response, response_size);
            return 1;
        }

        append_bulk(response, response_size, 0, redis_obj);
        return 1;
    }

    if (STRCMP(cmd, "MGET")) {
        if (!check_args_len(2, resp_obj, "mget", response, response_size)) return 1;
        size_t offset = append_fmt(response, response_size, 0, "*%d\r\n", resp_obj->array.len - 1);

        for (int i = 1; i < resp_obj->array.len; i++) {
            sds key_at = get_bulk_at(resp_obj, i);
            redis_object *redis_obj = lookup_redis_object(ht, key_at);
            offset = append_bulk(response, response_size, offset, redis_obj);
        }
        return 1;
    }

    if (STRCMP(cmd, "INCR")) {
        if (!check_args_len(2, resp_obj, "incr", response, response_size)) return 1;
        incr_decr(ht, get_bulk_at(resp_obj, 1), 1, response, response_size);
        return 1;
    }

    if (STRCMP(cmd, "DECR")) {
        if (!check_args_len(2, resp_obj, "decr", response, response_size)) return 1;
        incr_decr(ht, get_bulk_at(resp_obj, 1), -1, response, response_size);
        return 1;
    }

    if (STRCMP(cmd, "INCRBY")) {  // INCRBY key delta
        if (!check_args_len(3, resp_obj, "incrby", response, response_size)) return 1;
        long long delta = strtoll(get_bulk_at(resp_obj, 2), NULL, 10);
        incr_decr(ht, get_bulk_at(resp_obj, 1), delta, response, response_size);
        return 1;
    }

    if (STRCMP(cmd, "APPEND")) {
        if (!check_args_len(3, resp_obj, "append", response, response_size)) return 1;

        sds key = get_bulk_at(resp_obj, 1);
        sds value = get_bulk_at(resp_obj, 2);

        redis_object *redis_obj = lookup_redis_object(ht, key);

        if (redis_obj == NULL) {
            redis_obj = create_redis_object(OBJ_STRING, value, -1);
            ht_put(ht, key, redis_obj);
            snprintf(response, response_size, ":%u\r\n", sdslen((sds) redis_obj->ptr));
            return 1;
        }

        if (redis_obj->type != OBJ_STRING) {
            err_wrongtype(response, response_size);
            return 1;
        }

        sds existing_value = redis_obj->ptr;
        sds new_value = sdscat(existing_value, value);
        redis_obj->ptr = new_value;
        sdsfree(existing_value);

        snprintf(response, response_size, ":%u\r\n", sdslen(new_value));
        return 1;
    }

    return 0;
}

int handle_hash_commands(sds cmd, resp_object *resp_obj, hash_table *ht, char *response, size_t response_size) {
    sds key = get_bulk_at(resp_obj, 1);

    int array_len = resp_obj->array.len;
    if (STRCMP(cmd, "HSET")) {
        if (!check_args_len(4, resp_obj, "hset", response, response_size)) return 1;

        hash_table *hash = 0;
        redis_object *redis_obj = lookup_redis_object(ht, key);
        if (redis_obj == NULL) {
            redis_obj = create_redis_object(OBJ_HASH, hash, -1);
            ht_put(ht, key, redis_obj);
            hash = redis_obj->ptr;
        } else {
            if (redis_obj->type != OBJ_HASH) {
                err_wrongtype(response, response_size);
                return 1;
            }
            hash = redis_obj->ptr;
        }

        int added = 0;
        for (int i = 2; i < array_len; i+=2) {
            sds field = get_bulk_at(resp_obj, i);

            if (i + 1 >= array_len) {
                err_wrong_args(response, response_size, "hset");
                return 1;
            }

            sds value = get_bulk_at(resp_obj, i + 1);
            if (!ht_exists(hash, field)) added++;
            ht_put(hash, field, sdsdup(value));
        }

        snprintf(response, response_size, ":%d\r\n", added);
        return 1;
    }

    if (STRCMP(cmd, "HGET")) {
        if (!check_args_len(3, resp_obj, "hget", response, response_size)) return 1;

        redis_object *redis_obj = lookup_redis_object(ht, key);
        if (redis_obj == NULL) {
            snprintf(response, response_size, "$-1\r\n");
            return 1;
        }
        if (redis_obj->type != OBJ_HASH) {
            err_wrongtype(response, response_size);
            return 1;
        }

        sds field = get_bulk_at(resp_obj, 2);
        sds field_value = ht_get(redis_obj->ptr, field);

        if (field_value == NULL) {
            snprintf(response, response_size, "$-1\r\n");
        } else {
            snprintf(response, response_size, "$%u\r\n%s\r\n", sdslen(field_value), field_value);
        }
        return 1;
    }

    if (STRCMP(cmd, "HGETALL")) {
        if (!check_args_len(2, resp_obj, "hgetall", response, response_size)) return 1;

        redis_object *redis_obj = lookup_redis_object(ht, key);
        if (redis_obj == NULL) {
            snprintf(response, response_size, "*0\r\n");
            return 1;
        }
        if (redis_obj->type != OBJ_HASH) {
            err_wrongtype(response, response_size);
            return 1;
        }

        hash_table *hash = redis_obj->ptr;
        getall_ctx ctx = { .buf = response, .capacity = response_size, .len = 0 };
        ctx.len = append_fmt(response, response_size, ctx.len, "*%d\r\n", hash->size * 2);
        ht_foreach(hash, hgetall_append, &ctx);
        return 1;
    }

    if (STRCMP(cmd, "HDEL")) {
        if (!check_args_len(3, resp_obj, "hdel", response, response_size)) return 1;

        redis_object *redis_obj = lookup_redis_object(ht, key);
        if (redis_obj == NULL) {
            snprintf(response, response_size, ":0\r\n");
            return 1;
        }
        if (redis_obj->type != OBJ_HASH) {
            err_wrongtype(response, response_size);
            return 1;
        }

        hash_table *hash = redis_obj->ptr;

        int del = 0;
        for (int i = 2; i < array_len; i++) {
            sds field = get_bulk_at(resp_obj, i);
            if (ht_delete(hash, field)) del++;
        }
        snprintf(response, response_size, ":%d\r\n", del);
        return 1;
    }

    return 0;
}

int handle_set_commands(sds cmd, resp_object *resp_obj, hash_table *ht, char *response, size_t response_size) {
    sds key = get_bulk_at(resp_obj, 1);

    int array_len = resp_obj->array.len;
    if (STRCMP(cmd, "SADD")) {
        if (!check_args_len(3, resp_obj, "sadd", response, response_size)) return 1;

        int count = 0;
        set *s = 0;
        redis_object *redis_obj = lookup_redis_object(ht, key);
        if (redis_obj == NULL) {
            redis_obj = create_redis_object(OBJ_SET, s, -1);
            ht_put(ht, key, redis_obj);
            s = (set*) redis_obj->ptr;
        } else {
            if (redis_obj->type != OBJ_SET) {
                err_wrongtype(response, response_size);
                return 1;
            }
            s = (set*) redis_obj->ptr;
        }

        for (int i = 2; i < array_len; i++) {
            sds mem = get_bulk_at(resp_obj, i);
            if (set_add(s, mem)) count++;
        }

        snprintf(response, response_size, ":%d\r\n", count);
        return 1;
    }

    if (STRCMP(cmd, "SREM")) {
        if (!check_args_len(3, resp_obj, "srem", response, response_size)) return 1;

        redis_object *redis_obj = lookup_redis_object(ht, key);
        if (redis_obj == NULL) {
            snprintf(response, response_size, ":0\r\n");
            return 1;
        }
        if (redis_obj->type != OBJ_SET) {
            err_wrongtype(response, response_size);
            return 1;
        }

        int del = 0;
        set *s = (set*) redis_obj->ptr;
        for (int i = 2; i < array_len; i++) {
            sds mem = get_bulk_at(resp_obj, i);
            if (set_remove(s, mem)) del++;
        }
        snprintf(response, response_size, ":%d\r\n", del);
        return 1;
    }

    if (STRCMP(cmd, "SISMEMBER")) {
        if (!check_args_len(3, resp_obj, "sismember", response, response_size)) return 1;

        redis_object *redis_obj = lookup_redis_object(ht, key);
        if (redis_obj == NULL) {
            snprintf(response, response_size, ":0\r\n");
            return 1;
        }
        if (redis_obj->type != OBJ_SET) {
            err_wrongtype(response, response_size);
            return 1;
        }

        set *s = (set*) redis_obj->ptr;
        sds mem = get_bulk_at(resp_obj, 2);
        int is_member = set_contains(s, mem);

        snprintf(response, response_size, ":%d\r\n", is_member);
        return 1;
    }

    if (STRCMP(cmd, "SMEMBERS")) {
        if (!check_args_len(2, resp_obj, "smembers", response, response_size)) return 1;

        redis_object *redis_obj = lookup_redis_object(ht, key);
        if (redis_obj == NULL) {
            snprintf(response, response_size, "*0\r\n");
            return 1;
        }
        if (redis_obj->type != OBJ_SET) {
            err_wrongtype(response, response_size);
            return 1;
        }

        set *s = (set*) redis_obj->ptr;

        getall_ctx ctx = { .buf = response, .capacity = response_size, .len = 0 };
        ctx.len = append_fmt(response, response_size, ctx.len, "*%d\r\n", s->size);
        set_foreach(s, sgetall_append, &ctx);
        return 1;
    }

    return 0;
}

int handle_zset_commands(sds cmd, resp_object *resp_obj, hash_table *ht, char *response, size_t response_size) {

    sds key = get_bulk_at(resp_obj, 1);

    int array_len = resp_obj->array.len;
    if (STRCMP(cmd, "ZADD")) {
        if (!check_args_len(4, resp_obj, "zadd", response, response_size)) return 1;

        if (array_len % 2 != 0) {
            err_wrong_args(response, response_size, "zadd");
            return 1;
        }

        redis_object *redis_obj = lookup_redis_object(ht, key);
        if (redis_obj == NULL) {
            redis_obj = create_redis_object(OBJ_ZSET, NULL, -1);
            ht_put(ht, key, redis_obj);
        } else {
            if (redis_obj->type != OBJ_ZSET) {
                err_wrongtype(response, response_size);
                return 1;
            }
        }

        int count = 0;
        zset *zs = redis_obj->ptr;
        for (int i = 2; i < array_len; i+=2) {
            sds member = get_bulk_at(resp_obj, i);
            double score = strtof(get_bulk_at(resp_obj, i + 1), NULL);
            if (zset_add(zs, score, member)) count++;
        }
        snprintf(response, response_size, ":%d\r\n", count);
        return 1;
    }

    if (STRCMP(cmd, "ZREM")) {
        if (!check_args_len(3, resp_obj, "zrem", response, response_size)) return 1;

        redis_object *redis_obj = lookup_redis_object(ht, key);
        if (redis_obj == NULL) {
            snprintf(response, response_size, ":0\r\n");
            return 1;
        }
        if (redis_obj->type != OBJ_ZSET) {
            err_wrongtype(response, response_size);
            return 1;
        }

        int count = 0;
        zset *zs = redis_obj->ptr;
        for (int i = 2; i < array_len; i++) {
            sds member = get_bulk_at(resp_obj, i);
            if (zset_rem(zs, member)) count++;
        }
        snprintf(response, response_size, ":%d\r\n", count);
        return 1;
    }

    if (STRCMP(cmd, "ZRANGE")) {
        if (!check_args_len(4, resp_obj, "zrange", response, response_size)) return 1;

        redis_object *redis_obj = lookup_redis_object(ht, key);
        if (redis_obj == NULL) {
            snprintf(response, response_size, "*0\r\n");
            return 1;
        }
        if (redis_obj->type != OBJ_ZSET) {
            err_wrongtype(response, response_size);
            return 1;
        }

        zset *zs = redis_obj->ptr;

        double min = strtof(get_bulk_at(resp_obj, 2), NULL);
        double max = strtof(get_bulk_at(resp_obj, 3), NULL);

        int count = zset_range_by_score(zs, min, max, zrange_count, NULL);

        getall_ctx ctx = { .buf = response, .capacity = response_size, .len = 0 };
        ctx.len = append_fmt(response, response_size, ctx.len, "*%d\r\n", count);
        zset_range_by_score(zs, min, max, zrange_append, &ctx);
        return 1;
    }

    if (STRCMP(cmd, "ZSCORE")) {
        if (!check_args_len(3, resp_obj, "zscore", response, response_size)) return 1;

        redis_object *redis_obj = lookup_redis_object(ht, key);
        if (redis_obj == NULL) {
            snprintf(response, response_size, "$-1\r\n");
            return 1;
        }
        if (redis_obj->type != OBJ_ZSET) {
            err_wrongtype(response, response_size);
            return 1;
        }

        zset *zs = redis_obj->ptr;
        sds member = get_bulk_at(resp_obj, 2);

        double score = zset_find(zs, member);
        if (score == -1) {
            snprintf(response, response_size, "$-1\r\n");
        } else {
            char buf[32];
            int len = snprintf(buf, sizeof(buf), "%g", score);
            snprintf(response, response_size, "$%d\r\n%s\r\n", len, buf);
        }
        return 1;
    }

    return 0;
}
