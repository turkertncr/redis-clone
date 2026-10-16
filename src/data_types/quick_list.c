
#include <stdlib.h>
#include <string.h>

#include <alloc.h>
#include <data_types/quick_list.h>
#include <data_types/zip_list.h>

quicklist* quicklist_create() {
    quicklist *ql = zalloc(sizeof(*ql));
    if (ql == NULL) return NULL;

    ql->head = ql->tail = NULL;
    ql->count = 0;
    ql->len = 0;
    return ql;
}

quicklist_node* create_quicklist_node() {
    quicklist_node *node = zalloc(sizeof(*node));
    if (node == NULL) return NULL;

    node->prev = node->next = NULL;
    node->zl = ziplist_create();
    node->sz = ZL_BYTES(node->zl);
    node->count = 0;
    return node;
}

int quicklist_push_tail(quicklist *ql, sds value) {
    uint32_t value_len = sdslen(value);
    quicklist_node *node = ql->tail;

    if (!node || (node->sz + value_len) > QL_MAX_BYTES) {
        quicklist_node* new = create_quicklist_node();

        if (ql->tail == NULL) {
            ql->tail = new;
            ql->head = new;
        } else {
            ql->tail->next = new;
            new->prev = ql->tail;
            ql->tail = new;
        }

        node = new;
        ql->len++;
    }

    node->zl = ziplist_tail_push(node->zl, value, value_len);

    node->sz = ZL_BYTES(node->zl);
    node->count++;
    ql->count++;
    return (int) ql->count;
}

int quicklist_push_head(quicklist *ql, sds value) {
    uint32_t value_len = sdslen(value);
    quicklist_node *node = ql->head;

    if (!node || (node->sz + value_len) > QL_MAX_BYTES) {
        quicklist_node* new = create_quicklist_node();

        if (ql->head == NULL) {
            ql->head = new;
        } else {
            ql->head->prev = new;
            new->next = ql->head;
            ql->head = new;
        }

        node = new;
        ql->len++;
    }

    node->zl = ziplist_head_push(node->zl, value, value_len);

    node->sz = ZL_BYTES(node->zl);
    node->count++;
    ql->count++;
    return (int) ql->count;
}

int quicklist_pop_tail(quicklist *ql) {
    quicklist_node *node = ql->tail;

    if (!node || node->count == 0) { return 0; }

    node->zl = ziplist_tail_pop(node->zl);
    node->count--;
    ql->count--;

    if (node->count == 0) {
        ql->tail = node->prev;
        if (ql->tail) {
            ql->tail->next = NULL;
        } else {
            ql->head = NULL;
        }
        ql->len--;
        ziplist_free(node->zl);
        zfree(node);
    }
    return 1;
}

int quicklist_pop_head(quicklist *ql) {
    quicklist_node *node = ql->head;

    if (!node || node->count == 0) { return 0; }

    node->zl = ziplist_head_pop(node->zl);
    node->count--;
    ql->count--;

    if (node->count == 0) {
        ql->head = node->next;
        if (ql->head) {
            ql->head->prev = NULL;
        } else {
            ql->tail = NULL;
        }
        ql->len--;
        ziplist_free(node->zl);
        zfree(node);
    }
    return 1;
}

void quicklist_free(quicklist *ql) {
    quicklist_node *head = ql->head;
    while (head) {
        quicklist_node *next = head->next;
        ziplist_free(head->zl);
        zfree(head);
        head = next;
    }
    zfree(ql);
}

static quicklist_node* quicklist_unlink_node(quicklist *ql, quicklist_node *node);

quicklist_node* find_quicklist_node(quicklist *ql, const char *pivot, size_t pivot_size) {
    quicklist_node *node = ql->head;
    while (node && node->count > 0) {
        ziplist zl = node->zl;

        struct zlentry *entry = ziplist_find(zl, pivot, pivot_size);
        if (entry) {
            return node;
        }
        node = node->next;
    }
    return 0;
}

