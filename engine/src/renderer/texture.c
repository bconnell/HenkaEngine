#include "henka_internal.h"

#include "../core/checked.h"

#define STBI_MAX_DIMENSIONS HENKA_MAX_TEXTURE_DIMENSION
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <henka/log.h>

henka_result henka_texture_create_from_rgba8(
    henka_engine* engine,
    int width,
    int height,
    const unsigned char* pixels,
    henka_texture** out_texture)
{
    size_t decoded_bytes;

    if (engine == NULL || pixels == NULL || out_texture == NULL ||
        !henka_checked_rgba8_size(width, height, &decoded_bytes))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    (void)decoded_bytes;
    return henka_renderer_create_texture_from_rgba8(engine->renderer, width, height, pixels, out_texture);
}

henka_result henka_texture_create_from_file(henka_engine* engine, const char* path, henka_texture** out_texture)
{
    int channel_count;
    size_t decoded_bytes;
    int height;
    stbi_uc* pixels;
    henka_result result;
    int source_channel_count;
    int width;

    if (engine == NULL || path == NULL || out_texture == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    *out_texture = NULL;
    if (!stbi_info(path, &width, &height, &source_channel_count))
    {
        HENKA_LOG_ERROR("Unable to inspect texture '%s': %s", path, stbi_failure_reason());
        return HENKA_ERROR_RENDERER;
    }

    (void)source_channel_count;
    if (!henka_checked_rgba8_size(width, height, &decoded_bytes))
    {
        HENKA_LOG_ERROR("Texture '%s' exceeds the supported dimension or decoded-size limit", path);
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    pixels = stbi_load(path, &width, &height, &channel_count, STBI_rgb_alpha);
    if (pixels == NULL)
    {
        HENKA_LOG_ERROR("Unable to load texture '%s': %s", path, stbi_failure_reason());
        return HENKA_ERROR_RENDERER;
    }

    (void)channel_count;
    if (!henka_checked_rgba8_size(width, height, &decoded_bytes))
    {
        stbi_image_free(pixels);
        HENKA_LOG_ERROR("Decoded texture '%s' exceeds the supported bounds", path);
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    result = henka_texture_create_from_rgba8(engine, width, height, pixels, out_texture);
    stbi_image_free(pixels);
    return result;
}

void henka_texture_destroy(henka_texture* texture)
{
    henka_renderer_destroy_texture(texture);
}
