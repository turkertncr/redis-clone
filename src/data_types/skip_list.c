
#include <stdlib.h>

#include <include/data_types/skip_list.h>

skiplist* skiplist_create() {
    skiplist *sl = malloc(sizeof(*sl));
    if (sl == NULL) return NULL;

    sl->top_level = 1;
    sl->head = skiplist_node_create(0, NULL, MAX_LEVEL);
    sl->size = 0;
    return sl;
}

void skiplist_free(skiplist* skipl) {
    skiplist_node *curr = skipl->head;
    while (curr != NULL) {
        skiplist_node *next = curr->level_array[0].next;
        skiplist_node_free(curr);
        curr = next;
    }
    free(skipl);
}

skiplist_node* skiplist_node_create(double score, sds member, int level) {
    skiplist_node *node = malloc(sizeof(*node) + level * sizeof(*node->level_array));
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

void skiplist_node_free(skiplist_node* node) {
    sdsfree(node->member);
    free(node);
}

int skiplist_rand() {
    int level = 0;
    while ((float)rand() / RAND_MAX < P && level < MAX_LEVEL) {
        level++;
    }
    return level;
}

int skiplist_cmp(double score1, sds member1, double score2, sds member2) {
    if (score1 != score2) return score1 > score2 ? 1 : -1;
    return sdscmp(member1, member2);
}

skiplist_node *skiplist_insert(skiplist *skipl, double score, sds member) {

    skiplist_node *curr = skipl->head;
    skiplist_node *update[MAX_LEVEL];

    for (int i = skipl->top_level - 1; i >= 0; i--) {
        while (curr->level_array[i].next != NULL &&
               skiplist_cmp(score, member, curr->level_array[i].next->score, curr->level_array[i].next->member) == 1) {
            curr = curr->level_array[i].next;
        }
        update[i] = curr;
    }

    curr = curr->level_array[0].next;

    if (curr != NULL && skiplist_cmp(score, member, curr->score, curr->member) == 0) {
        return curr;
    }

    int level = skiplist_rand();
    if (level < 1) level = 1;

    if (level > skipl->top_level) {
        for (int i = skipl->top_level; i < level; i++) {
            update[i] = skipl->head;
        }
        skipl->top_level = level;
    }

    skiplist_node *node = skiplist_node_create(score, member, level);
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

int skiplist_delete(skiplist *skipl, double score, sds member) {

    skiplist_node *curr = skipl->head;
    skiplist_node *update[MAX_LEVEL];

    for (int i = skipl->top_level - 1; i >= 0; i--) {
        while (curr->level_array[i].next != NULL &&
               skiplist_cmp(score, member, curr->level_array[i].next->score, curr->level_array[i].next->member) == 1) {
            curr = curr->level_array[i].next;
        }
        update[i] = curr;
    }

    curr = curr->level_array[0].next;

    if (curr == NULL || skiplist_cmp(score, member, curr->score, curr->member) != 0) {
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
    skiplist_node_free(curr);

    return 1;
}

skiplist_node *skiplist_search(skiplist *skipl, double score, sds member) {

    skiplist_node *curr = skipl->head;

    for (int i = skipl->top_level - 1; i >= 0; i--) {
        while (curr->level_array[i].next != NULL &&
               skiplist_cmp(score, member, curr->level_array[i].next->score, curr->level_array[i].next->member) == 1) {
            curr = curr->level_array[i].next;
        }
    }
    curr = curr->level_array[0].next;

    if (curr != NULL &&
        skiplist_cmp(score, member, curr->score, curr->member) == 0) {
        return curr;
    }
    return NULL;
}

int skiplist_range(skiplist *skipl, double min, double max,sl_iter_range_fn fn, void* user_data) {
    skiplist_node *curr = skipl->head;

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

