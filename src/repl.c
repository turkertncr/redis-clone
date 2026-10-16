#include <stdio.h>
#include <alloc.h>
#include <data_types/hash_table.h>
#include <repl.h>
#include <command_handler.h>
#include <resp_parser.h>

static hash_table *ht;

static void execute_inline(char *args[], size_t args_size) {
    sds resp = build_resp(args, args_size);

    resp_object *resp_obj = NULL;
    resp_status status = parse_resp(resp, resp + sdslen(resp), &resp_obj, NULL);
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
    execute_inline(cmd.args, cmd.len);
    zfree(cmd.args);
}
