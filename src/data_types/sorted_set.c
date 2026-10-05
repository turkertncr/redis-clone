
#include <stdlib.h>
#include <include/data_types/sorted_set.h>

void zset_free_fn(void* ptr) {
    free(ptr);
}

zset* zset_create() {
    zset *zset = malloc(sizeof(*zset));
    if (zset == NULL) return NULL;

    zset->sl = skiplist_create();
    zset->ht = ht_create(zset_free_fn, REF_MODE, HT_SMALL_CAPACITY);

    if (zset->sl == NULL || zset->ht == NULL) {
        zset_free(zset);
        return NULL;
    }

    zset->size = 0;
    return zset;
}

int zset_add(zset *zset, double score, sds mem) {
    if (!zset) { return 0; }

    double *existing = ht_get(zset->ht, mem);
    if (existing != NULL) {
        double old_score = *existing;
        ht_delete(zset->ht, mem);
        skiplist_delete(zset->sl, old_score, mem);
    }

    skiplist_node *node = node = skiplist_insert(zset->sl, score, mem);
    if (node == NULL) return 0;

    double *score_p = malloc(sizeof(*score_p));
    if (score_p == NULL) return 0;
    *score_p = score;
    ht_put(zset->ht, node->member, score_p);

    int ret = 0;
    if (existing == NULL) { zset->size++; ret++; }
    return ret;
}

int zset_rem(zset *zset, sds mem) {
    if (!zset) { return 0; }

    double* score = ht_get(zset->ht, mem);
    if (score == NULL) { return 0; }

    double s = *score;
    if (ht_delete(zset->ht, mem) && skiplist_delete(zset->sl, s, mem)) {
        zset->size--;
        return 1;
    }
    return 0;
}

int zset_find(zset *zset, sds mem, double *score_out) {
    if (!zset) return 0;
    double *score = (double*) ht_get(zset->ht, mem);
    if (score == NULL) return 0;
    if (score_out) *score_out = *score;
    return 1;
}

void zset_incr(zset *zset, sds mem, double incr) {
    if (!zset) return;

    double *existing = (double*) ht_get(zset->ht, mem);
    double new_score = (existing != NULL) ? (*existing + incr) : incr;

    zset_add(zset, new_score, mem);
}

int zset_size(zset *zset) {
    if (!zset) { return -1; }
    return zset->size;
}

void zset_free(zset* zset) {
    if (!zset) { return; }
    ht_free(zset->ht);
    skiplist_free(zset->sl);
    free(zset);
}

int zset_range_by_score(zset *zset, double min, double max, sl_iter_range_fn fn, void *user_data) {
    if (!zset) { return -1; }
    return skiplist_range(zset->sl, min, max, fn, user_data);
}