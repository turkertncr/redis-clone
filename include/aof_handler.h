//
// Created by turker on 23.09.2026.
//

#ifndef REDIS_CLONE_ASYNC_H
#define REDIS_CLONE_ASYNC_H

#include <stdio.h>

#include "queue.h"

typedef struct aof_handler {
    pthread_t thread;
    queue queue;
    FILE *f;
} aof_handler;

void* aof_write(void* args);
int aof_handler_init(aof_handler *handler, const char *aof_path);

#endif //REDIS_CLONE_ASYNC_H
