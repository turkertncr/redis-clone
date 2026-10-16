
#include <stdio.h>
#include <stdlib.h>
#include <alloc.h>
#include <resp_parser.h>
#include <string.h>

void free_respobj(resp_object* args) {
    if (args == NULL) return;

    switch (args->type) {
        case RESP_STRING:
            sdsfree(args->str);
            break;
        case RESP_ERROR:
            zfree(args->str);
            break;
        case RESP_BULK:
            sdsfree(args->bulk);
            break;
        case RESP_ARRAY:
            for (int i = 0; i < args->array.len; i++) {
                free_respobj(args->array.ptr[i]);
            }
            zfree(args->array.ptr);
            break;
        default:
            break;
    }

    zfree(args);
}

void free_array_items(resp_object** ptr, int len) {
    if (ptr == NULL) return;
    for (int i = 0; i < len; i++) {
        free_respobj(ptr[i]);
    }
    zfree(ptr);
}

resp_status parse_bulk(sds input, const char* end, resp_object* args, sds* next) {

    char* f_crlf = strstr(input, "\r\n");
    if (f_crlf == NULL) {
        return RESP_INCOMPLETE;
    }

    char* num_end;
    long long length = strtoll(input + 1, &num_end, 10);

    if (num_end != f_crlf) {
        args->type = RESP_nil;
        return RESP_ERR;
    }

    if (length < -1) {
        args->type = RESP_nil;
        return RESP_ERR;
    }

    if (length == -1) {
        args->type = RESP_BULK;
        args->bulk = NULL;
        *next = f_crlf + 2;
        return RESP_OK;
    }

    char* str = f_crlf + 2;
    long long available = end - str;
    if (available < length + 2) {
        return RESP_INCOMPLETE;
    }
    if (str[length] != '\r' || str[length + 1] != '\n') {
        args->type = RESP_nil;
        return RESP_ERR;
    }

    char* x = sdsnewlen(str, length);
    if (x == NULL) {
        printf("Allocation failed");
        args->type = RESP_nil;
        return RESP_ERR;
    }

    args->type = RESP_BULK;
    args->bulk = x;

    *next = str + length + 2;
    return RESP_OK;
}

resp_status parse_string(sds input, resp_object* args, sds* next) {
    char buff[256];

    char* crlf = strstr(input, "\r\n");
    if (!crlf) {
        return RESP_INCOMPLETE;
    }

    if (sscanf(input, "+%255[^\r\n]", buff) != 1) {
        args->type = RESP_nil;
        return RESP_ERR;
    }

    char* str = sdsnew(buff);
    if (str == NULL) {
        args->type = RESP_nil;
        return RESP_ERR;
    }

    args->type = RESP_STRING;
    args->str = str;
    *next = crlf + 2;
    return RESP_OK;
}

resp_status parse_error(sds input, resp_object* args, sds* next) {
    char buff[256];

    char* crlf = strstr(input, "\r\n");
    if (!crlf) {
        return RESP_INCOMPLETE;
    }

    if (sscanf(input, "-%255[^\r\n]", buff) != 1) {
        args->type = RESP_nil;
        return RESP_ERR;
    }

    char* str = strdup(buff);
    if (str == NULL) {
        args->type = RESP_nil;
        return RESP_ERR;
    }

    args->type = RESP_ERROR;
    args->str = str;
    *next = crlf + 2;
    return RESP_OK;
}

resp_status parse_int(sds input, resp_object* args, sds* next) {

    char* crlf = strstr(input, "\r\n");
    if (!crlf) {
        return RESP_INCOMPLETE;
    }

    if (sscanf(input, ":%lld", &args->integer) != 1) {
        printf("Invalid number format! \n");
        args->type = RESP_nil;
        return RESP_ERR;
    }

    args->type = RESP_INT;
    *next = crlf + 2;
    return RESP_OK;
}

