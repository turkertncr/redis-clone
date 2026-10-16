
#include <pthread.h>
#include <stdio.h>
#include <aof_handler.h>
#include <util.h>
#include <config.h>
#include <string.h>
#include <command_handler.h>
#include <resp_parser.h>

void aof_read(aof_handler *handler, hash_table *db) {
    sds buffer = sdsempty();
    char chunk[1024];

    resp_object *args = 0;
    size_t offset = 0;
    size_t read_bytes;
    while ((read_bytes = fread(chunk, 1, sizeof(chunk), handler->file)) > 0) {
        buffer = sdsappendlen(buffer, chunk, read_bytes);

        resp_status status;
        do {
            sds next = 0;
            status = parse_resp(buffer + offset, buffer + sdslen(buffer), &args, &next);
            if (status == RESP_OK) {
                offset = next - buffer;
                sds out = sdsempty();
                execute(args, db, &out);
                // print_respobj(args, 25);
                // fprintf(stderr, "%s\n", out);
            }
        } while (status == RESP_OK);
    }
    sdsfree(buffer);
}

void* aof_write(void *args) {

    aof_handler *handler = args;
    long long last = current_time_ms();

    queue *q = &handler->queue;
    sds data = queue_pop(q);
    while (data != NULL) {
        fprintf(handler->file, "%s", data);
        aofsync sync = handler->aofsync;
        if (sync != NO) {
            if (sync == EVERYSEC) {
                long long now = current_time_ms();
                if (now - last >= 1000) {
                    fflush(handler->file);
                    last = now;
                }
            } else {
                fflush(handler->file);
            }
        }
        sdsfree(data);
        data = queue_pop(q);
    }

    return 0;
}

int aof_handler_init(aof_handler *handler, config *cfg, hash_table *db) {
    if (!handler) { return 0; }

    sds aof_path = get_config(cfg, "aofpath");
    handler->file = fopen(aof_path, "ab+");
    if (!handler->file) {
        fprintf(stderr, "%s fopen failed\n", __func__);
        return 0;
    }
    aof_read(handler, db);

    sds aof_sync = get_config(cfg, "appendfsync");
    if (strcmp(aof_sync, "always") == 0) { handler->aofsync = ALWAYS; }
    if (strcmp(aof_sync, "everysec") == 0) { handler->aofsync = EVERYSEC; }
    if (strcmp(aof_sync, "no") == 0) { handler->aofsync = NO; }

    queue_init(&handler->queue);
    pthread_create(&handler->thread, NULL, aof_write, handler);
    return 1;
}
