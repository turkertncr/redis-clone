
#include <pthread.h>
#include <stdio.h>
#include <aof_handler.h>
#include <util.h>
#include <config.h>
#include <string.h>
#include <command_handler.h>
#include <errno.h>
#include <log.h>
#include <resp_parser.h>
#include <server.h>

void aof_read(aof_handler *handler, redis_db *db) {
    sds buffer = sdsempty();
    char chunk[1024];
    long long start = current_time_ms();

    resp_value *args = 0;
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
            }
        } while (status == RESP_OK);
    }
    sdsfree(buffer);

    long long end = current_time_ms();
    log_info("DB loaded from aof: %lld seconds", end - start);
}

void* aof_write(void *args) {
    // todo check result of fprintf, fflush, fsync
    aof_handler *handler = args;
    long long last = current_time_ms();

    queue *q = &handler->queue;
    sds data;
    int dirty = 0;
    while (1) {
        queue_status status = queue_pop(q, &data);
        aofsync sync = handler->aofsync;
        long long now = current_time_ms();

        if (status == OK) {
            dirty = 1;
            fprintf(handler->file, "%s", data);
            fflush(handler->file);
            if (sync == ALWAYS) fsync(fileno(handler->file));
        }
        else if (status == SHUTDOWN) break;

        if (sync == EVERYSEC && dirty && now - last >= 1000) {
            fsync(fileno(handler->file));
            last = now;
            dirty = 0;
        }
    }
    return 0;
}

int aof_handler_init(aof_handler *handler, config *cfg, redis_db *db) {
    if (!handler) { return 0; }

    sds aof_path = get_config(cfg, "aofpath");
    if (!aof_path) {
        log_err("Could not get aofpath");
        return 0;
    }

    handler->file = fopen(aof_path, "ab+");
    if (!handler->file) {
        log_err("%s fopen failed %s", aof_path, strerror(errno));
        return 0;
    }
    aof_read(handler, db);

    sds aof_sync = get_config(cfg, "appendfsync");
    if (aof_sync) {
        if (strcmp(aof_sync, "always") == 0) { handler->aofsync = ALWAYS; }
        if (strcmp(aof_sync, "everysec") == 0) { handler->aofsync = EVERYSEC; }
        if (strcmp(aof_sync, "no") == 0) { handler->aofsync = NO; }
    } else {
        handler->aofsync = EVERYSEC;
    }

    queue_init(&handler->queue);
    pthread_create(&handler->thread, NULL, aof_write, handler);
    return 1;
}
