#include <stdio.h>

#include <include/client.h>
#include <include/redis_object.h>
#include <include/server.h>
#include <include/config.h>

#include "include/aof_handler.h"

int main(int argc, char **argv) {
    hash_table *ht = ht_create((ht_value_free_fn) redis_object_free);
    if (argc > 1) {
        return run_client();
    }

    const char *cfg_path = "redis.conf";
    if (!init_config(cfg_path)) {
        fprintf(stderr, "redis.conf initialization failed\n");
        return 0;
    }

    char *password = get_config("requirepass");
    if (password) {
        set_required_password(password);
    }

    const char *aof_path = get_config("aof_path");
    if (!aof_path) { fprintf(stderr, "aof_path initialization failed\n"); return 0; }

    aof_handler aof_handler;
    if (!aof_handler_init(&aof_handler, aof_path)) {
        fprintf(stderr, "aof_handler initialization failed\n");
        return 0;
    }
    return init_socket(ht, &aof_handler.queue);
}
