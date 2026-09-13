#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <henka/core.h>
#include <henka/scene.h>
#include <henka/scene_document.h>

#include "../engine/src/core/memory_internal.h"

static bool test_scene_document_files_equal(const char* left_path, const char* right_path)
{
    FILE* left = fopen(left_path, "rb");
    FILE* right = fopen(right_path, "rb");
    unsigned char left_buffer[256];
    unsigned char right_buffer[256];
    size_t left_size;
    size_t right_size;
    bool equal = true;

    if (left == NULL || right == NULL)
    {
        if (left != NULL) fclose(left);
        if (right != NULL) fclose(right);
        return false;
    }
    do
    {
        left_size = fread(left_buffer, 1U, sizeof(left_buffer), left);
        right_size = fread(right_buffer, 1U, sizeof(right_buffer), right);
        if (left_size != right_size || memcmp(left_buffer, right_buffer, left_size) != 0)
        {
            equal = false;
            break;
        }
    } while (left_size != 0U);
    if (fgetc(left) != EOF || fgetc(right) != EOF)
    {
        equal = false;
    }
    fclose(left);
    fclose(right);
    return equal;
}

static bool test_scene_document_write_bytes(const char* path, const void* data, size_t size)
{
    FILE* file = fopen(path, "wb");
    bool result;
    if (file == NULL)
    {
        return false;
    }
    result = fwrite(data, 1U, size, file) == size;
    if (fclose(file) != 0)
    {
        result = false;
    }
    return result;
}

static bool test_scene_document_patch_u32(const char* path, long offset, uint32_t value)
{
    unsigned char bytes[4];
    FILE* file = fopen(path, "r+b");
    bool result;
    if (file == NULL)
    {
        return false;
    }
    bytes[0] = (unsigned char)(value & UINT32_C(0xFF));
    bytes[1] = (unsigned char)((value >> 8U) & UINT32_C(0xFF));
    bytes[2] = (unsigned char)((value >> 16U) & UINT32_C(0xFF));
    bytes[3] = (unsigned char)((value >> 24U) & UINT32_C(0xFF));
    result = fseek(file, offset, SEEK_SET) == 0 &&
        fwrite(bytes, 1U, sizeof(bytes), file) == sizeof(bytes);
    if (fclose(file) != 0)
    {
        result = false;
    }
    return result;
}

static bool test_scene_document_legacy_write_u16(
    unsigned char* buffer,
    size_t capacity,
    size_t* position,
    uint16_t value)
{
    if (buffer == NULL || position == NULL || *position > capacity - 2U)
    {
        return false;
    }
    buffer[(*position)++] = (unsigned char)(value & 0xFFU);
    buffer[(*position)++] = (unsigned char)((value >> 8U) & 0xFFU);
    return true;
}

static bool test_scene_document_legacy_write_u32(
    unsigned char* buffer,
    size_t capacity,
    size_t* position,
    uint32_t value)
{
    size_t index;
    if (buffer == NULL || position == NULL || *position > capacity - 4U)
    {
        return false;
    }
    for (index = 0U; index < 4U; ++index)
    {
        buffer[(*position)++] = (unsigned char)((value >> (index * 8U)) & 0xFFU);
    }
    return true;
}

static bool test_scene_document_legacy_write_u64(
    unsigned char* buffer,
    size_t capacity,
    size_t* position,
    uint64_t value)
{
    size_t index;
    if (buffer == NULL || position == NULL || *position > capacity - 8U)
    {
        return false;
    }
    for (index = 0U; index < 8U; ++index)
    {
        buffer[(*position)++] = (unsigned char)((value >> (index * 8U)) & 0xFFU);
    }
    return true;
}

static bool test_scene_document_legacy_write_float(
    unsigned char* buffer,
    size_t capacity,
    size_t* position,
    float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return test_scene_document_legacy_write_u32(buffer, capacity, position, bits);
}

static bool test_scene_document_legacy_write_string(
    unsigned char* buffer,
    size_t capacity,
    size_t* position,
    const char* value)
{
    const size_t length = strlen(value);
    return length <= UINT16_MAX &&
        test_scene_document_legacy_write_u16(buffer, capacity, position, (uint16_t)length) &&
        *position <= capacity - length &&
        (memcpy(buffer + *position, value, length), *position += length, true);
}

static uint32_t test_scene_document_legacy_checksum(
    const unsigned char* data,
    size_t size)
{
    uint32_t checksum = UINT32_C(0xFFFFFFFF);
    size_t index;
    for (index = 0U; index < size; ++index)
    {
        uint32_t bit;
        checksum ^= data[index];
        for (bit = 0U; bit < 8U; ++bit)
        {
            checksum = (checksum & 1U) != 0U
                ? (checksum >> 1U) ^ UINT32_C(0xEDB88320)
                : checksum >> 1U;
        }
    }
    return ~checksum;
}

static bool test_scene_document_legacy_read_u16(
    const unsigned char* buffer,
    size_t capacity,
    size_t* position,
    uint16_t* out_value)
{
    if (buffer == NULL || position == NULL || out_value == NULL ||
        *position > capacity || sizeof(uint16_t) > capacity - *position)
    {
        return false;
    }
    *out_value = (uint16_t)buffer[*position] |
        (uint16_t)((uint16_t)buffer[*position + 1U] << 8U);
    *position += sizeof(uint16_t);
    return true;
}

static bool test_scene_document_legacy_read_u32(
    const unsigned char* buffer,
    size_t capacity,
    size_t* position,
    uint32_t* out_value)
{
    uint32_t value = 0U;
    size_t index;

    if (buffer == NULL || position == NULL || out_value == NULL ||
        *position > capacity || sizeof(uint32_t) > capacity - *position)
    {
        return false;
    }
    for (index = 0U; index < sizeof(uint32_t); ++index)
    {
        value |= (uint32_t)buffer[*position + index] << (index * 8U);
    }
    *position += sizeof(uint32_t);
    *out_value = value;
    return true;
}

static bool test_scene_document_legacy_skip_bytes(
    const unsigned char* buffer,
    size_t capacity,
    size_t* position,
    size_t bytes)
{
    if (buffer == NULL || position == NULL || *position > capacity ||
        bytes > capacity - *position)
    {
        return false;
    }
    *position += bytes;
    return true;
}

static bool test_scene_document_legacy_skip_string(
    const unsigned char* buffer,
    size_t capacity,
    size_t* position)
{
    uint16_t length;

    return test_scene_document_legacy_read_u16(
               buffer, capacity, position, &length) &&
        test_scene_document_legacy_skip_bytes(
            buffer, capacity, position, (size_t)length);
}

static bool test_scene_document_legacy_compact_current_extensions(
    unsigned char* data,
    size_t source_size,
    size_t object_count,
    bool remove_texture_extensions,
    size_t* out_size)
{
    const size_t header_bytes = 40U;
    const size_t material_prefix_bytes =
        10U * sizeof(uint32_t) +
        9U * sizeof(uint32_t) +
        24U * sizeof(uint32_t) +
        sizeof(uint32_t);
    const size_t material_texture_count = 7U;
    size_t read_position = header_bytes;
    size_t write_position = header_bytes;
    size_t object_index;

    if (data == NULL || out_size == NULL || source_size < header_bytes ||
        object_count > (source_size - header_bytes) /
            (2U * sizeof(uint64_t) + sizeof(uint32_t)))
    {
        return false;
    }

    for (object_index = 0U; object_index < object_count; ++object_index)
    {
        const size_t object_start = read_position;
        const size_t current_physics_bytes = 76U;
        size_t texture_extension_start;
        size_t texture_extension_end;
        size_t capsule_extension_start;
        size_t capsule_extension_end;
        size_t physics_start;
        size_t object_end;
        size_t prefix_bytes;
        size_t middle_bytes;
        size_t suffix_bytes;
        uint32_t behavior_count;
        size_t texture_index;
        size_t behavior_index;

        if (!test_scene_document_legacy_skip_bytes(
                data, source_size, &read_position,
                2U * sizeof(uint64_t) + sizeof(uint32_t)) ||
            !test_scene_document_legacy_skip_string(
                data, source_size, &read_position) ||
            !test_scene_document_legacy_skip_bytes(
                data, source_size, &read_position,
                10U * sizeof(uint32_t) + 2U * sizeof(uint32_t) +
                    3U * sizeof(uint32_t)) ||
            !test_scene_document_legacy_skip_string(
                data, source_size, &read_position) ||
            !test_scene_document_legacy_skip_bytes(
                data, source_size, &read_position, sizeof(uint32_t)) ||
            !test_scene_document_legacy_skip_string(
                data, source_size, &read_position) ||
            !test_scene_document_legacy_skip_bytes(
                data, source_size, &read_position, material_prefix_bytes))
        {
            return false;
        }
        texture_extension_start = read_position;
        if (!test_scene_document_legacy_skip_bytes(
                data, source_size, &read_position, sizeof(uint32_t)))
        {
            return false;
        }
        for (texture_index = 0U;
             texture_index < material_texture_count;
             ++texture_index)
        {
            if (!test_scene_document_legacy_skip_string(
                    data, source_size, &read_position))
            {
                return false;
            }
        }
        texture_extension_end = read_position;
        if (!test_scene_document_legacy_skip_bytes(
                data, source_size, &read_position, sizeof(uint32_t)) ||
            !test_scene_document_legacy_skip_string(
                data, source_size, &read_position))
        {
            return false;
        }
        physics_start = read_position;
        if (!test_scene_document_legacy_skip_bytes(
                data, source_size, &read_position, current_physics_bytes) ||
            !test_scene_document_legacy_skip_string(
                data, source_size, &read_position) ||
            !test_scene_document_legacy_skip_bytes(
                data, source_size, &read_position, 5U * sizeof(uint32_t)) ||
            !test_scene_document_legacy_read_u32(
                data, source_size, &read_position, &behavior_count))
        {
            return false;
        }
        capsule_extension_start = physics_start + 6U * sizeof(uint32_t);
        capsule_extension_end = capsule_extension_start + 2U * sizeof(uint32_t);
        for (behavior_index = 0U;
             behavior_index < (size_t)behavior_count;
             ++behavior_index)
        {
            if (!test_scene_document_legacy_skip_bytes(
                    data, source_size, &read_position,
                    sizeof(uint32_t) + sizeof(uint64_t) + sizeof(uint32_t)) ||
                !test_scene_document_legacy_skip_string(
                    data, source_size, &read_position))
            {
                return false;
            }
        }
        if (!test_scene_document_legacy_skip_bytes(
                data, source_size, &read_position,
                8U * sizeof(uint32_t) + 2U * sizeof(uint32_t)))
        {
            return false;
        }
        object_end = read_position;
        if (object_end < capsule_extension_end ||
            texture_extension_start < object_start ||
            texture_extension_end < texture_extension_start ||
            capsule_extension_start < texture_extension_end ||
            capsule_extension_end < capsule_extension_start ||
            write_position > source_size)
        {
            return false;
        }
        prefix_bytes = (remove_texture_extensions ?
            texture_extension_start : capsule_extension_start) - object_start;
        middle_bytes = remove_texture_extensions ?
            capsule_extension_start - texture_extension_end : 0U;
        suffix_bytes = object_end - capsule_extension_end;
        if (prefix_bytes > source_size - write_position ||
            middle_bytes > source_size - write_position - prefix_bytes ||
            suffix_bytes > source_size - write_position - prefix_bytes - middle_bytes)
        {
            return false;
        }
        memmove(
            data + write_position,
            data + object_start,
            prefix_bytes);
        write_position += prefix_bytes;
        if (middle_bytes > 0U)
        {
            memmove(
                data + write_position,
                data + texture_extension_end,
                middle_bytes);
            write_position += middle_bytes;
        }
        memmove(
            data + write_position,
            data + capsule_extension_end,
            suffix_bytes);
        write_position += suffix_bytes;
    }

    if (write_position > read_position)
    {
        return false;
    }
    memmove(
        data + write_position,
        data + read_position,
        source_size - read_position);
    write_position += source_size - read_position;
    *out_size = write_position;
    return true;
}

