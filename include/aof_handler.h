#ifndef REDIS_CLONE_ASYNC_H
#define REDIS_CLONE_ASYNC_H

#include <stdio.h>
#include <queue.h>
#include <config.h>

typedef enum {
    ALWAYS,
    EVERYSEC,
    NO
} aofsync;

typedef struct aof_handler {
    pthread_t thread;
    queue queue;
    FILE *file;
    aofsync aofsync;
} aof_handler;

void* aof_write(void* args);
int aof_handler_init(aof_handler *handler, config *cfg, hash_table *db);

#endif //REDIS_CLONE_ASYNC_H
