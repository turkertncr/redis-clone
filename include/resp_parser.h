//
// Created by turker on 4.08.2026.
//

#ifndef REDIS_CLONE_RESP_PARSER_H
#define REDIS_CLONE_RESP_PARSER_H

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "data_types/redis_string.h"

typedef enum {
    RESP_nil = 0,
    RESP_INT,
    RESP_STRING,
    RESP_BULK,
    RESP_ARRAY,
    RESP_ERROR
} resp_type;

typedef struct resp_object {
    resp_type type;
    union {
        long long integer;
        sds str;
        sds bulk;
        struct {
            int len;
            struct resp_object **ptr;
        } array;
    };
} resp_object;

void free_respobj(resp_object* obj);
void free_array_items(resp_object** ptr, int len);
sds parse_bulk(sds input, resp_object* obj);
sds parse_string(sds input, resp_object* obj);
sds parse_error(sds input, resp_object* obj);
sds parse_int(sds input, resp_object* obj);
sds parse_array(sds input, resp_object* obj);
sds handle_type(sds input, char type, resp_object* obj);
void print_respobj(resp_object* obj, int depth);
resp_object* parse_resp(sds input);
sds get_bulk_at(resp_object* resp_obj, int index);

#endif //REDIS_CLONE_RESP_PARSER_H
