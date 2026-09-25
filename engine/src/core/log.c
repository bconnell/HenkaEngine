#include <henka/log.h>

#include <stdio.h>
#include <time.h>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <stdatomic.h>
#endif

#if defined(_WIN32)
static volatile LONG g_henka_log_minimum_level = (LONG)HENKA_LOG_LEVEL_INFO;
#else
static _Atomic int g_henka_log_minimum_level = HENKA_LOG_LEVEL_INFO;
#endif

static bool henka_log_level_is_valid(henka_log_level level)
{
    return level >= HENKA_LOG_LEVEL_INFO && level <= HENKA_LOG_LEVEL_FATAL;
}

henka_result henka_log_set_minimum_level(henka_log_level level)
{
    if (!henka_log_level_is_valid(level))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
#if defined(_WIN32)
    (void)InterlockedExchange(
        &g_henka_log_minimum_level,
        (LONG)level);
#else
    atomic_store_explicit(
        &g_henka_log_minimum_level,
        (int)level,
        memory_order_relaxed);
#endif
    return HENKA_SUCCESS;
}

henka_log_level henka_log_get_minimum_level(void)
{
#if defined(_WIN32)
    return (henka_log_level)InterlockedCompareExchange(
        &g_henka_log_minimum_level,
        0L,
        0L);
#else
    return (henka_log_level)atomic_load_explicit(
        &g_henka_log_minimum_level,
        memory_order_relaxed);
#endif
}

bool henka_log_should_write(henka_log_level level)
{
    return henka_log_level_is_valid(level) &&
        level >= henka_log_get_minimum_level();
}

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

    if (!henka_log_should_write(level))
    {
        return;
    }
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
