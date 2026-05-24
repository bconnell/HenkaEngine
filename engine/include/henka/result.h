#ifndef HENKA_RESULT_H
#define HENKA_RESULT_H

typedef enum henka_result
{
    HENKA_SUCCESS = 0,
    HENKA_ERROR_INVALID_ARGUMENT,
    HENKA_ERROR_OUT_OF_MEMORY,
    HENKA_ERROR_PLATFORM,
    HENKA_ERROR_RENDERER,
    HENKA_ERROR_UNKNOWN
} henka_result;

const char* henka_result_to_string(henka_result result);

#endif