int quicklist_insert(quicklist *ql, const char *pivot, size_t pivot_size, sds value, int before) {

    quicklist_node *node = find_quicklist_node(ql, pivot, pivot_size);
    if (!node) { return 0; }
    ziplist zl = node->zl;
    struct zlhdr *hdr = getzlhdr(zl);

    size_t value_len = sdslen(value);
    size_t zlbytes = hdr->zlbytes;

    if (zlbytes + value_len > QL_MAX_BYTES) {
        quicklist_node *new_block = create_quicklist_node();
        struct zlentry *entry = zltail(zl);

        size_t moved_bytes = 0;
        while (zlbytes + value_len - moved_bytes > QL_MAX_BYTES) {
            const size_t entry_size = entry->currlen + zlentry_hdrsize;

            new_block->zl = ziplist_head_push(new_block->zl, (const char *)entry->data, entry->currlen);
            new_block->count++;
            new_block->sz += entry_size;

            zl = ziplist_tail_pop(zl);
            node->count--;
            node->sz -= entry_size;
            moved_bytes += entry_size;

            if (node->count == 0) break;
            entry = zltail(zl);
        }

        if (node->next) {
            quicklist_node *next = node->next;
            next->prev = new_block;
            new_block->next = next;
        } else {
            ql->tail = new_block;
        }
        node->next = new_block;
        new_block->prev = node;
        node->zl = zl;
        ql->len++;

        if (node->count == 0 || ziplist_find(node->zl, pivot, pivot_size) == NULL) {
            if (node->count == 0) {
                quicklist_unlink_node(ql, node);
            }
            node = new_block;
        }
    }
    node->zl = ziplist_insert(node->zl, pivot, pivot_size, value, value_len, before);
    node->sz = ZL_BYTES(node->zl);
    node->count++;
    ql->count++;
    return 1;
}

static quicklist_node* quicklist_unlink_node(quicklist *ql, quicklist_node *node) {
    quicklist_node *prev = node->prev;
    quicklist_node *next = node->next;

    if (prev) { prev->next = next; } else { ql->head = next; }
    if (next) { next->prev = prev; } else { ql->tail = prev; }

    ziplist_free(node->zl);
    zfree(node);
    ql->len--;

    return next;
}

int quicklist_remove(quicklist *ql, sds value, int count) {

    int from_head = (count >= 0);
    quicklist_node *node = from_head ? ql->head : ql->tail;

    int unlimited = (count == 0);
    int remaining = unlimited ? 0 : (count > 0 ? count : -count);
    int deleted = 0;

    while (node && (unlimited || remaining > 0)) {
        quicklist_node *step = from_head ? node->next : node->prev;

        uint16_t before_len = getzlhdr(node->zl)->zllen;
        int node_count = unlimited ? 0 : (from_head ? remaining : -remaining);
        node->zl = ziplist_remove(node->zl, value, sdslen(value), node_count);
        if (node->zl == NULL) { return deleted; }

        uint16_t rem = before_len - getzlhdr(node->zl)->zllen;
        node->count -= rem;
        node->sz = ZL_BYTES(node->zl);
        ql->count -= rem;
        deleted += rem;

        if (!unlimited) { remaining -= rem; }

        if (node->count == 0) {
            quicklist_unlink_node(ql, node);
        }

        node = step;
    }

    return deleted;
}

struct zlentry *quicklist_get_at(quicklist *ql, int index) {

    if (index < 0) { index += (int) ql->count; }
    if (index < 0 || index >= (int) ql->count) { return NULL; }

    quicklist_node *node = ql->head;
    while (node) {
        if (index < (int) node->count) {
            return ziplist_get_at(node->zl, index);
        }
        index -= (int) node->count;
        node = node->next;
    }
    return NULL;
}

int quicklist_range(quicklist *ql, int start, int stop, quicklist_foreach_fn fn, void *user_data) {
    int len = (int) ql->count;
    if (len == 0) { return 0; }

    if (start < 0) { start += len; }
    if (stop < 0) { stop += len; }
    if (start < 0) { start = 0; }
    if (stop >= len) { stop = len - 1; }
    if (start > stop || start >= len) { return 0; }

    int counter = 0;
    int index = 0;
    quicklist_node *node = ql->head;

    while (node && index <= stop) {
        int node_start = index;
        int node_end = index + (int) node->count - 1;

        if (node_end >= start) {
            int local_start = (start > node_start) ? start - node_start : 0;
            int local_stop = (stop < node_end) ? stop - node_start : (int) node->count - 1;

            struct zlentry *entry = zlhead(node->zl);
            for (int i = 0; i < local_start; i++) {
                entry = (struct zlentry *)((unsigned char *)entry + zlentry_hdrsize + entry->currlen);
            }
            for (int i = local_start; i <= local_stop; i++) {
                fn(entry, user_data);
                counter++;
                entry = (struct zlentry *)((unsigned char *)entry + zlentry_hdrsize + entry->currlen);
            }
        }

        index = node_end + 1;
        node = node->next;
    }

    return counter;
}