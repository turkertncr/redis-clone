#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <include/command_handler.h>
#include <include/data_types/hash_table.h>

#define SPLIT_MAX_ARGS 64

static hash_table *ht;

struct split {
    size_t len;
    char** args;
};

static struct split split_cmd(char *args) {
    struct split result = { .len = 0, .args = malloc(sizeof(char*) * SPLIT_MAX_ARGS) };

    char *tok = strtok(args, " ");
    while (tok != NULL && result.len < SPLIT_MAX_ARGS) {
        result.args[result.len++] = tok;
        tok = strtok(NULL, " ");
    }

    return result;
}

static getall_ctx build_resp(char **resp_arr, size_t resp_arr_size) {
    size_t resp_size = 1024;
    char *resp = malloc(resp_size);
    getall_ctx ctx = { .buf = resp, .capacity = resp_size, .len = 0 };
    ctx.len = append_fmt(ctx.buf, ctx.capacity, ctx.len, "*%zu\r\n", resp_arr_size);
    for (size_t i = 0; i < resp_arr_size; i++) {
        sds val = sdsnew(resp_arr[i]);
        ctx.len = append_b(ctx.buf, ctx.capacity, ctx.len, val);
        sdsfree(val);
    }
    return ctx;
}

static void execute(char *args[], size_t args_size) {
    size_t response_size = 16384;
    char response[response_size];

    getall_ctx ctx = build_resp(args, args_size);
    sds resp = sdsnewlen(ctx.buf, ctx.len);
    free(ctx.buf);

    resp_object *resp_obj = parse_resp(resp);
    execute_command(resp_obj, ht, response, response_size);
    printf("%s\n", response);

    sdsfree(resp);
    free_respobj(resp_obj);
}

void repl_init(hash_table *table) {
    ht = table;
}

void execute_with_str(char *args) {
    struct split cmd = split_cmd(args);
    execute(cmd.args, cmd.len);
    free(cmd.args);
}
