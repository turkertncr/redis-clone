
#include <ctype.h>
#include <stdio.h>
#include <server.h>
#include <config.h>
#include <aof_handler.h>
#include <command_handler.h>



int main(int argc, char **argv) {
    redis_db redis_db;
    db_init(&redis_db);

    config cfg;
    const char *cfg_path = argc > 1 ? argv[1] : "redis.conf";
    if (!init_config(&cfg, cfg_path)) {
        fprintf(stderr, "redis.conf initialization failed\n");
        return 0;
    }

    char *password = get_config(&cfg, "requirepass");
    if (password) {
        set_required_password(password);
    }

    aof_handler aof_handler;
    if (!aof_handler_init(&aof_handler, &cfg, &redis_db)) {
        fprintf(stderr, "aof_handler initialization failed\n");
        return 0;
    }
    int enabled = sdscmp(get_config(&cfg, "appendonly"), sdsnew("yes")) == 0;
    if (enabled) return init_socket(&redis_db, &aof_handler.queue);
    return init_socket(&redis_db, NULL);
}