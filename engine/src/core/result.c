#include <henka/result.h>

const char* henka_result_to_string(henka_result result)
{
    switch (result)
    {
        case HENKA_SUCCESS:
            return "success";
        case HENKA_ERROR_INVALID_ARGUMENT:
            return "invalid argument";
        case HENKA_ERROR_OUT_OF_MEMORY:
            return "out of memory";
        case HENKA_ERROR_PLATFORM:
            return "platform error";
        case HENKA_ERROR_RENDERER:
            return "renderer error";
        case HENKA_ERROR_UNKNOWN:
        default:
            return "unknown error";
    }
}
