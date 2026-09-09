#ifndef REDIS_CLONE_SERVER_H
#define REDIS_CLONE_SERVER_H

#ifdef _WIN32
#include <winsock2.h>
#else
typedef int SOCKET;
#endif

#include <include/resp_parser.h>
#include <include/data_types/hash_table.h>

#define PORT 6379
#define MAX_CONNECTIONS 10

void handle_client_request(const SOCKET* client_sock, resp_object* resp_obj, hash_table* ht);
int init_socket(hash_table* ht);

#endif //REDIS_CLONE_SERVER_H
