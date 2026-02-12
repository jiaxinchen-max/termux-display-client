#include "include/syslog.h"
static const char* level_names[] = {
        "EMERG", "ALERT", "CRIT", "ERROR", "WARN", "NOTICE", "INFO", "DEBUG"
};
static int current_level = LOG_DEBUG;

__LIBC_HIDDEN__ void syslog(int priority, const char *format, ...) {
    if (priority > current_level)
        return;

    time_t t = time(NULL);
    struct tm tm_info;
    localtime_r(&t, &tm_info);
    char ts[20];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm_info);

    fprintf(stderr, "[%s] %-6s: ", ts, level_names[priority]);

    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);

    fprintf(stderr, "\n");
}

__LIBC_HIDDEN__ void syslog_set_level(int level) {
    current_level = level;
}