static bool test_scene_document_patch_u64_and_checksum(
    const char* path,
    long payload_offset,
    uint64_t value)
{
    FILE* file = fopen(path, "r+b");
    unsigned char* data = NULL;
    long length;
    size_t size;
    size_t index;
    bool result = false;
    if (file == NULL || fseek(file, 0L, SEEK_END) != 0 ||
        (length = ftell(file)) < 40L || fseek(file, 0L, SEEK_SET) != 0)
    {
        if (file != NULL) fclose(file);
        return false;
    }
    size = (size_t)length;
    if ((uint64_t)payload_offset > (uint64_t)size ||
        size - (size_t)payload_offset < sizeof(value))
    {
        fclose(file);
        return false;
    }
    data = (unsigned char*)malloc(size);
    if (data != NULL && fread(data, 1U, size, file) == size)
    {
        for (index = 0U; index < sizeof(value); ++index)
        {
            data[(size_t)payload_offset + index] =
                (unsigned char)((value >> (index * 8U)) & UINT64_C(0xFF));
        }
        (void)test_scene_document_legacy_write_u32(
            data,
            size,
            &(size_t){32U},
            test_scene_document_legacy_checksum(data + 40U, size - 40U));
        result = fseek(file, 0L, SEEK_SET) == 0 &&
            fwrite(data, 1U, size, file) == size;
    }
    free(data);
    if (fclose(file) != 0)
    {
        result = false;
    }
    return result;
}

static bool test_scene_document_patch_u32_and_checksum(
    const char* path,
    long payload_offset,
    uint32_t value)
{
    FILE* file = fopen(path, "r+b");
    unsigned char* data = NULL;
    long length;
    size_t size;
    bool result = false;
    if (file == NULL || fseek(file, 0L, SEEK_END) != 0 ||
        (length = ftell(file)) < 40L || fseek(file, 0L, SEEK_SET) != 0)
    {
        if (file != NULL) fclose(file);
        return false;
    }
    size = (size_t)length;
    if ((uint64_t)payload_offset > (uint64_t)size ||
        size - (size_t)payload_offset < sizeof(value))
    {
        fclose(file);
        return false;
    }
    data = (unsigned char*)malloc(size);
    if (data != NULL && fread(data, 1U, size, file) == size)
    {
        (void)test_scene_document_legacy_write_u32(
            data,
            size,
            &(size_t){(size_t)payload_offset},
            value);
        (void)test_scene_document_legacy_write_u32(
            data,
            size,
            &(size_t){32U},
            test_scene_document_legacy_checksum(data + 40U, size - 40U));
        result = fseek(file, 0L, SEEK_SET) == 0 &&
            fwrite(data, 1U, size, file) == size;
    }
    free(data);
    if (fclose(file) != 0)
    {
        result = false;
    }
    return result;
}

static bool test_scene_document_patch_v10_environment_mode(
    const char* path,
    uint32_t mode)
{
    const size_t environment_bytes = 49U * sizeof(uint32_t);
    const size_t render_settings_bytes = 18U * sizeof(uint32_t);
    const size_t render_resources_bytes = 144U * sizeof(uint32_t);
    FILE* file = NULL;
    long length;
    bool result;

    if (path == NULL ||
#if defined(_WIN32)
        fopen_s(&file, path, "rb") != 0 ||
#else
        (file = fopen(path, "rb")) == NULL ||
#endif
        fseek(file, 0L, SEEK_END) != 0 ||
        (length = ftell(file)) <
            (long)(40U + environment_bytes + render_settings_bytes + render_resources_bytes))
    {
        if (file != NULL) fclose(file);
        return false;
    }
    result = fclose(file) == 0 &&
        test_scene_document_patch_u32_and_checksum(
            path,
            length - (long)environment_bytes - (long)render_settings_bytes -
                (long)render_resources_bytes +
                (long)(11U * sizeof(uint32_t)),
            mode);
    return result;
}

static bool test_scene_document_write_legacy_fixture(const char* path)
{
    const henka_scene_document_object object = henka_scene_document_object_default();
    unsigned char payload[2048];
    unsigned char header[40];
    size_t position = 0U;
    FILE* file;
    bool result = true;
    memset(payload, 0, sizeof(payload));
    memset(header, 0, sizeof(header));
    result = result && test_scene_document_legacy_write_u64(payload, sizeof(payload), &position, 1U);
    result = result && test_scene_document_legacy_write_u32(payload, sizeof(payload), &position, 3U);
    result = result && test_scene_document_legacy_write_string(payload, sizeof(payload), &position, "legacy");
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.transform.position.x);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.transform.position.y);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.transform.position.z);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.transform.rotation.x);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.transform.rotation.y);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.transform.rotation.z);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.transform.rotation.w);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.transform.scale.x);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.transform.scale.y);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.transform.scale.z);
    result = result && test_scene_document_legacy_write_u32(payload, sizeof(payload), &position, (uint32_t)object.source.kind);
    result = result && test_scene_document_legacy_write_u32(payload, sizeof(payload), &position, (uint32_t)object.source.primitive);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.source.primitive_dimensions.x);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.source.primitive_dimensions.y);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.source.primitive_dimensions.z);
    result = result && test_scene_document_legacy_write_string(payload, sizeof(payload), &position, object.source.path);
    result = result && test_scene_document_legacy_write_u32(payload, sizeof(payload), &position, (uint32_t)object.source.asset_kind);
    result = result && test_scene_document_legacy_write_string(payload, sizeof(payload), &position, object.renderer.material_path);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.renderer.base_color.x);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.renderer.base_color.y);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.renderer.base_color.z);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.renderer.base_color.w);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.renderer.metallic);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.renderer.roughness);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.renderer.emissive.x);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.renderer.emissive.y);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.renderer.emissive.z);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.renderer.emissive_strength);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.interaction.max_distance);
    result = result && test_scene_document_legacy_write_string(payload, sizeof(payload), &position, object.interaction.prompt);
    result = result && test_scene_document_legacy_write_u32(payload, sizeof(payload), &position, (uint32_t)object.physics.body_type);
    result = result && test_scene_document_legacy_write_u32(payload, sizeof(payload), &position, (uint32_t)object.physics.shape);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.physics.collider_offset.x);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.physics.collider_offset.y);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.physics.collider_offset.z);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.physics.sphere_radius);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.physics.box_half_extents.x);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.physics.box_half_extents.y);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.physics.box_half_extents.z);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.physics.mass);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.physics.material.restitution);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.physics.material.static_friction);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.physics.material.dynamic_friction);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.physics.material.linear_damping);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.physics.material.angular_damping);
    result = result && test_scene_document_legacy_write_u32(payload, sizeof(payload), &position, object.physics.layer);
    result = result && test_scene_document_legacy_write_u32(payload, sizeof(payload), &position, object.physics.mask);
    if (!result)
    {
        return false;
    }
    header[0] = 'H'; header[1] = 'S'; header[2] = 'C'; header[3] = 'N';
    result = test_scene_document_legacy_write_u32(header, sizeof(header), &(size_t){4U}, 1U) &&
        test_scene_document_legacy_write_u32(header, sizeof(header), &(size_t){8U}, 40U) &&
        test_scene_document_legacy_write_u64(header, sizeof(header), &(size_t){12U}, (uint64_t)position) &&
        test_scene_document_legacy_write_u32(header, sizeof(header), &(size_t){20U}, 1U) &&
        test_scene_document_legacy_write_u64(header, sizeof(header), &(size_t){24U}, 2U) &&
        test_scene_document_legacy_write_u32(header, sizeof(header), &(size_t){32U}, test_scene_document_legacy_checksum(payload, position));
    if (!result)
    {
        return false;
    }
#if defined(_WIN32)
    if (fopen_s(&file, path, "wb") != 0)
    {
        return false;
    }
#else
    file = fopen(path, "wb");
#endif
    if (file == NULL)
    {
        return false;
    }
    result = fwrite(header, 1U, sizeof(header), file) == sizeof(header) &&
        fwrite(payload, 1U, position, file) == position && fclose(file) == 0;
    return result;
}

