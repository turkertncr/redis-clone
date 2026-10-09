
#include <stdio.h>
#include <stdlib.h>
#include <include/resp_parser.h>
#include <string.h>

void free_respobj(resp_object* obj) {
    if (obj == NULL) return;

    switch (obj->type) {
        case RESP_STRING:
            sdsfree(obj->str);
            break;
        case RESP_ERROR:
            free(obj->str);
            break;
        case RESP_BULK:
            sdsfree(obj->bulk);
            break;
        case RESP_ARRAY:
            for (int i = 0; i < obj->array.len; i++) {
                free_respobj(obj->array.ptr[i]);
            }
            free(obj->array.ptr);
            break;
        default:
            break;
    }

    free(obj);
}

void free_array_items(resp_object** ptr, int len) {
    if (ptr == NULL) return;
    for (int i = 0; i < len; i++) {
        free_respobj(ptr[i]);
    }
    free(ptr);
}

resp_status parse_bulk(sds input, const char* end, resp_object* obj, sds* next) {

    char* f_crlf = strstr(input, "\r\n");
    if (f_crlf == NULL) {
        return INCOMPLETE;
    }

    char* num_end;
    long long length = strtoll(input + 1, &num_end, 10);

    if (num_end != f_crlf) {
        obj->type = RESP_nil;
        return ERROR;
    }

    if (length < -1) {
        obj->type = RESP_nil;
        return ERROR;
    }

    if (length == -1) {
        obj->type = RESP_BULK;
        obj->bulk = NULL;
        *next = f_crlf + 2;
        return OK;
    }

    char* str = f_crlf + 2;
    long long available = end - str;
    if (available < length + 2) {
        return INCOMPLETE;
    }
    if (str[length] != '\r' || str[length + 1] != '\n') {
        obj->type = RESP_nil;
        return ERROR;
    }

    char* x = sdsnewlen(str, length);
    if (x == NULL) {
        printf("Allocation failed");
        obj->type = RESP_nil;
        return ERROR;
    }

    obj->type = RESP_BULK;
    obj->bulk = x;

    *next = str + length + 2;
    return OK;
}

resp_status parse_string(sds input, resp_object* obj, sds* next) {
    char buff[256];

    char* crlf = strstr(input, "\r\n");
    if (!crlf) {
        return INCOMPLETE;
    }

    if (sscanf(input, "+%255[^\r\n]", buff) != 1) {
        obj->type = RESP_nil;
        return ERROR;
    }

    char* str = sdsnew(buff);
    if (str == NULL) {
        obj->type = RESP_nil;
        return ERROR;
    }

    obj->type = RESP_STRING;
    obj->str = str;
    *next = crlf + 2;
    return OK;
}

resp_status parse_error(sds input, resp_object* obj, sds* next) {
    char buff[256];

    char* crlf = strstr(input, "\r\n");
    if (!crlf) {
        return INCOMPLETE;
    }

    if (sscanf(input, "-%255[^\r\n]", buff) != 1) {
        obj->type = RESP_nil;
        return ERROR;
    }

    char* str = strdup(buff);
    if (str == NULL) {
        obj->type = RESP_nil;
        return ERROR;
    }

    obj->type = RESP_ERROR;
    obj->str = str;
    *next = crlf + 2;
    return OK;
}

resp_status parse_int(sds input, resp_object* obj, sds* next) {

    char* crlf = strstr(input, "\r\n");
    if (!crlf) {
        return INCOMPLETE;
    }

    if (sscanf(input, ":%lld", &obj->integer) != 1) {
        printf("Invalid number format! \n");
        obj->type = RESP_nil;
        return ERROR;
    }

    obj->type = RESP_INT;
    *next = crlf + 2;
    return OK;
}

