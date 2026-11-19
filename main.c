
#include <ctype.h>
#include <stdio.h>
#include <server.h>
#include <config.h>
#include <aof_handler.h>
#include <command_handler.h>
#include <log.h>
#ifndef _WIN32
#include <signal.h>
#endif

int main(int argc, char **argv) {
#ifndef _WIN32
    signal(SIGPIPE, SIG_IGN);
#endif

    redis_db redis_db;
    db_init(&redis_db);
    command_handler_init();

    config cfg;
    const char *cfg_path = argc > 1 ? argv[1] : "redis.conf";
    if (!init_config(&cfg, cfg_path)) {
        log_err("Redis configuration initialization failed");
        return 1;
    }

    char *password = get_config(&cfg, "requirepass");
    if (password) {
        set_required_password(password);
    }

    aof_handler aof_handler;
    if (!aof_handler_init(&aof_handler, &cfg, &redis_db)) {
        log_err("AOF initialization failed");
        return 0;
    }
    int enabled = sdscmp(get_config(&cfg, "appendonly"), sdsnew("yes")) == 0;
    if (enabled) return init_socket(&redis_db, &aof_handler.queue);

    return init_socket(&redis_db, NULL);
}