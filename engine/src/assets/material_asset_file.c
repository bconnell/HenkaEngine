#include "henka_internal.h"

#include <henka/persistence.h>
#include <henka/memory.h>

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HENKA_MATERIAL_FILE_VERSION 1
#define HENKA_MATERIAL_FILE_KEY_CAPACITY 96U

static bool henka_material_file_make_key(
    char* buffer,
    size_t capacity,
    const char* suffix)
{
    int length;

    if (buffer == NULL || capacity == 0U || suffix == NULL)
    {
        return false;
    }
    length = snprintf(buffer, capacity, "part.0.%s", suffix);
    return length > 0 && (size_t)length < capacity;
}

static bool henka_material_file_parse_int(
    const char* text,
    int* out_value)
{
    char* end = NULL;
    long value;

    if (text == NULL || out_value == NULL)
    {
        return false;
    }
    errno = 0;
    value = strtol(text, &end, 10);
    if (errno == ERANGE || end == text || *end != '\0' ||
        value < INT_MIN || value > INT_MAX)
    {
        return false;
    }
    *out_value = (int)value;
    return true;
}

static bool henka_material_file_equals_ignore_case(
    const char* text,
    const char* expected)
{
    const unsigned char* left = (const unsigned char*)text;
    const unsigned char* right = (const unsigned char*)expected;

    if (left == NULL || right == NULL)
    {
        return false;
    }
    while (*left != '\0' && *right != '\0')
    {
        if (tolower(*left) != tolower(*right))
        {
            return false;
        }
        ++left;
        ++right;
    }
    return *left == '\0' && *right == '\0';
}

static bool henka_material_file_parse_bool(
    const char* text,
    bool* out_value)
{
    if (text == NULL || out_value == NULL)
    {
        return false;
    }
    if (henka_material_file_equals_ignore_case(text, "true") ||
        henka_material_file_equals_ignore_case(text, "yes") ||
        henka_material_file_equals_ignore_case(text, "on") ||
        strcmp(text, "1") == 0)
    {
        *out_value = true;
        return true;
    }
    if (henka_material_file_equals_ignore_case(text, "false") ||
        henka_material_file_equals_ignore_case(text, "no") ||
        henka_material_file_equals_ignore_case(text, "off") ||
        strcmp(text, "0") == 0)
    {
        *out_value = false;
        return true;
    }
    return false;
}

static henka_result henka_material_file_set_float(
    henka_settings* settings,
    const char* suffix,
    float value)
{
    char key[HENKA_MATERIAL_FILE_KEY_CAPACITY];

    return henka_material_file_make_key(key, sizeof(key), suffix)
        ? henka_settings_set_float(settings, key, value)
        : HENKA_ERROR_LIMIT;
}

static henka_result henka_material_file_set_int(
    henka_settings* settings,
    const char* suffix,
    int value)
{
    char key[HENKA_MATERIAL_FILE_KEY_CAPACITY];

    return henka_material_file_make_key(key, sizeof(key), suffix)
        ? henka_settings_set_int(settings, key, value)
        : HENKA_ERROR_LIMIT;
}

static henka_result henka_material_file_set_bool(
    henka_settings* settings,
    const char* suffix,
    bool value)
{
    char key[HENKA_MATERIAL_FILE_KEY_CAPACITY];

    return henka_material_file_make_key(key, sizeof(key), suffix)
        ? henka_settings_set_bool(settings, key, value)
        : HENKA_ERROR_LIMIT;
}

static henka_result henka_material_file_set_string(
    henka_settings* settings,
    const char* suffix,
    const char* value)
{
    char key[HENKA_MATERIAL_FILE_KEY_CAPACITY];

    return henka_material_file_make_key(key, sizeof(key), suffix)
        ? henka_settings_set_string(settings, key, value)
        : HENKA_ERROR_LIMIT;
}

static bool henka_material_file_get_float(
    const henka_settings* settings,
    const char* suffix,
    float* out_value)
{
    char key[HENKA_MATERIAL_FILE_KEY_CAPACITY];
    float value;

    if (settings == NULL || out_value == NULL ||
        !henka_material_file_make_key(key, sizeof(key), suffix) ||
        !henka_settings_has_key(settings, key))
    {
        return false;
    }
    value = henka_settings_get_float(settings, key, NAN);
    if (!isfinite(value))
    {
        return false;
    }
    *out_value = value;
    return true;
}

