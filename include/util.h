
#ifndef REDIS_CLONE_UTIL_H
#define REDIS_CLONE_UTIL_H

#include <stddef.h>
#include <data_types/redis_string.h>

struct split {
    size_t len;
    char** args;
};

long long current_time_ms(void);

void append_fmt(sds *out, const char *fmt, ...);
void append_b(sds *out, sds val);

struct split split_cmd(char *args);
sds build_resp(char **resp_arr, size_t resp_arr_size);

#endif //REDIS_CLONE_UTIL_H
