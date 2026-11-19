
#ifndef REDIS_CLONE_LOG_H
#define REDIS_CLONE_LOG_H

typedef enum {
    INFO,
    WARN,
    ERR
} log_level;

#if defined(__GNUC__) || defined(__clang__)
#define LOG_PRINTF(fmt_idx, args_idx) __attribute__((format(printf, fmt_idx, args_idx)))
#else
#define LOG_PRINTF(fmt_idx, args_idx)
#endif

void logger(log_level level, const char *fmt, ...) LOG_PRINTF(2, 3);
void log_info(const char *fmt, ...) LOG_PRINTF(1, 2);
void log_warn(const char *fmt, ...) LOG_PRINTF(1, 2);
void log_err(const char *fmt, ...) LOG_PRINTF(1, 2);

#endif //REDIS_CLONE_LOG_H