static bool henka_material_file_get_int(
    const henka_settings* settings,
    const char* suffix,
    int* out_value)
{
    char key[HENKA_MATERIAL_FILE_KEY_CAPACITY];
    const char* value;
    int parsed;

    if (settings == NULL || out_value == NULL ||
        !henka_material_file_make_key(key, sizeof(key), suffix) ||
        !henka_settings_has_key(settings, key))
    {
        return false;
    }
    value = henka_settings_get_string(settings, key, NULL);
    if (!henka_material_file_parse_int(value, &parsed))
    {
        return false;
    }
    *out_value = parsed;
    return true;
}

static bool henka_material_file_get_bool(
    const henka_settings* settings,
    const char* suffix,
    bool* out_value)
{
    char key[HENKA_MATERIAL_FILE_KEY_CAPACITY];
    const char* value;
    bool parsed;

    if (settings == NULL || out_value == NULL ||
        !henka_material_file_make_key(key, sizeof(key), suffix) ||
        !henka_settings_has_key(settings, key))
    {
        return false;
    }
    value = henka_settings_get_string(settings, key, NULL);
    if (!henka_material_file_parse_bool(value, &parsed))
    {
        return false;
    }
    *out_value = parsed;
    return true;
}

static henka_texture_descriptor henka_material_file_texture_descriptor(
    henka_material_texture_slot slot)
{
    henka_texture_descriptor descriptor;

    switch (slot)
    {
        case HENKA_MATERIAL_TEXTURE_SLOT_NORMAL:
            return henka_texture_descriptor_default_normal();
        case HENKA_MATERIAL_TEXTURE_SLOT_METALLIC_ROUGHNESS:
            descriptor = henka_texture_descriptor_default_data();
            descriptor.usage = HENKA_TEXTURE_USAGE_METALLIC_ROUGHNESS;
            return descriptor;
        case HENKA_MATERIAL_TEXTURE_SLOT_OCCLUSION:
            descriptor = henka_texture_descriptor_default_data();
            descriptor.usage = HENKA_TEXTURE_USAGE_OCCLUSION;
            return descriptor;
        case HENKA_MATERIAL_TEXTURE_SLOT_EMISSIVE:
            descriptor = henka_texture_descriptor_default_color();
            descriptor.usage = HENKA_TEXTURE_USAGE_EMISSIVE;
            return descriptor;
        case HENKA_MATERIAL_TEXTURE_SLOT_TRANSMISSION:
        case HENKA_MATERIAL_TEXTURE_SLOT_THICKNESS:
            return henka_texture_descriptor_default_data();
        case HENKA_MATERIAL_TEXTURE_SLOT_BASE_COLOR:
        default:
            return henka_texture_descriptor_default_color();
    }
}

static henka_result henka_material_file_save_texture(
    henka_asset_manager* manager,
    henka_settings* settings,
    const char* suffix,
    const henka_texture* texture)
{
    henka_asset_metadata metadata;
    char* confined_source_path = NULL;
    henka_result result;

    if (texture == NULL)
    {
        return henka_material_file_set_string(settings, suffix, "");
    }
    memset(&metadata, 0, sizeof(metadata));
    if (henka_assets_get_texture_metadata(manager, texture, &metadata) != HENKA_SUCCESS ||
        metadata.source_path == NULL || metadata.source_path[0] == '\0' ||
        !metadata.reload_supported || metadata.fallback)
    {
        return HENKA_ERROR_ASSET_SOURCE;
    }
    result = henka_path_resolve_confined("", metadata.source_path, &confined_source_path);
    if (result == HENKA_SUCCESS)
    {
        result = henka_material_file_set_string(settings, suffix, confined_source_path);
    }
    henka_free(confined_source_path);
    return result;
}

