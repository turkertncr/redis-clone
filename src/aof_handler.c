
#include <pthread.h>
#include <stdio.h>
#include <string.h>

#include <include/aof_handler.h>

void* aof_write(void *args) {

    aof_handler *handler = args;

    queue *q = &handler->queue;
    sds data = queue_pop(q);
    while (data != NULL) {
        fprintf(handler->f, "%s", data);
        fflush(handler->f);
        sdsfree(data);
        data = queue_pop(q);
    }

    return 0;
}

int aof_handler_init(aof_handler *handler, const char *aof_path) {
    if (!handler) { return 0; }

    handler->f = fopen(aof_path, "w");
    if (!handler->f) {
        fprintf(stderr, "%s fopen failed\n", __func__);
        return 0;
    }

    queue_init(&handler->queue);
    pthread_create(&handler->thread, NULL, aof_write, handler);
    return 1;
}
