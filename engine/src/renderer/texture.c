#include "henka_internal.h"

#include <string.h>

#include "../core/checked.h"

#include <henka/log.h>
#include <henka/memory.h>

#define STBI_MAX_DIMENSIONS HENKA_MAX_TEXTURE_DIMENSION
#define STBI_MALLOC(size) henka_malloc(size)
#define STBI_REALLOC(pointer, size) henka_realloc(pointer, size)
#define STBI_FREE(pointer) henka_free(pointer)
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

static henka_result henka_texture_source_failure_result(void)
{
    const char* reason;

    reason = stbi_failure_reason();
    if (reason != NULL && strcmp(reason, "outofmem") == 0)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }

    return HENKA_ERROR_ASSET_SOURCE;
}

henka_result henka_texture_create_from_rgba8(
    henka_engine* engine,
    int width,
    int height,
    const unsigned char* pixels,
    henka_texture** out_texture)
{
    size_t decoded_bytes;

    if (out_texture != NULL)
    {
        *out_texture = NULL;
    }

    if (engine == NULL ||
        engine->renderer == NULL ||
        engine->renderer->backend_state == NULL ||
        pixels == NULL ||
        out_texture == NULL ||
        !henka_checked_rgba8_size(width, height, &decoded_bytes))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    (void)decoded_bytes;
    return henka_renderer_create_texture_from_rgba8(
        engine->renderer,
        width,
        height,
        pixels,
        out_texture);
}

henka_result henka_texture_create_from_file(
    henka_engine* engine,
    const char* path,
    henka_texture** out_texture)
{
    int channel_count;
    size_t decoded_bytes;
    int height;
    stbi_uc* pixels;
    henka_result result;
    int source_channel_count;
    int width;

    if (out_texture != NULL)
    {
        *out_texture = NULL;
    }

    if (engine == NULL ||
        engine->renderer == NULL ||
        engine->renderer->backend_state == NULL ||
        path == NULL ||
        out_texture == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    if (!stbi_info(path, &width, &height, &source_channel_count))
    {
        HENKA_LOG_ERROR(
            "Unable to inspect texture '%s': %s",
            path,
            stbi_failure_reason());
        return henka_texture_source_failure_result();
    }

    (void)source_channel_count;
    if (!henka_checked_rgba8_size(width, height, &decoded_bytes))
    {
        HENKA_LOG_ERROR(
            "Texture '%s' exceeds the supported dimension or decoded-size limit",
            path);
        return HENKA_ERROR_ASSET_SOURCE;
    }

    pixels = stbi_load(
        path,
        &width,
        &height,
        &channel_count,
        STBI_rgb_alpha);
    if (pixels == NULL)
    {
        HENKA_LOG_ERROR(
            "Unable to load texture '%s': %s",
            path,
            stbi_failure_reason());
        return henka_texture_source_failure_result();
    }

    (void)channel_count;
    if (!henka_checked_rgba8_size(width, height, &decoded_bytes))
    {
        stbi_image_free(pixels);
        HENKA_LOG_ERROR(
            "Decoded texture '%s' exceeds the supported bounds",
            path);
        return HENKA_ERROR_ASSET_SOURCE;
    }

    result = henka_texture_create_from_rgba8(
        engine,
        width,
        height,
        pixels,
        out_texture);
    stbi_image_free(pixels);
    return result;
}

henka_result henka_texture_create_borrowed_alias(
    const henka_texture* source,
    henka_texture** out_texture)
{
    henka_texture* alias;

    if (out_texture != NULL)
    {
        *out_texture = NULL;
    }

    if (source == NULL ||
        source->renderer == NULL ||
        source->backend_data == NULL ||
        source->width <= 0 ||
        source->height <= 0 ||
        out_texture == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    alias = henka_calloc(1U, sizeof(*alias));
    if (alias == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }

    alias->renderer = source->renderer;
    alias->backend_data = source->backend_data;
    alias->width = source->width;
    alias->height = source->height;
    alias->owns_backend = false;
    alias->asset_manager_owned = false;

    *out_texture = alias;
    return HENKA_SUCCESS;
}

henka_result henka_texture_adopt_owned_payload(
    henka_texture* target,
    henka_texture* replacement)
{
    if (target == NULL ||
        replacement == NULL ||
        target == replacement ||
        target->renderer == NULL ||
        target->renderer != replacement->renderer ||
        target->owns_backend ||
        target->backend_data == NULL ||
        !replacement->owns_backend ||
        replacement->backend_data == NULL ||
        replacement->width <= 0 ||
        replacement->height <= 0)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    target->backend_data = replacement->backend_data;
    target->width = replacement->width;
    target->height = replacement->height;
    target->owns_backend = true;

    replacement->backend_data = NULL;
    replacement->owns_backend = false;
    henka_free(replacement);
    return HENKA_SUCCESS;
}

void henka_texture_destroy(henka_texture* texture)
{
    if (texture == NULL)
    {
        return;
    }

    if (texture->asset_manager_owned)
    {
        HENKA_LOG_WARN(
            "ignored an attempt to destroy a manager-owned borrowed texture");
        return;
    }

    henka_renderer_destroy_texture(texture);
}

void henka_texture_destroy_owned(henka_texture* texture)
{
    if (texture == NULL)
    {
        return;
    }

    texture->asset_manager_owned = false;
    henka_renderer_destroy_texture(texture);
}