static henka_result henka_material_file_load_texture(
    henka_asset_manager* manager,
    const henka_settings* settings,
    const char* suffix,
    henka_material_texture_slot slot,
    henka_texture** out_texture)
{
    char key[HENKA_MATERIAL_FILE_KEY_CAPACITY];
    const char* source_path;
    henka_texture_descriptor descriptor;
    henka_texture* texture = NULL;
    henka_asset_metadata metadata;
    henka_result result;

    if (out_texture == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_texture = NULL;
    if (manager == NULL || settings == NULL ||
        !henka_material_file_make_key(key, sizeof(key), suffix) ||
        !henka_settings_has_key(settings, key))
    {
        return HENKA_ERROR_ASSET_SOURCE;
    }
    source_path = henka_settings_get_string(settings, key, NULL);
    if (source_path == NULL)
    {
        return HENKA_ERROR_ASSET_SOURCE;
    }
    if (source_path[0] == '\0')
    {
        return HENKA_SUCCESS;
    }
    descriptor = henka_material_file_texture_descriptor(slot);
    result = henka_assets_load_texture_with_descriptor(
        manager, source_path, &descriptor, &texture);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    memset(&metadata, 0, sizeof(metadata));
    if (henka_assets_get_texture_metadata(manager, texture, &metadata) != HENKA_SUCCESS ||
        !metadata.loaded || metadata.fallback || !metadata.reload_supported)
    {
        return HENKA_ERROR_ASSET_SOURCE;
    }
    *out_texture = texture;
    return HENKA_SUCCESS;
}

henka_result henka_material_asset_file_save(
    henka_asset_manager* manager,
    const char* resolved_path,
    const henka_material* material)
{
    henka_settings* settings = NULL;
    henka_result result;

    if (manager == NULL || resolved_path == NULL || material == NULL ||
        material->terrain_layers_enabled ||
        henka_material_validate(material) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    result = henka_settings_create(&settings);
    if (result == HENKA_SUCCESS)
    {
        result = henka_settings_set_int(
            settings, "material.file_version", HENKA_MATERIAL_FILE_VERSION);
    }
#define HENKA_MATERIAL_FILE_WRITE_FLOAT(field, value) do { \
    if (result == HENKA_SUCCESS) result = henka_material_file_set_float(settings, field, value); \
} while (0)
#define HENKA_MATERIAL_FILE_WRITE_INT(field, value) do { \
    if (result == HENKA_SUCCESS) result = henka_material_file_set_int(settings, field, value); \
} while (0)
#define HENKA_MATERIAL_FILE_WRITE_BOOL(field, value) do { \
    if (result == HENKA_SUCCESS) result = henka_material_file_set_bool(settings, field, value); \
} while (0)
    HENKA_MATERIAL_FILE_WRITE_INT("material.type", (int)material->type);
    HENKA_MATERIAL_FILE_WRITE_INT("material.alpha_mode", (int)material->alpha_mode);
    HENKA_MATERIAL_FILE_WRITE_FLOAT("material.base_color.x", material->base_color.x);
    HENKA_MATERIAL_FILE_WRITE_FLOAT("material.base_color.y", material->base_color.y);
    HENKA_MATERIAL_FILE_WRITE_FLOAT("material.base_color.z", material->base_color.z);
    HENKA_MATERIAL_FILE_WRITE_FLOAT("material.base_color.w", material->base_color.w);
    HENKA_MATERIAL_FILE_WRITE_FLOAT("material.emissive_color.x", material->emissive_color.x);
    HENKA_MATERIAL_FILE_WRITE_FLOAT("material.emissive_color.y", material->emissive_color.y);
    HENKA_MATERIAL_FILE_WRITE_FLOAT("material.emissive_color.z", material->emissive_color.z);
    HENKA_MATERIAL_FILE_WRITE_FLOAT("material.metallic", material->metallic);
    HENKA_MATERIAL_FILE_WRITE_FLOAT("material.roughness", material->roughness);
    HENKA_MATERIAL_FILE_WRITE_FLOAT("material.specular_factor", material->specular_factor);
    HENKA_MATERIAL_FILE_WRITE_FLOAT("material.specular_color.x", material->specular_color.x);
    HENKA_MATERIAL_FILE_WRITE_FLOAT("material.specular_color.y", material->specular_color.y);
    HENKA_MATERIAL_FILE_WRITE_FLOAT("material.specular_color.z", material->specular_color.z);
    HENKA_MATERIAL_FILE_WRITE_FLOAT("material.ior", material->ior);
    HENKA_MATERIAL_FILE_WRITE_FLOAT("material.transmission", material->transmission);
    HENKA_MATERIAL_FILE_WRITE_FLOAT("material.thickness", material->thickness);
    HENKA_MATERIAL_FILE_WRITE_FLOAT("material.attenuation_distance", material->attenuation_distance);
    HENKA_MATERIAL_FILE_WRITE_FLOAT("material.attenuation_color.x", material->attenuation_color.x);
    HENKA_MATERIAL_FILE_WRITE_FLOAT("material.attenuation_color.y", material->attenuation_color.y);
    HENKA_MATERIAL_FILE_WRITE_FLOAT("material.attenuation_color.z", material->attenuation_color.z);
    HENKA_MATERIAL_FILE_WRITE_FLOAT("material.subsurface", material->subsurface);
    HENKA_MATERIAL_FILE_WRITE_FLOAT("material.subsurface_color.x", material->subsurface_color.x);
    HENKA_MATERIAL_FILE_WRITE_FLOAT("material.subsurface_color.y", material->subsurface_color.y);
    HENKA_MATERIAL_FILE_WRITE_FLOAT("material.subsurface_color.z", material->subsurface_color.z);
    HENKA_MATERIAL_FILE_WRITE_FLOAT("material.normal_scale", material->normal_scale);
    HENKA_MATERIAL_FILE_WRITE_FLOAT("material.occlusion_strength", material->occlusion_strength);
    HENKA_MATERIAL_FILE_WRITE_FLOAT("material.emissive_strength", material->emissive_strength);
    HENKA_MATERIAL_FILE_WRITE_FLOAT("material.clearcoat", material->clearcoat);
    HENKA_MATERIAL_FILE_WRITE_FLOAT("material.clearcoat_roughness", material->clearcoat_roughness);
    HENKA_MATERIAL_FILE_WRITE_FLOAT("material.alpha_cutoff", material->alpha_cutoff);
    HENKA_MATERIAL_FILE_WRITE_FLOAT("material.sheen_color.x", material->sheen_color.x);
    HENKA_MATERIAL_FILE_WRITE_FLOAT("material.sheen_color.y", material->sheen_color.y);
    HENKA_MATERIAL_FILE_WRITE_FLOAT("material.sheen_color.z", material->sheen_color.z);
    HENKA_MATERIAL_FILE_WRITE_FLOAT("material.sheen_roughness", material->sheen_roughness);
    HENKA_MATERIAL_FILE_WRITE_INT("material.base_color_uv_set", (int)material->base_color_uv_set);
    HENKA_MATERIAL_FILE_WRITE_INT("material.normal_uv_set", (int)material->normal_uv_set);
    HENKA_MATERIAL_FILE_WRITE_INT("material.metallic_roughness_uv_set", (int)material->metallic_roughness_uv_set);
    HENKA_MATERIAL_FILE_WRITE_INT("material.occlusion_uv_set", (int)material->occlusion_uv_set);
    HENKA_MATERIAL_FILE_WRITE_INT("material.emissive_uv_set", (int)material->emissive_uv_set);
    HENKA_MATERIAL_FILE_WRITE_INT("material.transmission_uv_set", (int)material->transmission_uv_set);
    HENKA_MATERIAL_FILE_WRITE_INT("material.thickness_uv_set", (int)material->thickness_uv_set);
    HENKA_MATERIAL_FILE_WRITE_BOOL("material.use_texture", material->use_texture);
    HENKA_MATERIAL_FILE_WRITE_BOOL("material.use_lighting", material->use_lighting);
    HENKA_MATERIAL_FILE_WRITE_BOOL("material.depth_test", material->depth_test);
    HENKA_MATERIAL_FILE_WRITE_BOOL("material.double_sided", material->double_sided);
    HENKA_MATERIAL_FILE_WRITE_BOOL("material.cast_shadows", material->cast_shadows);
    HENKA_MATERIAL_FILE_WRITE_BOOL("material.receive_shadows", material->receive_shadows);
