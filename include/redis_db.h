
#ifndef REDIS_CLONE_REDIS_DB_H
#define REDIS_CLONE_REDIS_DB_H
#include "redis_object.h"
#include "data_types/hash_table.h"

typedef struct {
    hash_table *dict;
    hash_table *expires;
} redis_db;

void db_init(redis_db *db);

robj* db_lookup(redis_db *db, sds key);

void db_put(redis_db *db, sds key, robj *obj);

int db_del(redis_db *db, sds key);

int db_set_expire(redis_db *db, sds key, long long when);

int db_remove_expire(redis_db *db, sds key);

#endif //REDIS_CLONE_REDIS_DB_H
