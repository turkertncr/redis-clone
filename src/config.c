
#include <stdio.h>
#include <string.h>

#include <config.h>
#include <data_types/hash_table.h>
#include <alloc.h>

static char** parse(char *str) {
    size_t len = strlen(str);
    while (len > 0 && (str[len - 1] == '\n' || str[len - 1] == '\r')) {
        str[--len] = '\0';
    }

    char *sp = strchr(str, ' ');
    if (!sp) {
        return NULL;
    }

    *sp = '\0';
    char *key = str;
    char *value = sp + 1;
    while (*value == ' ' || *value == '\t') {
        value++;
    }

    char **result = zalloc(sizeof(char*) * 2);
    if (!result) {
        return NULL;
    }
    result[0] = sdsnew(key);
    result[1] = sdsnew(value);
    return result;
}

static int read_file(config *cfg, const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "Can't open file %s\n", path);
        return 0;
    }

    char buf[1024];
    while (fgets(buf, sizeof(buf), f)) {
        if (buf[0] == '#') continue;
        char **o = parse(buf);
        if (o) {
            ht_put(cfg->options, o[0], o[1]);
        }
    }
    return 1;
}

int init_config(config *cfg, const char *path) {
    cfg->options = ht_create((ht_value_free_fn) sdsfree, 0, HT_MEDIUM_CAPACITY);
    return read_file(cfg, path ? path : "redis.conf");
}

sds get_config(config *cfg, char *key) {
    if (!key) {
        fprintf(stderr, "Invalid find config key %s\n", key);
        return 0;
    }
    sds lookup_key = sdsnew(key);
    char *config = ht_get(cfg->options, lookup_key);
    sdsfree(lookup_key);
    if (!config) {
        fprintf(stderr, "Can't find config key %s\n", key);
    }
    return config;
}