resp_status parse_array(sds input, const char* end, resp_object* obj, sds* next) {

    char* crlf = strstr(input, "\r\n");
    if (!crlf) {
        return INCOMPLETE;
    }

    char* num_end;
    long long array_len = strtoll(input + 1, &num_end, 10);
    if (num_end != crlf || array_len < 0) {
        obj->type = RESP_nil;
        return ERROR;
    }

    if (array_len == 0) {
        obj->type = RESP_ARRAY;
        obj->array.len = 0;
        obj->array.ptr = NULL;
        *next = crlf + 2;
        return OK;
    }

    obj->type = RESP_ARRAY;
    obj->array.len = (int) array_len;
    obj->array.ptr = malloc(array_len * sizeof(resp_object*));
    if (obj->array.ptr == NULL) {
        obj->type = RESP_nil;
        return ERROR;
    }

    char* data = crlf + 2;
    for (int i = 0; i < array_len; i++) {
        if (data >= end) {
            free_array_items(obj->array.ptr, i);
            obj->array.ptr = NULL;
            obj->array.len = 0;
            return INCOMPLETE;
        }
        char type = data[0];

        resp_object* token = malloc(sizeof(*token));
        if (token == NULL) {
            free_array_items(obj->array.ptr, i);
            obj->array.ptr = NULL;
            obj->array.len = 0;
            obj->type = RESP_nil;
            return ERROR;
        }
        *token = (resp_object){0};

        sds token_next = NULL;
        resp_status status = handle_type(data, end, type, token, &token_next);
        if (status != OK) {
            free_respobj(token);
            free_array_items(obj->array.ptr, i);
            obj->array.ptr = NULL;
            obj->array.len = 0;
            if (status == INCOMPLETE) {
                return INCOMPLETE;
            }
            obj->type = RESP_nil;
            return ERROR;
        }

        obj->array.ptr[i] = token;
        data = token_next;
    }

    *next = data;
    return OK;
}

resp_status handle_type(sds input, const char* end, char type, resp_object* obj, sds* next) {
    switch (type) {
        case '+':
            return parse_string(input, obj, next);
        case '-' :
            return parse_error(input, obj, next);
        case ':' :
            return parse_int(input, obj, next);
        case '$' :
            return parse_bulk(input, end, obj, next);
        case '*' :
            return parse_array(input, end, obj, next);
        default :
            obj->type = RESP_nil;
            return ERROR;
    }
}

void print_respobj(resp_object* obj, int depth) {
    if (obj == NULL) {
        printf("%*s(null)\n", depth * 2, "");
        return;
    }

    switch (obj->type) {
        case RESP_INT:
            printf("%*s(integer) %lld\n", depth * 2, "", obj->integer);
            break;
        case RESP_STRING:
            printf("%*s+%s\n", depth * 2, "", obj->str);
            break;
        case RESP_ERROR:
            printf("%*s-%s\n", depth * 2, "", obj->str);
            break;
        case RESP_BULK:
            if (sdslen(obj->bulk) == -1) {
                printf("%*s(nil)\n", depth * 2, "");
            } else {
                printf("%*s\"%.*s\"\n", depth * 2, "", sdslen(obj->bulk), obj->bulk);
            }
            break;
        case RESP_ARRAY:
            printf("%*s(array) len=%d\n", depth * 2, "", obj->array.len);
            for (int i = 0; i < obj->array.len; i++) {
                print_respobj(obj->array.ptr[i], depth + 1);
            }
            break;
        case RESP_nil:
        default:
            printf("%*s(nil type)\n", depth * 2, "");
            break;
    }
}

resp_status parse_resp(sds input, const char* end, resp_object** obj_out, sds *next) {
    if (input == NULL || input >= end) return INCOMPLETE;

    char type = *input;
    resp_object* obj = malloc(sizeof(*obj));
    if (obj == NULL) return ERROR;
    *obj = (resp_object){0};

    sds tail = NULL;
    resp_status status = handle_type(input, end, type, obj, &tail);
    if (status != OK) {
        free_respobj(obj);
        return status;
    }

    *obj_out = obj;
    if (tail != NULL) { *next = tail; }
    return status;
}

sds get_bulk_at(resp_object* resp_obj, int index) {
    if (index < 0 || index >= resp_obj->array.len) { return NULL; }
    const resp_object *resp = resp_obj->array.ptr[index];
    if (resp->type != RESP_BULK) { return NULL; }
    return resp->bulk;
}
