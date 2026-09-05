#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <henka/core.h>
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

static bool test_scene_document_write_v3_to_v7_fixture(
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
            version != 6U && version != 7U))
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
            (version == 5U ? "v5" : (version == 6U ? "v6" : "v7"))));
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
        !test_scene_document_write_v3_to_v7_fixture(v3_path, 3U) ||
        !test_scene_document_files_equal(first_path, second_path) ||
        !test_scene_document_patch_u32(second_path, 4L, UINT32_C(4)) ||
        henka_scene_document_format_inspection(
            document, inspection, sizeof(inspection), &inspection_size) != HENKA_SUCCESS ||
        inspection_size == 0U || strstr(inspection, "HSCN version=8 objects=257") == NULL)
    {
        fprintf(stderr, "scene document test failed during deterministic save/inspection\n");
        goto cleanup;
    }
    if (!test_scene_document_write_v3_to_v7_fixture(v5_path, 5U) ||
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
    if (!test_scene_document_write_v3_to_v7_fixture(v4_path, 4U) ||
        henka_scene_document_load_file(loaded, ".", v4_path) != HENKA_SUCCESS ||
        henka_scene_document_get_object(loaded, first_id, &loaded_object) != HENKA_SUCCESS ||
        loaded_object.audio.streaming)
    {
        fprintf(stderr, "scene document test failed during v4 migration\n");
        goto cleanup;
    }
    if (!test_scene_document_write_v3_to_v7_fixture(v6_path, 6U) ||
        henka_scene_document_load_file(loaded, ".", v6_path) != HENKA_SUCCESS ||
        henka_scene_document_has_camera(loaded) ||
        henka_scene_document_get_camera(loaded, &(henka_camera){0}) == HENKA_SUCCESS)
    {
        fprintf(stderr, "scene document test failed during v6 compatibility load\n");
        goto cleanup;
    }
    if (!test_scene_document_write_v3_to_v7_fixture(v7_path, 7U) ||
        henka_scene_document_load_file(loaded, ".", v7_path) != HENKA_SUCCESS ||
        henka_scene_document_get_object_at(loaded, 0U, &loaded_object) != HENKA_SUCCESS ||
        loaded_object.character_controller.enabled ||
        henka_scene_document_has_camera(loaded))
    {
        fprintf(stderr, "scene document test failed during v7 compatibility load\n");
        goto cleanup;
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
        strstr(inspection, "HSCN version=8") == NULL)
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
    henka_scene_document_destroy(exhausted);
    henka_scene_document_destroy(camera_document);
    henka_scene_document_destroy(loaded);
    henka_scene_document_destroy(document);
    return result;
}