static bool test_scene_document_write_v2_fixture(const char* path)
{
    const henka_scene_document_object object = henka_scene_document_object_default();
    unsigned char payload[2048];
    unsigned char header[40];
    size_t position = 0U;
    FILE* file;
    bool result = true;
    memset(payload, 0, sizeof(payload));
    memset(header, 0, sizeof(header));
    result = result && test_scene_document_legacy_write_u64(payload, sizeof(payload), &position, 1U);
    result = result && test_scene_document_legacy_write_u32(payload, sizeof(payload), &position, 3U);
    result = result && test_scene_document_legacy_write_string(payload, sizeof(payload), &position, "v2");
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.transform.position.x);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.transform.position.y);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.transform.position.z);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.transform.rotation.x);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.transform.rotation.y);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.transform.rotation.z);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.transform.rotation.w);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.transform.scale.x);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.transform.scale.y);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.transform.scale.z);
    result = result && test_scene_document_legacy_write_u32(payload, sizeof(payload), &position, (uint32_t)object.source.kind);
    result = result && test_scene_document_legacy_write_u32(payload, sizeof(payload), &position, (uint32_t)object.source.primitive);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.source.primitive_dimensions.x);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.source.primitive_dimensions.y);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.source.primitive_dimensions.z);
    result = result && test_scene_document_legacy_write_string(payload, sizeof(payload), &position, object.source.path);
    result = result && test_scene_document_legacy_write_u32(payload, sizeof(payload), &position, (uint32_t)object.source.asset_kind);
    result = result && test_scene_document_legacy_write_string(payload, sizeof(payload), &position, object.renderer.material_path);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.renderer.base_color.x);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.renderer.base_color.y);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.renderer.base_color.z);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.renderer.base_color.w);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.renderer.metallic);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.renderer.roughness);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.renderer.emissive.x);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.renderer.emissive.y);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.renderer.emissive.z);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.renderer.emissive_strength);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.interaction.max_distance);
    result = result && test_scene_document_legacy_write_string(payload, sizeof(payload), &position, object.interaction.prompt);
    result = result && test_scene_document_legacy_write_u32(payload, sizeof(payload), &position, (uint32_t)object.physics.body_type);
    result = result && test_scene_document_legacy_write_u32(payload, sizeof(payload), &position, (uint32_t)object.physics.shape);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.physics.collider_offset.x);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.physics.collider_offset.y);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.physics.collider_offset.z);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.physics.sphere_radius);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.physics.box_half_extents.x);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.physics.box_half_extents.y);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.physics.box_half_extents.z);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.physics.mass);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.physics.material.restitution);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.physics.material.static_friction);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.physics.material.dynamic_friction);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.physics.material.linear_damping);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.physics.material.angular_damping);
    result = result && test_scene_document_legacy_write_u32(payload, sizeof(payload), &position, object.physics.layer);
    result = result && test_scene_document_legacy_write_u32(payload, sizeof(payload), &position, object.physics.mask);
    result = result && test_scene_document_legacy_write_u32(payload, sizeof(payload), &position, 0U);
    if (!result)
    {
        return false;
    }
    header[0] = 'H'; header[1] = 'S'; header[2] = 'C'; header[3] = 'N';
    result = test_scene_document_legacy_write_u32(header, sizeof(header), &(size_t){4U}, 2U) &&
        test_scene_document_legacy_write_u32(header, sizeof(header), &(size_t){8U}, 40U) &&
        test_scene_document_legacy_write_u64(header, sizeof(header), &(size_t){12U}, (uint64_t)position) &&
        test_scene_document_legacy_write_u32(header, sizeof(header), &(size_t){20U}, 1U) &&
        test_scene_document_legacy_write_u64(header, sizeof(header), &(size_t){24U}, 2U) &&
        test_scene_document_legacy_write_u32(header, sizeof(header), &(size_t){32U}, test_scene_document_legacy_checksum(payload, position));
    if (!result)
    {
        return false;
    }
#if defined(_WIN32)
    if (fopen_s(&file, path, "wb") != 0)
    {
        return false;
    }
#else
    file = fopen(path, "wb");
#endif
    if (file == NULL)
    {
        return false;
    }
    result = fwrite(header, 1U, sizeof(header), file) == sizeof(header) &&
        fwrite(payload, 1U, position, file) == position && fclose(file) == 0;
    return result;
}

static bool test_scene_document_write_v3_to_v8_fixture(
    const char* path,
    uint32_t version)
{
    const henka_scene_document_object object = henka_scene_document_object_default();
    const henka_audio_listener listener = henka_audio_listener_default();
    unsigned char payload[2048];
    unsigned char header[40];
    size_t position = 0U;
    FILE* file;
    bool result = true;

    if (path == NULL ||
        (version != 3U && version != 4U && version != 5U &&
            version != 6U && version != 7U && version != 8U))
    {
        return false;
    }
    memset(payload, 0, sizeof(payload));
    memset(header, 0, sizeof(header));
    result = result && test_scene_document_legacy_write_u64(payload, sizeof(payload), &position, 1U);
    if (version >= 6U)
    {
        result = result && test_scene_document_legacy_write_u64(
            payload,
            sizeof(payload),
            &position,
            HENKA_INVALID_SCENE_DOCUMENT_ID);
    }
    result = result && test_scene_document_legacy_write_u32(payload, sizeof(payload), &position, 3U);
    result = result && test_scene_document_legacy_write_string(
        payload,
        sizeof(payload),
        &position,
        version == 3U ? "v3" : (version == 4U ? "v4" :
            (version == 5U ? "v5" : (version == 6U ? "v6" :
                (version == 7U ? "v7" : "v8")))));
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.transform.position.x);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.transform.position.y);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.transform.position.z);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.transform.rotation.x);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.transform.rotation.y);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.transform.rotation.z);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.transform.rotation.w);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.transform.scale.x);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.transform.scale.y);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.transform.scale.z);
    result = result && test_scene_document_legacy_write_u32(payload, sizeof(payload), &position, (uint32_t)object.source.kind);
    result = result && test_scene_document_legacy_write_u32(payload, sizeof(payload), &position, (uint32_t)object.source.primitive);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.source.primitive_dimensions.x);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.source.primitive_dimensions.y);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.source.primitive_dimensions.z);
    result = result && test_scene_document_legacy_write_string(payload, sizeof(payload), &position, object.source.path);
    result = result && test_scene_document_legacy_write_u32(payload, sizeof(payload), &position, (uint32_t)object.source.asset_kind);
    result = result && test_scene_document_legacy_write_string(payload, sizeof(payload), &position, object.renderer.material_path);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.renderer.base_color.x);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.renderer.base_color.y);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.renderer.base_color.z);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.renderer.base_color.w);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.renderer.metallic);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.renderer.roughness);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.renderer.emissive.x);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.renderer.emissive.y);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.renderer.emissive.z);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.renderer.emissive_strength);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.interaction.max_distance);
    result = result && test_scene_document_legacy_write_string(payload, sizeof(payload), &position, object.interaction.prompt);
    result = result && test_scene_document_legacy_write_u32(payload, sizeof(payload), &position, (uint32_t)object.physics.body_type);
    result = result && test_scene_document_legacy_write_u32(payload, sizeof(payload), &position, (uint32_t)object.physics.shape);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.physics.collider_offset.x);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.physics.collider_offset.y);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.physics.collider_offset.z);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.physics.sphere_radius);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.physics.box_half_extents.x);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.physics.box_half_extents.y);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.physics.box_half_extents.z);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.physics.mass);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.physics.material.restitution);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.physics.material.static_friction);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.physics.material.dynamic_friction);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.physics.material.linear_damping);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.physics.material.angular_damping);
    result = result && test_scene_document_legacy_write_u32(payload, sizeof(payload), &position, object.physics.layer);
    result = result && test_scene_document_legacy_write_u32(payload, sizeof(payload), &position, object.physics.mask);
    result = result && test_scene_document_legacy_write_string(payload, sizeof(payload), &position, object.audio.clip_path);
    result = result && test_scene_document_legacy_write_u32(payload, sizeof(payload), &position, (uint32_t)object.audio.bus);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.audio.gain);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.audio.pitch);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.audio.min_distance);
    result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, object.audio.max_distance);
    result = result && test_scene_document_legacy_write_u32(payload, sizeof(payload), &position, 0U);
    if (version >= 8U)
    {
        result = result && test_scene_document_legacy_write_float(
            payload, sizeof(payload), &position, object.character_controller.radius);
        result = result && test_scene_document_legacy_write_float(
            payload, sizeof(payload), &position, object.character_controller.half_height);
        result = result && test_scene_document_legacy_write_float(
            payload, sizeof(payload), &position, object.character_controller.max_speed);
        result = result && test_scene_document_legacy_write_float(
            payload, sizeof(payload), &position, object.character_controller.jump_speed);
        result = result && test_scene_document_legacy_write_float(
            payload, sizeof(payload), &position, object.character_controller.acceleration);
        result = result && test_scene_document_legacy_write_float(
            payload, sizeof(payload), &position, object.character_controller.deceleration);
        result = result && test_scene_document_legacy_write_float(
            payload, sizeof(payload), &position, object.character_controller.air_control);
        result = result && test_scene_document_legacy_write_float(
            payload, sizeof(payload), &position, object.character_controller.slope_limit_degrees);
        result = result && test_scene_document_legacy_write_u32(
            payload, sizeof(payload), &position, object.character_controller.layer);
        result = result && test_scene_document_legacy_write_u32(
            payload, sizeof(payload), &position, object.character_controller.mask);
    }
    if (version >= 4U)
    {
        result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, listener.position.x);
        result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, listener.position.y);
        result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, listener.position.z);
        result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, listener.forward.x);
        result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, listener.forward.y);
        result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, listener.forward.z);
        result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, listener.up.x);
        result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, listener.up.y);
        result = result && test_scene_document_legacy_write_float(payload, sizeof(payload), &position, listener.up.z);
    }
    if (version >= 7U)
    {
        result = result && test_scene_document_legacy_write_u32(
            payload, sizeof(payload), &position, 0U);
    }
    if (!result)
    {
        return false;
    }
    header[0] = 'H'; header[1] = 'S'; header[2] = 'C'; header[3] = 'N';
    result = test_scene_document_legacy_write_u32(header, sizeof(header), &(size_t){4U}, version) &&
        test_scene_document_legacy_write_u32(header, sizeof(header), &(size_t){8U}, 40U) &&
        test_scene_document_legacy_write_u64(header, sizeof(header), &(size_t){12U}, (uint64_t)position) &&
        test_scene_document_legacy_write_u32(header, sizeof(header), &(size_t){20U}, 1U) &&
        test_scene_document_legacy_write_u64(header, sizeof(header), &(size_t){24U}, 2U) &&
        test_scene_document_legacy_write_u32(header, sizeof(header), &(size_t){32U}, test_scene_document_legacy_checksum(payload, position));
    if (!result)
    {
        return false;
    }
