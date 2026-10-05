//
// Created by turker on 18.09.2026.
//

#ifndef REDIS_CLONE_COMMAND_HANDLER_H
#define REDIS_CLONE_COMMAND_HANDLER_H

#include "resp_parser.h"
#include "data_types/hash_table.h"

typedef struct {
    sds out;
} getall_ctx;

typedef struct {
    int authenticated;
    sds pending;
} client_ctx;

typedef void (command_fun)(resp_object *args, hash_table *db, sds *out);

#define CMD_NOAUTH (1 << 0)
#define CMD_WRITE (1 << 1)

#define SET_NX (1 << 0) // adds if key doesnt exists
#define SET_XX (1 << 1) // adds if key exists
#define SET_GET (1 << 2) // returns older value
#define SET_KEEPTTL (1 << 3)
#define SET_EX (1 << 4) // sets ttl in seconds
#define SET_PX (1 << 5) // sets ttl in milis

#define SET_EXPIRE_MASK (SET_EX | SET_PX | SET_KEEPTTL)
#define SET_EXISTENCE_MASK (SET_NX | SET_XX)

typedef struct {
    const char *name;
    int argc;
    command_fun *fun;
    int flags;
} command;

void append_fmt(sds *out, const char *fmt, ...);
void append_b(sds *out, sds val);
void set_required_password(const char *password);
int auth_required(void);
const command* execute_command(resp_object *args, hash_table *db, client_ctx *client, sds *out);

#endif //REDIS_CLONE_COMMAND_HANDLER_H
