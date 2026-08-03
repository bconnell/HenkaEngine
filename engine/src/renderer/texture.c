#include "henka_internal.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>
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

henka_texture_descriptor henka_texture_descriptor_default_color(void)
{
    return (henka_texture_descriptor){
        HENKA_TEXTURE_COLOR_SPACE_SRGB,
        HENKA_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR,
        HENKA_TEXTURE_FILTER_LINEAR,
        HENKA_TEXTURE_WRAP_REPEAT,
        HENKA_TEXTURE_WRAP_REPEAT,
        true,
        false,
        HENKA_TEXTURE_USAGE_COLOR,
        0.0f};
}

henka_texture_descriptor henka_texture_descriptor_default_data(void)
{
    return (henka_texture_descriptor){
        HENKA_TEXTURE_COLOR_SPACE_LINEAR,
        HENKA_TEXTURE_FILTER_LINEAR,
        HENKA_TEXTURE_FILTER_LINEAR,
        HENKA_TEXTURE_WRAP_CLAMP_TO_EDGE,
        HENKA_TEXTURE_WRAP_CLAMP_TO_EDGE,
        false,
        false,
        HENKA_TEXTURE_USAGE_GENERIC_DATA,
        0.0f};
}

henka_texture_descriptor henka_texture_descriptor_default_normal(void)
{
    henka_texture_descriptor descriptor = henka_texture_descriptor_default_color();

    descriptor.color_space = HENKA_TEXTURE_COLOR_SPACE_LINEAR;
    descriptor.usage = HENKA_TEXTURE_USAGE_NORMAL;
    return descriptor;
}

henka_result henka_texture_descriptor_validate(
    const henka_texture_descriptor* descriptor)
{
    if (descriptor == NULL ||
        descriptor->color_space > HENKA_TEXTURE_COLOR_SPACE_LINEAR ||
        descriptor->min_filter > HENKA_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR ||
        descriptor->mag_filter > HENKA_TEXTURE_FILTER_LINEAR ||
        descriptor->wrap_u > HENKA_TEXTURE_WRAP_MIRRORED_REPEAT ||
        descriptor->wrap_v > HENKA_TEXTURE_WRAP_MIRRORED_REPEAT ||
        descriptor->usage > HENKA_TEXTURE_USAGE_UI ||
        !isfinite(descriptor->anisotropy) ||
        descriptor->anisotropy < 0.0f ||
        (descriptor->min_filter == HENKA_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR &&
            !descriptor->generate_mipmaps) ||
        ((descriptor->usage == HENKA_TEXTURE_USAGE_NORMAL ||
            descriptor->usage == HENKA_TEXTURE_USAGE_METALLIC_ROUGHNESS ||
            descriptor->usage == HENKA_TEXTURE_USAGE_OCCLUSION ||
            descriptor->usage == HENKA_TEXTURE_USAGE_GENERIC_DATA) &&
            descriptor->color_space != HENKA_TEXTURE_COLOR_SPACE_LINEAR) ||
        (descriptor->usage == HENKA_TEXTURE_USAGE_UI &&
            descriptor->color_space != HENKA_TEXTURE_COLOR_SPACE_SRGB))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    return HENKA_SUCCESS;
}

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

static henka_texture_alpha_mode henka_texture_classify_alpha(
    const unsigned char* pixels,
    size_t pixel_count)
{
    size_t index;
    bool has_fractional;
    bool has_transparent;

    has_fractional = false;
    has_transparent = false;
    for (index = 0U; index < pixel_count; ++index)
    {
        unsigned char alpha = pixels[index * 4U + 3U];
        if (alpha != 255U)
        {
            has_transparent = true;
        }
        if (alpha != 0U && alpha != 255U)
        {
            has_fractional = true;
        }
    }

    if (!has_transparent)
    {
        return HENKA_TEXTURE_ALPHA_OPAQUE;
    }
    return has_fractional ? HENKA_TEXTURE_ALPHA_BLEND : HENKA_TEXTURE_ALPHA_MASKED;
}

static bool henka_texture_checked_rgba32f_size(
    int width,
    int height,
    size_t* out_size)
{
    uint64_t pixels;
    uint64_t bytes;

    if (width <= 0 || height <= 0 || out_size == NULL ||
        (uint64_t)width > UINT64_MAX / (uint64_t)height)
    {
        return false;
    }
    pixels = (uint64_t)width * (uint64_t)height;
    if (pixels > UINT64_MAX / 4U ||
        pixels * 4U > UINT64_MAX / sizeof(float))
    {
        return false;
    }
    bytes = pixels * 4U * sizeof(float);
    if (bytes > (uint64_t)SIZE_MAX)
    {
        return false;
    }
    *out_size = (size_t)bytes;
    return true;
}

