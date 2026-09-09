
#include <include/command_handler.h>
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

char* parse_bulk(sds input, resp_object* obj) {

    char* f_crlf = strstr(input, "\r\n");
    if (f_crlf == NULL) {
        obj->type = RESP_nil;
        return NULL;
    }

    char* end;
    long long length = strtoll(input + 1, &end, 10);

    if (end != f_crlf) {
        obj->type = RESP_nil;
        return NULL;
    }

    if (length == -1) {
        obj->type = RESP_BULK;
        obj->bulk = NULL;
        return f_crlf + 2;
    }

    char* str = f_crlf + 2;
    char* x =  sdsnewlen(str,length);
    if (x == NULL) {
        printf("Allocation failed");
        obj->type = RESP_nil;
        return NULL;
    }

    memcpy(x, str, length);
    x[length] = '\0';

    obj->type = RESP_BULK;
    obj->bulk = x;

    return str + length + 2;
}

char* parse_string(char* input, resp_object* obj) {
    char buff[256];

    char* crlf = strstr(input, "\r\n");
    if (!crlf) {
        obj->type = RESP_nil;
        return NULL;
    }

    if (sscanf(input, "+%255[^\r\n]", buff) != 1) {
        obj->type = RESP_nil;
        return NULL;
    }

    char* str = sdsnew(buff);
    if (str == NULL) {
        obj->type = RESP_nil;
        return NULL;
    }

    obj->type = RESP_STRING;
    obj->str = str;
    return crlf + 2;
}

char* parse_error(char* input, resp_object* obj) {
    char buff[256];

    char* crlf = strstr(input, "\r\n");
    if (!crlf) {
        obj->type = RESP_nil;
        return NULL;
    }

    if (sscanf(input, "-%255[^\r\n]", buff) != 1) {
        obj->type = RESP_nil;
        return NULL;
    }

    char* str = strdup(buff);
    if (str == NULL) {
        obj->type = RESP_nil;
        return NULL;
    }

    obj->type = RESP_ERROR;
    obj->str = str;
    return crlf + 2;
}

char* parse_int(sds input, resp_object* obj) {

    char* crlf = strstr(input, "\r\n");
    if (!crlf) {
        obj->type = RESP_nil;
        return NULL;
    }

    if (sscanf(input, ":%lld", &obj->integer) != 1) {
        printf("Invalid number format! \n");
        obj->type = RESP_nil;
        return NULL;
    }

    obj->type = RESP_INT;
    return crlf + 2;
}

char* parse_array(sds input, resp_object* obj) {

    char* crlf = strstr(input, "\r\n");
    if (!crlf) {
        obj->type = RESP_nil;
        return NULL;
    }

    char* end;
    long long array_len = strtoll(input + 1, &end, 10);
    if (end != crlf || array_len < 0) {
        obj->type = RESP_nil;
        return NULL;
    }

    if (array_len == 0) {
        obj->type = RESP_ARRAY;
        obj->array.len = 0;
        obj->array.ptr = NULL;
        return crlf + 2;
    }

    obj->type = RESP_ARRAY;
    obj->array.len = (int) array_len;
    obj->array.ptr = malloc(array_len * sizeof(resp_object*));
    if (obj->array.ptr == NULL) {
        obj->type = RESP_nil;
        return NULL;
    }

    char* data = crlf + 2;
    for (int i = 0; i < array_len; i++) {
        char type = data[0];

        resp_object* token = malloc(sizeof(*token));
        if (token == NULL) {
            free_array_items(obj->array.ptr, i);
            obj->type = RESP_nil;
            return NULL;
        }
        *token = (resp_object){0};

        data = handle_type(data, type, token);
        if (!data) {
            free_respobj(token);
            free_array_items(obj->array.ptr, i);
            obj->type = RESP_nil;
            return NULL;
        }

        obj->array.ptr[i] = token;
    }

    return data;
}

char* handle_type(sds input, char type, resp_object* obj) {
    switch (type) {
        case '+':
            return parse_string(input, obj);
        case '-' :
            return parse_error(input, obj);
        case ':' :
            return parse_int(input, obj);
        case '$' :
            return parse_bulk(input, obj);
        case '*' :
            return parse_array(input, obj);
        default :
            obj->type = RESP_nil;
            return NULL;
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

resp_object* parse_resp(sds input) {
    if (input == NULL) return NULL;

    char type = *input;
    resp_object* obj = malloc(sizeof(*obj));
    if (obj == NULL) return NULL;

    handle_type(input, type, obj);
    return obj;
}

sds get_bulk_at(resp_object* resp_obj, int index) {
    if (index < 0 || index >= resp_obj->array.len) { return NULL; }
    return resp_obj->array.ptr[index]->bulk;
}