#if defined(_WIN32)
    if (fopen_s(&file, path, "wb") != 0)
    {
        return false;
    }
#else
    file = fopen(path, "wb");
#endif
    if (file == NULL)
    {
        return false;
    }
    return fwrite(header, 1U, sizeof(header), file) == sizeof(header) &&
        fwrite(payload, 1U, position, file) == position && fclose(file) == 0;
}

static bool test_scene_document_write_v9_fixture(
    const char* source_path,
    const char* destination_path)
{
    const size_t environment_bytes = 49U * sizeof(uint32_t);
    const size_t render_settings_bytes = 18U * sizeof(uint32_t);
    const size_t render_resources_bytes = 144U * sizeof(uint32_t);
    FILE* source = NULL;
    unsigned char* data = NULL;
    long length;
    size_t source_size;
    size_t destination_size;
    size_t compacted_size;
    uint32_t object_count;
    bool result = false;

    if (source_path == NULL || destination_path == NULL ||
#if defined(_WIN32)
        (fopen_s(&source, source_path, "rb") != 0) ||
#else
        (source = fopen(source_path, "rb")) == NULL ||
#endif
        fseek(source, 0L, SEEK_END) != 0 ||
        (length = ftell(source)) < 40L ||
        fseek(source, 0L, SEEK_SET) != 0)
    {
        if (source != NULL) fclose(source);
        return false;
    }
    source_size = (size_t)length;
    if (source_size < 40U + environment_bytes + render_settings_bytes +
            render_resources_bytes ||
        (data = (unsigned char*)malloc(source_size)) == NULL ||
        fread(data, 1U, source_size, source) != source_size)
    {
        free(data);
        fclose(source);
        return false;
    }
    fclose(source);
    if (!test_scene_document_legacy_read_u32(
            data, source_size, &(size_t){20U}, &object_count) ||
        !test_scene_document_legacy_compact_current_extensions(
            data, source_size, (size_t)object_count, true, &compacted_size) ||
        compacted_size < environment_bytes + render_settings_bytes +
            render_resources_bytes)
    {
        free(data);
        return false;
    }
    destination_size = compacted_size - environment_bytes - render_settings_bytes -
        render_resources_bytes;
    (void)test_scene_document_legacy_write_u32(
        data, source_size, &(size_t){4U}, 9U);
    (void)test_scene_document_legacy_write_u64(
        data, source_size, &(size_t){12U}, (uint64_t)(destination_size - 40U));
    (void)test_scene_document_legacy_write_u32(
        data,
        source_size,
        &(size_t){32U},
        test_scene_document_legacy_checksum(data + 40U, destination_size - 40U));
    result = test_scene_document_write_bytes(
        destination_path, data, destination_size);
    free(data);
    return result;
}

static bool test_scene_document_write_v10_fixture(
    const char* source_path,
    const char* destination_path)
{
    const size_t render_settings_bytes = 18U * sizeof(uint32_t);
    const size_t render_resources_bytes = 144U * sizeof(uint32_t);
    FILE* source = NULL;
    unsigned char* data = NULL;
    long length;
    size_t source_size;
    size_t destination_size;
    size_t compacted_size;
    uint32_t object_count;
    bool result = false;

    if (source_path == NULL || destination_path == NULL ||
#if defined(_WIN32)
        (fopen_s(&source, source_path, "rb") != 0) ||
#else
        (source = fopen(source_path, "rb")) == NULL ||
#endif
        fseek(source, 0L, SEEK_END) != 0 ||
        (length = ftell(source)) < 40L ||
        fseek(source, 0L, SEEK_SET) != 0)
    {
        if (source != NULL) fclose(source);
        return false;
    }
    source_size = (size_t)length;
    if (source_size < 40U + render_settings_bytes + render_resources_bytes ||
        (data = (unsigned char*)malloc(source_size)) == NULL ||
        fread(data, 1U, source_size, source) != source_size)
    {
        free(data);
        fclose(source);
        return false;
    }
    fclose(source);
    if (!test_scene_document_legacy_read_u32(
            data, source_size, &(size_t){20U}, &object_count) ||
        !test_scene_document_legacy_compact_current_extensions(
            data, source_size, (size_t)object_count, true, &compacted_size) ||
        compacted_size < render_settings_bytes + render_resources_bytes)
    {
        free(data);
        return false;
    }
    destination_size = compacted_size - render_settings_bytes - render_resources_bytes;
    (void)test_scene_document_legacy_write_u32(
        data, source_size, &(size_t){4U}, 10U);
    (void)test_scene_document_legacy_write_u64(
        data, source_size, &(size_t){12U}, (uint64_t)(destination_size - 40U));
    (void)test_scene_document_legacy_write_u32(
        data,
        source_size,
        &(size_t){32U},
        test_scene_document_legacy_checksum(data + 40U, destination_size - 40U));
    result = test_scene_document_write_bytes(
        destination_path, data, destination_size);
    free(data);
    return result;
}

static bool test_scene_document_write_v11_fixture(
    const char* source_path,
    const char* destination_path)
{
    const size_t render_resources_bytes = 144U * sizeof(uint32_t);
    FILE* source = NULL;
    unsigned char* data = NULL;
    long length;
    size_t source_size;
    size_t destination_size;
    size_t compacted_size;
    uint32_t object_count;
    bool result = false;

    if (source_path == NULL || destination_path == NULL ||
#if defined(_WIN32)
        (fopen_s(&source, source_path, "rb") != 0) ||
#else
        (source = fopen(source_path, "rb")) == NULL ||
#endif
        fseek(source, 0L, SEEK_END) != 0 ||
        (length = ftell(source)) < 40L ||
        fseek(source, 0L, SEEK_SET) != 0)
    {
        if (source != NULL) fclose(source);
        return false;
    }
    source_size = (size_t)length;
    if (source_size < 40U + render_resources_bytes ||
        (data = (unsigned char*)malloc(source_size)) == NULL ||
        fread(data, 1U, source_size, source) != source_size)
    {
        free(data);
        fclose(source);
        return false;
    }
    fclose(source);
    if (!test_scene_document_legacy_read_u32(
            data, source_size, &(size_t){20U}, &object_count) ||
        !test_scene_document_legacy_compact_current_extensions(
            data,
            source_size,
            (size_t)object_count,
            true,
            &compacted_size) ||
        compacted_size < render_resources_bytes)
    {
        free(data);
        return false;
    }
    destination_size = compacted_size - render_resources_bytes;
    (void)test_scene_document_legacy_write_u32(
        data, source_size, &(size_t){4U}, 11U);
    (void)test_scene_document_legacy_write_u64(
        data, source_size, &(size_t){12U}, (uint64_t)(destination_size - 40U));
    (void)test_scene_document_legacy_write_u32(
        data,
        source_size,
        &(size_t){32U},
        test_scene_document_legacy_checksum(data + 40U, destination_size - 40U));
    result = test_scene_document_write_bytes(
        destination_path, data, destination_size);
    free(data);
    return result;
}

static bool test_scene_document_write_v12_fixture(
    const char* source_path,
    const char* destination_path)
{
    FILE* source = NULL;
    unsigned char* data = NULL;
    long length;
    size_t source_size;
    size_t destination_size;
    uint32_t object_count;
    bool result = false;

    if (source_path == NULL || destination_path == NULL ||
#if defined(_WIN32)
        (fopen_s(&source, source_path, "rb") != 0) ||
#else
        (source = fopen(source_path, "rb")) == NULL ||
#endif
        fseek(source, 0L, SEEK_END) != 0 ||
        (length = ftell(source)) < 40L ||
        fseek(source, 0L, SEEK_SET) != 0)
    {
        if (source != NULL) fclose(source);
        return false;
    }
    source_size = (size_t)length;
    if ((data = (unsigned char*)malloc(source_size)) == NULL ||
        fread(data, 1U, source_size, source) != source_size)
    {
        free(data);
        fclose(source);
        return false;
    }
    fclose(source);
    if (!test_scene_document_legacy_read_u32(
            data, source_size, &(size_t){20U}, &object_count) ||
        !test_scene_document_legacy_compact_current_extensions(
            data, source_size, (size_t)object_count, true, &destination_size))
    {
        free(data);
        return false;
    }
    (void)test_scene_document_legacy_write_u32(
        data, source_size, &(size_t){4U}, 12U);
    (void)test_scene_document_legacy_write_u64(
        data, source_size, &(size_t){12U}, (uint64_t)(destination_size - 40U));
    (void)test_scene_document_legacy_write_u32(
        data,
        source_size,
        &(size_t){32U},
        test_scene_document_legacy_checksum(data + 40U, destination_size - 40U));
    result = test_scene_document_write_bytes(
        destination_path, data, destination_size);
    free(data);
    return result;
}

