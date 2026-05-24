#include "henka_internal.h"

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
    if (engine == NULL || pixels == NULL || out_texture == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    return henka_renderer_create_texture_from_rgba8(engine->renderer, width, height, pixels, out_texture);
}

henka_result henka_texture_create_from_file(henka_engine* engine, const char* path, henka_texture** out_texture)
{
    stbi_uc* pixels;
    int channel_count;
    int height;
    henka_result result;
    int width;

    if (engine == NULL || path == NULL || out_texture == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    pixels = stbi_load(path, &width, &height, &channel_count, STBI_rgb_alpha);
    if (pixels == NULL)
    {
        HENKA_LOG_ERROR("Unable to load texture '%s': %s", path, stbi_failure_reason());
        return HENKA_ERROR_RENDERER;
    }

    result = henka_texture_create_from_rgba8(engine, width, height, pixels, out_texture);
    stbi_image_free(pixels);
    return result;
}

void henka_texture_destroy(henka_texture* texture)
{
    henka_renderer_destroy_texture(texture);
}
