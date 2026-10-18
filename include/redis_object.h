
#ifndef REDIS_CLONE_REDIS_OBJECT_H
#define REDIS_CLONE_REDIS_OBJECT_H


typedef enum {
    OBJ_STRING = 0,
    OBJ_LIST,
    OBJ_SET,
    OBJ_ZSET,
    OBJ_HASH
} robj_type;

typedef struct redis_object {
    robj_type type;
    void* ptr;
    long long expires_at;
} robj;

robj* create_robj(robj_type type, void* ptr, long long expires_at);
void robj_free(robj* obj);

#endif //REDIS_CLONE_REDIS_OBJECT_H
