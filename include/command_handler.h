#ifndef REDIS_CLONE_COMMAND_HANDLER_H
#define REDIS_CLONE_COMMAND_HANDLER_H

#include <include/resp_parser.h>
#include <include/data_types/hash_table.h>

typedef struct {
    char *buf;
    size_t capacity;
    size_t len;
} getall_ctx;

size_t append_fmt(char *response, size_t response_size, size_t offset, const char *fmt, ...);
size_t append_b(char *buf, size_t capacity, size_t len, sds val);
void execute_command(resp_object* resp_obj, hash_table* ht, char* response, size_t response_size);
int handle_connection_commands(sds cmd, resp_object *resp_obj, char *response, size_t response_size);
int handle_key_commands(sds cmd, resp_object* resp_obj, hash_table* ht, char* response, size_t response_size);
int handle_string_commands(sds cmd, resp_object *resp_obj, hash_table *ht, char *response, size_t response_size);
int handle_hash_commands(sds cmd, resp_object *resp_obj, hash_table *ht, char *response, size_t response_size);
int handle_set_commands(sds cmd, resp_object *resp_obj, hash_table *ht, char *response, size_t response_size);
int handle_zset_commands(sds cmd, resp_object *resp_obj, hash_table *ht, char *response, size_t response_size);

#endif //REDIS_CLONE_COMMAND_HANDLER_H
