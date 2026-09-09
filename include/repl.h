#ifndef REDIS_CLONE_REPL_H
#define REDIS_CLONE_REPL_H

#include "data_types/hash_table.h"

struct split {
    size_t len;
    char** args;
};

struct split split_cmd(char *args);
getall_ctx build_resp(char **resp_arr, size_t resp_arr_size);
void repl_init(hash_table *ht);
void execute_with_str(char *args);

#endif //REDIS_CLONE_REPL_H
