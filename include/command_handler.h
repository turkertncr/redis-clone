//
// Created by turker on 18.09.2026.
//

#ifndef REDIS_CLONE_COMMAND_HANDLER_H
#define REDIS_CLONE_COMMAND_HANDLER_H

#include "resp_parser.h"
#include "data_types/hash_table.h"

typedef struct {
    char *buf;
    size_t capacity;
    size_t len;
} getall_ctx;

typedef struct {
    int authenticated;
} client_ctx;

typedef void (command_fun)(resp_object *args, hash_table *db, char *response, size_t response_size);

#define CMD_NOAUTH (1 << 0)

typedef struct {
    const char *name;
    int arity;
    command_fun *fun;
    int flags;
} command;

size_t append_fmt(char *response, size_t response_size, size_t offset, const char *fmt, ...);
size_t append_b(char *buf, size_t capacity, size_t len, sds val);
void set_required_password(const char *password);
int auth_required(void);
void execute_command(resp_object *args, hash_table *db, client_ctx *client, char *response, size_t response_size);

#endif //REDIS_CLONE_COMMAND_HANDLER_H