static bool henka_texture_flip_vertical_float(
    float* pixels,
    int width,
    int height)
{
    size_t row_bytes;
    float* row;
    float* opposite;

    if (pixels == NULL || width <= 0 || height <= 0 ||
        (size_t)width > SIZE_MAX / (4U * sizeof(float)))
    {
        return false;
    }
    row_bytes = (size_t)width * 4U * sizeof(float);
    row = henka_malloc(row_bytes);
    if (row == NULL)
    {
        return false;
    }
    for (int y = 0; y < height / 2; ++y)
    {
        opposite = pixels + (size_t)(height - y - 1) * (size_t)width * 4U;
        memcpy(row, pixels + (size_t)y * (size_t)width * 4U, row_bytes);
        memcpy(pixels + (size_t)y * (size_t)width * 4U, opposite, row_bytes);
        memcpy(opposite, row, row_bytes);
    }
    henka_free(row);
    return true;
}

static henka_result henka_texture_read_source(
    const char* path,
    unsigned char** out_bytes,
    size_t* out_size,
    henka_texture_failure_category* out_failure)
{
    FILE* file;
    long file_size;
    size_t read_count;
    unsigned char* bytes;

    if (out_bytes != NULL)
    {
        *out_bytes = NULL;
    }
    if (out_size != NULL)
    {
        *out_size = 0U;
    }
    if (out_failure != NULL)
    {
        *out_failure = HENKA_TEXTURE_FAILURE_NONE;
    }

    if (path == NULL || out_bytes == NULL || out_size == NULL || out_failure == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

#if defined(_MSC_VER)
    file = NULL;
    if (fopen_s(&file, path, "rb") != 0 || file == NULL)
#else
    file = fopen(path, "rb");
    if (file == NULL)
#endif
    {
        *out_failure = HENKA_TEXTURE_FAILURE_MISSING_OR_UNREADABLE;
        return HENKA_ERROR_ASSET_SOURCE;
    }
    if (fseek(file, 0L, SEEK_END) != 0)
    {
        fclose(file);
        *out_failure = HENKA_TEXTURE_FAILURE_MISSING_OR_UNREADABLE;
        return HENKA_ERROR_ASSET_SOURCE;
    }
    file_size = ftell(file);
    if (file_size <= 0L)
    {
        fclose(file);
        *out_failure = file_size == 0L ?
            HENKA_TEXTURE_FAILURE_DECODE_CORRUPTION :
            HENKA_TEXTURE_FAILURE_MISSING_OR_UNREADABLE;
        return HENKA_ERROR_ASSET_SOURCE;
    }
    if ((unsigned long long)file_size > (unsigned long long)HENKA_MAX_TEXTURE_ENCODED_BYTES ||
        file_size > (long)INT_MAX)
    {
        fclose(file);
        *out_failure = HENKA_TEXTURE_FAILURE_ENCODED_SIZE_LIMIT;
        return HENKA_ERROR_ASSET_SOURCE;
    }
    if (fseek(file, 0L, SEEK_SET) != 0)
    {
        fclose(file);
        *out_failure = HENKA_TEXTURE_FAILURE_MISSING_OR_UNREADABLE;
        return HENKA_ERROR_ASSET_SOURCE;
    }

    bytes = henka_malloc((size_t)file_size);
    if (bytes == NULL)
    {
        fclose(file);
        *out_failure = HENKA_TEXTURE_FAILURE_ALLOCATION;
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    read_count = fread(bytes, 1U, (size_t)file_size, file);
    fclose(file);
    if (read_count != (size_t)file_size)
    {
        henka_free(bytes);
        *out_failure = HENKA_TEXTURE_FAILURE_MISSING_OR_UNREADABLE;
        return HENKA_ERROR_ASSET_SOURCE;
    }

    *out_bytes = bytes;
    *out_size = read_count;
    return HENKA_SUCCESS;
}

static bool henka_texture_flip_vertical(
    unsigned char* pixels,
    int width,
    int height)
{
    size_t row_bytes = (size_t)width * 4U;
    unsigned char* row = henka_malloc(row_bytes);
    int y;

    if (row == NULL)
    {
        return false;
    }
    for (y = 0; y < height / 2; ++y)
    {
        unsigned char* top = pixels + ((size_t)y * row_bytes);
        unsigned char* bottom = pixels + ((size_t)(height - y - 1) * row_bytes);
        memcpy(row, top, row_bytes);
        memcpy(top, bottom, row_bytes);
        memcpy(bottom, row, row_bytes);
    }
    henka_free(row);
    return true;
}

henka_result henka_texture_create_from_rgba8(
    henka_engine* engine,
    int width,
    int height,
    const unsigned char* pixels,
    henka_texture** out_texture)
{
    henka_texture_descriptor descriptor = henka_texture_descriptor_default_color();

    return henka_texture_create_from_rgba8_with_descriptor(
        engine,
        width,
        height,
        pixels,
        &descriptor,
        out_texture);
}

henka_result henka_texture_create_from_rgba8_with_descriptor(
    henka_engine* engine,
    int width,
    int height,
    const unsigned char* pixels,
    const henka_texture_descriptor* descriptor,
    henka_texture** out_texture)
{
    size_t decoded_bytes;
    henka_texture_descriptor effective_descriptor;
    henka_result result;

    if (out_texture != NULL)
    {
        *out_texture = NULL;
    }

    if (engine == NULL ||
        engine->renderer == NULL ||
        engine->renderer->backend_state == NULL ||
        pixels == NULL || out_texture == NULL || descriptor == NULL ||
        henka_texture_descriptor_validate(descriptor) != HENKA_SUCCESS ||
        !henka_checked_rgba8_size(width, height, &decoded_bytes))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    effective_descriptor = *descriptor;
    result = henka_renderer_create_texture_from_rgba8_with_descriptor(
        engine->renderer,
        width,
        height,
        pixels,
        &effective_descriptor,
        out_texture);
    if (result == HENKA_SUCCESS)
    {
        (*out_texture)->descriptor = effective_descriptor;
        (*out_texture)->width = width;
        (*out_texture)->height = height;
        (*out_texture)->original_channel_count = 4;
        (*out_texture)->source_class = HENKA_TEXTURE_SOURCE_CLASS_LDR_8_BIT;
        (*out_texture)->alpha_mode = henka_texture_classify_alpha(
            pixels,
            decoded_bytes / 4U);
        (*out_texture)->last_failure = HENKA_TEXTURE_FAILURE_NONE;
        (*out_texture)->content_revision = 1U;
    }
    return result;
}

henka_result henka_texture_create_from_rgba32f_with_descriptor(
    henka_engine* engine,
    int width,
    int height,
    const float* pixels,
    const henka_texture_descriptor* descriptor,
    henka_texture** out_texture)
{
    if (out_texture != NULL)
    {
        *out_texture = NULL;
    }
    if (engine == NULL || engine->renderer == NULL || pixels == NULL ||
        out_texture == NULL || descriptor == NULL ||
        henka_texture_descriptor_validate(descriptor) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return henka_renderer_create_texture_from_rgba32f_with_descriptor(
        engine->renderer,
        width,
        height,
        pixels,
        descriptor,
        out_texture);
}

henka_result henka_texture_create_from_file(
    henka_engine* engine,
    const char* path,
    henka_texture** out_texture)
{
    henka_texture_descriptor descriptor = henka_texture_descriptor_default_color();

    return henka_texture_create_from_file_with_descriptor(
        engine,
        path,
        &descriptor,
        out_texture);
}

static bool henka_ktx2_read_u32(
    const unsigned char* data, size_t data_size, size_t offset, uint32_t* out_value)
{
    if (data == NULL || out_value == NULL || offset > data_size || data_size - offset < sizeof(uint32_t)) return false;
    memcpy(out_value, data + offset, sizeof(*out_value));
    return true;
}

static bool henka_ktx2_read_u64(
    const unsigned char* data, size_t data_size, size_t offset, uint64_t* out_value)
{
    if (data == NULL || out_value == NULL || offset > data_size || data_size - offset < sizeof(uint64_t)) return false;
    memcpy(out_value, data + offset, sizeof(*out_value));
    return true;
}

static henka_result henka_texture_create_from_ktx2_memory_with_mip_limit(
    henka_engine* engine,
    const unsigned char* data,
    size_t data_size,
    const henka_texture_descriptor* descriptor,
    uint32_t max_resident_mips,
    henka_texture** out_texture)
{
#if defined(HENKA_WITH_KTX2_TRANSCODER)
    henka_result transcoder_result;

    if (out_texture != NULL) *out_texture = NULL;
    if (engine == NULL || data == NULL || descriptor == NULL || out_texture == NULL ||
        henka_texture_descriptor_validate(descriptor) != HENKA_SUCCESS)
        return HENKA_ERROR_INVALID_ARGUMENT;
    transcoder_result = henka_renderer_create_texture_from_ktx2_memory_with_mip_limit(
        engine->renderer, data, data_size, descriptor, max_resident_mips, out_texture);
    if (transcoder_result == HENKA_SUCCESS)
    {
        (*out_texture)->source_byte_size = data_size;
        (*out_texture)->original_channel_count = 4;
        (*out_texture)->source_class = HENKA_TEXTURE_SOURCE_CLASS_LDR_8_BIT;
        (*out_texture)->content_revision = 1U;
    }
    return transcoder_result;
#else
    static const unsigned char identifier[12] =
        {0xABU, 0x4BU, 0x54U, 0x58U, 0x20U, 0x32U, 0x30U, 0xBBU, 0x0DU, 0x0AU, 0x1AU, 0x0AU};
    uint32_t vk_format, width, height, depth, layers, faces, levels, supercompression;
    uint64_t level_offset, level_length, level_uncompressed_length;
    uint64_t base_level_offset, base_level_length;
    size_t expected_bytes, end_offset, level_table_bytes, level_table_end;
    uint32_t level_index, level_width, level_height;
    henka_result result;

    if (out_texture != NULL) *out_texture = NULL;
    if (max_resident_mips != 0U)
        return HENKA_ERROR_ASSET_SOURCE;
    if (engine == NULL || data == NULL || descriptor == NULL || out_texture == NULL || data_size < 104U ||
        memcmp(data, identifier, sizeof(identifier)) != 0 ||
        henka_texture_descriptor_validate(descriptor) != HENKA_SUCCESS ||
        !henka_ktx2_read_u32(data, data_size, 12U, &vk_format) ||
        !henka_ktx2_read_u32(data, data_size, 20U, &width) ||
        !henka_ktx2_read_u32(data, data_size, 24U, &height) ||
        !henka_ktx2_read_u32(data, data_size, 28U, &depth) ||
        !henka_ktx2_read_u32(data, data_size, 32U, &layers) ||
        !henka_ktx2_read_u32(data, data_size, 36U, &faces) ||
        !henka_ktx2_read_u32(data, data_size, 40U, &levels) ||
        !henka_ktx2_read_u32(data, data_size, 44U, &supercompression) ||
        width == 0U || height == 0U || width > HENKA_MAX_TEXTURE_DIMENSION || height > HENKA_MAX_TEXTURE_DIMENSION ||
        depth > 1U || layers > 1U || faces != 1U || levels == 0U || levels > 16U || supercompression != 0U ||
        (vk_format != 37U && vk_format != 43U) ||
        ((vk_format == 43U) != (descriptor->color_space == HENKA_TEXTURE_COLOR_SPACE_SRGB)) ||
        !henka_checked_rgba8_size((int)width, (int)height, &expected_bytes) || expected_bytes > HENKA_MAX_TEXTURE_DECODED_BYTES ||
        !henka_ktx2_read_u64(data, data_size, 80U, &base_level_offset) ||
        !henka_ktx2_read_u64(data, data_size, 88U, &base_level_length) ||
        !henka_ktx2_read_u64(data, data_size, 96U, &level_uncompressed_length) ||
        base_level_offset > (uint64_t)SIZE_MAX || base_level_length > (uint64_t)SIZE_MAX ||
        level_uncompressed_length != (uint64_t)expected_bytes || base_level_length != (uint64_t)expected_bytes ||
        !henka_checked_size_add((size_t)base_level_offset, (size_t)base_level_length, &end_offset) || end_offset > data_size)
        return HENKA_ERROR_ASSET_SOURCE;
    if (!henka_checked_size_multiply((size_t)levels, 24U, &level_table_bytes) ||
        !henka_checked_size_add(80U, level_table_bytes, &level_table_end) || level_table_end > data_size) return HENKA_ERROR_ASSET_SOURCE;
    level_width = width;
    level_height = height;
    for (level_index = 1U; level_index < levels; ++level_index)
    {
        size_t level_record;
        size_t level_expected;
        if (!henka_checked_size_multiply((size_t)level_index, 24U, &level_record) ||
            !henka_checked_size_add(80U, level_record, &level_record) ||
            !henka_ktx2_read_u64(data, data_size, level_record, &level_offset) ||
            !henka_ktx2_read_u64(data, data_size, level_record + 8U, &level_length) ||
            !henka_ktx2_read_u64(data, data_size, level_record + 16U, &level_uncompressed_length) ||
            !henka_checked_rgba8_size((int)level_width, (int)level_height, &level_expected) ||
            level_uncompressed_length != (uint64_t)level_expected || level_length != (uint64_t)level_expected ||
            level_offset > (uint64_t)SIZE_MAX || level_length > (uint64_t)SIZE_MAX ||
            !henka_checked_size_add((size_t)level_offset, (size_t)level_length, &end_offset) || end_offset > data_size)
            return HENKA_ERROR_ASSET_SOURCE;
        if (level_width > 1U) level_width /= 2U;
        if (level_height > 1U) level_height /= 2U;
    }
    result = henka_texture_create_from_rgba8_with_descriptor(
        engine, (int)width, (int)height, data + (size_t)base_level_offset, descriptor, out_texture);
    if (result == HENKA_SUCCESS)
    {
        (*out_texture)->source_byte_size = data_size;
        (*out_texture)->original_channel_count = 4;
        (*out_texture)->source_class = HENKA_TEXTURE_SOURCE_CLASS_LDR_8_BIT;
        (*out_texture)->content_revision = 1U;
    }
    return result;
#endif
}

henka_result henka_texture_create_from_ktx2_memory(
    henka_engine* engine,
    const unsigned char* data,
    size_t data_size,
    const henka_texture_descriptor* descriptor,
    henka_texture** out_texture)
{
    return henka_texture_create_from_ktx2_memory_with_mip_limit(
        engine,
        data,
        data_size,
        descriptor,
        0U,
        out_texture);
}

henka_result henka_texture_create_from_file_with_descriptor_and_mip_limit(
    henka_engine* engine,
    const char* path,
    const henka_texture_descriptor* descriptor,
    uint32_t max_resident_mips,
    henka_texture** out_texture)
{
    unsigned char* source_bytes;
    size_t source_byte_size;
    henka_texture_failure_category failure;
    int channel_count;
    size_t decoded_bytes;
    size_t hdr_decoded_bytes;
    float* hdr_pixels;
    henka_texture_descriptor hdr_descriptor;
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
        path == NULL || out_texture == NULL || descriptor == NULL ||
        henka_texture_descriptor_validate(descriptor) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    source_bytes = NULL;
    source_byte_size = 0U;
    failure = HENKA_TEXTURE_FAILURE_NONE;
    result = henka_texture_read_source(
        path,
        &source_bytes,
        &source_byte_size,
        &failure);
    if (result != HENKA_SUCCESS)
    {
        HENKA_LOG_ERROR("Unable to read texture '%s' (%d)", path, (int)failure);
        return result;
    }

    if (source_byte_size >= 12U &&
        memcmp(source_bytes, "\xAB\x4B\x54\x58\x20\x32\x30\xBB\x0D\x0A\x1A\x0A", 12U) == 0)
    {
        result = henka_texture_create_from_ktx2_memory_with_mip_limit(
            engine, source_bytes, source_byte_size, descriptor, max_resident_mips, out_texture);
        henka_free(source_bytes);
        return result;
    }

    if (max_resident_mips != 0U)
    {
        henka_free(source_bytes);
        return HENKA_ERROR_ASSET_SOURCE;
    }

    if (!stbi_info_from_memory(
            source_bytes,
            (int)source_byte_size,
            &width,
            &height,
            &source_channel_count))
    {
        HENKA_LOG_ERROR(
            "Unable to inspect texture '%s': %s",
            path,
            stbi_failure_reason());
        henka_free(source_bytes);
        return henka_texture_source_failure_result();
    }

    if (width <= 0 || height <= 0)
    {
        henka_free(source_bytes);
        return HENKA_ERROR_ASSET_SOURCE;
    }
    if (stbi_is_hdr_from_memory(source_bytes, (int)source_byte_size))
    {
        if (!henka_texture_checked_rgba32f_size(
                width,
                height,
                &hdr_decoded_bytes))
        {
            henka_free(source_bytes);
            return HENKA_ERROR_ASSET_SOURCE;
        }
        hdr_pixels = stbi_loadf_from_memory(
            source_bytes,
            (int)source_byte_size,
            &width,
            &height,
            &channel_count,
            STBI_rgb_alpha);
        if (hdr_pixels == NULL || channel_count != source_channel_count ||
            !henka_texture_checked_rgba32f_size(
                width,
                height,
                &hdr_decoded_bytes))
        {
            stbi_image_free(hdr_pixels);
            henka_free(source_bytes);
            return henka_texture_source_failure_result();
        }
        for (size_t index = 0U;
             index < hdr_decoded_bytes / sizeof(float);
             ++index)
        {
            if (!isfinite(hdr_pixels[index]) || hdr_pixels[index] < 0.0f)
            {
                stbi_image_free(hdr_pixels);
                henka_free(source_bytes);
                HENKA_LOG_ERROR("HDR texture '%s' contains a non-finite or negative sample", path);
                return HENKA_ERROR_ASSET_SOURCE;
            }
        }
        if (descriptor->vertical_flip &&
            !henka_texture_flip_vertical_float(hdr_pixels, width, height))
        {
            stbi_image_free(hdr_pixels);
            henka_free(source_bytes);
            return HENKA_ERROR_OUT_OF_MEMORY;
        }
        hdr_descriptor = *descriptor;
        hdr_descriptor.color_space = HENKA_TEXTURE_COLOR_SPACE_LINEAR;
        result = henka_texture_create_from_rgba32f_with_descriptor(
            engine,
            width,
            height,
            hdr_pixels,
            &hdr_descriptor,
            out_texture);
        stbi_image_free(hdr_pixels);
        henka_free(source_bytes);
        if (result == HENKA_SUCCESS)
        {
            (*out_texture)->source_byte_size = source_byte_size;
            (*out_texture)->original_channel_count = source_channel_count;
            (*out_texture)->source_class = HENKA_TEXTURE_SOURCE_CLASS_HDR;
        }
        return result;
    }
    if (stbi_is_16_bit_from_memory(source_bytes, (int)source_byte_size))
    {
        henka_free(source_bytes);
        HENKA_LOG_ERROR("Texture '%s' is HDR or 16-bit and is not supported by the RGBA8 path", path);
        return HENKA_ERROR_ASSET_SOURCE;
    }
    if (!henka_checked_rgba8_size(width, height, &decoded_bytes))
    {
        HENKA_LOG_ERROR(
            "Texture '%s' exceeds the supported dimension or decoded-size limit",
            path);
        henka_free(source_bytes);
        return HENKA_ERROR_ASSET_SOURCE;
    }

    pixels = stbi_load_from_memory(
        source_bytes,
        (int)source_byte_size,
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
        henka_free(source_bytes);
        return henka_texture_source_failure_result();
    }

    if (channel_count != source_channel_count ||
        !henka_checked_rgba8_size(width, height, &decoded_bytes))
    {
        stbi_image_free(pixels);
        HENKA_LOG_ERROR(
            "Decoded texture '%s' exceeds the supported bounds",
            path);
        henka_free(source_bytes);
        return HENKA_ERROR_ASSET_SOURCE;
    }

    if (descriptor->vertical_flip)
    {
        if (!henka_texture_flip_vertical(pixels, width, height))
        {
            stbi_image_free(pixels);
            henka_free(source_bytes);
            return HENKA_ERROR_OUT_OF_MEMORY;
        }
    }

    result = henka_texture_create_from_rgba8_with_descriptor(
        engine,
        width,
        height,
        pixels,
        descriptor,
        out_texture);
    stbi_image_free(pixels);
    henka_free(source_bytes);
    if (result == HENKA_SUCCESS)
    {
        (*out_texture)->source_byte_size = source_byte_size;
        (*out_texture)->original_channel_count = source_channel_count;
    }
    return result;
}

henka_result henka_texture_create_from_file_with_descriptor(
    henka_engine* engine,
    const char* path,
    const henka_texture_descriptor* descriptor,
    henka_texture** out_texture)
{
    return henka_texture_create_from_file_with_descriptor_and_mip_limit(
        engine,
        path,
        descriptor,
        0U,
        out_texture);
}

henka_result henka_texture_get_info(
    const henka_texture* texture,
    henka_texture_info* out_info)
{
    if (texture == NULL || out_info == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    out_info->width = texture->width;
    out_info->height = texture->height;
    out_info->original_channel_count = texture->original_channel_count;
    out_info->source_byte_size = texture->source_byte_size;
    out_info->color_space = texture->descriptor.color_space;
    out_info->descriptor = texture->descriptor;
    out_info->usage = texture->descriptor.usage;
    out_info->alpha_mode = texture->alpha_mode;
    out_info->source_class = texture->source_class;
    out_info->last_failure = texture->last_failure;
    out_info->fallback_alias = texture->fallback_alias;
    out_info->backend_ready = texture->backend_data != NULL;
    out_info->gpu_compressed = texture->gpu_compressed;
    out_info->resident_gpu_bytes = texture->resident_gpu_bytes;
    out_info->resident_mip_count = texture->resident_mip_count;
    out_info->mip_count = texture->mip_count;
    out_info->content_revision = texture->content_revision;
    return HENKA_SUCCESS;
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
    alias->original_channel_count = source->original_channel_count;
    alias->source_byte_size = source->source_byte_size;
    alias->descriptor = source->descriptor;
    alias->alpha_mode = source->alpha_mode;
    alias->source_class = source->source_class;
    alias->last_failure = source->last_failure;
    alias->fallback_alias = true;
    alias->gpu_compressed = source->gpu_compressed;
    alias->resident_gpu_bytes = source->resident_gpu_bytes;
    alias->resident_mip_count = source->resident_mip_count;
    alias->mip_count = source->mip_count;
    alias->content_revision = source->content_revision;
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
    if (target->content_revision == UINT64_MAX)
    {
        HENKA_LOG_ERROR("texture content revision exhausted; refusing replacement");
        return HENKA_ERROR_RENDERER;
    }

    target->backend_data = replacement->backend_data;
    target->width = replacement->width;
    target->height = replacement->height;
    target->original_channel_count = replacement->original_channel_count;
    target->source_byte_size = replacement->source_byte_size;
    target->descriptor = replacement->descriptor;
    target->alpha_mode = replacement->alpha_mode;
    target->source_class = replacement->source_class;
    target->last_failure = replacement->last_failure;
    target->fallback_alias = false;
    target->gpu_compressed = replacement->gpu_compressed;
    target->resident_gpu_bytes = replacement->resident_gpu_bytes;
    target->resident_mip_count = replacement->resident_mip_count;
    target->mip_count = replacement->mip_count;
    target->content_revision = target->content_revision == 0U ? 1U : target->content_revision + 1U;
    target->owns_backend = true;

    replacement->backend_data = NULL;
    replacement->owns_backend = false;
    henka_free(replacement);
    return HENKA_SUCCESS;
}

henka_result henka_texture_replace_owned_payload(
    henka_texture* target,
    henka_texture* replacement)
{
    henka_texture* old_payload;

    if (target == NULL || replacement == NULL || target == replacement ||
        target->renderer == NULL || target->renderer != replacement->renderer ||
        !target->owns_backend || target->backend_data == NULL ||
        !replacement->owns_backend || replacement->backend_data == NULL ||
        replacement->width <= 0 || replacement->height <= 0)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (target->content_revision == UINT64_MAX)
    {
        HENKA_LOG_ERROR("texture content revision exhausted; refusing residency replacement");
        return HENKA_ERROR_RENDERER;
    }
    old_payload = henka_calloc(1U, sizeof(*old_payload));
    if (old_payload == NULL)
        return HENKA_ERROR_OUT_OF_MEMORY;
    *old_payload = *target;
    old_payload->asset_manager_owned = false;

    target->backend_data = replacement->backend_data;
    target->width = replacement->width;
    target->height = replacement->height;
    target->original_channel_count = replacement->original_channel_count;
    target->source_byte_size = replacement->source_byte_size;
    target->descriptor = replacement->descriptor;
    target->alpha_mode = replacement->alpha_mode;
    target->source_class = replacement->source_class;
    target->last_failure = replacement->last_failure;
    target->fallback_alias = false;
    target->gpu_compressed = replacement->gpu_compressed;
    target->resident_gpu_bytes = replacement->resident_gpu_bytes;
    target->resident_mip_count = replacement->resident_mip_count;
    target->mip_count = replacement->mip_count;
    target->content_revision += 1U;
    target->owns_backend = true;
    replacement->backend_data = NULL;
    replacement->owns_backend = false;
    henka_free(replacement);
    henka_renderer_destroy_texture(old_payload);
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
