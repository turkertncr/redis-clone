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

typedef void (command_fun)(resp_object *args, hash_table *db, char *response, size_t response_size);

typedef struct {
    const char *name;
    int arity;
    command_fun *fun;
} command;

size_t append_fmt(char *response, size_t response_size, size_t offset, const char *fmt, ...);
size_t append_b(char *buf, size_t capacity, size_t len, sds val);
void execute_command(resp_object *args, hash_table *db, char *response, size_t response_size);

#endif //REDIS_CLONE_COMMAND_HANDLER_H
