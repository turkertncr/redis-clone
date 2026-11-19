
#include <log.h>
#include <stdio.h>
#include <time.h>
#include <util.h>
#include <stdarg.h>

static const char *lvltostr(log_level lvl) {
    switch (lvl) {
        case INFO: return "INFO";
        case WARN: return "WARN";
        case ERR:  return "ERR ";
        default:   return "????";
    }
}

static void vlogger(log_level level, const char *fmt, va_list ap) {
    long long now_ms = current_time_ms();
    time_t now = (time_t)(now_ms / 1000);
    int ms = (int)(now_ms % 1000);

    struct tm tm;
    if (localtime_r(&now, &tm) == NULL) {
        return;
    }

    char timebuf[64];
    strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", &tm);

    char buf[1024];
    int n = snprintf(buf, sizeof(buf), "%s.%03d [%s] ", timebuf, ms, lvltostr(level));
    if (n < 0) return;
    if ((size_t)n >= sizeof(buf)) n = sizeof(buf) - 1;

    vsnprintf(buf + n, sizeof(buf) - n, fmt, ap);

    fprintf(stderr, "%s\n", buf);
}

void logger(log_level level, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vlogger(level, fmt, ap);
    va_end(ap);
}

void log_info(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vlogger(INFO, fmt, ap);
    va_end(ap);
}

void log_warn(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vlogger(WARN, fmt, ap);
    va_end(ap);
}

void log_err(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vlogger(ERR, fmt, ap);
    va_end(ap);
}
