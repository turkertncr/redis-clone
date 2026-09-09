
#include <stdlib.h>

#include <include/data_types/set.h>

set* set_create() {
    set *set = malloc(sizeof(*set));
    if (set == NULL) return NULL;

    set->ht = ht_create((ht_value_free_fn) sdsfree);
    if (set->ht == NULL) {
        free(set);
        return NULL;
    }
    set->size = 0;
    return set;
}

int set_add(set *set, sds value) {
    if (!set) return 0;
    if (ht_get(set->ht, value) != NULL) return 0;
    if (ht_put(set->ht, value, NULL) == 0) return 0;
    set->size++;
    return 1;
}

int set_remove(set *set, sds value) {
    if (!set || set->size == 0) { return 0; }
    if (ht_delete(set->ht, value) == 0) return 0;
    set->size--;
    return 1;
}

int set_contains(set *set, sds value) {
    if (!set || set->size == 0) { return 0; }
    return ht_exists(set->ht, value);
}

int set_size(set *set) {
    if (set == NULL) { return -1; }
    return set->size;
}

void set_free(set *set) {
    if (set == NULL) { return; }
    ht_free(set->ht);
    free(set);
}

void set_foreach(set *set, ht_iter_fn func, void *data) {
    if (set == NULL) { return; }
    ht_foreach(set->ht, func, data);
}