#undef HENKA_MATERIAL_FILE_WRITE_BOOL
#undef HENKA_MATERIAL_FILE_WRITE_INT
#undef HENKA_MATERIAL_FILE_WRITE_FLOAT

#define HENKA_MATERIAL_FILE_WRITE_TEXTURE(field, texture) do { \
    if (result == HENKA_SUCCESS) result = henka_material_file_save_texture( \
        manager, settings, field, texture); \
} while (0)
    HENKA_MATERIAL_FILE_WRITE_TEXTURE("material.base_color_texture", material->base_color_texture);
    HENKA_MATERIAL_FILE_WRITE_TEXTURE("material.normal_texture", material->normal_texture);
    HENKA_MATERIAL_FILE_WRITE_TEXTURE("material.metallic_roughness_texture", material->metallic_roughness_texture);
    HENKA_MATERIAL_FILE_WRITE_TEXTURE("material.occlusion_texture", material->occlusion_texture);
    HENKA_MATERIAL_FILE_WRITE_TEXTURE("material.emissive_texture", material->emissive_texture);
    HENKA_MATERIAL_FILE_WRITE_TEXTURE("material.transmission_texture", material->transmission_texture);
    HENKA_MATERIAL_FILE_WRITE_TEXTURE("material.thickness_texture", material->thickness_texture);
