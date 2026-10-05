#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <include/command_handler.h>
#include <include/data_types/hash_table.h>
#include <include/repl.h>

#define SPLIT_MAX_ARGS 64

static hash_table *ht;

struct split split_cmd(char *args) {
    struct split result = { .len = 0, .args = malloc(sizeof(char*) * SPLIT_MAX_ARGS) };

    char *tok = strtok(args, " \t\r\n");
    while (tok != NULL && result.len < SPLIT_MAX_ARGS) {
        result.args[result.len++] = tok;
        tok = strtok(NULL, " \t\r\n");
    }

    return result;
}

sds build_resp(char **resp_arr, size_t resp_arr_size) {
    sds out = sdsempty();
    append_fmt(&out, "*%zu\r\n", resp_arr_size);

    for (size_t i = 0; i < resp_arr_size; i++) {
        sds val = sdsnew(resp_arr[i]);
        append_b(&out, val);
        sdsfree(val);
    }
    return out;
}

static void execute(char *args[], size_t args_size) {
    sds resp = build_resp(args, args_size);

    resp_object *resp_obj = NULL;
    resp_status status = parse_resp(resp, resp + sdslen(resp), &resp_obj);
    if (status != OK) {
        printf("invalid command\n");
        sdsfree(resp);
        return;
    }

    client_ctx local = {1};
    sds out = sdsempty();
    execute_command(resp_obj, ht, &local, &out);
    fwrite(out, 1, sdslen(out), stdout);
    putchar('\n');

    sdsfree(out);
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
