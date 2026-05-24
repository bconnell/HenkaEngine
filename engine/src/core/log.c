#include <henka/log.h>

#include <stdio.h>
#include <time.h>

static const char* henka_log_level_to_string(henka_log_level level)
{
    switch (level)
    {
        case HENKA_LOG_LEVEL_INFO:
            return "INFO";
        case HENKA_LOG_LEVEL_WARNING:
            return "WARN";
        case HENKA_LOG_LEVEL_ERROR:
            return "ERROR";
        case HENKA_LOG_LEVEL_FATAL:
            return "FATAL";
        default:
            return "UNKNOWN";
    }
}

void henka_log_write_v(henka_log_level level, const char* file, int line, const char* format, va_list args)
{
    time_t now;
    struct tm time_info;

    now = time(NULL);
#if defined(_WIN32)
    localtime_s(&time_info, &now);
#else
    localtime_r(&now, &time_info);
#endif

    fprintf(stderr,
        "[%02d:%02d:%02d] %-5s %s:%d: ",
        time_info.tm_hour,
        time_info.tm_min,
        time_info.tm_sec,
        henka_log_level_to_string(level),
        file,
        line);
    vfprintf(stderr, format, args);
    fputc('\n', stderr);
}

void henka_log_write(henka_log_level level, const char* file, int line, const char* format, ...)
{
    va_list args;

    va_start(args, format);
    henka_log_write_v(level, file, line, format, args);
    va_end(args);
}