#undef HENKA_MATERIAL_FILE_WRITE_TEXTURE

    if (result == HENKA_SUCCESS)
    {
        result = henka_path_ensure_parent_directory(resolved_path);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_settings_save_file(settings, resolved_path);
    }
    henka_settings_destroy(settings);
    return result;
}

static henka_result henka_material_file_check_size(const char* path)
{
    FILE* file = NULL;
    long size;

#if defined(_WIN32)
    if (fopen_s(&file, path, "rb") != 0)
    {
        file = NULL;
    }
#else
    file = fopen(path, "rb");
#endif
    if (file == NULL)
    {
        return HENKA_ERROR_ASSET_SOURCE;
    }
    if (fseek(file, 0L, SEEK_END) != 0)
    {
        fclose(file);
        return HENKA_ERROR_ASSET_SOURCE;
    }
    size = ftell(file);
    fclose(file);
    if (size < 0L)
    {
        return HENKA_ERROR_ASSET_SOURCE;
    }
    return (uint64_t)size > (uint64_t)HENKA_MATERIAL_MAX_FILE_BYTES
        ? HENKA_ERROR_LIMIT : HENKA_SUCCESS;
}

henka_result henka_material_asset_file_load(
    henka_asset_manager* manager,
    const char* resolved_path,
    henka_shader* shader,
    henka_material* out_material)
{
    henka_settings* settings = NULL;
    henka_material candidate;
    const char* file_version_text;
    int file_version;
    int type;
    int alpha_mode;
    int base_color_uv_set;
    int normal_uv_set;
    int metallic_roughness_uv_set;
    int occlusion_uv_set;
    int emissive_uv_set;
    int transmission_uv_set;
    int thickness_uv_set;
    bool use_texture;
    bool use_lighting;
    bool depth_test;
    bool double_sided;
    bool cast_shadows;
    bool receive_shadows;
    henka_result result;

    if (manager == NULL || resolved_path == NULL || shader == NULL || out_material == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_material_file_check_size(resolved_path);
    if (result == HENKA_SUCCESS)
    {
        result = henka_settings_create(&settings);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_settings_load_file(settings, resolved_path);
    }
    if (result != HENKA_SUCCESS)
    {
        henka_settings_destroy(settings);
        return result;
    }

    file_version = 0;
    file_version_text = henka_settings_get_string(
        settings, "material.file_version", NULL);
    if (file_version_text != NULL &&
        !henka_material_file_parse_int(file_version_text, &file_version))
    {
        henka_settings_destroy(settings);
        return HENKA_ERROR_ASSET_SOURCE;
    }
    if (file_version < 0 || file_version > HENKA_MATERIAL_FILE_VERSION)
    {
        henka_settings_destroy(settings);
        return HENKA_ERROR_ASSET_SOURCE;
    }
    candidate = henka_material_default();
    candidate.shader = shader;

#define HENKA_MATERIAL_FILE_READ_FLOAT(field, value) do { \
    if (!henka_material_file_get_float(settings, field, &value)) { \
        henka_settings_destroy(settings); return HENKA_ERROR_ASSET_SOURCE; \
    } \
} while (0)
    HENKA_MATERIAL_FILE_READ_FLOAT("material.base_color.x", candidate.base_color.x);
    HENKA_MATERIAL_FILE_READ_FLOAT("material.base_color.y", candidate.base_color.y);
    HENKA_MATERIAL_FILE_READ_FLOAT("material.base_color.z", candidate.base_color.z);
    HENKA_MATERIAL_FILE_READ_FLOAT("material.base_color.w", candidate.base_color.w);
    HENKA_MATERIAL_FILE_READ_FLOAT("material.emissive_color.x", candidate.emissive_color.x);
    HENKA_MATERIAL_FILE_READ_FLOAT("material.emissive_color.y", candidate.emissive_color.y);
    HENKA_MATERIAL_FILE_READ_FLOAT("material.emissive_color.z", candidate.emissive_color.z);
    HENKA_MATERIAL_FILE_READ_FLOAT("material.metallic", candidate.metallic);
    HENKA_MATERIAL_FILE_READ_FLOAT("material.roughness", candidate.roughness);
    HENKA_MATERIAL_FILE_READ_FLOAT("material.specular_factor", candidate.specular_factor);
    HENKA_MATERIAL_FILE_READ_FLOAT("material.specular_color.x", candidate.specular_color.x);
    HENKA_MATERIAL_FILE_READ_FLOAT("material.specular_color.y", candidate.specular_color.y);
    HENKA_MATERIAL_FILE_READ_FLOAT("material.specular_color.z", candidate.specular_color.z);
    HENKA_MATERIAL_FILE_READ_FLOAT("material.ior", candidate.ior);
    HENKA_MATERIAL_FILE_READ_FLOAT("material.transmission", candidate.transmission);
    HENKA_MATERIAL_FILE_READ_FLOAT("material.thickness", candidate.thickness);
    HENKA_MATERIAL_FILE_READ_FLOAT("material.attenuation_distance", candidate.attenuation_distance);
    HENKA_MATERIAL_FILE_READ_FLOAT("material.attenuation_color.x", candidate.attenuation_color.x);
    HENKA_MATERIAL_FILE_READ_FLOAT("material.attenuation_color.y", candidate.attenuation_color.y);
    HENKA_MATERIAL_FILE_READ_FLOAT("material.attenuation_color.z", candidate.attenuation_color.z);
    HENKA_MATERIAL_FILE_READ_FLOAT("material.subsurface", candidate.subsurface);
    HENKA_MATERIAL_FILE_READ_FLOAT("material.subsurface_color.x", candidate.subsurface_color.x);
    HENKA_MATERIAL_FILE_READ_FLOAT("material.subsurface_color.y", candidate.subsurface_color.y);
    HENKA_MATERIAL_FILE_READ_FLOAT("material.subsurface_color.z", candidate.subsurface_color.z);
    HENKA_MATERIAL_FILE_READ_FLOAT("material.normal_scale", candidate.normal_scale);
    HENKA_MATERIAL_FILE_READ_FLOAT("material.occlusion_strength", candidate.occlusion_strength);
    HENKA_MATERIAL_FILE_READ_FLOAT("material.emissive_strength", candidate.emissive_strength);
    HENKA_MATERIAL_FILE_READ_FLOAT("material.clearcoat", candidate.clearcoat);
    HENKA_MATERIAL_FILE_READ_FLOAT("material.clearcoat_roughness", candidate.clearcoat_roughness);
    HENKA_MATERIAL_FILE_READ_FLOAT("material.alpha_cutoff", candidate.alpha_cutoff);
    HENKA_MATERIAL_FILE_READ_FLOAT("material.sheen_color.x", candidate.sheen_color.x);
    HENKA_MATERIAL_FILE_READ_FLOAT("material.sheen_color.y", candidate.sheen_color.y);
    HENKA_MATERIAL_FILE_READ_FLOAT("material.sheen_color.z", candidate.sheen_color.z);
    HENKA_MATERIAL_FILE_READ_FLOAT("material.sheen_roughness", candidate.sheen_roughness);
#undef HENKA_MATERIAL_FILE_READ_FLOAT

    if (!henka_material_file_get_int(settings, "material.type", &type) ||
        !henka_material_file_get_int(settings, "material.alpha_mode", &alpha_mode) ||
        !henka_material_file_get_int(settings, "material.base_color_uv_set", &base_color_uv_set) ||
        !henka_material_file_get_int(settings, "material.normal_uv_set", &normal_uv_set) ||
        !henka_material_file_get_int(settings, "material.metallic_roughness_uv_set", &metallic_roughness_uv_set) ||
        !henka_material_file_get_int(settings, "material.occlusion_uv_set", &occlusion_uv_set) ||
        !henka_material_file_get_int(settings, "material.emissive_uv_set", &emissive_uv_set) ||
        !henka_material_file_get_int(settings, "material.transmission_uv_set", &transmission_uv_set) ||
        !henka_material_file_get_int(settings, "material.thickness_uv_set", &thickness_uv_set) ||
        !henka_material_file_get_bool(settings, "material.use_texture", &use_texture) ||
        !henka_material_file_get_bool(settings, "material.use_lighting", &use_lighting) ||
        !henka_material_file_get_bool(settings, "material.depth_test", &depth_test) ||
        !henka_material_file_get_bool(settings, "material.double_sided", &double_sided) ||
        !henka_material_file_get_bool(settings, "material.cast_shadows", &cast_shadows) ||
        !henka_material_file_get_bool(settings, "material.receive_shadows", &receive_shadows) ||
        type < (int)HENKA_MATERIAL_TYPE_LIT || type > (int)HENKA_MATERIAL_TYPE_VERTEX_COLOR ||
        alpha_mode < (int)HENKA_MATERIAL_ALPHA_OPAQUE || alpha_mode > (int)HENKA_MATERIAL_ALPHA_BLENDED ||
        base_color_uv_set < 0 || base_color_uv_set > 1 || normal_uv_set < 0 || normal_uv_set > 1 ||
        metallic_roughness_uv_set < 0 || metallic_roughness_uv_set > 1 ||
        occlusion_uv_set < 0 || occlusion_uv_set > 1 || emissive_uv_set < 0 || emissive_uv_set > 1 ||
        transmission_uv_set < 0 || transmission_uv_set > 1 || thickness_uv_set < 0 || thickness_uv_set > 1)
    {
        henka_settings_destroy(settings);
        return HENKA_ERROR_ASSET_SOURCE;
    }
    candidate.type = (henka_material_type)type;
    candidate.alpha_mode = (henka_material_alpha_mode)alpha_mode;
    candidate.base_color_uv_set = (uint32_t)base_color_uv_set;
    candidate.normal_uv_set = (uint32_t)normal_uv_set;
    candidate.metallic_roughness_uv_set = (uint32_t)metallic_roughness_uv_set;
    candidate.occlusion_uv_set = (uint32_t)occlusion_uv_set;
    candidate.emissive_uv_set = (uint32_t)emissive_uv_set;
    candidate.transmission_uv_set = (uint32_t)transmission_uv_set;
    candidate.thickness_uv_set = (uint32_t)thickness_uv_set;
    candidate.use_texture = use_texture;
    candidate.use_lighting = use_lighting;
    candidate.depth_test = depth_test;
    candidate.double_sided = double_sided;
    candidate.cast_shadows = cast_shadows;
    candidate.receive_shadows = receive_shadows;
    candidate.terrain_layers_enabled = false;

#define HENKA_MATERIAL_FILE_READ_TEXTURE(field, slot, target) do { \
    result = henka_material_file_load_texture(manager, settings, field, slot, &candidate.target); \
    if (result != HENKA_SUCCESS) { henka_settings_destroy(settings); return result; } \
} while (0)
    HENKA_MATERIAL_FILE_READ_TEXTURE("material.base_color_texture", HENKA_MATERIAL_TEXTURE_SLOT_BASE_COLOR, base_color_texture);
    HENKA_MATERIAL_FILE_READ_TEXTURE("material.normal_texture", HENKA_MATERIAL_TEXTURE_SLOT_NORMAL, normal_texture);
    HENKA_MATERIAL_FILE_READ_TEXTURE("material.metallic_roughness_texture", HENKA_MATERIAL_TEXTURE_SLOT_METALLIC_ROUGHNESS, metallic_roughness_texture);
    HENKA_MATERIAL_FILE_READ_TEXTURE("material.occlusion_texture", HENKA_MATERIAL_TEXTURE_SLOT_OCCLUSION, occlusion_texture);
    HENKA_MATERIAL_FILE_READ_TEXTURE("material.emissive_texture", HENKA_MATERIAL_TEXTURE_SLOT_EMISSIVE, emissive_texture);
    HENKA_MATERIAL_FILE_READ_TEXTURE("material.transmission_texture", HENKA_MATERIAL_TEXTURE_SLOT_TRANSMISSION, transmission_texture);
    HENKA_MATERIAL_FILE_READ_TEXTURE("material.thickness_texture", HENKA_MATERIAL_TEXTURE_SLOT_THICKNESS, thickness_texture);
#undef HENKA_MATERIAL_FILE_READ_TEXTURE

    henka_settings_destroy(settings);
    if (henka_material_validate(&candidate) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_ASSET_SOURCE;
    }
    *out_material = candidate;
    return HENKA_SUCCESS;
}
