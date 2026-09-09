
#ifndef REDIS_CLONE_REDIS_OBJECT_H
#define REDIS_CLONE_REDIS_OBJECT_H

#include <include/data_types/hash_table.h>

typedef enum {
    OBJ_STRING = 0,
    OBJ_LIST,
    OBJ_SET,
    OBJ_ZSET,
    OBJ_HASH
} redis_type;

typedef struct redis_object {
    redis_type type;
    void* ptr;
    long long expires_at;
} redis_object;

long long current_time_ms(void);
redis_object* create_redis_object(redis_type type, void* ptr, long long ttl_seconds);
redis_object* lookup_redis_object(hash_table* ht, sds key);
void redis_object_free(redis_object* obj);

#endif //REDIS_CLONE_REDIS_OBJECT_H
