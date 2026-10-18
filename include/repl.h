#ifndef REDIS_CLONE_REPL_H
#define REDIS_CLONE_REPL_H

#include "server.h"

void repl_init(redis_db *db);
void execute_with_str(char *args);

#endif //REDIS_CLONE_REPL_H
