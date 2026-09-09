
#include <include/redis_object.h>
#include <include/client.h>
#include <include/server.h>

int main(int argc, char **argv) {
    if (argc > 1) {
        return run_client();
    }

    hash_table *ht = ht_create((ht_value_free_fn) redis_object_free);
    return init_socket(ht);
}