static bool test_scene_document_write_v13_fixture(
    const char* source_path,
    const char* destination_path)
{
    FILE* source = NULL;
    unsigned char* data = NULL;
    long length;
    size_t source_size;
    size_t destination_size;
    uint32_t object_count;
    bool result = false;

    if (source_path == NULL || destination_path == NULL ||
#if defined(_WIN32)
        (fopen_s(&source, source_path, "rb") != 0) ||
#else
        (source = fopen(source_path, "rb")) == NULL ||
#endif
        fseek(source, 0L, SEEK_END) != 0 ||
        (length = ftell(source)) < 40L ||
        fseek(source, 0L, SEEK_SET) != 0)
    {
        if (source != NULL) fclose(source);
        return false;
    }
    source_size = (size_t)length;
    if ((data = (unsigned char*)malloc(source_size)) == NULL ||
        fread(data, 1U, source_size, source) != source_size)
    {
        free(data);
        fclose(source);
        return false;
    }
    fclose(source);
    if (!test_scene_document_legacy_read_u32(
            data, source_size, &(size_t){20U}, &object_count) ||
        !test_scene_document_legacy_compact_current_extensions(
            data, source_size, (size_t)object_count, false, &destination_size))
    {
        free(data);
        return false;
    }
    (void)test_scene_document_legacy_write_u32(
        data, source_size, &(size_t){4U}, 13U);
    (void)test_scene_document_legacy_write_u64(
        data, source_size, &(size_t){12U}, (uint64_t)(destination_size - 40U));
    (void)test_scene_document_legacy_write_u32(
        data,
        source_size,
        &(size_t){32U},
        test_scene_document_legacy_checksum(data + 40U, destination_size - 40U));
    result = test_scene_document_write_bytes(
        destination_path, data, destination_size);
    free(data);
    return result;
}

static bool test_scene_document_patch_render_resource_u32(
    const char* path,
    size_t resource_u32_index,
    uint32_t value)
{
    FILE* file = NULL;
    long length;
    const size_t render_resources_bytes = 144U * sizeof(uint32_t);
    const size_t header_bytes = 40U;
    size_t resource_start;

    if (path == NULL ||
#if defined(_WIN32)
        fopen_s(&file, path, "rb") != 0 ||
#else
        (file = fopen(path, "rb")) == NULL ||
#endif
        fseek(file, 0L, SEEK_END) != 0 ||
        (length = ftell(file)) < (long)(header_bytes + render_resources_bytes))
    {
        if (file != NULL) fclose(file);
        return false;
    }
    resource_start = (size_t)length - render_resources_bytes;
    fclose(file);
    return test_scene_document_patch_u32_and_checksum(
        path,
        (long)(resource_start + resource_u32_index * sizeof(uint32_t)),
        value);
}

static void test_scene_document_save_propagates_path_errors(void)
{
    henka_scene_document* document = NULL;
    henka_result result;

    assert(henka_scene_document_create(&document) == HENKA_SUCCESS);

    /* The first destination-path allocation must remain an allocation error;
     * it must not be collapsed into invalid-argument. */
    henka_memory_test_fail_after(0U);
    result = henka_scene_document_save_file(
        document, ".", "test_tmp/scene_document_path_error.hscene");
    henka_memory_test_disable_failures();
    assert(result == HENKA_ERROR_OUT_OF_MEMORY);

    /* Six allocations reach parent-directory preparation. Its allocation
     * failure must also remain visible to the caller. */
    henka_memory_test_fail_after(6U);
    result = henka_scene_document_save_file(
        document, ".", "test_tmp/scene_document_parent_error.hscene");
    henka_memory_test_disable_failures();
    assert(result == HENKA_ERROR_OUT_OF_MEMORY);

    henka_scene_document_destroy(document);
}

static void test_scene_document_load_propagates_path_errors(void)
{
    henka_scene_document* document = NULL;
    henka_result result;

    assert(henka_scene_document_create(&document) == HENKA_SUCCESS);

    /* The destination path allocation must remain an allocation error;
     * it must not be collapsed into invalid-argument. */
    henka_memory_test_fail_after(0U);
    result = henka_scene_document_load_file(
        document, ".", "test_tmp/scene_document_load_path_error.hscene");
    henka_memory_test_disable_failures();
    assert(result == HENKA_ERROR_OUT_OF_MEMORY);

    henka_scene_document_destroy(document);
}

