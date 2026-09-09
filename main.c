#include <include/command_handler.h>
#include <include/redis_object.h>
#include <include/repl.h>
#include <include/server.h>

static hash_table *ht;

int main(int argc, char **argv) {
    ht = ht_create((ht_value_free_fn) redis_object_free);
    // init_socket(ht);
   // repl_init(ht);

    //execute_with_str("PING");
}
