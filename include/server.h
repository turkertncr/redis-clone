#ifndef REDIS_CLONE_SERVER_H
#define REDIS_CLONE_SERVER_H
#include "redis_db.h"

#ifdef _WIN32
#include <winsock2.h>
#else
typedef int SOCKET;
#endif

#include <queue.h>

#define PORT 6379
#define MAX_CONNECTIONS 16

int init_socket(redis_db *redis_db, queue *q);

#endif //REDIS_CLONE_SERVER_H
