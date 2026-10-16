#ifndef REDIS_CLONE_SERVER_H
#define REDIS_CLONE_SERVER_H

#ifdef _WIN32
#include <winsock2.h>
#else
typedef int SOCKET;
#endif

#include <data_types/hash_table.h>
#include <queue.h>

#define PORT 6379
#define MAX_CONNECTIONS 16

int init_socket(hash_table* ht, queue *q);

#endif //REDIS_CLONE_SERVER_H
