
#include <ctype.h>
#include <stdio.h>
#include <redis_object.h>
#include <server.h>
#include <config.h>
#include <aof_handler.h>
#include <command_handler.h>

int main(int argc, char **argv) {
    hash_table *ht = ht_create((ht_value_free_fn) redis_object_free, 0, HT_DEFAULT_CAPACITY);

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
    if (!aof_handler_init(&aof_handler, &cfg, ht)) {
        fprintf(stderr, "aof_handler initialization failed\n");
        return 0;
    }
    int enabled = sdscmp(get_config(&cfg, "appendonly"), sdsnew("yes")) == 0;
    if (enabled) return init_socket(ht, &aof_handler.queue);
    return init_socket(ht, NULL);
}