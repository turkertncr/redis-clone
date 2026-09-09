
#include <stdlib.h>

#include "include/data_types/skip_list.h"

sl* sl_create() {
    sl *sl = malloc(sizeof(*sl));
    if (sl == NULL) return NULL;

    sl->top_level = 1;
    sl->head = sl_node_create(0, NULL, MAX_LEVEL);
    sl->size = 0;
    return sl;
}

void sl_free(sl* skipl) {
    sl_node *curr = skipl->head;
    while (curr != NULL) {
        sl_node *next = curr->level_array[0].next;
        sl_node_free(curr);
        curr = next;
    }
    free(skipl);
}

sl_node* sl_node_create(double score, sds member, int level) {
    sl_node *node = malloc(sizeof(*node) + level * sizeof(*node->level_array));
    if (node == NULL) return NULL;

    node->score = score;
    node->member = sdsdup(member);
    node->prev = NULL;
    node->level = level;

    for (int i = 0; i < level; i++) {
        node->level_array[i].next = NULL;
    }

    return node;
}

void sl_node_free(sl_node* node) {
    sdsfree(node->member);
    free(node);
}

int sl_rand() {
    int level = 0;
    while ((float)rand() / RAND_MAX < P && level < MAX_LEVEL) {
        level++;
    }
    return level;
}

int sl_cmp(double score1, sds member1, double score2, sds member2) {
    if (score1 != score2) return score1 > score2 ? 1 : -1;
    return sdscmp(member1, member2);
}

sl_node *sl_insert(sl *skipl, double score, sds member) {

    sl_node *curr = skipl->head;
    sl_node *update[MAX_LEVEL];

    for (int i = skipl->top_level - 1; i >= 0; i--) {
        while (curr->level_array[i].next != NULL &&
               sl_cmp(score, member, curr->level_array[i].next->score, curr->level_array[i].next->member) == 1) {
            curr = curr->level_array[i].next;
        }
        update[i] = curr;
    }

    curr = curr->level_array[0].next;

    if (curr != NULL && sl_cmp(score, member, curr->score, curr->member) == 0) {
        return curr;
    }

    int level = sl_rand();
    if (level < 1) level = 1;

    if (level > skipl->top_level) {
        for (int i = skipl->top_level; i < level; i++) {
            update[i] = skipl->head;
        }
        skipl->top_level = level;
    }

    sl_node *node = sl_node_create(score, member, level);
    for (int i = 0; i < level; i++) {
        node->level_array[i].next = update[i]->level_array[i].next;
        update[i]->level_array[i].next = node;
    }

    node->prev = (update[0] == skipl->head) ? NULL : update[0];
    if (node->level_array[0].next != NULL) {
        node->level_array[0].next->prev = node;
    }

    skipl->size++;

    return node;
}

int sl_delete(sl *skipl, double score, sds member) {

    sl_node *curr = skipl->head;
    sl_node *update[MAX_LEVEL];

    for (int i = skipl->top_level - 1; i >= 0; i--) {
        while (curr->level_array[i].next != NULL &&
               sl_cmp(score, member, curr->level_array[i].next->score, curr->level_array[i].next->member) == 1) {
            curr = curr->level_array[i].next;
        }
        update[i] = curr;
    }

    curr = curr->level_array[0].next;

    if (curr == NULL || sl_cmp(score, member, curr->score, curr->member) != 0) {
        return 0;
    }

    for (int i = 0; i < skipl->top_level && i < curr->level; i++) {
        update[i]->level_array[i].next = curr->level_array[i].next;
    }

    if (curr->level_array[0].next != NULL) {
        curr->level_array[0].next->prev = curr->prev;
    }

    while (skipl->top_level > 1 && skipl->head->level_array[skipl->top_level - 1].next == NULL) {
        skipl->top_level--;
    }

    skipl->size--;
    sl_node_free(curr);

    return 1;
}

sl_node *sl_search(sl *skipl, double score, sds member) {

    sl_node *curr = skipl->head;

    for (int i = skipl->top_level - 1; i >= 0; i--) {
        while (curr->level_array[i].next != NULL &&
               sl_cmp(score, member, curr->level_array[i].next->score, curr->level_array[i].next->member) == 1) {
            curr = curr->level_array[i].next;
        }
    }

    curr = curr->level_array[0].next;

    if (curr != NULL && sl_cmp(score, member, curr->score, curr->member) == 0) {
        return curr;
    }

    return NULL;
}

int sl_range(sl *skipl, double min, double max,sl_iter_range_fn fn, void* user_data) {
    sl_node *curr = skipl->head;

    int counter = 0;
    for (int i = skipl->top_level - 1; i >= 0; i--) {
        while (curr->level_array[i].next != NULL && curr->level_array[i].next->score < min) {
            curr = curr->level_array[i].next;
        }
    }

    curr = curr->level_array[0].next;
    while (curr != NULL && curr->score <= max) {
        fn(curr, user_data);
        counter++;
        curr = curr->level_array[0].next;
    }

    return counter;
}