int main(void)
{
    const char* first_path = "build/test_tmp/scene_document_slice_b.hscene";
    const char* second_path = "build/test_tmp/scene_document_slice_b_copy.hscene";
    const char* malformed_path = "build/test_tmp/scene_document_malformed.hscene";
    const char* legacy_path = "build/test_tmp/scene_document_legacy_v1.hscene";
    const char* v2_path = "build/test_tmp/scene_document_legacy_v2.hscene";
    const char* v3_path = "build/test_tmp/scene_document_legacy_v3.hscene";
    const char* v4_path = "build/test_tmp/scene_document_legacy_v4.hscene";
    const char* v5_path = "build/test_tmp/scene_document_legacy_v5.hscene";
    const char* v6_path = "build/test_tmp/scene_document_legacy_v6.hscene";
    const char* v7_path = "build/test_tmp/scene_document_legacy_v7.hscene";
    const char* v8_path = "build/test_tmp/scene_document_legacy_v8.hscene";
    const char* v9_path = "build/test_tmp/scene_document_legacy_v9.hscene";
    const char* v10_path = "build/test_tmp/scene_document_legacy_v10.hscene";
    const char* v11_path = "build/test_tmp/scene_document_legacy_v11.hscene";
    const char* v12_path = "build/test_tmp/scene_document_legacy_v12.hscene";
    const char* v13_path = "build/test_tmp/scene_document_legacy_v13.hscene";
    const char* malformed_resources_path =
        "build/test_tmp/scene_document_malformed_resources.hscene";
    const char* camera_path = "build/test_tmp/scene_document_camera.hscene";
    const unsigned char malformed_data[] = {'H', 'S', 'C', 'N', 1U};
    henka_scene_document* document = NULL;
    henka_scene_document* loaded = NULL;
    henka_scene_document* exhausted = NULL;
    henka_scene_document* camera_document = NULL;
    henka_scene_document_object object;
    henka_scene_document_object loaded_object;
    henka_scene_document_object invalid_object;
    henka_scene_document_object maximum_id_object;
    henka_scene_document_object recycled_id_object;
    henka_scene_document_behavior behavior;
    henka_scene_document_behavior loaded_behavior;
    henka_audio_listener authored_listener = henka_audio_listener_default();
    henka_audio_listener loaded_listener;
    henka_scene_environment_desc loaded_environment;
    henka_scene_render_settings loaded_render_settings;
    henka_scene_render_resources authored_render_resources;
    henka_scene_render_resources loaded_render_resources;
    henka_camera authored_camera;
    henka_camera loaded_camera;
    henka_scene_document_id first_id = HENKA_INVALID_SCENE_DOCUMENT_ID;
    henka_scene_document_id added_id = HENKA_INVALID_SCENE_DOCUMENT_ID;
    henka_scene_document_id duplicate_id = HENKA_INVALID_SCENE_DOCUMENT_ID;
    henka_scene_document_behavior_id behavior_id = HENKA_INVALID_SCENE_DOCUMENT_BEHAVIOR_ID;
    char inspection[HENKA_SCENE_DOCUMENT_MAX_INSPECTION_BYTES];
    size_t index;
    size_t inspection_size = 0U;
    int result = 1;

    if (henka_scene_document_create(&document) != HENKA_SUCCESS ||
        henka_scene_document_create(&loaded) != HENKA_SUCCESS ||
        henka_scene_document_create(&exhausted) != HENKA_SUCCESS ||
        henka_scene_document_create(&camera_document) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    test_scene_document_save_propagates_path_errors();
    test_scene_document_load_propagates_path_errors();
    authored_listener.position = (henka_vec3){4.0f, 2.0f, -6.0f};
    authored_listener.forward = (henka_vec3){0.0f, -0.25f, -1.0f};
    authored_listener.up = (henka_vec3){0.0f, 1.0f, -0.1f};
    if (henka_scene_document_set_audio_listener(document, authored_listener) != HENKA_SUCCESS ||
        henka_scene_document_get_audio_listener(document, &loaded_listener) != HENKA_SUCCESS ||
        loaded_listener.position.x != authored_listener.position.x ||
        loaded_listener.forward.y != authored_listener.forward.y ||
        loaded_listener.up.z != authored_listener.up.z)
    {
        goto cleanup;
    }
    authored_render_resources = henka_scene_render_resources_default();
    authored_render_resources.local_light_active[1] = true;
    authored_render_resources.local_lights[1] = (henka_scene_light_desc){
        HENKA_SCENE_LIGHT_POINT,
        {1.0f, 2.0f, 3.0f},
        {0.0f, -1.0f, -1.0f},
        {0.8f, 0.7f, 0.6f},
        12.0f,
        10.0f,
        0.0f,
        0.0f,
        true};
    authored_render_resources.reflection_probe_active[3] = true;
    authored_render_resources.reflection_probes[3] = (henka_scene_reflection_probe_desc){
        {-2.0f, 1.0f, 4.0f},
        {3.0f, 4.0f, 5.0f},
        0.65f,
        true,
        true};
    if (henka_scene_document_set_render_resources(
            document, authored_render_resources) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    loaded_listener.forward = (henka_vec3){0.0f, 0.0f, 0.0f};
    if (henka_scene_document_set_audio_listener(document, loaded_listener) != HENKA_ERROR_INVALID_ARGUMENT ||
        henka_scene_document_get_audio_listener(document, &loaded_listener) != HENKA_SUCCESS ||
        loaded_listener.position.x != authored_listener.position.x ||
        loaded_listener.forward.y != authored_listener.forward.y ||
        loaded_listener.up.z != authored_listener.up.z)
    {
        goto cleanup;
    }
    for (index = 0U; index < 256U; ++index)
    {
        int written;
        object = henka_scene_document_object_default();
        written = snprintf(object.name, sizeof(object.name), "object_%zu", index);
        if (written <= 0 || (size_t)written >= sizeof(object.name))
        {
            goto cleanup;
        }
        object.transform.position.x = (float)index;
        object.source.kind = HENKA_SCENE_DOCUMENT_SOURCE_PRIMITIVE;
        object.source.primitive = index % 2U == 0U
            ? HENKA_SCENE_DOCUMENT_PRIMITIVE_BOX
            : HENKA_SCENE_DOCUMENT_PRIMITIVE_SPHERE;
        object.source.primitive_dimensions = (henka_vec3){1.0f, 2.0f, 3.0f};
        object.interaction.enabled = index % 3U == 0U;
        object.interaction.max_distance = 12.0f;
        (void)snprintf(object.interaction.prompt, sizeof(object.interaction.prompt), "Use object %zu", index);
        if (index == 0U)
        {
            object.audio.enabled = true;
            object.audio.looping = true;
            object.audio.spatial = true;
            object.audio.bus = HENKA_AUDIO_BUS_AMBIENCE;
            object.audio.gain = 0.75f;
            object.audio.pitch = 1.25f;
            object.audio.min_distance = 2.0f;
            object.audio.max_distance = 40.0f;
            (void)snprintf(
                object.audio.clip_path,
                sizeof(object.audio.clip_path),
                "audio/scene_wind.wav");
            object.character_controller.enabled = true;
            object.character_controller.radius = 0.45f;
            object.character_controller.half_height = 0.5f;
            object.character_controller.max_speed = 2.0f;
            object.character_controller.jump_speed = 4.0f;
            object.character_controller.acceleration = 3.0f;
            object.character_controller.deceleration = 5.0f;
            object.character_controller.air_control = 0.75f;
            object.character_controller.slope_limit_degrees = 42.0f;
            object.renderer.material_override = true;
            object.renderer.material_type = HENKA_MATERIAL_TYPE_UNLIT;
            object.renderer.base_color_uv_set = 1;
            object.renderer.normal_uv_set = 1;
            object.renderer.metallic_roughness_uv_set = 1;
            object.renderer.occlusion_uv_set = 1;
            object.renderer.emissive_uv_set = 1;
            object.renderer.transmission_uv_set = 1;
            object.renderer.thickness_uv_set = 1;
            object.renderer.specular_factor = 0.72f;
            object.renderer.specular_color = (henka_vec3){0.21f, 0.32f, 0.43f};
            object.renderer.ior = 1.31f;
            object.renderer.transmission = 0.24f;
            object.renderer.thickness = 0.73f;
            object.renderer.attenuation_distance = 12.5f;
            object.renderer.attenuation_color = (henka_vec3){0.41f, 0.52f, 0.63f};
            object.renderer.subsurface = 0.17f;
            object.renderer.subsurface_color = (henka_vec3){0.64f, 0.35f, 0.26f};
            object.renderer.normal_scale = 0.83f;
            object.renderer.occlusion_strength = 0.74f;
            object.renderer.clearcoat = 0.36f;
            object.renderer.clearcoat_roughness = 0.29f;
            object.renderer.alpha_cutoff = 0.41f;
            object.renderer.alpha_mode = HENKA_MATERIAL_ALPHA_BLENDED;
            object.renderer.use_texture = true;
            object.renderer.use_lighting = false;
            object.renderer.depth_test = false;
            object.renderer.double_sided = true;
            object.renderer.cast_shadows = false;
            object.renderer.receive_shadows = false;
            object.renderer.sheen_color = (henka_vec3){0.18f, 0.27f, 0.39f};
            object.renderer.sheen_roughness = 0.48f;
        }
        if (henka_scene_document_add_object(document, &object, &added_id) != HENKA_SUCCESS)
        {
            goto cleanup;
        }
        if (index == 0U)
        {
            const henka_scene_document_id original_id = added_id;
            first_id = added_id;
            if (henka_scene_document_duplicate_object(document, original_id, &duplicate_id) != HENKA_SUCCESS ||
                duplicate_id == original_id)
            {
                goto cleanup;
            }
        }
    }
    behavior = henka_scene_document_behavior_default();
    behavior.language = HENKA_SCRIPT_LANGUAGE_LUA;
    (void)snprintf(behavior.asset_path, sizeof(behavior.asset_path), "scripts/rotate.lua");
    if (henka_scene_document_add_behavior(document, first_id, &behavior, &behavior_id) != HENKA_SUCCESS ||
        behavior_id == HENKA_INVALID_SCENE_DOCUMENT_BEHAVIOR_ID ||
        henka_scene_document_get_behavior_count(document, first_id) != 1U ||
        henka_scene_document_get_behavior_at(
            document,
            first_id,
            0U,
            &loaded_behavior) != HENKA_SUCCESS ||
        loaded_behavior.id != behavior_id ||
        henka_scene_document_get_behavior_at(
            document,
            first_id,
            1U,
            &loaded_behavior) == HENKA_SUCCESS ||
        henka_scene_document_add_behavior(
            document,
            first_id,
            &(henka_scene_document_behavior){
                HENKA_INVALID_SCENE_DOCUMENT_BEHAVIOR_ID,
                true,
                HENKA_SCRIPT_LANGUAGE_HENKASCRIPT,
                "scripts/rotate.lua"},
            &behavior_id) == HENKA_SUCCESS ||
        henka_scene_document_get_object_count(document) != 257U ||
        henka_scene_document_validate(document) != HENKA_SUCCESS ||
        henka_scene_document_save_file(document, ".", first_path) != HENKA_SUCCESS ||
        henka_scene_document_save_file(document, ".", second_path) != HENKA_SUCCESS ||
        !test_scene_document_write_v3_to_v8_fixture(v3_path, 3U) ||
        !test_scene_document_files_equal(first_path, second_path) ||
        !test_scene_document_patch_u32(second_path, 4L, UINT32_C(4)) ||
        henka_scene_document_format_inspection(
            document, inspection, sizeof(inspection), &inspection_size) != HENKA_SUCCESS ||
        inspection_size == 0U || strstr(inspection, "HSCN version=14 objects=257") == NULL ||
        henka_scene_document_load_file(loaded, ".", first_path) != HENKA_SUCCESS ||
        henka_scene_document_get_render_resources(loaded, &loaded_render_resources) != HENKA_SUCCESS ||
        !loaded_render_resources.local_light_active[1] ||
        loaded_render_resources.local_lights[1].position.x != 1.0f ||
        loaded_render_resources.local_lights[1].direction.y != -1.0f / sqrtf(2.0f) ||
        !loaded_render_resources.reflection_probe_active[3] ||
        loaded_render_resources.reflection_probes[3].influence != 0.65f)
    {
        fprintf(stderr, "scene document test failed during deterministic save/inspection\n");
        goto cleanup;
    }
    if (!test_scene_document_write_v13_fixture(first_path, v13_path) ||
        henka_scene_document_load_file(loaded, ".", v13_path) != HENKA_SUCCESS ||
        henka_scene_document_get_object_count(loaded) != 257U ||
        henka_scene_document_get_render_resources(loaded, &loaded_render_resources) != HENKA_SUCCESS ||
        !loaded_render_resources.local_light_active[1] ||
        !loaded_render_resources.reflection_probe_active[3] ||
        !test_scene_document_write_v12_fixture(first_path, v12_path) ||
        henka_scene_document_load_file(loaded, ".", v12_path) != HENKA_SUCCESS ||
        henka_scene_document_get_render_resources(loaded, &loaded_render_resources) != HENKA_SUCCESS ||
        !loaded_render_resources.local_light_active[1] ||
        !loaded_render_resources.reflection_probe_active[3] ||
        !test_scene_document_write_v11_fixture(first_path, v11_path) ||
        henka_scene_document_load_file(loaded, ".", v11_path) != HENKA_SUCCESS ||
        henka_scene_document_get_render_resources(loaded, &loaded_render_resources) != HENKA_SUCCESS ||
        loaded_render_resources.local_light_active[0] ||
        loaded_render_resources.local_light_active[1] ||
        loaded_render_resources.reflection_probe_active[3])
    {
        fprintf(stderr, "scene document test failed during v11 resource compatibility load\n");
        goto cleanup;
    }
    if (henka_scene_document_load_file(loaded, ".", first_path) != HENKA_SUCCESS ||
        henka_scene_document_save_file(document, ".", malformed_resources_path) != HENKA_SUCCESS ||
        !test_scene_document_patch_render_resource_u32(
            malformed_resources_path,
            28U,
            0U) ||
        henka_scene_document_load_file(loaded, ".", malformed_resources_path) == HENKA_SUCCESS ||
        henka_scene_document_get_render_resources(loaded, &loaded_render_resources) != HENKA_SUCCESS ||
        !loaded_render_resources.local_light_active[1] ||
        loaded_render_resources.local_lights[1].range != 10.0f)
    {
        fprintf(stderr, "scene document test failed during malformed resource retention\n");
        goto cleanup;
    }
    if (!test_scene_document_write_v10_fixture(first_path, v10_path) ||
        henka_scene_document_load_file(loaded, ".", v10_path) != HENKA_SUCCESS ||
        henka_scene_document_get_render_settings(
            loaded,
            &loaded_render_settings) != HENKA_SUCCESS ||
        loaded_render_settings.light_intensity != 3.0f ||
        loaded_render_settings.fog.enabled)
    {
        fprintf(stderr, "scene document test failed during v10 compatibility load\n");
        goto cleanup;
    }
    if (!test_scene_document_write_v9_fixture(first_path, v9_path) ||
        henka_scene_document_load_file(loaded, ".", v9_path) != HENKA_SUCCESS ||
        henka_scene_document_get_object_count(loaded) != 257U ||
        henka_scene_document_get_environment(loaded, &loaded_environment) != HENKA_SUCCESS ||
        loaded_environment.mode != HENKA_SCENE_ENVIRONMENT_GRADIENT ||
        loaded_environment.intensity != 1.5f ||
        loaded_environment.hdr_texture != NULL)
    {
        fprintf(stderr, "scene document test failed during v9 compatibility load\n");
        goto cleanup;
    }
    if (henka_scene_document_load_file(loaded, ".", first_path) != HENKA_SUCCESS ||
        !test_scene_document_patch_v10_environment_mode(second_path, 99U) ||
        henka_scene_document_load_file(loaded, ".", second_path) == HENKA_SUCCESS ||
        henka_scene_document_get_object_count(loaded) != 257U ||
        henka_scene_document_get_environment(loaded, &loaded_environment) != HENKA_SUCCESS ||
        loaded_environment.mode != HENKA_SCENE_ENVIRONMENT_GRADIENT ||
        loaded_environment.intensity != 1.5f)
    {
        fprintf(stderr, "scene document test failed during malformed v10 environment retention\n");
        goto cleanup;
    }
    if (!test_scene_document_write_v3_to_v8_fixture(v5_path, 5U) ||
        henka_scene_document_load_file(loaded, ".", v5_path) != HENKA_SUCCESS ||
        henka_scene_document_get_object_at(loaded, 0U, &loaded_object) != HENKA_SUCCESS ||
        loaded_object.parent_id != HENKA_INVALID_SCENE_DOCUMENT_ID ||
        henka_scene_document_save_file(document, ".", first_path) != HENKA_SUCCESS)
    {
        fprintf(stderr, "scene document test failed during v5 compatibility load\n");
        goto cleanup;
    }
    if (henka_scene_document_load_file(loaded, ".", first_path) != HENKA_SUCCESS ||
        henka_scene_document_get_object_count(loaded) != 257U ||
        henka_scene_document_get_object(loaded, first_id, &loaded_object) != HENKA_SUCCESS ||
        strcmp(loaded_object.name, "object_0") != 0 ||
        loaded_object.source.kind != HENKA_SCENE_DOCUMENT_SOURCE_PRIMITIVE ||
        !loaded_object.audio.enabled ||
        !loaded_object.audio.looping ||
        !loaded_object.audio.spatial ||
        loaded_object.audio.bus != HENKA_AUDIO_BUS_AMBIENCE ||
        loaded_object.audio.gain != 0.75f ||
        loaded_object.audio.pitch != 1.25f ||
        loaded_object.audio.min_distance != 2.0f ||
        loaded_object.audio.max_distance != 40.0f ||
        strcmp(loaded_object.audio.clip_path, "audio/scene_wind.wav") != 0 ||
        !loaded_object.character_controller.enabled ||
        loaded_object.character_controller.radius != 0.45f ||
        loaded_object.character_controller.half_height != 0.5f ||
        loaded_object.character_controller.max_speed != 2.0f ||
        loaded_object.character_controller.jump_speed != 4.0f ||
        loaded_object.character_controller.acceleration != 3.0f ||
        loaded_object.character_controller.deceleration != 5.0f ||
        loaded_object.character_controller.air_control != 0.75f ||
        loaded_object.character_controller.slope_limit_degrees != 42.0f ||
        loaded_object.renderer.material_type != HENKA_MATERIAL_TYPE_UNLIT ||
        loaded_object.renderer.base_color_uv_set != 1 ||
        loaded_object.renderer.normal_uv_set != 1 ||
        loaded_object.renderer.metallic_roughness_uv_set != 1 ||
        loaded_object.renderer.occlusion_uv_set != 1 ||
        loaded_object.renderer.emissive_uv_set != 1 ||
        loaded_object.renderer.transmission_uv_set != 1 ||
        loaded_object.renderer.thickness_uv_set != 1 ||
        loaded_object.renderer.specular_factor != 0.72f ||
        loaded_object.renderer.specular_color.x != 0.21f ||
        loaded_object.renderer.specular_color.y != 0.32f ||
        loaded_object.renderer.specular_color.z != 0.43f ||
        loaded_object.renderer.ior != 1.31f ||
        loaded_object.renderer.transmission != 0.24f ||
        loaded_object.renderer.thickness != 0.73f ||
        loaded_object.renderer.attenuation_distance != 12.5f ||
        loaded_object.renderer.attenuation_color.x != 0.41f ||
        loaded_object.renderer.attenuation_color.y != 0.52f ||
        loaded_object.renderer.attenuation_color.z != 0.63f ||
        loaded_object.renderer.subsurface != 0.17f ||
        loaded_object.renderer.subsurface_color.x != 0.64f ||
        loaded_object.renderer.subsurface_color.y != 0.35f ||
        loaded_object.renderer.subsurface_color.z != 0.26f ||
        loaded_object.renderer.normal_scale != 0.83f ||
        loaded_object.renderer.occlusion_strength != 0.74f ||
        loaded_object.renderer.clearcoat != 0.36f ||
        loaded_object.renderer.clearcoat_roughness != 0.29f ||
        loaded_object.renderer.alpha_cutoff != 0.41f ||
        loaded_object.renderer.alpha_mode != HENKA_MATERIAL_ALPHA_BLENDED ||
        !loaded_object.renderer.use_texture ||
        loaded_object.renderer.use_lighting ||
        loaded_object.renderer.depth_test ||
        !loaded_object.renderer.double_sided ||
        loaded_object.renderer.cast_shadows ||
        loaded_object.renderer.receive_shadows ||
        loaded_object.renderer.sheen_color.x != 0.18f ||
        loaded_object.renderer.sheen_color.y != 0.27f ||
        loaded_object.renderer.sheen_color.z != 0.39f ||
        loaded_object.renderer.sheen_roughness != 0.48f ||
        henka_scene_document_get_audio_listener(loaded, &loaded_listener) != HENKA_SUCCESS ||
        loaded_listener.position.x != authored_listener.position.x ||
        loaded_listener.position.y != authored_listener.position.y ||
        loaded_listener.position.z != authored_listener.position.z ||
        loaded_listener.forward.y != authored_listener.forward.y ||
        loaded_listener.up.z != authored_listener.up.z ||
        henka_scene_document_get_behavior_count(loaded, first_id) != 1U ||
        henka_scene_document_get_behavior(
            loaded,
            first_id,
            behavior_id,
            &loaded_behavior) != HENKA_SUCCESS ||
        loaded_behavior.language != HENKA_SCRIPT_LANGUAGE_LUA ||
        strcmp(loaded_behavior.asset_path, "scripts/rotate.lua") != 0)
    {
        fprintf(stderr, "scene document test failed during round-trip load\n");
        goto cleanup;
    }
    invalid_object = loaded_object;
    invalid_object.character_controller.radius = 0.0f;
    invalid_object.physics.enabled = true;
    if (henka_scene_document_set_object(loaded, &invalid_object) != HENKA_ERROR_INVALID_ARGUMENT ||
        henka_scene_document_get_object(loaded, first_id, &invalid_object) != HENKA_SUCCESS ||
        !invalid_object.character_controller.enabled ||
        invalid_object.character_controller.radius != 0.45f ||
        invalid_object.physics.enabled)
    {
        fprintf(stderr, "scene document test failed during invalid controller transaction\n");
        goto cleanup;
    }
    if (henka_scene_document_get_object(loaded, duplicate_id, &loaded_object) != HENKA_SUCCESS ||
        loaded_object.parent_id != HENKA_INVALID_SCENE_DOCUMENT_ID)
    {
        fprintf(stderr, "scene document test failed during root hierarchy migration\n");
        goto cleanup;
    }
    loaded_object.parent_id = first_id;
    if (henka_scene_document_set_object(loaded, &loaded_object) != HENKA_SUCCESS ||
        henka_scene_document_validate(loaded) != HENKA_SUCCESS ||
        henka_scene_document_save_file(loaded, ".", first_path) != HENKA_SUCCESS ||
        henka_scene_document_load_file(loaded, ".", first_path) != HENKA_SUCCESS ||
        henka_scene_document_get_object(loaded, duplicate_id, &loaded_object) != HENKA_SUCCESS ||
        loaded_object.parent_id != first_id)
    {
        fprintf(stderr, "scene document test failed during parent hierarchy round-trip\n");
        goto cleanup;
    }
    if (!test_scene_document_patch_u64_and_checksum(first_path, 48L, duplicate_id) ||
        henka_scene_document_load_file(loaded, ".", first_path) == HENKA_SUCCESS ||
        henka_scene_document_get_object(loaded, duplicate_id, &loaded_object) != HENKA_SUCCESS ||
        loaded_object.parent_id != first_id ||
        henka_scene_document_save_file(loaded, ".", first_path) != HENKA_SUCCESS)
    {
        fprintf(stderr, "scene document test failed during malformed hierarchy retention\n");
        goto cleanup;
    }
    loaded_object.parent_id = UINT64_C(999999999);
    if (henka_scene_document_set_object(loaded, &loaded_object) == HENKA_SUCCESS ||
        henka_scene_document_get_object(loaded, duplicate_id, &loaded_object) != HENKA_SUCCESS ||
        loaded_object.parent_id != first_id ||
        henka_scene_document_get_object(loaded, first_id, &object) != HENKA_SUCCESS)
    {
        fprintf(stderr, "scene document test failed during invalid parent rejection\n");
        goto cleanup;
    }
    object.parent_id = duplicate_id;
    if (henka_scene_document_set_object(loaded, &object) == HENKA_SUCCESS ||
        henka_scene_document_get_object(loaded, first_id, &object) != HENKA_SUCCESS ||
        object.parent_id != HENKA_INVALID_SCENE_DOCUMENT_ID ||
        henka_scene_document_remove_object(loaded, first_id) == HENKA_SUCCESS)
    {
        fprintf(stderr, "scene document test failed during hierarchy cycle/removal checks\n");
        goto cleanup;
    }
    loaded_behavior.enabled = false;
    if (henka_scene_document_set_behavior(loaded, first_id, &loaded_behavior) != HENKA_SUCCESS ||
        henka_scene_document_get_behavior(loaded, first_id, behavior_id, &loaded_behavior) != HENKA_SUCCESS ||
        loaded_behavior.enabled ||
        henka_scene_document_remove_behavior(loaded, first_id, behavior_id) != HENKA_SUCCESS ||
        henka_scene_document_get_behavior_count(loaded, first_id) != 0U)
    {
        fprintf(stderr, "scene document test failed during behavior mutation\n");
        goto cleanup;
    }
    if (!test_scene_document_write_legacy_fixture(legacy_path) ||
        henka_scene_document_load_file(loaded, ".", legacy_path) != HENKA_SUCCESS ||
        henka_scene_document_get_object_count(loaded) != 1U ||
        henka_scene_document_get_object_at(loaded, 0U, &loaded_object) != HENKA_SUCCESS ||
        strcmp(loaded_object.name, "legacy") != 0 ||
        henka_scene_document_get_audio_listener(loaded, &loaded_listener) != HENKA_SUCCESS ||
        loaded_listener.position.x != 0.0f ||
        loaded_listener.forward.z != -1.0f ||
        loaded_listener.up.y != 1.0f ||
        henka_scene_document_get_behavior_count(loaded, loaded_object.id) != 0U ||
        henka_scene_document_load_file(loaded, ".", first_path) != HENKA_SUCCESS ||
        henka_scene_document_get_object_count(loaded) != 257U)
    {
        fprintf(stderr, "scene document test failed during v1 migration\n");
        goto cleanup;
    }
    if (!test_scene_document_write_v3_to_v8_fixture(v4_path, 4U) ||
        henka_scene_document_load_file(loaded, ".", v4_path) != HENKA_SUCCESS ||
        henka_scene_document_get_object(loaded, first_id, &loaded_object) != HENKA_SUCCESS ||
        loaded_object.audio.streaming)
    {
        fprintf(stderr, "scene document test failed during v4 migration\n");
        goto cleanup;
    }
    if (!test_scene_document_write_v3_to_v8_fixture(v6_path, 6U) ||
        henka_scene_document_load_file(loaded, ".", v6_path) != HENKA_SUCCESS ||
        henka_scene_document_has_camera(loaded) ||
        henka_scene_document_get_camera(loaded, &(henka_camera){0}) == HENKA_SUCCESS)
    {
        fprintf(stderr, "scene document test failed during v6 compatibility load\n");
        goto cleanup;
    }
    if (!test_scene_document_write_v3_to_v8_fixture(v7_path, 7U) ||
        henka_scene_document_load_file(loaded, ".", v7_path) != HENKA_SUCCESS ||
        henka_scene_document_get_object_at(loaded, 0U, &loaded_object) != HENKA_SUCCESS ||
        loaded_object.character_controller.enabled ||
        henka_scene_document_has_camera(loaded))
    {
        fprintf(stderr, "scene document test failed during v7 compatibility load\n");
        goto cleanup;
    }
    {
        const henka_scene_document_object default_object =
            henka_scene_document_object_default();
        henka_result v8_result;
        henka_result v8_object_result;
        bool v8_fixture_result = test_scene_document_write_v3_to_v8_fixture(v8_path, 8U);
        loaded_object = default_object;
        v8_result = v8_fixture_result
            ? henka_scene_document_load_file(loaded, ".", v8_path)
            : HENKA_ERROR_INVALID_ARGUMENT;
        v8_object_result = v8_result == HENKA_SUCCESS
            ? henka_scene_document_get_object_at(loaded, 0U, &loaded_object)
            : HENKA_ERROR_INVALID_ARGUMENT;
        if (!v8_fixture_result || v8_result != HENKA_SUCCESS ||
            v8_object_result != HENKA_SUCCESS ||
            loaded_object.character_controller.enabled ||
            loaded_object.character_controller.radius != default_object.character_controller.radius ||
            loaded_object.renderer.material_type != HENKA_MATERIAL_TYPE_LIT ||
            loaded_object.renderer.specular_factor != 1.0f ||
            !loaded_object.renderer.use_lighting)
        {
            fprintf(stderr, "scene document test failed during v8 compatibility load\n");
            goto cleanup;
        }
    }
    authored_camera = henka_camera_create_perspective(
        55.0f * HENKA_DEG_TO_RAD,
        16.0f / 9.0f,
        0.2f,
        800.0f);
    authored_camera.position = (henka_vec3){2.0f, 3.0f, 9.0f};
    authored_camera.yaw_radians = -0.4f;
    authored_camera.pitch_radians = 0.2f;
    authored_camera.roll_radians = 0.05f;
    if (henka_scene_document_set_camera(camera_document, &authored_camera) != HENKA_SUCCESS ||
        henka_scene_document_save_file(camera_document, ".", camera_path) != HENKA_SUCCESS ||
        henka_scene_document_load_file(loaded, ".", camera_path) != HENKA_SUCCESS ||
        !henka_scene_document_has_camera(loaded) ||
        henka_scene_document_get_camera(loaded, &loaded_camera) != HENKA_SUCCESS ||
        loaded_camera.position.x != authored_camera.position.x ||
        loaded_camera.yaw_radians != authored_camera.yaw_radians ||
        loaded_camera.projection_mode != authored_camera.projection_mode ||
        !test_scene_document_patch_u32_and_checksum(camera_path, 76L, 2U) ||
        henka_scene_document_load_file(loaded, ".", camera_path) == HENKA_SUCCESS ||
        henka_scene_document_get_camera(loaded, &loaded_camera) != HENKA_SUCCESS ||
        loaded_camera.position.x != authored_camera.position.x)
    {
        fprintf(stderr, "scene document test failed during camera validation/retention\n");
        goto cleanup;
    }
    if (henka_scene_document_load_file(loaded, ".", v3_path) != HENKA_SUCCESS ||
        henka_scene_document_get_audio_listener(loaded, &loaded_listener) != HENKA_SUCCESS ||
        loaded_listener.position.x != 0.0f ||
        loaded_listener.forward.z != -1.0f ||
        loaded_listener.up.y != 1.0f)
    {
        fprintf(stderr, "scene document test failed during v3 migration\n");
        goto cleanup;
    }
    if (!test_scene_document_write_v2_fixture(v2_path) ||
        henka_scene_document_load_file(loaded, ".", v2_path) != HENKA_SUCCESS ||
        henka_scene_document_get_object_count(loaded) != 1U ||
        henka_scene_document_get_object_at(loaded, 0U, &loaded_object) != HENKA_SUCCESS ||
        strcmp(loaded_object.name, "v2") != 0 ||
        loaded_object.audio.enabled ||
        loaded_object.audio.clip_path[0] != '\0' ||
        henka_scene_document_get_audio_listener(loaded, &loaded_listener) != HENKA_SUCCESS ||
        loaded_listener.position.x != 0.0f ||
        loaded_listener.forward.z != -1.0f ||
        loaded_listener.up.y != 1.0f ||
        henka_scene_document_get_behavior_count(loaded, loaded_object.id) != 0U)
    {
        fprintf(stderr, "scene document test failed during v2 migration\n");
        goto cleanup;
    }
    if (henka_scene_document_load_file(loaded, ".", first_path) != HENKA_SUCCESS ||
        henka_scene_document_save_file(document, ".", "../escape.hscene") != HENKA_ERROR_INVALID_ARGUMENT ||
        !test_scene_document_write_bytes(malformed_path, malformed_data, sizeof(malformed_data)) ||
        henka_scene_document_load_file(loaded, ".", malformed_path) == HENKA_SUCCESS ||
        henka_scene_document_get_object_count(loaded) != 257U)
    {
        fprintf(stderr, "scene document test failed during confinement/malformed retention\n");
        goto cleanup;
    }
    if (!test_scene_document_patch_u32(first_path, 36L, UINT32_C(1)) ||
        henka_scene_document_load_file(loaded, ".", first_path) == HENKA_SUCCESS ||
        henka_scene_document_get_object_count(loaded) != 257U ||
        !test_scene_document_patch_u32(second_path, 48L, UINT32_C(0x80000000)) ||
        henka_scene_document_load_file(loaded, ".", second_path) == HENKA_SUCCESS ||
        henka_scene_document_get_object_count(loaded) != 257U)
    {
        fprintf(stderr, "scene document test failed during corrupted-header retention\n");
        goto cleanup;
    }
    maximum_id_object = henka_scene_document_object_default();
    maximum_id_object.id = UINT64_MAX;
    recycled_id_object = henka_scene_document_object_default();
    recycled_id_object.id = UINT64_C(10000);
    if (henka_scene_document_add_object(exhausted, &maximum_id_object, &first_id) != HENKA_SUCCESS ||
        henka_scene_document_add_object(exhausted, &object, &duplicate_id) != HENKA_ERROR_LIMIT ||
        henka_scene_document_add_object(exhausted, &recycled_id_object, &duplicate_id) != HENKA_ERROR_LIMIT ||
        henka_scene_document_validate(exhausted) != HENKA_SUCCESS)
    {
        fprintf(stderr, "scene document test failed during ID exhaustion checks\n");
        goto cleanup;
    }
    if (henka_scene_document_get_object_at(document, 0U, &object) != HENKA_SUCCESS)
    {
        fprintf(stderr, "scene document test failed preparing streamed audio object\n");
        goto cleanup;
    }
    object.audio.streaming = true;
    if (henka_scene_document_set_object(document, &object) != HENKA_SUCCESS ||
        henka_scene_document_save_file(document, ".", first_path) != HENKA_SUCCESS ||
        henka_scene_document_load_file(loaded, ".", first_path) != HENKA_SUCCESS ||
        henka_scene_document_get_object(loaded, object.id, &loaded_object) != HENKA_SUCCESS ||
        !loaded_object.audio.streaming ||
        henka_scene_document_format_inspection(
            loaded, inspection, sizeof(inspection), &inspection_size) != HENKA_SUCCESS ||
        strstr(inspection, "HSCN version=14") == NULL)
    {
        fprintf(stderr, "scene document test failed during streamed audio v7 round-trip\n");
        goto cleanup;
    }
    if (!test_scene_document_patch_u32(first_path, 4L, UINT32_C(4)) ||
        henka_scene_document_load_file(loaded, ".", first_path) == HENKA_SUCCESS ||
        henka_scene_document_get_object(loaded, object.id, &loaded_object) != HENKA_SUCCESS ||
        !loaded_object.audio.streaming)
    {
        fprintf(stderr, "scene document test failed rejecting streamed v4 interpretation\n");
        goto cleanup;
    }
    result = 0;

cleanup:
    remove(camera_path);
    remove(v11_path);
    remove(malformed_resources_path);
    henka_scene_document_destroy(exhausted);
    henka_scene_document_destroy(camera_document);
    henka_scene_document_destroy(loaded);
    henka_scene_document_destroy(document);
    return result;
}
