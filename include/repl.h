#ifndef REDIS_CLONE_REPL_H
#define REDIS_CLONE_REPL_H

#include "data_types/hash_table.h"

void repl_init(hash_table *ht);
void execute_with_str(char *args);

#endif //REDIS_CLONE_REPL_H
