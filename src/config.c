//
// Created by turker on 18.09.2026.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "include/data_types/hash_table.h"
#include "include/data_types/redis_string.h"

static hash_table *options;

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

    char **result = malloc(sizeof(char*) * 2);
    if (!result) {
        return NULL;
    }
    result[0] = sdsnew(key);
    result[1] = sdsnew(value);
    return result;
}

static int read_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "Can't open file %s\n", path);
        return 0;
    }

    char buf[1024];
    while (fgets(buf, sizeof(buf), f)) {
        char **o = parse(buf);
        if (o) {
            ht_put(options, o[0], o[1]);
        }
    }
    return 1;
}

int init_config(const char *path) {
    options = ht_create(NULL, 0, HT_DEFAULT_CAPACITY);
    return read_file(path ? path : "redis.conf");
}

char* get_config(char *key) {
    if (!key) {
        fprintf(stderr, "Invalid find config key %s\n", key);
        return 0;
    }
    sds lookup_key = sdsnew(key);
    char *config = ht_get(options, lookup_key);
    sdsfree(lookup_key);
    if (!config) {
        fprintf(stderr, "Can't find config key %s\n", key);
    }
    return config;
}