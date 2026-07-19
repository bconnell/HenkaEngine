#ifndef HENKA_CHECKED_H
#define HENKA_CHECKED_H

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define HENKA_MAX_SHADER_SOURCE_BYTES ((size_t)1048576U)
#define HENKA_MAX_OBJ_SOURCE_BYTES ((size_t)16777216U)
#define HENKA_MAX_OBJ_RECORDS ((size_t)1000000U)
#define HENKA_MAX_OBJ_OUTPUT_ELEMENTS ((size_t)4000000U)
#define HENKA_MAX_TEXTURE_DIMENSION 16384
#define HENKA_MAX_TEXTURE_DECODED_BYTES ((size_t)268435456U)
#define HENKA_MAX_MESH_ELEMENTS ((size_t)4000000U)
#define HENKA_MAX_ASSET_CACHE_ENTRIES ((size_t)1048576U)
#define HENKA_MAX_SCENE_ENTITIES ((size_t)1048576U)
#define HENKA_MAX_PHYSICS_ITEMS ((size_t)1048576U)
#define HENKA_MAX_DEBUG_GRID_HALF_EXTENT 16384
#define HENKA_MAX_CIRCLE_SEGMENTS 65536

static inline bool henka_checked_size_add(size_t left, size_t right, size_t* out_value)
{
    if (out_value == NULL || left > SIZE_MAX - right)
    {
        return false;
    }

    *out_value = left + right;
    return true;
}

static inline bool henka_checked_size_multiply(size_t left, size_t right, size_t* out_value)
{
    if (out_value == NULL || (left != 0U && right > SIZE_MAX / left))
    {
        return false;
    }

    *out_value = left * right;
    return true;
}

static inline bool henka_checked_capacity(
    size_t current,
    size_t required,
    size_t initial,
    size_t maximum,
    size_t* out_capacity)
{
    size_t next;

    if (out_capacity == NULL || initial == 0U || initial > maximum ||
        current > maximum || required > maximum)
    {
        return false;
    }

    if (required <= current)
    {
        *out_capacity = current;
        return true;
    }

    next = current == 0U ? initial : current;
    while (next < required)
    {
        if (next > maximum / 2U)
        {
            next = maximum;
        }
        else
        {
            next *= 2U;
        }

        if (next < required && next == maximum)
        {
            return false;
        }
    }

    *out_capacity = next;
    return true;
}

static inline bool henka_checked_size_to_int(size_t value, int* out_value)
{
    if (out_value == NULL || value > (size_t)INT_MAX)
    {
        return false;
    }

    *out_value = (int)value;
    return true;
}

static inline bool henka_checked_size_to_u32(size_t value, uint32_t* out_value)
{
    if (out_value == NULL || value > (size_t)UINT32_MAX)
    {
        return false;
    }

    *out_value = (uint32_t)value;
    return true;
}

static inline bool henka_checked_rgba8_size(int width, int height, size_t* out_bytes)
{
    size_t pixel_count;
    size_t byte_count;

    if (out_bytes == NULL || width <= 0 || height <= 0 ||
        width > HENKA_MAX_TEXTURE_DIMENSION || height > HENKA_MAX_TEXTURE_DIMENSION)
    {
        return false;
    }

    if (!henka_checked_size_multiply((size_t)width, (size_t)height, &pixel_count) ||
        !henka_checked_size_multiply(pixel_count, 4U, &byte_count) ||
        byte_count > HENKA_MAX_TEXTURE_DECODED_BYTES)
    {
        return false;
    }

    *out_bytes = byte_count;
    return true;
}

#endif
