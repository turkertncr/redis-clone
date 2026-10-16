
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#include <alloc.h>
#include <util.h>

#define SPLIT_MAX_ARGS 64

long long current_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return ((long long)tv.tv_sec * 1000) + (tv.tv_usec / 1000);
}

void append_fmt(sds *out, const char *fmt, ...) {
    char tmp[256];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    if (n < 0) return;
    if ((size_t) n >= sizeof(tmp)) n = sizeof(tmp) - 1; /* vsnprintf truncated; don't read past tmp */
    *out = sdsappendlen(*out, tmp, n);
}

void append_b(sds *out, sds val) {
    if (val == NULL) { append_fmt(out, "$-1\r\n"); return; }
    append_fmt(out, "$%u\r\n", sdslen(val));
    *out = sdsappendlen(*out, val, sdslen(val));
    *out = sdsappendlen(*out, "\r\n", 2);
}

struct split split_cmd(char *args) {
    struct split result = { .len = 0, .args = zalloc(sizeof(char*) * SPLIT_MAX_ARGS) };

    char *tok = strtok(args, " \t\r\n");
    while (tok != NULL && result.len < SPLIT_MAX_ARGS) {
        result.args[result.len++] = tok;
        tok = strtok(NULL, " \t\r\n");
    }

    return result;
}

sds build_resp(char **resp_arr, size_t resp_arr_size) {
    sds out = sdsempty();
    append_fmt(&out, "*%zu\r\n", resp_arr_size);

    for (size_t i = 0; i < resp_arr_size; i++) {
        sds val = sdsnew(resp_arr[i]);
        append_b(&out, val);
        sdsfree(val);
    }
    return out;
}
