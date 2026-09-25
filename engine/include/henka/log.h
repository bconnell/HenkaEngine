#ifndef HENKA_LOG_H
#define HENKA_LOG_H

#include <stdbool.h>
#include <stdarg.h>

#include <henka/result.h>

typedef enum henka_log_level
{
    HENKA_LOG_LEVEL_INFO = 0,
    HENKA_LOG_LEVEL_WARNING,
    HENKA_LOG_LEVEL_ERROR,
    HENKA_LOG_LEVEL_FATAL
} henka_log_level;

void henka_log_write(henka_log_level level, const char* file, int line, const char* format, ...);
void henka_log_write_v(henka_log_level level, const char* file, int line, const char* format, va_list args);
henka_result henka_log_set_minimum_level(henka_log_level level);
henka_log_level henka_log_get_minimum_level(void);
bool henka_log_should_write(henka_log_level level);

#define HENKA_LOG_INFO(...)  henka_log_write(HENKA_LOG_LEVEL_INFO, __FILE__, __LINE__, __VA_ARGS__)
#define HENKA_LOG_WARN(...)  henka_log_write(HENKA_LOG_LEVEL_WARNING, __FILE__, __LINE__, __VA_ARGS__)
#define HENKA_LOG_ERROR(...) henka_log_write(HENKA_LOG_LEVEL_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define HENKA_LOG_FATAL(...) henka_log_write(HENKA_LOG_LEVEL_FATAL, __FILE__, __LINE__, __VA_ARGS__)

#endif
