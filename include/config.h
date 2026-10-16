#ifndef REDIS_CLONE_CONFIG_H
#define REDIS_CLONE_CONFIG_H
#include "data_types/hash_table.h"

typedef struct {
    hash_table *options;
} config;

int init_config(config *cfg, const char *path);
sds get_config(config *cfg, char *key);

#endif //REDIS_CLONE_CONFIG_H
