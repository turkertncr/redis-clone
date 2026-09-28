
#include <stdio.h>
#include <stdlib.h>
#include <include/queue.h>

void queue_init(queue *q) {
    q->head = NULL;
    q->tail = NULL;
    q->shutdown = 0;

    pthread_mutex_init(&q->mutex, NULL);
    pthread_cond_init(&q->not_empty, NULL);
}

node* create_node(sds data) {
    if (!data) { return NULL; }
    node *n = malloc(sizeof(node));
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

sds queue_pop(queue *q) {
    if (!q) { return NULL; }
    pthread_mutex_lock(&q->mutex);

    while (!q->head && !q->shutdown) {
        pthread_cond_wait(&q->not_empty, &q->mutex);
    }

    if (!q->head) {
        pthread_mutex_unlock(&q->mutex);
        return NULL;
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
    free(head);
    return data;
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
        free(curr);
        curr = next;
    }

    pthread_mutex_destroy(&q->mutex);
    pthread_cond_destroy(&q->not_empty);
}
