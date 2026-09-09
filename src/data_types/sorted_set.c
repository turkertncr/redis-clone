

#include <stdlib.h>

#include <include/data_types/sorted_set.h>

void zset_free_fn(void* ptr) {
    free(ptr);
}

zset* zset_create() {
    zset *zset = malloc(sizeof(*zset));
    if (zset == NULL) return NULL;

    zset->sl = sl_create();
    zset->ht = ht_create(zset_free_fn);

    if (zset->sl == NULL || zset->ht == NULL) {
        zset_free(zset);
        return NULL;
    }

    zset->size = 0;
    return zset;
}

int zset_add(zset *zset, double score, sds mem) {
    if (!zset) { return 0; }

    double *existing = (double*)ht_get(zset->ht, mem);
    if (existing != NULL) {
        sl_delete(zset->sl, *existing, mem);
    }

    if (sl_insert(zset->sl, score, mem) == NULL) return 0;

    double *score_p = malloc(sizeof(*score_p));
    if (score_p == NULL) return 0;
    *score_p = score;
    ht_put(zset->ht, mem, score_p);

    if (existing == NULL) zset->size++;
    return 1;
}

int zset_rem(zset *zset, sds mem) {
    if (!zset) { return 0; }

    double* score = ht_get(zset->ht, mem);
    if (score == NULL) { return 0; }

    if (sl_delete(zset->sl, *score, mem) &&
        ht_delete(zset->ht, mem)) {
        zset->size--;
        return 1;
    }
    return 0;
}

double zset_find(zset *zset, sds mem) {
    if (!zset) return -1;
    double *score = (double*) ht_get(zset->ht, mem);
    return score ? *score : -1;
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
    sl_free(zset->sl);
    free(zset);
}

int zset_range_by_score(zset *zset, double min, double max, sl_iter_range_fn fn, void *user_data) {
    if (!zset) { return -1; }
    return sl_range(zset->sl, min, max, fn, user_data);
}