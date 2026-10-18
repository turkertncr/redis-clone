#ifndef REDIS_CLONE_QUEUE_H
#define REDIS_CLONE_QUEUE_H

#include <pthread.h>

#include <data_types/redis_string.h>

typedef enum {
    OK,
    TIMEOUT,
    SHUTDOWN,
} queue_status;

typedef struct node {
    struct node *next;
    sds data;
} node;

typedef struct queue {
    node *head;
    node *tail;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    int shutdown;
} queue;

node* create_node(sds data);
void queue_init(queue *q);
void queue_push(queue *q, sds data);
queue_status queue_pop(queue *q, sds *out);
void queue_shutdown(queue *q);
void queue_destroy(queue *q);

#endif //REDIS_CLONE_QUEUE_H
