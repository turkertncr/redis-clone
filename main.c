
#include "include/client.h"
#include "include/redis_object.h"
#include "include/server.h"

int main(int argc, char **argv) {
    hash_table *ht = ht_create((ht_value_free_fn) redis_object_free);
    if (argc > 1) {
        return run_client();
    }
    return init_socket(ht);
}
