
#include <stdio.h>
#include <alloc.h>
#include <queue.h>
#include <util.h>

void queue_init(queue *q) {
    q->head = NULL;
    q->tail = NULL;
    q->shutdown = 0;

    pthread_condattr_t condattr;

    pthread_mutex_init(&q->mutex, NULL);
    pthread_condattr_init(&condattr);
    pthread_condattr_setclock(&condattr, CLOCK_MONOTONIC);
    pthread_cond_init(&q->not_empty, &condattr);
    pthread_condattr_destroy(&condattr);
}

node* create_node(sds data) {
    if (!data) { return NULL; }
    node *n = zalloc(sizeof(node));
    if (!n) { return NULL; }
    n->data = data;
    n->next = NULL;
    return n;
}

void queue_push(queue *q, sds data) {

    if (!q || !data) { return; }
    pthread_mutex_lock(&q->mutex);

    node *tail = q->tail;
    node *n = create_node(data);
    if (!n) {
        pthread_mutex_unlock(&q->mutex);
        return;
    }

    if (tail) {
        tail->next = n;
        q->tail = n;
    } else {
        q->head = n;
        q->tail = n;
    }

    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->mutex);
}

queue_status queue_pop(queue *q, sds *out) {
    if (!q) { return SHUTDOWN; }
    pthread_mutex_lock(&q->mutex);

    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    long wait_ms = 1000;
    ts.tv_sec += wait_ms / 1000;
    ts.tv_nsec += (wait_ms % 1000) * 1000000L;
    if (ts.tv_nsec >= 1000000000L) {
        ts.tv_sec += 1;
        ts.tv_nsec -= 1000000000L;
    }

    while (!q->head && !q->shutdown) {
        int r = pthread_cond_timedwait(&q->not_empty, &q->mutex, &ts);
        if (r == ETIMEDOUT) {
            pthread_mutex_unlock(&q->mutex);
            return TIMEOUT;
        }
    }

    if (!q->head) {
        pthread_mutex_unlock(&q->mutex);
        return SHUTDOWN;
    }

    node *head = q->head;
    sds data = head->data;

    node *next = head->next;
    if (next) {
        q->head = next;
    } else {
        q->head = NULL;
        q->tail = NULL;
    }

    pthread_mutex_unlock(&q->mutex);
    zfree(head);
    *out = data;
    return OK;
}

void queue_shutdown(queue *q) {
    pthread_mutex_lock(&q->mutex);
    q->shutdown = 1;
    pthread_cond_broadcast(&q->not_empty);
    pthread_mutex_unlock(&q->mutex);
}

void queue_destroy(queue *q) {
    if (!q) { return; }

    node *curr = q->head;
    while (curr) {
        node* next = curr->next;
        sdsfree(curr->data);
        zfree(curr);
        curr = next;
    }

    pthread_mutex_destroy(&q->mutex);
    pthread_cond_destroy(&q->not_empty);
}