resp_status parse_array(sds input, const char* end, resp_object* args, sds* next) {

    char* crlf = strstr(input, "\r\n");
    if (!crlf) {
        return RESP_INCOMPLETE;
    }

    char* num_end;
    long long array_len = strtoll(input + 1, &num_end, 10);
    if (num_end != crlf || array_len < 0) {
        args->type = RESP_nil;
        return RESP_ERR;
    }

    if (array_len == 0) {
        args->type = RESP_ARRAY;
        args->array.len = 0;
        args->array.ptr = NULL;
        *next = crlf + 2;
        return RESP_OK;
    }

    args->type = RESP_ARRAY;
    args->array.len = (int) array_len;
    args->array.ptr = zalloc(array_len * sizeof(resp_object*));
    if (args->array.ptr == NULL) {
        args->type = RESP_nil;
        return RESP_ERR;
    }

    char* data = crlf + 2;
    for (int i = 0; i < array_len; i++) {
        if (data >= end) {
            free_array_items(args->array.ptr, i);
            args->array.ptr = NULL;
            args->array.len = 0;
            return RESP_INCOMPLETE;
        }
        char type = data[0];

        resp_object* token = zalloc(sizeof(*token));
        if (token == NULL) {
            free_array_items(args->array.ptr, i);
            args->array.ptr = NULL;
            args->array.len = 0;
            args->type = RESP_nil;
            return RESP_ERR;
        }
        *token = (resp_object){0};

        sds token_next = NULL;
        resp_status status = handle_type(data, end, type, token, &token_next);
        if (status != RESP_OK) {
            free_respobj(token);
            free_array_items(args->array.ptr, i);
            args->array.ptr = NULL;
            args->array.len = 0;
            if (status == RESP_INCOMPLETE) {
                return RESP_INCOMPLETE;
            }
            args->type = RESP_nil;
            return RESP_ERR;
        }

        args->array.ptr[i] = token;
        data = token_next;
    }

    *next = data;
    return RESP_OK;
}

resp_status handle_type(sds input, const char* end, char type, resp_object* args, sds* next) {
    switch (type) {
        case '+':
            return parse_string(input, args, next);
        case '-' :
            return parse_error(input, args, next);
        case ':' :
            return parse_int(input, args, next);
        case '$' :
            return parse_bulk(input, end, args, next);
        case '*' :
            return parse_array(input, end, args, next);
        default :
            args->type = RESP_nil;
            return RESP_ERR;
    }
}

void print_respobj(resp_object* args, int depth) {
    if (args == NULL) {
        printf("%*s(null)\n", depth * 2, "");
        return;
    }

    switch (args->type) {
        case RESP_INT:
            printf("%*s(integer) %lld\n", depth * 2, "", args->integer);
            break;
        case RESP_STRING:
            printf("%*s+%s\n", depth * 2, "", args->str);
            break;
        case RESP_ERROR:
            printf("%*s-%s\n", depth * 2, "", args->str);
            break;
        case RESP_BULK:
            if (sdslen(args->bulk) == -1) {
                printf("%*s(nil)\n", depth * 2, "");
            } else {
                printf("%*s\"%.*s\"\n", depth * 2, "", sdslen(args->bulk), args->bulk);
            }
            break;
        case RESP_ARRAY:
            printf("%*s(array) len=%d\n", depth * 2, "", args->array.len);
            for (int i = 0; i < args->array.len; i++) {
                print_respobj(args->array.ptr[i], depth + 1);
            }
            break;
        case RESP_nil:
        default:
            printf("%*s(nil type)\n", depth * 2, "");
            break;
    }
}

resp_status parse_resp(sds input, const char* end, resp_object** args_out, sds *next) {
    if (input == NULL || input >= end) return RESP_INCOMPLETE;

    char type = *input;
    resp_object* args = zalloc(sizeof(*args));
    if (args == NULL) return RESP_ERR;
    *args = (resp_object){0};

    sds tail = NULL;
    resp_status status = handle_type(input, end, type, args, &tail);
    if (status != RESP_OK) {
        free_respobj(args);
        return status;
    }

    *args_out = args;
    if (tail != NULL) { *next = tail; }
    return status;
}

sds get_bulk_at(resp_object* resp_obj, int index) {
    if (index < 0 || index >= resp_obj->array.len) { return NULL; }
    const resp_object *resp = resp_obj->array.ptr[index];
    if (resp->type != RESP_BULK) { return NULL; }
    return resp->bulk;
}
