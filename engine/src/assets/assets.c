#include "henka_internal.h"

#include <henka/model.h>

#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include <henka/log.h>
#include <henka/memory.h>

#include <stb_image.h>

#include "../core/checked.h"

static char* henka_duplicate_string(const char* value)
{
    size_t allocation_size;
    char* copy;
    size_t length;

    if (value == NULL)
    {
        return NULL;
    }

    if (!henka_checked_c_string_length(value, HENKA_MAX_ASSET_PATH_BYTES, &length) ||
        !henka_checked_size_add(length, 1U, &allocation_size))
    {
        return NULL;
    }

    copy = henka_malloc(allocation_size);
    if (copy == NULL)
    {
        return NULL;
    }

    memcpy(copy, value, allocation_size);
    return copy;
}

static bool henka_asset_texture_path_is_ktx2(const char* path)
{
    size_t length;

    if (path == NULL)
        return false;
    length = strlen(path);
    return length >= 5U &&
        tolower((unsigned char)path[length - 5U]) == '.' &&
        tolower((unsigned char)path[length - 4U]) == 'k' &&
        tolower((unsigned char)path[length - 3U]) == 't' &&
        tolower((unsigned char)path[length - 2U]) == 'x' &&
        path[length - 1U] == '2';
}

static void henka_assets_add_failed_texture_bytes(
    henka_asset_manager* manager,
    uint64_t bytes)
{
    if (manager == NULL)
        return;
    if (UINT64_MAX - manager->texture_failed_bytes < bytes)
        manager->texture_failed_bytes = UINT64_MAX;
    else
        manager->texture_failed_bytes += bytes;
}

static void henka_assets_add_source_failed_texture_bytes(
    henka_asset_manager* manager,
    uint64_t bytes)
{
    if (manager == NULL)
        return;
    if (UINT64_MAX - manager->texture_source_failed_bytes < bytes)
        manager->texture_source_failed_bytes = UINT64_MAX;
    else
        manager->texture_source_failed_bytes += bytes;
}

static bool henka_assets_get_file_size(
    const char* path,
    uint64_t* out_size)
{
    FILE* file;
    long size;

    if (out_size != NULL)
        *out_size = 0U;
    if (path == NULL || out_size == NULL)
        return false;
#if defined(_WIN32)
    if (fopen_s(&file, path, "rb") != 0)
        file = NULL;
#else
    file = fopen(path, "rb");
#endif
    if (file == NULL)
        return false;
    if (fseek(file, 0L, SEEK_END) != 0)
    {
        fclose(file);
        return false;
    }
    size = ftell(file);
    fclose(file);
    if (size < 0L)
        return false;
    *out_size = (uint64_t)size;
    return true;
}

static const char* henka_asset_display_name(const char* path)
{
    const char* cursor;
    const char* last_separator;

    if (path == NULL)
    {
        return "";
    }

    last_separator = path;
    for (cursor = path; *cursor != '\0'; ++cursor)
    {
        if (*cursor == '/' || *cursor == '\\')
        {
            last_separator = cursor + 1;
        }
    }

    return last_separator;
}

static void henka_asset_fold_identity_case(char* path)
{
#if defined(_WIN32)
    char* cursor;

    if (path == NULL)
    {
        return;
    }

    for (cursor = path; *cursor != '\0'; ++cursor)
    {
        if (*cursor >= 'A' && *cursor <= 'Z')
        {
            *cursor = (char)(*cursor - 'A' + 'a');
        }
    }
#else
    (void)path;
#endif
}

char* henka_asset_copy_display_name(const char* path)
{
    return henka_duplicate_string(henka_asset_display_name(path));
}

static henka_result henka_assets_normalize_source_path(
    const char* path,
    char** out_path)
{
    size_t allocation_size;
    size_t copy_index;
    size_t length;
    char* normalized;
    size_t read_index;
    henka_result result;
    size_t segment_length;
    size_t segment_start;
    size_t write_index;

    if (out_path != NULL)
    {
        *out_path = NULL;
    }

    if (path == NULL || out_path == NULL ||
        !henka_checked_c_string_length(
            path,
            HENKA_MAX_ASSET_PATH_BYTES,
            &length) ||
        !henka_checked_size_add(
            length,
            1U,
            &allocation_size))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    if (length == 0U || path[0] == '/' || path[0] == '\\')
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    normalized = henka_malloc(allocation_size);
    if (normalized == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }

    read_index = 0U;
    write_index = 0U;
    while (read_index < length)
    {
        while (read_index < length &&
            (path[read_index] == '/' ||
                path[read_index] == '\\'))
        {
            read_index += 1U;
        }

        segment_start = read_index;
        while (read_index < length &&
            path[read_index] != '/' &&
            path[read_index] != '\\')
        {
            read_index += 1U;
        }

        segment_length = read_index - segment_start;
        if (segment_length == 0U)
        {
            continue;
        }

        if (segment_length == 1U &&
            path[segment_start] == '.')
        {
            continue;
        }

        if (write_index > 0U)
        {
            normalized[write_index] = '/';
            write_index += 1U;
        }

        for (copy_index = 0U;
            copy_index < segment_length;
            ++copy_index)
        {
            normalized[write_index] =
                path[segment_start + copy_index];
            write_index += 1U;
        }
    }

    if (write_index == 0U)
    {
        henka_free(normalized);
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    normalized[write_index] = '\0';
    result = henka_path_resolve_confined(
        "",
        normalized,
        out_path);
    henka_free(normalized);
    return result;
}

henka_result henka_assets_make_canonical_key(
    const char* path,
    char** out_key)
{
    henka_result result;

    result = henka_assets_normalize_source_path(path, out_key);
    if (result == HENKA_SUCCESS)
    {
        henka_asset_fold_identity_case(*out_key);
    }

    return result;
}

static char* henka_asset_create_shader_source_path(
    const char* vertex_key,
    const char* fragment_key)
{
    size_t allocation_size;
    size_t fragment_length;
    char* source_path;
    size_t vertex_length;

    if (!henka_checked_c_string_length(
            vertex_key,
            HENKA_MAX_ASSET_PATH_BYTES,
            &vertex_length) ||
        !henka_checked_c_string_length(
            fragment_key,
            HENKA_MAX_ASSET_PATH_BYTES,
            &fragment_length) ||
        !henka_checked_size_add(
            vertex_length,
            fragment_length,
            &allocation_size) ||
        !henka_checked_size_add(
            allocation_size,
            4U,
            &allocation_size))
    {
        return NULL;
    }

    source_path = henka_malloc(allocation_size);
    if (source_path == NULL)
    {
        return NULL;
    }

    if (snprintf(
            source_path,
            allocation_size,
            "%s + %s",
            vertex_key,
            fragment_key) < 0)
    {
        henka_free(source_path);
        return NULL;
    }

    return source_path;
}

static void henka_asset_set_summary(henka_asset_metadata* metadata, const char* summary, const char* error_summary)
{
    metadata->summary = summary;
    metadata->error_summary = error_summary;
}

static bool henka_texture_descriptors_equal(
    const henka_texture_descriptor* left,
    const henka_texture_descriptor* right)
{
    return left != NULL && right != NULL &&
        left->color_space == right->color_space &&
        left->min_filter == right->min_filter &&
        left->mag_filter == right->mag_filter &&
        left->wrap_u == right->wrap_u &&
        left->wrap_v == right->wrap_v &&
        left->generate_mipmaps == right->generate_mipmaps &&
        left->vertical_flip == right->vertical_flip &&
        left->usage == right->usage &&
        left->anisotropy == right->anisotropy;
}

static void henka_texture_descriptor_canonicalize(henka_texture_descriptor* descriptor)
{
    unsigned int anisotropy_units;

    if (descriptor == NULL)
    {
        return;
    }
    if (descriptor->anisotropy == 0.0f)
    {
        descriptor->anisotropy = 0.0f;
        return;
    }
    if (descriptor->anisotropy > 16.0f)
    {
        descriptor->anisotropy = 16.0f;
    }
    anisotropy_units = (unsigned int)floorf(descriptor->anisotropy * 1000.0f + 0.5f);
    descriptor->anisotropy = (float)anisotropy_units / 1000.0f;
}

static henka_result henka_assets_make_texture_cache_key(
    const char* path,
    const henka_texture_descriptor* descriptor,
    char** out_key)
{
    henka_texture_descriptor defaults;
    char* base_key;
    henka_texture_descriptor canonical_descriptor;
    henka_result result;
    size_t allocation_size;
    size_t length;
    int written;

    if (out_key != NULL)
    {
        *out_key = NULL;
    }
    if (path == NULL || descriptor == NULL || out_key == NULL ||
        henka_texture_descriptor_validate(descriptor) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    canonical_descriptor = *descriptor;
    henka_texture_descriptor_canonicalize(&canonical_descriptor);
    defaults = henka_texture_descriptor_default_color();
    base_key = NULL;
    result = henka_assets_make_canonical_key(path, &base_key);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    if (henka_texture_descriptors_equal(&canonical_descriptor, &defaults))
    {
        *out_key = base_key;
        return HENKA_SUCCESS;
    }
    if (!henka_checked_c_string_length(base_key, HENKA_MAX_ASSET_PATH_BYTES, &length) ||
        !henka_checked_size_add(length, 128U, &allocation_size))
    {
        henka_free(base_key);
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_key = henka_malloc(allocation_size);
    if (*out_key == NULL)
    {
        henka_free(base_key);
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    written = snprintf(
        *out_key,
        allocation_size,
        "%s|cs=%u|min=%u|mag=%u|u=%u|v=%u|mip=%u|flip=%u|use=%u|aniso=%u",
        base_key,
        (unsigned int)canonical_descriptor.color_space,
        (unsigned int)canonical_descriptor.min_filter,
        (unsigned int)canonical_descriptor.mag_filter,
        (unsigned int)canonical_descriptor.wrap_u,
        (unsigned int)canonical_descriptor.wrap_v,
        canonical_descriptor.generate_mipmaps ? 1U : 0U,
        canonical_descriptor.vertical_flip ? 1U : 0U,
        (unsigned int)canonical_descriptor.usage,
        (unsigned int)floorf(canonical_descriptor.anisotropy * 1000.0f + 0.5f));
    henka_free(base_key);
    if (written < 0 || (size_t)written >= allocation_size)
    {
        henka_free(*out_key);
        *out_key = NULL;
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return HENKA_SUCCESS;
}

static henka_texture* henka_asset_manager_get_texture_fallback(
    henka_asset_manager* manager,
    henka_texture_usage usage)
{
    switch (usage)
    {
        case HENKA_TEXTURE_USAGE_NORMAL:
            return manager->normal_texture;
        case HENKA_TEXTURE_USAGE_METALLIC_ROUGHNESS:
            return manager->metallic_roughness_texture;
        case HENKA_TEXTURE_USAGE_OCCLUSION:
            return manager->occlusion_texture;
        case HENKA_TEXTURE_USAGE_EMISSIVE:
            return manager->emissive_texture;
        case HENKA_TEXTURE_USAGE_COLOR:
        case HENKA_TEXTURE_USAGE_UI:
        case HENKA_TEXTURE_USAGE_GENERIC_DATA:
        default:
            return manager->error_texture;
    }
}

const char* henka_assets_get_type_label(henka_asset_type type)
{
    switch (type)
    {
        case HENKA_ASSET_TYPE_SHADER:
            return "Shader";
        case HENKA_ASSET_TYPE_TEXTURE:
            return "Texture";
        case HENKA_ASSET_TYPE_MESH:
            return "Mesh";
        case HENKA_ASSET_TYPE_MATERIAL:
            return "Material";
        case HENKA_ASSET_TYPE_GLTF_SCENE:
            return "glTF Scene";
        case HENKA_ASSET_TYPE_UNKNOWN:
        default:
            return "Unknown";
    }
}

henka_result henka_assets_resolve_path(const char* base_path, const char* asset_path, char** out_path)
{
    return henka_path_resolve_confined(base_path, asset_path, out_path);
}

static henka_result henka_asset_manager_grow_shaders(henka_asset_manager* manager)
{
    size_t allocation_size;
    henka_asset_shader_entry* entries;
    size_t new_capacity;
    size_t required;

    if (manager == NULL ||
        !henka_checked_size_add(manager->shader_count, 1U, &required) ||
        !henka_checked_capacity(
            manager->shader_capacity,
            required,
            8U,
            HENKA_MAX_ASSET_CACHE_ENTRIES,
            &new_capacity) ||
        !henka_checked_size_multiply(new_capacity, sizeof(*entries), &allocation_size))
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }

    entries = henka_realloc(manager->shader_entries, allocation_size);
    if (entries == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }

    manager->shader_entries = entries;
    manager->shader_capacity = new_capacity;
    return HENKA_SUCCESS;
}

static henka_result henka_asset_manager_grow_textures(henka_asset_manager* manager)
{
    size_t allocation_size;
    henka_asset_texture_entry* entries;
    size_t new_capacity;
    size_t required;

    if (manager == NULL ||
        !henka_checked_size_add(manager->texture_count, 1U, &required) ||
        !henka_checked_capacity(
            manager->texture_capacity,
            required,
            8U,
            HENKA_MAX_ASSET_CACHE_ENTRIES,
            &new_capacity) ||
        !henka_checked_size_multiply(new_capacity, sizeof(*entries), &allocation_size))
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }

    entries = henka_realloc(manager->texture_entries, allocation_size);
    if (entries == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }

    manager->texture_entries = entries;
    manager->texture_capacity = new_capacity;
    return HENKA_SUCCESS;
}

static henka_result henka_asset_manager_grow_meshes(henka_asset_manager* manager)
{
    size_t allocation_size;
    henka_asset_mesh_entry* entries;
    size_t new_capacity;
    size_t required;

    if (manager == NULL ||
        !henka_checked_size_add(manager->mesh_count, 1U, &required) ||
        !henka_checked_capacity(
            manager->mesh_capacity,
            required,
            8U,
            HENKA_MAX_ASSET_CACHE_ENTRIES,
            &new_capacity) ||
        !henka_checked_size_multiply(new_capacity, sizeof(*entries), &allocation_size))
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }

    entries = henka_realloc(manager->mesh_entries, allocation_size);
    if (entries == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }

    manager->mesh_entries = entries;
    manager->mesh_capacity = new_capacity;
    return HENKA_SUCCESS;
}

static henka_result henka_asset_manager_grow_materials(henka_asset_manager* manager)
{
    size_t allocation_size;
    size_t new_capacity;
    size_t required;
    henka_material_asset** entries;

    if (manager == NULL ||
        !henka_checked_size_add(manager->material_count, 1U, &required) ||
        !henka_checked_capacity(manager->material_capacity, required, 8U,
            HENKA_MAX_ASSET_CACHE_ENTRIES, &new_capacity) ||
        !henka_checked_size_multiply(new_capacity, sizeof(*entries), &allocation_size))
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    entries = henka_realloc(manager->material_entries, allocation_size);
    if (entries == NULL) return HENKA_ERROR_OUT_OF_MEMORY;
    manager->material_entries = entries;
    manager->material_capacity = new_capacity;
    return HENKA_SUCCESS;
}

static henka_result henka_asset_manager_grow_gltf_scenes(henka_asset_manager* manager)
{
    size_t allocation_size;
    size_t new_capacity;
    size_t required;
    henka_gltf_scene_asset** entries;

    if (manager == NULL ||
        !henka_checked_size_add(manager->gltf_scene_count, 1U, &required) ||
        !henka_checked_capacity(manager->gltf_scene_capacity, required, 4U,
            HENKA_MAX_ASSET_CACHE_ENTRIES, &new_capacity) ||
        !henka_checked_size_multiply(new_capacity, sizeof(*entries), &allocation_size))
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    entries = henka_realloc(manager->gltf_scene_entries, allocation_size);
    if (entries == NULL) return HENKA_ERROR_OUT_OF_MEMORY;
    manager->gltf_scene_entries = entries;
    manager->gltf_scene_capacity = new_capacity;
    return HENKA_SUCCESS;
}

static const henka_asset_shader_entry*
henka_asset_manager_find_shader_entry_const(
    const henka_asset_manager* manager,
    const char* vertex_key,
    const char* fragment_key,
    const henka_shader_contract_desc* contract)
{
    size_t index;

    if (manager == NULL ||
        vertex_key == NULL ||
        fragment_key == NULL ||
        henka_shader_contract_desc_validate(contract) != HENKA_SUCCESS)
    {
        return NULL;
    }

    for (index = 0U; index < manager->shader_count; ++index)
    {
        if (strcmp(
                manager->shader_entries[index].vertex_key,
                vertex_key) == 0 &&
            strcmp(
                manager->shader_entries[index].fragment_key,
                fragment_key) == 0 &&
            manager->shader_entries[index].contract_type == contract->type &&
            manager->shader_entries[index].contract_version == contract->version)
        {
            return &manager->shader_entries[index];
        }
    }

    return NULL;
}

static henka_asset_shader_entry*
henka_asset_manager_find_shader_entry(
    henka_asset_manager* manager,
    const char* vertex_key,
    const char* fragment_key,
    const henka_shader_contract_desc* contract)
{
    return (henka_asset_shader_entry*)
        henka_asset_manager_find_shader_entry_const(
            manager,
            vertex_key,
            fragment_key,
            contract);
}

static const henka_asset_texture_entry*
henka_asset_manager_find_texture_entry_const(
    const henka_asset_manager* manager,
    const char* key)
{
    size_t index;

    if (manager == NULL || key == NULL)
    {
        return NULL;
    }

    for (index = 0U; index < manager->texture_count; ++index)
    {
        if (strcmp(manager->texture_entries[index].key, key) == 0)
        {
            return &manager->texture_entries[index];
        }
    }

    return NULL;
}

static henka_asset_texture_entry*
henka_asset_manager_find_texture_entry(
    henka_asset_manager* manager,
    const char* key)
{
    return (henka_asset_texture_entry*)
        henka_asset_manager_find_texture_entry_const(manager, key);
}

static const henka_asset_mesh_entry*
henka_asset_manager_find_mesh_entry_const(
    const henka_asset_manager* manager,
    const char* key)
{
    size_t index;

    if (manager == NULL || key == NULL)
    {
        return NULL;
    }

    for (index = 0U; index < manager->mesh_count; ++index)
    {
        if (strcmp(manager->mesh_entries[index].key, key) == 0)
        {
            return &manager->mesh_entries[index];
        }
    }

    return NULL;
}

static henka_asset_mesh_entry*
henka_asset_manager_find_mesh_entry(
    henka_asset_manager* manager,
    const char* key)
{
    return (henka_asset_mesh_entry*)
        henka_asset_manager_find_mesh_entry_const(manager, key);
}

static henka_material_asset* henka_asset_manager_find_material_entry(
    henka_asset_manager* manager,
    const char* key)
{
    size_t index;
    if (manager == NULL || key == NULL) return NULL;
    for (index = 0U; index < manager->material_count; ++index)
    {
        if (strcmp(manager->material_entries[index]->key, key) == 0)
            return manager->material_entries[index];
    }
    return NULL;
}

static henka_gltf_scene_asset* henka_asset_manager_find_gltf_scene_entry(
    henka_asset_manager* manager,
    const char* key)
{
    size_t index;
    if (manager == NULL || key == NULL) return NULL;
    for (index = 0U; index < manager->gltf_scene_count; ++index)
    {
        if (strcmp(manager->gltf_scene_entries[index]->key, key) == 0)
            return manager->gltf_scene_entries[index];
    }
    return NULL;
}

static void henka_assets_destroy_gltf_scene_payload(henka_gltf_scene_asset* asset)
{
    size_t index;
    if (asset == NULL) return;
    for (index = 0U; index < asset->data.primitive_count; ++index)
    {
        if (asset->primitive_meshes[index] != NULL)
            henka_mesh_destroy_owned(asset->primitive_meshes[index]);
        asset->primitive_meshes[index] = NULL;
    }
    henka_model_scene_data_destroy(&asset->data);
    memset(asset->materials, 0, sizeof(asset->materials));
    memset(asset->material_assets, 0, sizeof(asset->material_assets));
    memset(asset->material_ready, 0, sizeof(asset->material_ready));
}

/* Mesh cache lookups use entries so fallback metadata remains path-specific. */

/* Failed asset retries replace entries only after a successful new load. */

static henka_result henka_asset_manager_create_fallback_textures(
    henka_asset_manager* manager)
{
    static const unsigned char white_pixels[] =
    {
        255U, 255U, 255U, 255U
    };
    static const unsigned char error_pixels[] =
    {
        255U, 0U, 255U, 255U, 0U, 0U, 0U, 255U,
        0U, 0U, 0U, 255U, 255U, 0U, 255U, 255U
    };
    static const unsigned char normal_pixels[] = {128U, 128U, 255U, 255U};
    static const unsigned char metallic_roughness_pixels[] = {0U, 128U, 0U, 255U};
    static const unsigned char occlusion_pixels[] = {255U, 255U, 255U, 255U};
    static const unsigned char emissive_pixels[] = {0U, 0U, 0U, 255U};
    henka_texture_descriptor descriptor;
    henka_result result;

    result = henka_texture_create_from_rgba8(
        manager->engine,
        1,
        1,
        white_pixels,
        &manager->white_texture);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    manager->white_texture->asset_manager_owned = true;

    result = henka_texture_create_from_rgba8(
        manager->engine,
        2,
        2,
        error_pixels,
        &manager->error_texture);
    if (result != HENKA_SUCCESS)
    {
        henka_texture_destroy_owned(manager->white_texture);
        manager->white_texture = NULL;
        return result;
    }
    manager->error_texture->asset_manager_owned = true;

    descriptor = henka_texture_descriptor_default_normal();
    result = henka_texture_create_from_rgba8_with_descriptor(
        manager->engine, 1, 1, normal_pixels, &descriptor, &manager->normal_texture);
    if (result != HENKA_SUCCESS)
    {
        henka_texture_destroy_owned(manager->error_texture);
        henka_texture_destroy_owned(manager->white_texture);
        manager->error_texture = NULL;
        manager->white_texture = NULL;
        return result;
    }
    manager->normal_texture->asset_manager_owned = true;

    descriptor = henka_texture_descriptor_default_data();
    descriptor.usage = HENKA_TEXTURE_USAGE_METALLIC_ROUGHNESS;
    result = henka_texture_create_from_rgba8_with_descriptor(
        manager->engine, 1, 1, metallic_roughness_pixels, &descriptor, &manager->metallic_roughness_texture);
    if (result != HENKA_SUCCESS)
    {
        henka_texture_destroy_owned(manager->normal_texture);
        henka_texture_destroy_owned(manager->error_texture);
        henka_texture_destroy_owned(manager->white_texture);
        manager->normal_texture = NULL;
        manager->error_texture = NULL;
        manager->white_texture = NULL;
        return result;
    }
    manager->metallic_roughness_texture->asset_manager_owned = true;

    descriptor.usage = HENKA_TEXTURE_USAGE_OCCLUSION;
    result = henka_texture_create_from_rgba8_with_descriptor(
        manager->engine, 1, 1, occlusion_pixels, &descriptor, &manager->occlusion_texture);
    if (result != HENKA_SUCCESS)
    {
        henka_texture_destroy_owned(manager->metallic_roughness_texture);
        henka_texture_destroy_owned(manager->normal_texture);
        henka_texture_destroy_owned(manager->error_texture);
        henka_texture_destroy_owned(manager->white_texture);
        manager->metallic_roughness_texture = NULL;
        manager->normal_texture = NULL;
        manager->error_texture = NULL;
        manager->white_texture = NULL;
        return result;
    }
    manager->occlusion_texture->asset_manager_owned = true;

    descriptor = henka_texture_descriptor_default_color();
    descriptor.usage = HENKA_TEXTURE_USAGE_EMISSIVE;
    result = henka_texture_create_from_rgba8_with_descriptor(
        manager->engine, 1, 1, emissive_pixels, &descriptor, &manager->emissive_texture);
    if (result != HENKA_SUCCESS)
    {
        henka_texture_destroy_owned(manager->occlusion_texture);
        henka_texture_destroy_owned(manager->metallic_roughness_texture);
        henka_texture_destroy_owned(manager->normal_texture);
        henka_texture_destroy_owned(manager->error_texture);
        henka_texture_destroy_owned(manager->white_texture);
        manager->occlusion_texture = NULL;
        manager->metallic_roughness_texture = NULL;
        manager->normal_texture = NULL;
        manager->error_texture = NULL;
        manager->white_texture = NULL;
        return result;
    }
    manager->emissive_texture->asset_manager_owned = true;

    return HENKA_SUCCESS;
}

static henka_result henka_asset_manager_create_fallback_mesh(
    henka_asset_manager* manager)
{
    henka_result result;

    result = henka_mesh_create_cube(
        manager->engine,
        &manager->fallback_mesh);
    if (result == HENKA_SUCCESS)
    {
        manager->fallback_mesh->asset_manager_owned = true;
    }

    return result;
}

henka_result henka_asset_manager_create(
    struct henka_engine* engine,
    struct henka_asset_manager** out_manager)
{
    henka_asset_manager* manager;
    henka_result result;

    if (out_manager != NULL)
    {
        *out_manager = NULL;
    }

    if (engine == NULL || out_manager == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    manager = henka_calloc(1U, sizeof(*manager));
    if (manager == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }

    manager->engine = engine;

    result = henka_asset_manager_create_fallback_textures(manager);
    if (result != HENKA_SUCCESS)
    {
        henka_free(manager);
        return result;
    }

    result = henka_asset_manager_create_fallback_mesh(manager);
    if (result != HENKA_SUCCESS)
    {
        henka_texture_destroy_owned(manager->emissive_texture);
        henka_texture_destroy_owned(manager->occlusion_texture);
        henka_texture_destroy_owned(manager->metallic_roughness_texture);
        henka_texture_destroy_owned(manager->normal_texture);
        henka_texture_destroy_owned(manager->error_texture);
        henka_texture_destroy_owned(manager->white_texture);
        henka_free(manager);
        return result;
    }

    *out_manager = manager;
    return HENKA_SUCCESS;
}

void henka_asset_manager_destroy(
    struct henka_asset_manager* manager)
{
    size_t index;

    if (manager == NULL)
    {
        return;
    }

    (void)henka_assets_end_texture_residency_frame(manager);
    (void)henka_assets_cancel_texture_residency_requests(manager, NULL);

    for (index = 0U; index < manager->shader_count; ++index)
    {
        henka_shader_destroy_owned(
            manager->shader_entries[index].shader);
        henka_free(manager->shader_entries[index].vertex_key);
        henka_free(manager->shader_entries[index].fragment_key);
        henka_free(manager->shader_entries[index].source_path);
        henka_free(manager->shader_entries[index].display_name);
    }

    for (index = 0U; index < manager->texture_count; ++index)
    {
        if (manager->texture_entries[index].owns_texture)
        {
            henka_texture_destroy_owned(
                manager->texture_entries[index].texture);
        }
        henka_free(manager->texture_entries[index].key);
        henka_free(manager->texture_entries[index].source_path);
        henka_free(manager->texture_entries[index].display_name);
    }

    for (index = 0U; index < manager->mesh_count; ++index)
    {
        if (manager->mesh_entries[index].owns_mesh)
        {
            henka_mesh_destroy_owned(
                manager->mesh_entries[index].mesh);
        }
        henka_free(manager->mesh_entries[index].key);
        henka_free(manager->mesh_entries[index].source_path);
        henka_free(manager->mesh_entries[index].display_name);
    }

    for (index = 0U; index < manager->material_count; ++index)
    {
        henka_free(manager->material_entries[index]->key);
        henka_free(manager->material_entries[index]->source_path);
        henka_free(manager->material_entries[index]->display_name);
        henka_free(manager->material_entries[index]);
    }

    for (index = 0U; index < manager->gltf_scene_count; ++index)
    {
        henka_assets_destroy_gltf_scene_payload(manager->gltf_scene_entries[index]);
        henka_free(manager->gltf_scene_entries[index]->key);
        henka_free(manager->gltf_scene_entries[index]->source_path);
        henka_free(manager->gltf_scene_entries[index]->display_name);
        henka_free(manager->gltf_scene_entries[index]);
    }

    henka_free(manager->shader_entries);
    henka_free(manager->texture_entries);
    henka_free(manager->mesh_entries);
    henka_free(manager->material_entries);
    henka_free(manager->gltf_scene_entries);
    henka_mesh_destroy_owned(manager->fallback_mesh);
    henka_texture_destroy_owned(manager->white_texture);
    henka_texture_destroy_owned(manager->error_texture);
    henka_texture_destroy_owned(manager->normal_texture);
    henka_texture_destroy_owned(manager->metallic_roughness_texture);
    henka_texture_destroy_owned(manager->occlusion_texture);
    henka_texture_destroy_owned(manager->emissive_texture);
    henka_free(manager);
}

henka_result henka_assets_load_shader(
    henka_asset_manager* manager,
    const char* vertex_path,
    const char* fragment_path,
    henka_shader** out_shader)
{
    henka_shader_contract_desc contract =
        henka_shader_contract_desc_default(HENKA_SHADER_CONTRACT_MATERIAL);

    return henka_assets_load_shader_with_contract(
        manager,
        vertex_path,
        fragment_path,
        &contract,
        out_shader);
}

henka_result henka_assets_load_shader_with_contract(
    henka_asset_manager* manager,
    const char* vertex_path,
    const char* fragment_path,
    const henka_shader_contract_desc* contract,
    henka_shader** out_shader)
{
    char* display_name;
    char* fragment_key;
    char* fragment_source_path;
    char* resolved_fragment_path;
    char* resolved_vertex_path;
    char* source_path;
    henka_asset_shader_entry* existing_entry;
    henka_shader* shader;
    henka_result result;
    char* vertex_key;
    char* vertex_source_path;

    if (out_shader != NULL)
    {
        *out_shader = NULL;
    }

    if (manager == NULL ||
        vertex_path == NULL ||
        fragment_path == NULL ||
        henka_shader_contract_desc_validate(contract) != HENKA_SUCCESS ||
        out_shader == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    vertex_key = NULL;
    fragment_key = NULL;
    vertex_source_path = NULL;
    fragment_source_path = NULL;
    result = henka_assets_make_canonical_key(
        vertex_path,
        &vertex_key);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    result = henka_assets_normalize_source_path(
        vertex_path,
        &vertex_source_path);
    if (result != HENKA_SUCCESS)
    {
        henka_free(fragment_key);
        henka_free(vertex_key);
        return result;
    }

    result = henka_assets_normalize_source_path(
        fragment_path,
        &fragment_source_path);
    if (result != HENKA_SUCCESS)
    {
        henka_free(vertex_source_path);
        henka_free(fragment_key);
        henka_free(vertex_key);
        return result;
    }

    result = henka_assets_make_canonical_key(
        fragment_path,
        &fragment_key);
    if (result != HENKA_SUCCESS)
    {
        henka_free(fragment_source_path);
        henka_free(vertex_source_path);
        henka_free(vertex_key);
        return result;
    }

    existing_entry = henka_asset_manager_find_shader_entry(
        manager,
        vertex_key,
        fragment_key,
        contract);
    if (existing_entry != NULL)
    {
        *out_shader = existing_entry->shader;
        henka_free(fragment_source_path);
        henka_free(vertex_source_path);
        henka_free(fragment_key);
        henka_free(vertex_key);
        return HENKA_SUCCESS;
    }

    display_name = henka_asset_copy_display_name(vertex_source_path);
    source_path = henka_asset_create_shader_source_path(
        vertex_source_path,
        fragment_source_path);
    if (display_name == NULL || source_path == NULL)
    {
        henka_free(source_path);
        henka_free(display_name);
        henka_free(fragment_source_path);
        henka_free(vertex_source_path);
        henka_free(fragment_key);
        henka_free(vertex_key);
        return HENKA_ERROR_OUT_OF_MEMORY;
    }

    resolved_vertex_path = NULL;
    resolved_fragment_path = NULL;
    result = henka_assets_resolve_path(
        henka_engine_get_asset_base_path(manager->engine),
        vertex_source_path,
        &resolved_vertex_path);
    if (result != HENKA_SUCCESS)
    {
        henka_free(source_path);
        henka_free(display_name);
        henka_free(fragment_source_path);
        henka_free(vertex_source_path);
        henka_free(fragment_key);
        henka_free(vertex_key);
        return result;
    }

    result = henka_assets_resolve_path(
        henka_engine_get_asset_base_path(manager->engine),
        fragment_source_path,
        &resolved_fragment_path);
    if (result != HENKA_SUCCESS)
    {
        henka_free(resolved_vertex_path);
        henka_free(source_path);
        henka_free(display_name);
        henka_free(fragment_source_path);
        henka_free(vertex_source_path);
        henka_free(fragment_key);
        henka_free(vertex_key);
        return result;
    }

    shader = NULL;
    result = henka_shader_create_from_files_with_contract(
        manager->engine,
        resolved_vertex_path,
        resolved_fragment_path,
        contract,
        &shader);
    henka_free(resolved_vertex_path);
    henka_free(resolved_fragment_path);
    henka_free(fragment_source_path);
    henka_free(vertex_source_path);
    if (result != HENKA_SUCCESS)
    {
        henka_free(source_path);
        henka_free(display_name);
        henka_free(fragment_key);
        henka_free(vertex_key);
        return result;
    }

    if (manager->shader_count == manager->shader_capacity)
    {
        result = henka_asset_manager_grow_shaders(manager);
        if (result != HENKA_SUCCESS)
        {
            henka_shader_destroy(shader);
            henka_free(source_path);
            henka_free(display_name);
            henka_free(fragment_key);
            henka_free(vertex_key);
            return result;
        }
    }

    shader->asset_manager_owned = true;
    manager->shader_entries[manager->shader_count].vertex_key =
        vertex_key;
    manager->shader_entries[manager->shader_count].fragment_key =
        fragment_key;
    manager->shader_entries[manager->shader_count].source_path =
        source_path;
    manager->shader_entries[manager->shader_count].display_name =
        display_name;
    manager->shader_entries[manager->shader_count].shader = shader;
    manager->shader_entries[manager->shader_count].contract_type = contract->type;
    manager->shader_entries[manager->shader_count].contract_version = contract->version;
    manager->shader_entries[manager->shader_count].metadata.type =
        HENKA_ASSET_TYPE_SHADER;
    manager->shader_entries[manager->shader_count].metadata.source_path =
        source_path;
    manager->shader_entries[manager->shader_count].metadata.display_name =
        display_name;
    manager->shader_entries[manager->shader_count].metadata.loaded = true;
    manager->shader_entries[manager->shader_count].metadata.fallback = false;
    manager->shader_entries[manager->shader_count].metadata.reload_supported =
        false;
    henka_asset_set_summary(
        &manager->shader_entries[manager->shader_count].metadata,
        "Shader loaded from canonical vertex and fragment sources.",
        "");
    manager->shader_count += 1U;
    *out_shader = shader;
    return HENKA_SUCCESS;
}

henka_result henka_assets_load_texture(
    henka_asset_manager* manager,
    const char* path,
    henka_texture** out_texture)
{
    henka_texture_descriptor descriptor = henka_texture_descriptor_default_color();
    return henka_assets_load_texture_with_descriptor(manager, path, &descriptor, out_texture);
}

henka_result henka_assets_load_texture_with_descriptor(
    henka_asset_manager* manager,
    const char* path,
    const henka_texture_descriptor* descriptor,
    henka_texture** out_texture)
{
    char* display_name;
    henka_texture_descriptor canonical_descriptor;
    henka_asset_texture_entry* existing_entry;
    bool fallback_active;
    char* key;
    char* resolved_path;
    char* source_path;
    henka_texture* texture;
    henka_texture_info texture_info;
    uint64_t source_size;
    bool source_size_known;
    size_t index;
    henka_result result;

    if (out_texture != NULL)
    {
        *out_texture = NULL;
    }

    if (manager == NULL ||
        path == NULL ||
        out_texture == NULL || descriptor == NULL ||
        henka_texture_descriptor_validate(descriptor) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    canonical_descriptor = *descriptor;
    henka_texture_descriptor_canonicalize(&canonical_descriptor);

    key = NULL;
    source_path = NULL;
    result = henka_assets_make_texture_cache_key(path, &canonical_descriptor, &key);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    result = henka_assets_normalize_source_path(
        path,
        &source_path);
    if (result != HENKA_SUCCESS)
    {
        henka_free(key);
        return result;
    }

    for (index = 0U; index < manager->texture_count; ++index)
    {
        henka_asset_texture_entry* runtime_entry = &manager->texture_entries[index];
        if (!runtime_entry->metadata.reload_supported &&
            runtime_entry->source_path != NULL &&
            strcmp(runtime_entry->source_path, source_path) == 0)
        {
            *out_texture = runtime_entry->texture;
            henka_free(source_path);
            henka_free(key);
            return HENKA_SUCCESS;
        }
    }

    existing_entry =
        henka_asset_manager_find_texture_entry(
            manager,
            key);
    if (existing_entry != NULL)
    {
        *out_texture = existing_entry->texture;
        henka_free(source_path);
        henka_free(key);
        return HENKA_SUCCESS;
    }

    resolved_path = NULL;
    result = henka_assets_resolve_path(
        henka_engine_get_asset_base_path(
            manager->engine),
        source_path,
        &resolved_path);
    if (result != HENKA_SUCCESS)
    {
        henka_free(key);
        henka_free(source_path);
        return result;
    }

    fallback_active = false;
    texture = NULL;
    result = henka_texture_create_from_file_with_descriptor(
        manager->engine,
        resolved_path,
        &canonical_descriptor,
        &texture);
    source_size_known = henka_assets_get_file_size(resolved_path, &source_size);
    henka_free(resolved_path);
    if (result == HENKA_ERROR_ASSET_SOURCE)
    {
        if (source_size_known)
            henka_assets_add_source_failed_texture_bytes(manager, source_size);
        else if (manager->texture_unknown_source_failure_count < UINT64_MAX)
            ++manager->texture_unknown_source_failure_count;
        HENKA_LOG_ERROR(
            "Using a path-specific error-texture alias because '%s' could not be loaded",
            source_path);
        result = henka_texture_create_borrowed_alias(
            henka_asset_manager_get_texture_fallback(manager, canonical_descriptor.usage),
            &texture);
        if (result != HENKA_SUCCESS)
        {
            henka_free(key);
            henka_free(source_path);
            return result;
        }
        fallback_active = true;
    }
    else if (result != HENKA_SUCCESS)
    {
        henka_free(key);
        henka_free(source_path);
        return result;
    }

    memset(&texture_info, 0, sizeof(texture_info));
    if (!fallback_active &&
        henka_texture_get_info(texture, &texture_info) == HENKA_SUCCESS &&
        ((manager->texture_residency_budget_bytes != 0U &&
            (texture_info.resident_gpu_bytes > manager->texture_residency_budget_bytes ||
                manager->texture_resident_bytes >
                    manager->texture_residency_budget_bytes - texture_info.resident_gpu_bytes)) ||
            manager->texture_resident_bytes > UINT64_MAX - texture_info.resident_gpu_bytes))
    {
        if (manager->texture_budget_rejection_count < UINT32_MAX)
            ++manager->texture_budget_rejection_count;
        henka_assets_add_failed_texture_bytes(
            manager,
            texture_info.resident_gpu_bytes);
        henka_texture_destroy(texture);
        henka_free(key);
        henka_free(source_path);
        return HENKA_ERROR_LIMIT;
    }

    display_name =
        henka_asset_copy_display_name(source_path);
    if (display_name == NULL)
    {
        henka_texture_destroy(texture);
        henka_free(key);
        henka_free(source_path);
        return HENKA_ERROR_OUT_OF_MEMORY;
    }

    if (manager->texture_count ==
        manager->texture_capacity)
    {
        result =
            henka_asset_manager_grow_textures(manager);
        if (result != HENKA_SUCCESS)
        {
            henka_texture_destroy(texture);
            henka_free(display_name);
            henka_free(key);
            henka_free(source_path);
            return result;
        }
    }

    texture->asset_manager_owned = true;
    texture->fallback_alias = fallback_active;
    manager->texture_entries[
        manager->texture_count].descriptor = canonical_descriptor;
    manager->texture_entries[
        manager->texture_count].resident_gpu_bytes = fallback_active ? 0U : texture_info.resident_gpu_bytes;
    manager->texture_entries[
        manager->texture_count].key = key;
    manager->texture_entries[
        manager->texture_count].source_path =
        source_path;
    manager->texture_entries[
        manager->texture_count].display_name =
        display_name;
    manager->texture_entries[
        manager->texture_count].texture = texture;
    manager->texture_entries[
        manager->texture_count].owns_texture = true;
    manager->texture_entries[
        manager->texture_count].metadata.type =
        HENKA_ASSET_TYPE_TEXTURE;
    manager->texture_entries[
        manager->texture_count].metadata.source_path =
        source_path;
    manager->texture_entries[
        manager->texture_count].metadata.display_name =
        display_name;
    manager->texture_entries[
        manager->texture_count].metadata.loaded =
        !fallback_active;
    manager->texture_entries[
        manager->texture_count].metadata.fallback =
        fallback_active;
    manager->texture_entries[
        manager->texture_count].metadata.reload_supported =
        fallback_active;
    manager->texture_entries[
        manager->texture_count].metadata.has_texture_descriptor = true;
    manager->texture_entries[
        manager->texture_count].metadata.texture_descriptor = canonical_descriptor;
    henka_asset_set_summary(
        &manager->texture_entries[
            manager->texture_count].metadata,
        fallback_active ?
            "A path-specific texture fallback is active and can be retried without changing the borrowed pointer." :
            "Texture loaded from the canonical asset path.",
        fallback_active ?
            "Texture source loading failed. Retry after fixing the source asset; allocation and renderer failures are not cached as fallbacks." :
            "");
    manager->texture_count += 1U;
    if (!fallback_active)
    {
        manager->texture_resident_bytes += texture_info.resident_gpu_bytes;
        if (UINT64_MAX - manager->texture_uploaded_bytes >= texture_info.resident_gpu_bytes)
            manager->texture_uploaded_bytes += texture_info.resident_gpu_bytes;
    }
    *out_texture = texture;
    return HENKA_SUCCESS;
}

henka_result henka_assets_adopt_runtime_texture(
    henka_asset_manager* manager,
    const char* identity,
    henka_texture* texture,
    henka_texture** out_texture)
{
    char* display_name;
    char* key;
    char* source_path;
    henka_asset_texture_entry* existing_entry;
    henka_texture_info texture_info;
    henka_result result;

    if (out_texture != NULL)
    {
        *out_texture = NULL;
    }
    if (manager == NULL || identity == NULL || texture == NULL ||
        out_texture == NULL || texture->asset_manager_owned ||
        texture->fallback_alias || texture->backend_data == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    key = NULL;
    source_path = NULL;
    memset(&texture_info, 0, sizeof(texture_info));
    result = henka_texture_get_info(texture, &texture_info);
    if (result != HENKA_SUCCESS || !texture_info.backend_ready ||
        texture_info.fallback_alias || texture_info.resident_gpu_bytes == 0U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_assets_make_canonical_key(identity, &source_path);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    result = henka_assets_make_texture_cache_key(
        source_path,
        &texture_info.descriptor,
        &key);
    if (result != HENKA_SUCCESS)
    {
        henka_free(source_path);
        return result;
    }
    existing_entry = henka_asset_manager_find_texture_entry(manager, key);
    if (existing_entry != NULL)
    {
        henka_free(source_path);
        henka_free(key);
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if ((manager->texture_residency_budget_bytes != 0U &&
            (texture_info.resident_gpu_bytes > manager->texture_residency_budget_bytes ||
             manager->texture_resident_bytes >
                manager->texture_residency_budget_bytes - texture_info.resident_gpu_bytes)) ||
        manager->texture_resident_bytes > UINT64_MAX - texture_info.resident_gpu_bytes)
    {
        if (manager->texture_budget_rejection_count < UINT32_MAX)
        {
            ++manager->texture_budget_rejection_count;
        }
        henka_assets_add_failed_texture_bytes(manager, texture_info.resident_gpu_bytes);
        henka_free(source_path);
        henka_free(key);
        return HENKA_ERROR_LIMIT;
    }

    display_name = henka_asset_copy_display_name(source_path);
    if (display_name == NULL)
    {
        henka_free(source_path);
        henka_free(key);
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    if (manager->texture_count == manager->texture_capacity)
    {
        result = henka_asset_manager_grow_textures(manager);
        if (result != HENKA_SUCCESS)
        {
            henka_free(source_path);
            henka_free(display_name);
            henka_free(key);
            return result;
        }
    }

    texture->asset_manager_owned = true;
    texture->fallback_alias = false;
    manager->texture_entries[manager->texture_count].key = key;
    manager->texture_entries[manager->texture_count].source_path = source_path;
    manager->texture_entries[manager->texture_count].display_name = display_name;
    manager->texture_entries[manager->texture_count].texture = texture;
    manager->texture_entries[manager->texture_count].owns_texture = true;
    manager->texture_entries[manager->texture_count].descriptor = texture_info.descriptor;
    manager->texture_entries[manager->texture_count].resident_gpu_bytes = texture_info.resident_gpu_bytes;
    manager->texture_entries[manager->texture_count].metadata.type = HENKA_ASSET_TYPE_TEXTURE;
    manager->texture_entries[manager->texture_count].metadata.source_path =
        manager->texture_entries[manager->texture_count].source_path;
    manager->texture_entries[manager->texture_count].metadata.display_name = display_name;
    manager->texture_entries[manager->texture_count].metadata.loaded = true;
    manager->texture_entries[manager->texture_count].metadata.fallback = false;
    manager->texture_entries[manager->texture_count].metadata.reload_supported = false;
    manager->texture_entries[manager->texture_count].metadata.has_texture_descriptor = true;
    manager->texture_entries[manager->texture_count].metadata.texture_descriptor = texture_info.descriptor;
    henka_asset_set_summary(
        &manager->texture_entries[manager->texture_count].metadata,
        "Runtime texture adopted by the asset manager under a stable identity.",
        "Runtime textures have no source-file reload path.");
    manager->texture_count += 1U;
    manager->texture_resident_bytes += texture_info.resident_gpu_bytes;
    if (UINT64_MAX - manager->texture_uploaded_bytes >= texture_info.resident_gpu_bytes)
    {
        manager->texture_uploaded_bytes += texture_info.resident_gpu_bytes;
    }
    *out_texture = texture;
    return HENKA_SUCCESS;
}

henka_result henka_assets_set_texture_residency_budget(
    henka_asset_manager* manager,
    uint64_t budget_bytes)
{
    if (manager == NULL)
        return HENKA_ERROR_INVALID_ARGUMENT;
    if (budget_bytes != 0U && manager->texture_resident_bytes > budget_bytes)
        return HENKA_ERROR_LIMIT;
    manager->texture_residency_budget_bytes = budget_bytes;
    return HENKA_SUCCESS;
}

henka_result henka_assets_get_texture_residency_diagnostics(
    const henka_asset_manager* manager,
    henka_texture_residency_diagnostics* out_diagnostics)
{
    size_t index;
    size_t pinned_texture_count = 0U;

    if (out_diagnostics != NULL)
        memset(out_diagnostics, 0, sizeof(*out_diagnostics));
    if (manager == NULL || out_diagnostics == NULL)
        return HENKA_ERROR_INVALID_ARGUMENT;
    out_diagnostics->budget_bytes = manager->texture_residency_budget_bytes;
    out_diagnostics->resident_bytes = manager->texture_resident_bytes;
    out_diagnostics->uploaded_bytes = manager->texture_uploaded_bytes;
    out_diagnostics->evicted_bytes = manager->texture_evicted_bytes;
    out_diagnostics->trimmed_bytes = manager->texture_trimmed_bytes;
    out_diagnostics->demoted_bytes = manager->texture_demoted_bytes;
    out_diagnostics->failed_bytes = manager->texture_failed_bytes;
    out_diagnostics->source_failed_bytes = manager->texture_source_failed_bytes;
    out_diagnostics->budget_rejection_count = manager->texture_budget_rejection_count;
    out_diagnostics->unknown_failed_request_count = manager->texture_unknown_failed_request_count;
    out_diagnostics->unknown_source_failure_count = manager->texture_unknown_source_failure_count;
    out_diagnostics->managed_texture_count = manager->texture_count;
    out_diagnostics->queued_request_count = manager->texture_residency_request_count;
    out_diagnostics->completed_request_count = manager->texture_residency_completed_requests;
    out_diagnostics->failed_request_count = manager->texture_residency_failed_requests;
    out_diagnostics->cancelled_request_count = manager->texture_residency_cancelled_requests;
    out_diagnostics->eviction_count = manager->texture_residency_eviction_count;
    out_diagnostics->eviction_failure_count = manager->texture_residency_eviction_failure_count;
    out_diagnostics->trim_count = manager->texture_residency_trim_count;
    out_diagnostics->trim_failure_count = manager->texture_residency_trim_failure_count;
    out_diagnostics->progression_mode =
        HENKA_TEXTURE_RESIDENCY_PROGRESS_SYNCHRONOUS_MAIN_THREAD;
    for (index = 0U; index < manager->texture_count; ++index)
    {
        if (manager->texture_entries[index].metadata.fallback)
            ++out_diagnostics->fallback_texture_count;
        if (manager->texture_residency_frame_active &&
            manager->texture_entries[index].residency_pinned)
        {
            ++pinned_texture_count;
            if (UINT64_MAX - out_diagnostics->pinned_bytes >=
                manager->texture_entries[index].resident_gpu_bytes)
                out_diagnostics->pinned_bytes +=
                    manager->texture_entries[index].resident_gpu_bytes;
            else
                out_diagnostics->pinned_bytes = UINT64_MAX;
        }
    }
    out_diagnostics->pinned_texture_count = pinned_texture_count;
    out_diagnostics->budget_exceeded =
        manager->texture_residency_budget_bytes != 0U &&
        manager->texture_resident_bytes > manager->texture_residency_budget_bytes;
    return HENKA_SUCCESS;
}

henka_result henka_assets_begin_texture_residency_frame(
    henka_asset_manager* manager,
    uint64_t frame_index)
{
    if (manager == NULL)
        return HENKA_ERROR_INVALID_ARGUMENT;
    manager->texture_residency_frame_index = frame_index;
    manager->texture_residency_frame_active = true;
    for (size_t index = 0U; index < manager->texture_count; ++index)
        manager->texture_entries[index].residency_pinned = false;
    return HENKA_SUCCESS;
}

henka_result henka_assets_end_texture_residency_frame(
    henka_asset_manager* manager)
{
    if (manager == NULL)
        return HENKA_ERROR_INVALID_ARGUMENT;
    manager->texture_residency_frame_active = false;
    for (size_t index = 0U; index < manager->texture_count; ++index)
        manager->texture_entries[index].residency_pinned = false;
    return HENKA_SUCCESS;
}

henka_result henka_assets_pin_texture_for_residency_frame(
    henka_asset_manager* manager,
    henka_texture* texture)
{
    size_t index;

    if (manager == NULL || texture == NULL || !manager->texture_residency_frame_active)
        return HENKA_ERROR_INVALID_ARGUMENT;
    for (index = 0U; index < manager->texture_count; ++index)
    {
        if (manager->texture_entries[index].texture == texture)
        {
            manager->texture_entries[index].residency_pinned = true;
            return HENKA_SUCCESS;
        }
    }
    return HENKA_ERROR_ASSET_SOURCE;
}

henka_result henka_assets_queue_texture_residency_request(
    henka_asset_manager* manager,
    henka_texture* texture,
    uint32_t resident_mip_count)
{
    return henka_assets_queue_texture_residency_request_with_priority(
        manager, texture, resident_mip_count, 0U);
}

henka_result henka_assets_queue_texture_residency_request_with_priority(
    henka_asset_manager* manager,
    henka_texture* texture,
    uint32_t resident_mip_count,
    uint32_t priority)
{
    size_t index;
    size_t weakest_index;

    if (manager == NULL || texture == NULL || resident_mip_count == 0U)
        return HENKA_ERROR_INVALID_ARGUMENT;
    for (index = 0U; index < manager->texture_count; ++index)
    {
        if (manager->texture_entries[index].texture == texture)
        {
            if (manager->texture_entries[index].metadata.fallback ||
                !manager->texture_entries[index].owns_texture)
                return HENKA_ERROR_ASSET_SOURCE;
            break;
        }
    }
    if (index == manager->texture_count)
        return HENKA_ERROR_INVALID_ARGUMENT;
    for (index = 0U; index < manager->texture_residency_request_count; ++index)
    {
        if (manager->texture_residency_request_textures[index] == texture)
        {
            /* A texture can be referenced by several visible materials. Keep
             * the strongest request so a later, farther reference cannot
             * demote a nearer reference before the request is serviced. */
            if (manager->texture_residency_request_mips[index] < resident_mip_count)
                manager->texture_residency_request_mips[index] = resident_mip_count;
            if (manager->texture_residency_request_priorities[index] < priority)
                manager->texture_residency_request_priorities[index] = priority;
            manager->texture_residency_request_revisions[index] = texture->content_revision;
            return HENKA_SUCCESS;
        }
    }
    if (manager->texture_residency_request_count >= HENKA_MAX_TEXTURE_RESIDENCY_REQUESTS)
    {
        weakest_index = 0U;
        for (index = 1U;
             index < manager->texture_residency_request_count;
             ++index)
        {
            if (manager->texture_residency_request_priorities[index] <
                    manager->texture_residency_request_priorities[weakest_index] ||
                (manager->texture_residency_request_priorities[index] ==
                        manager->texture_residency_request_priorities[weakest_index] &&
                    manager->texture_residency_request_mips[index] <
                        manager->texture_residency_request_mips[weakest_index]))
                weakest_index = index;
        }
        if (priority < manager->texture_residency_request_priorities[weakest_index] ||
            (priority == manager->texture_residency_request_priorities[weakest_index] &&
                resident_mip_count <=
                    manager->texture_residency_request_mips[weakest_index]))
            return HENKA_ERROR_LIMIT;
        manager->texture_residency_request_textures[weakest_index] = texture;
        manager->texture_residency_request_mips[weakest_index] = resident_mip_count;
        manager->texture_residency_request_priorities[weakest_index] = priority;
        manager->texture_residency_request_revisions[weakest_index] = texture->content_revision;
        return HENKA_SUCCESS;
    }
    index = manager->texture_residency_request_count++;
    manager->texture_residency_request_textures[index] = texture;
    manager->texture_residency_request_mips[index] = resident_mip_count;
    manager->texture_residency_request_priorities[index] = priority;
    manager->texture_residency_request_revisions[index] = texture->content_revision;
    return HENKA_SUCCESS;
}

henka_result henka_assets_process_texture_residency_requests(
    henka_asset_manager* manager,
    size_t max_requests,
    size_t* out_processed_requests)
{
    size_t processed = 0U;

    if (out_processed_requests != NULL) *out_processed_requests = 0U;
    if (manager == NULL) return HENKA_ERROR_INVALID_ARGUMENT;
    while (manager->texture_residency_request_count > 0U &&
        (max_requests == 0U || processed < max_requests))
    {
        size_t request_index = 0U;
        size_t index;
        henka_texture* texture;
        uint32_t resident_mips;
        uint64_t request_revision;
        uint64_t failed_bytes_before;
        henka_texture_info info;
        henka_result result;

        for (index = 1U; index < manager->texture_residency_request_count; ++index)
        {
            if (manager->texture_residency_request_priorities[index] >
                    manager->texture_residency_request_priorities[request_index] ||
                (manager->texture_residency_request_priorities[index] ==
                        manager->texture_residency_request_priorities[request_index] &&
                    manager->texture_residency_request_mips[index] >
                        manager->texture_residency_request_mips[request_index]))
            {
                request_index = index;
            }
        }
        texture = manager->texture_residency_request_textures[request_index];
        resident_mips = manager->texture_residency_request_mips[request_index];
        request_revision = manager->texture_residency_request_revisions[request_index];

        if (request_index + 1U < manager->texture_residency_request_count)
        {
            memmove(
                &manager->texture_residency_request_textures[request_index],
                &manager->texture_residency_request_textures[request_index + 1U],
                (manager->texture_residency_request_count - request_index - 1U) * sizeof(manager->texture_residency_request_textures[0]));
            memmove(
                &manager->texture_residency_request_mips[request_index],
                &manager->texture_residency_request_mips[request_index + 1U],
                (manager->texture_residency_request_count - request_index - 1U) * sizeof(manager->texture_residency_request_mips[0]));
            memmove(
                &manager->texture_residency_request_priorities[request_index],
                &manager->texture_residency_request_priorities[request_index + 1U],
                (manager->texture_residency_request_count - request_index - 1U) * sizeof(manager->texture_residency_request_priorities[0]));
            memmove(
                &manager->texture_residency_request_revisions[request_index],
                &manager->texture_residency_request_revisions[request_index + 1U],
                (manager->texture_residency_request_count - request_index - 1U) * sizeof(manager->texture_residency_request_revisions[0]));
        }
        --manager->texture_residency_request_count;
        if (texture == NULL || texture->content_revision != request_revision)
        {
            if (manager->texture_residency_cancelled_requests < UINT64_MAX)
                ++manager->texture_residency_cancelled_requests;
            ++processed;
            continue;
        }
        memset(&info, 0, sizeof(info));
        failed_bytes_before = manager->texture_failed_bytes;
        result = henka_assets_set_texture_resident_mips(manager, texture, resident_mips, &info);
        if (result == HENKA_SUCCESS)
        {
            if (manager->texture_residency_completed_requests < UINT64_MAX)
                ++manager->texture_residency_completed_requests;
        }
        else if (manager->texture_residency_failed_requests < UINT64_MAX)
        {
            ++manager->texture_residency_failed_requests;
            if (manager->texture_failed_bytes == failed_bytes_before &&
                manager->texture_unknown_failed_request_count < UINT64_MAX)
                ++manager->texture_unknown_failed_request_count;
        }
        ++processed;
    }
    if (out_processed_requests != NULL) *out_processed_requests = processed;
    return HENKA_SUCCESS;
}

henka_result henka_assets_cancel_texture_residency_requests(
    henka_asset_manager* manager,
    size_t* out_cancelled_requests)
{
    size_t cancelled;

    if (out_cancelled_requests != NULL)
        *out_cancelled_requests = 0U;
    if (manager == NULL)
        return HENKA_ERROR_INVALID_ARGUMENT;
    cancelled = manager->texture_residency_request_count;
    if (UINT64_MAX - manager->texture_residency_cancelled_requests <
        (uint64_t)cancelled)
        manager->texture_residency_cancelled_requests = UINT64_MAX;
    else
        manager->texture_residency_cancelled_requests += (uint64_t)cancelled;
    manager->texture_residency_request_count = 0U;
    memset(manager->texture_residency_request_textures, 0,
        sizeof(manager->texture_residency_request_textures));
    memset(manager->texture_residency_request_mips, 0,
        sizeof(manager->texture_residency_request_mips));
    memset(manager->texture_residency_request_priorities, 0,
        sizeof(manager->texture_residency_request_priorities));
    memset(manager->texture_residency_request_revisions, 0,
        sizeof(manager->texture_residency_request_revisions));
    if (out_cancelled_requests != NULL)
        *out_cancelled_requests = cancelled;
    return HENKA_SUCCESS;
}

henka_result henka_assets_set_texture_resident_mips(
    henka_asset_manager* manager,
    henka_texture* texture,
    uint32_t resident_mip_count,
    henka_texture_info* out_info)
{
    henka_asset_texture_entry* entry = NULL;
    henka_texture* replacement = NULL;
    henka_texture_info replacement_info;
    char* resolved_path = NULL;
    uint64_t resident_without_old;
    size_t index;
    henka_result result;

    if (out_info != NULL)
        memset(out_info, 0, sizeof(*out_info));
    if (manager == NULL || texture == NULL || resident_mip_count == 0U)
        return HENKA_ERROR_INVALID_ARGUMENT;
    for (index = 0U; index < manager->texture_count; ++index)
    {
        if (manager->texture_entries[index].texture == texture)
        {
            entry = &manager->texture_entries[index];
            break;
        }
    }
    if (entry == NULL || !entry->owns_texture || entry->metadata.fallback)
        return HENKA_ERROR_ASSET_SOURCE;
    if (entry->resident_gpu_bytes == 0U ||
        manager->texture_resident_bytes < entry->resident_gpu_bytes)
        return HENKA_ERROR_RENDERER;

    result = henka_assets_resolve_path(
        henka_engine_get_asset_base_path(manager->engine),
        entry->source_path,
        &resolved_path);
    if (result != HENKA_SUCCESS)
        return result;
    result = henka_texture_create_from_file_with_descriptor_and_mip_limit(
        manager->engine,
        resolved_path,
        &entry->descriptor,
        resident_mip_count,
        &replacement);
    henka_free(resolved_path);
    if (result != HENKA_SUCCESS)
        return result;

    memset(&replacement_info, 0, sizeof(replacement_info));
    if (henka_texture_get_info(replacement, &replacement_info) != HENKA_SUCCESS)
    {
        henka_texture_destroy(replacement);
        return HENKA_ERROR_RENDERER;
    }
    resident_without_old = manager->texture_resident_bytes - entry->resident_gpu_bytes;
    if ((manager->texture_residency_budget_bytes != 0U &&
            (replacement_info.resident_gpu_bytes > manager->texture_residency_budget_bytes ||
                resident_without_old >
                    manager->texture_residency_budget_bytes - replacement_info.resident_gpu_bytes)) ||
        resident_without_old > UINT64_MAX - replacement_info.resident_gpu_bytes)
    {
        if (manager->texture_budget_rejection_count < UINT32_MAX)
            ++manager->texture_budget_rejection_count;
        henka_assets_add_failed_texture_bytes(
            manager,
            replacement_info.resident_gpu_bytes);
        henka_texture_destroy(replacement);
        return HENKA_ERROR_LIMIT;
    }
    result = henka_texture_replace_owned_payload(entry->texture, replacement);
    if (result != HENKA_SUCCESS)
    {
        henka_assets_add_failed_texture_bytes(
            manager,
            replacement_info.resident_gpu_bytes);
        henka_texture_destroy(replacement);
        return result;
    }
    manager->texture_resident_bytes = resident_without_old + replacement_info.resident_gpu_bytes;
    if (UINT64_MAX - manager->texture_uploaded_bytes >= replacement_info.resident_gpu_bytes)
        manager->texture_uploaded_bytes += replacement_info.resident_gpu_bytes;
    entry->resident_gpu_bytes = replacement_info.resident_gpu_bytes;
    henka_asset_set_summary(
        &entry->metadata,
        "KTX2 texture residency changed transactionally by bounded top-mip upload.",
        "Background streaming and automatic policy eviction are not enabled.");
    if (out_info != NULL)
        *out_info = replacement_info;
    return HENKA_SUCCESS;
}

henka_result henka_assets_trim_texture_residency(
    henka_asset_manager* manager,
    uint64_t target_bytes,
    size_t max_evictions,
    size_t* out_evicted_textures)
{
    size_t evicted = 0U;

    if (out_evicted_textures != NULL)
        *out_evicted_textures = 0U;
    if (manager == NULL || target_bytes == 0U)
        return HENKA_ERROR_INVALID_ARGUMENT;

    while (manager->texture_resident_bytes > target_bytes &&
        (max_evictions == 0U || evicted < max_evictions))
    {
        size_t candidate = SIZE_MAX;
        uint64_t candidate_bytes = 0U;
        size_t index;
        henka_texture_info candidate_info;
        henka_texture_info resident_info;
        henka_result result;

        for (index = 0U; index < manager->texture_count; ++index)
        {
            henka_asset_texture_entry* entry = &manager->texture_entries[index];
            henka_texture_info info;

            if (!entry->owns_texture || entry->metadata.fallback ||
                !henka_asset_texture_path_is_ktx2(entry->source_path) ||
                entry->resident_gpu_bytes <= candidate_bytes ||
                henka_texture_get_info(entry->texture, &info) != HENKA_SUCCESS ||
                info.resident_mip_count <= 1U ||
                (manager->texture_residency_frame_active && entry->residency_pinned))
            {
                continue;
            }
            candidate = index;
            candidate_bytes = entry->resident_gpu_bytes;
        }
        if (candidate == SIZE_MAX)
            break;

        memset(&candidate_info, 0, sizeof(candidate_info));
        result = henka_texture_get_info(
            manager->texture_entries[candidate].texture,
            &candidate_info);
        if (result == HENKA_SUCCESS)
        {
            result = henka_assets_set_texture_resident_mips(
                manager,
                manager->texture_entries[candidate].texture,
                1U,
                NULL);
        }
        if (result != HENKA_SUCCESS)
        {
            if (manager->texture_residency_trim_failure_count < UINT64_MAX)
                ++manager->texture_residency_trim_failure_count;
            break;
        }
        memset(&resident_info, 0, sizeof(resident_info));
        if (henka_texture_get_info(
                manager->texture_entries[candidate].texture,
                &resident_info) == HENKA_SUCCESS &&
            candidate_info.resident_gpu_bytes > resident_info.resident_gpu_bytes)
        {
            const uint64_t demoted_bytes =
                candidate_info.resident_gpu_bytes - resident_info.resident_gpu_bytes;
            if (UINT64_MAX - manager->texture_trimmed_bytes >= demoted_bytes)
                manager->texture_trimmed_bytes +=
                    demoted_bytes;
            else
                manager->texture_trimmed_bytes = UINT64_MAX;
            if (UINT64_MAX - manager->texture_demoted_bytes >= demoted_bytes)
                manager->texture_demoted_bytes +=
                    demoted_bytes;
            else
                manager->texture_demoted_bytes = UINT64_MAX;
        }
        if (manager->texture_residency_trim_count < UINT64_MAX)
            ++manager->texture_residency_trim_count;
        ++evicted;
    }

    if (out_evicted_textures != NULL)
        *out_evicted_textures = evicted;
    return manager->texture_resident_bytes <= target_bytes ? HENKA_SUCCESS : HENKA_ERROR_LIMIT;
}

henka_result henka_assets_enforce_texture_residency_budget(
    henka_asset_manager* manager,
    size_t max_evictions,
    size_t* out_evicted_textures)
{
    if (out_evicted_textures != NULL)
        *out_evicted_textures = 0U;
    if (manager == NULL)
        return HENKA_ERROR_INVALID_ARGUMENT;
    if (manager->texture_residency_budget_bytes == 0U)
        return HENKA_SUCCESS;
    return henka_assets_trim_texture_residency(
        manager,
        manager->texture_residency_budget_bytes,
        max_evictions,
        out_evicted_textures);
}

henka_result henka_assets_load_obj_mesh(
    henka_asset_manager* manager,
    const char* path,
    henka_mesh** out_mesh)
{
    char* display_name;
    henka_asset_mesh_entry* existing_entry;
    char* key;
    char* resolved_path;
    char* source_path;
    henka_mesh* mesh;
    henka_result result;

    if (out_mesh != NULL)
    {
        *out_mesh = NULL;
    }

    if (manager == NULL || path == NULL || out_mesh == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    key = NULL;
    source_path = NULL;
    result = henka_assets_make_canonical_key(path, &key);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    result = henka_assets_normalize_source_path(path, &source_path);
    if (result != HENKA_SUCCESS)
    {
        henka_free(key);
        return result;
    }

    existing_entry = henka_asset_manager_find_mesh_entry(
        manager,
        key);
    if (existing_entry != NULL)
    {
        *out_mesh = existing_entry->mesh;
        henka_free(source_path);
        henka_free(key);
        return HENKA_SUCCESS;
    }

    resolved_path = NULL;
    result = henka_assets_resolve_path(
        henka_engine_get_asset_base_path(manager->engine),
        source_path,
        &resolved_path);
    if (result != HENKA_SUCCESS)
    {
        henka_free(key);
        henka_free(source_path);
        return result;
    }

    mesh = NULL;
    result = henka_mesh_create_from_obj(
        manager->engine,
        resolved_path,
        &mesh);
    henka_free(resolved_path);
    if (result != HENKA_SUCCESS)
    {
        HENKA_LOG_ERROR(
            "Using the fallback mesh because '%s' could not be loaded",
            source_path);
        mesh = manager->fallback_mesh;
    }

    display_name = henka_asset_copy_display_name(source_path);
    if (display_name == NULL)
    {
        if (mesh != manager->fallback_mesh)
        {
            henka_mesh_destroy(mesh);
        }
        henka_free(key);
        henka_free(source_path);
        return HENKA_ERROR_OUT_OF_MEMORY;
    }

    if (manager->mesh_count == manager->mesh_capacity)
    {
        result = henka_asset_manager_grow_meshes(manager);
        if (result != HENKA_SUCCESS)
        {
            if (mesh != manager->fallback_mesh)
            {
                henka_mesh_destroy(mesh);
            }
            henka_free(display_name);
            henka_free(key);
            henka_free(source_path);
            return result;
        }
    }

    if (mesh != manager->fallback_mesh)
    {
        mesh->asset_manager_owned = true;
    }

    manager->mesh_entries[manager->mesh_count].key = key;
    manager->mesh_entries[manager->mesh_count].source_path =
        source_path;
    manager->mesh_entries[manager->mesh_count].display_name =
        display_name;
    manager->mesh_entries[manager->mesh_count].mesh = mesh;
    manager->mesh_entries[manager->mesh_count].owns_mesh =
        mesh != manager->fallback_mesh;
    manager->mesh_entries[manager->mesh_count].metadata.type =
        HENKA_ASSET_TYPE_MESH;
    manager->mesh_entries[manager->mesh_count].metadata.source_path =
        source_path;
    manager->mesh_entries[manager->mesh_count].metadata.display_name =
        display_name;
    manager->mesh_entries[manager->mesh_count].metadata.loaded =
        mesh != manager->fallback_mesh;
    manager->mesh_entries[manager->mesh_count].metadata.fallback =
        mesh == manager->fallback_mesh;
    manager->mesh_entries[manager->mesh_count].metadata.reload_supported =
        mesh == manager->fallback_mesh;
    henka_asset_set_summary(
        &manager->mesh_entries[manager->mesh_count].metadata,
        mesh == manager->fallback_mesh ?
            "Mesh fallback is active and can be retried." :
            "Mesh loaded from the canonical asset path.",
        mesh == manager->fallback_mesh ?
            "Mesh load failed and the fallback mesh was used. Retry after fixing the source asset." :
            "");
    manager->mesh_count += 1U;
    *out_mesh = mesh;
    return HENKA_SUCCESS;
}

henka_result henka_assets_load_gltf_mesh(
    henka_asset_manager* manager,
    const char* path,
    henka_mesh** out_mesh)
{
    char* display_name;
    henka_asset_mesh_entry* existing_entry;
    char* key = NULL;
    char* resolved_path = NULL;
    char* source_path = NULL;
    henka_mesh* mesh = NULL;
    henka_result result;

    if (out_mesh != NULL) *out_mesh = NULL;
    if (manager == NULL || path == NULL || out_mesh == NULL) return HENKA_ERROR_INVALID_ARGUMENT;
    result = henka_assets_make_canonical_key(path, &key);
    if (result != HENKA_SUCCESS) return result;
    result = henka_assets_normalize_source_path(path, &source_path);
    if (result != HENKA_SUCCESS) { henka_free(key); return result; }
    existing_entry = henka_asset_manager_find_mesh_entry(manager, key);
    if (existing_entry != NULL)
    {
        *out_mesh = existing_entry->mesh;
        henka_free(source_path); henka_free(key);
        return HENKA_SUCCESS;
    }
    result = henka_assets_resolve_path(henka_engine_get_asset_base_path(manager->engine), source_path, &resolved_path);
    if (result == HENKA_SUCCESS) result = henka_mesh_create_from_gltf(manager->engine, resolved_path, &mesh);
    henka_free(resolved_path);
    if (result != HENKA_SUCCESS)
    {
        HENKA_LOG_ERROR("Using the fallback mesh because glTF '%s' could not be loaded", source_path);
        mesh = manager->fallback_mesh;
    }
    display_name = henka_asset_copy_display_name(source_path);
    if (display_name == NULL)
    {
        if (mesh != manager->fallback_mesh) henka_mesh_destroy(mesh);
        henka_free(source_path); henka_free(key);
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    if (manager->mesh_count == manager->mesh_capacity)
    {
        result = henka_asset_manager_grow_meshes(manager);
        if (result != HENKA_SUCCESS)
        {
            if (mesh != manager->fallback_mesh) henka_mesh_destroy(mesh);
            henka_free(display_name); henka_free(source_path); henka_free(key);
            return result;
        }
    }
    if (mesh != manager->fallback_mesh) mesh->asset_manager_owned = true;
    manager->mesh_entries[manager->mesh_count].key = key;
    manager->mesh_entries[manager->mesh_count].source_path = source_path;
    manager->mesh_entries[manager->mesh_count].display_name = display_name;
    manager->mesh_entries[manager->mesh_count].mesh = mesh;
    manager->mesh_entries[manager->mesh_count].owns_mesh = mesh != manager->fallback_mesh;
    manager->mesh_entries[manager->mesh_count].metadata.type = HENKA_ASSET_TYPE_MESH;
    manager->mesh_entries[manager->mesh_count].metadata.source_path = source_path;
    manager->mesh_entries[manager->mesh_count].metadata.display_name = display_name;
    manager->mesh_entries[manager->mesh_count].metadata.loaded = mesh != manager->fallback_mesh;
    manager->mesh_entries[manager->mesh_count].metadata.fallback = mesh == manager->fallback_mesh;
    manager->mesh_entries[manager->mesh_count].metadata.reload_supported = mesh == manager->fallback_mesh;
    henka_asset_set_summary(&manager->mesh_entries[manager->mesh_count].metadata,
        mesh == manager->fallback_mesh ? "glTF mesh fallback is active and can be retried." : "glTF mesh loaded from the canonical asset path.",
        mesh == manager->fallback_mesh ? "glTF mesh load failed and the fallback mesh was used. Retry after fixing the source asset." : "");
    manager->mesh_count += 1U;
    *out_mesh = mesh;
    return HENKA_SUCCESS;
}

static henka_result henka_assets_resolve_material_uri(
    const char* material_path,
    const char* uri,
    char** out_path)
{
    const char* separator;
    size_t directory_length;
    char* directory;
    henka_result result;

    if (out_path != NULL) *out_path = NULL;
    if (material_path == NULL || uri == NULL || out_path == NULL || uri[0] == '\0')
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    separator = strrchr(material_path, '/');
    directory_length = separator == NULL ? 0U : (size_t)(separator - material_path);
    directory = henka_malloc(directory_length + 1U);
    if (directory == NULL) return HENKA_ERROR_OUT_OF_MEMORY;
    if (directory_length > 0U) memcpy(directory, material_path, directory_length);
    directory[directory_length] = '\0';
    result = henka_assets_resolve_path(directory, uri, out_path);
    henka_free(directory);
    return result;
}

static henka_result henka_assets_resolve_gltf_material_texture(
    henka_asset_manager* manager,
    const char* material_path,
    const char* uri,
    henka_texture_descriptor descriptor,
    henka_texture** out_texture)
{
    char* resolved_path = NULL;
    henka_result result;

    if (out_texture != NULL) *out_texture = NULL;
    if (uri == NULL) return HENKA_SUCCESS;
    result = henka_assets_resolve_material_uri(material_path, uri, &resolved_path);
    if (result == HENKA_SUCCESS)
    {
        result = henka_assets_load_texture_with_descriptor(manager, resolved_path, &descriptor, out_texture);
    }
    henka_free(resolved_path);
    return result;
}

static henka_result henka_assets_create_embedded_texture(
    henka_engine* engine,
    const unsigned char* data,
    size_t data_size,
    const henka_texture_descriptor* descriptor,
    henka_texture** out_texture)
{
    static const unsigned char ktx2_identifier[12] =
        {0xABU, 0x4BU, 0x54U, 0x58U, 0x20U, 0x32U, 0x30U, 0xBBU, 0x0DU, 0x0AU, 0x1AU, 0x0AU};
    int width;
    int height;
    int source_channels;
    int channels;
    size_t decoded_bytes;
    float* hdr_pixels;
    stbi_uc* pixels;
    henka_texture_descriptor hdr_descriptor;
    henka_result result;

    if (out_texture != NULL) *out_texture = NULL;
    if (engine == NULL || data == NULL || data_size == 0U || data_size > HENKA_MAX_TEXTURE_ENCODED_BYTES ||
        data_size > (size_t)INT_MAX || descriptor == NULL || out_texture == NULL ||
        henka_texture_descriptor_validate(descriptor) != HENKA_SUCCESS) return HENKA_ERROR_ASSET_SOURCE;
    if (data_size >= sizeof(ktx2_identifier) && memcmp(data, ktx2_identifier, sizeof(ktx2_identifier)) == 0)
        return henka_texture_create_from_ktx2_memory(engine, data, data_size, descriptor, out_texture);
    if (!stbi_info_from_memory(data, (int)data_size, &width, &height, &source_channels) || width <= 0 || height <= 0)
        return HENKA_ERROR_ASSET_SOURCE;
    if (stbi_is_hdr_from_memory(data, (int)data_size))
    {
        if (!henka_checked_size_multiply((size_t)width, (size_t)height, &decoded_bytes) ||
            !henka_checked_size_multiply(decoded_bytes, 4U * sizeof(float), &decoded_bytes)) return HENKA_ERROR_ASSET_SOURCE;
        hdr_pixels = stbi_loadf_from_memory(data, (int)data_size, &width, &height, &channels, STBI_rgb_alpha);
        if (hdr_pixels == NULL || channels != source_channels ||
            !henka_checked_size_multiply((size_t)width, (size_t)height, &decoded_bytes) ||
            !henka_checked_size_multiply(decoded_bytes, 4U * sizeof(float), &decoded_bytes))
        {
            stbi_image_free(hdr_pixels);
            return HENKA_ERROR_ASSET_SOURCE;
        }
        for (size_t index = 0U; index < decoded_bytes / sizeof(float); ++index)
            if (!isfinite(hdr_pixels[index]) || hdr_pixels[index] < 0.0f)
            {
                stbi_image_free(hdr_pixels);
                return HENKA_ERROR_ASSET_SOURCE;
            }
        hdr_descriptor = *descriptor;
        hdr_descriptor.color_space = HENKA_TEXTURE_COLOR_SPACE_LINEAR;
        result = henka_texture_create_from_rgba32f_with_descriptor(
            engine, width, height, hdr_pixels, &hdr_descriptor, out_texture);
        stbi_image_free(hdr_pixels);
        if (result == HENKA_SUCCESS)
        {
            (*out_texture)->source_byte_size = data_size;
            (*out_texture)->original_channel_count = source_channels;
            (*out_texture)->source_class = HENKA_TEXTURE_SOURCE_CLASS_HDR;
            (*out_texture)->descriptor = hdr_descriptor;
            (*out_texture)->content_revision = 1U;
        }
        return result;
    }
    if (stbi_is_16_bit_from_memory(data, (int)data_size) || !henka_checked_rgba8_size(width, height, &decoded_bytes))
        return HENKA_ERROR_ASSET_SOURCE;
    pixels = stbi_load_from_memory(data, (int)data_size, &width, &height, &channels, STBI_rgb_alpha);
    if (pixels == NULL || channels != source_channels || !henka_checked_rgba8_size(width, height, &decoded_bytes))
    {
        stbi_image_free(pixels);
        return HENKA_ERROR_ASSET_SOURCE;
    }
    result = henka_texture_create_from_rgba8_with_descriptor(
        engine, width, height, pixels, descriptor, out_texture);
    stbi_image_free(pixels);
    if (result == HENKA_SUCCESS)
    {
        (*out_texture)->source_byte_size = data_size;
        (*out_texture)->original_channel_count = source_channels;
    }
    return result;
}

static uint32_t henka_assets_embedded_texture_hash(
    const unsigned char* data,
    size_t data_size)
{
    uint32_t hash = 2166136261U;
    size_t index;
    for (index = 0U; index < data_size; ++index)
    {
        hash ^= data[index];
        hash *= 16777619U;
    }
    return hash;
}

static henka_result henka_assets_load_embedded_texture(
    henka_asset_manager* manager,
    const char* source_path,
    const unsigned char* data,
    size_t data_size,
    const henka_texture_descriptor* descriptor,
    henka_texture** out_texture)
{
    char identity[HENKA_MAX_ASSET_PATH_BYTES];
    char* key = NULL;
    char* normalized_source = NULL;
    char* display_name = NULL;
    henka_asset_texture_entry* existing;
    henka_texture* texture = NULL;
    bool fallback = false;
    int written;
    henka_result result;

    if (out_texture != NULL) *out_texture = NULL;
    if (manager == NULL || source_path == NULL || data == NULL || data_size == 0U || descriptor == NULL || out_texture == NULL)
        return HENKA_ERROR_INVALID_ARGUMENT;
    written = snprintf(identity, sizeof(identity), "%s#embedded-%08x", source_path,
        (unsigned int)henka_assets_embedded_texture_hash(data, data_size));
    if (written < 0 || (size_t)written >= sizeof(identity)) return HENKA_ERROR_INVALID_ARGUMENT;
    result = henka_assets_make_texture_cache_key(identity, descriptor, &key);
    if (result != HENKA_SUCCESS) return result;
    existing = henka_asset_manager_find_texture_entry(manager, key);
    if (existing != NULL)
    {
        *out_texture = existing->texture;
        henka_free(key);
        return HENKA_SUCCESS;
    }
    result = henka_assets_normalize_source_path(identity, &normalized_source);
    if (result != HENKA_SUCCESS) { henka_free(key); return result; }
    result = henka_assets_create_embedded_texture(manager->engine, data, data_size, descriptor, &texture);
    if (result == HENKA_ERROR_ASSET_SOURCE)
    {
        result = henka_texture_create_borrowed_alias(
            henka_asset_manager_get_texture_fallback(manager, descriptor->usage), &texture);
        fallback = true;
    }
    if (result != HENKA_SUCCESS)
    {
        henka_free(key);
        henka_free(normalized_source);
        return result;
    }
    display_name = henka_asset_copy_display_name(normalized_source);
    if (display_name == NULL)
    {
        henka_texture_destroy(texture);
        henka_free(key);
        henka_free(normalized_source);
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    if (manager->texture_count == manager->texture_capacity)
    {
        result = henka_asset_manager_grow_textures(manager);
        if (result != HENKA_SUCCESS)
        {
            henka_texture_destroy(texture);
            henka_free(display_name);
            henka_free(key);
            henka_free(normalized_source);
            return result;
        }
    }
    texture->asset_manager_owned = true;
    texture->fallback_alias = fallback;
    manager->texture_entries[manager->texture_count].key = key;
    manager->texture_entries[manager->texture_count].source_path = normalized_source;
    manager->texture_entries[manager->texture_count].display_name = display_name;
    manager->texture_entries[manager->texture_count].texture = texture;
    manager->texture_entries[manager->texture_count].owns_texture = true;
    manager->texture_entries[manager->texture_count].descriptor = *descriptor;
    manager->texture_entries[manager->texture_count].metadata.type = HENKA_ASSET_TYPE_TEXTURE;
    manager->texture_entries[manager->texture_count].metadata.source_path = normalized_source;
    manager->texture_entries[manager->texture_count].metadata.display_name = display_name;
    manager->texture_entries[manager->texture_count].metadata.loaded = !fallback;
    manager->texture_entries[manager->texture_count].metadata.fallback = fallback;
    manager->texture_entries[manager->texture_count].metadata.reload_supported = fallback;
    manager->texture_entries[manager->texture_count].metadata.has_texture_descriptor = true;
    manager->texture_entries[manager->texture_count].metadata.texture_descriptor = *descriptor;
    henka_asset_set_summary(&manager->texture_entries[manager->texture_count].metadata,
        fallback ? "Embedded image fallback is active." : "Embedded glTF image is manager-owned and cached by content identity.",
        fallback ? "Embedded image decode failed; the semantic fallback remains active." : "");
    manager->texture_count += 1U;
    *out_texture = texture;
    return HENKA_SUCCESS;
}

static henka_result henka_assets_resolve_gltf_material_texture_source(
    henka_asset_manager* manager,
    const char* source_path,
    const char* uri,
    const unsigned char* embedded_data,
    size_t embedded_size,
    henka_texture_descriptor descriptor,
    henka_texture** out_texture)
{
    if (embedded_data != NULL && embedded_size > 0U)
        return henka_assets_load_embedded_texture(
            manager, source_path, embedded_data, embedded_size, &descriptor, out_texture);
    return henka_assets_resolve_gltf_material_texture(
        manager, source_path, uri, descriptor, out_texture);
}

static henka_result henka_assets_resolve_gltf_material_source(
    henka_asset_manager* manager,
    const char* source_path,
    henka_shader* shader,
    const henka_model_material_source* source,
    henka_material* out_material)
{
    henka_material candidate;
    henka_texture_descriptor descriptor;
    henka_result result;

    if (out_material != NULL) *out_material = henka_material_default();
    if (manager == NULL || source_path == NULL || shader == NULL || source == NULL || out_material == NULL)
        return HENKA_ERROR_INVALID_ARGUMENT;
    candidate = source->material;
    candidate.shader = shader;
    candidate.name = "glTF Material";
    candidate.base_color_texture = NULL;
    candidate.normal_texture = NULL;
    candidate.metallic_roughness_texture = NULL;
    candidate.occlusion_texture = NULL;
    candidate.emissive_texture = NULL;
    candidate.use_texture = false;

    descriptor = henka_texture_descriptor_default_color();
    result = henka_assets_resolve_gltf_material_texture_source(
        manager, source_path, source->base_color_uri, source->base_color_embedded_data,
        source->base_color_embedded_size, descriptor, &candidate.base_color_texture);
    if (result == HENKA_SUCCESS && candidate.base_color_texture != NULL) candidate.use_texture = true;
    descriptor = henka_texture_descriptor_default_normal();
    if (result == HENKA_SUCCESS) result = henka_assets_resolve_gltf_material_texture_source(
        manager, source_path, source->normal_uri, source->normal_embedded_data,
        source->normal_embedded_size, descriptor, &candidate.normal_texture);
    descriptor = henka_texture_descriptor_default_data();
    descriptor.usage = HENKA_TEXTURE_USAGE_METALLIC_ROUGHNESS;
    if (result == HENKA_SUCCESS) result = henka_assets_resolve_gltf_material_texture_source(
        manager, source_path, source->metallic_roughness_uri, source->metallic_roughness_embedded_data,
        source->metallic_roughness_embedded_size, descriptor, &candidate.metallic_roughness_texture);
    descriptor = henka_texture_descriptor_default_data();
    descriptor.usage = HENKA_TEXTURE_USAGE_OCCLUSION;
    if (result == HENKA_SUCCESS) result = henka_assets_resolve_gltf_material_texture_source(
        manager, source_path, source->occlusion_uri, source->occlusion_embedded_data,
        source->occlusion_embedded_size, descriptor, &candidate.occlusion_texture);
    descriptor = henka_texture_descriptor_default_color();
    descriptor.usage = HENKA_TEXTURE_USAGE_EMISSIVE;
    if (result == HENKA_SUCCESS) result = henka_assets_resolve_gltf_material_texture_source(
        manager, source_path, source->emissive_uri, source->emissive_embedded_data,
        source->emissive_embedded_size, descriptor, &candidate.emissive_texture);
    if (result == HENKA_SUCCESS) result = henka_material_validate(&candidate);
    if (result == HENKA_SUCCESS) *out_material = candidate;
    return result;
}

static henka_result henka_assets_build_gltf_material_instance(
    henka_asset_manager* manager,
    const char* path,
    henka_shader* shader,
    henka_mesh** out_mesh,
    henka_material* out_material)
{
    char* source_path = NULL;
    char* resolved_path = NULL;
    henka_model_data model;
    henka_material candidate;
    henka_result result;

    if (out_mesh != NULL) *out_mesh = NULL;
    if (out_material != NULL) *out_material = henka_material_default();
    if (manager == NULL || path == NULL || shader == NULL || out_mesh == NULL || out_material == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    memset(&model, 0, sizeof(model));
    result = henka_assets_normalize_source_path(path, &source_path);
    if (result != HENKA_SUCCESS) return result;
    result = henka_assets_resolve_path(
        henka_engine_get_asset_base_path(manager->engine), source_path, &resolved_path);
    if (result == HENKA_SUCCESS) result = henka_model_data_load_gltf(resolved_path, &model);
    henka_free(resolved_path);
    if (result != HENKA_SUCCESS)
    {
        henka_free(source_path);
        return result;
    }

    result = henka_assets_load_gltf_mesh(manager, source_path, out_mesh);
    if (result != HENKA_SUCCESS)
    {
        henka_model_data_destroy(&model);
        henka_free(source_path);
        return result;
    }

    if (model.has_material) result = henka_assets_resolve_gltf_material_source(
        manager, source_path, shader, &model.material_source, &candidate);
    else
    {
        candidate = henka_material_default();
        candidate.shader = shader;
        result = henka_material_validate(&candidate);
    }
    if (result == HENKA_SUCCESS) *out_material = candidate;
    else *out_mesh = NULL;
    henka_model_data_destroy(&model);
    henka_free(source_path);
    return result;
}

henka_result henka_assets_load_gltf_material_asset(
    henka_asset_manager* manager,
    const char* path,
    henka_shader* shader,
    henka_material_asset** out_asset)
{
    char* key = NULL;
    char* source_path = NULL;
    char* display_name = NULL;
    henka_material_asset* asset = NULL;
    henka_material candidate;
    henka_mesh* mesh = NULL;
    henka_result result;

    if (out_asset != NULL) *out_asset = NULL;
    if (manager == NULL || path == NULL || shader == NULL || out_asset == NULL)
        return HENKA_ERROR_INVALID_ARGUMENT;
    result = henka_assets_make_canonical_key(path, &key);
    if (result != HENKA_SUCCESS) return result;
    result = henka_assets_normalize_source_path(path, &source_path);
    if (result != HENKA_SUCCESS) { henka_free(key); return result; }
    asset = henka_asset_manager_find_material_entry(manager, key);
    if (asset != NULL)
    {
        *out_asset = asset;
        henka_free(key);
        henka_free(source_path);
        return HENKA_SUCCESS;
    }

    result = henka_assets_build_gltf_material_instance(manager, source_path, shader, &mesh, &candidate);
    if (result != HENKA_SUCCESS)
    {
        henka_free(key);
        henka_free(source_path);
        return result;
    }
    display_name = henka_asset_copy_display_name(source_path);
    if (display_name == NULL)
    {
        henka_free(key);
        henka_free(source_path);
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    asset = henka_calloc(1U, sizeof(*asset));
    if (asset == NULL)
    {
        henka_free(display_name);
        henka_free(key);
        henka_free(source_path);
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    if (manager->material_count == manager->material_capacity)
    {
        result = henka_asset_manager_grow_materials(manager);
        if (result != HENKA_SUCCESS)
        {
            henka_free(asset);
            henka_free(display_name);
            henka_free(key);
            henka_free(source_path);
            return result;
        }
    }
    asset->key = key;
    asset->source_path = source_path;
    asset->display_name = display_name;
    asset->material = candidate;
    asset->revision = 1U;
    asset->metadata.type = HENKA_ASSET_TYPE_MATERIAL;
    asset->metadata.source_path = asset->source_path;
    asset->metadata.display_name = asset->display_name;
    asset->metadata.loaded = true;
    asset->metadata.fallback = false;
    asset->metadata.reload_supported = true;
    henka_asset_set_summary(&asset->metadata,
        "glTF material asset loaded through the shared material and texture dependency path.",
        "");
    manager->material_entries[manager->material_count++] = asset;
    *out_asset = asset;
    return HENKA_SUCCESS;
}

henka_result henka_assets_get_material_asset_material(
    const henka_material_asset* asset,
    henka_material* out_material)
{
    if (out_material != NULL) *out_material = henka_material_default();
    if (asset == NULL || out_material == NULL) return HENKA_ERROR_INVALID_ARGUMENT;
    *out_material = asset->material;
    return HENKA_SUCCESS;
}

enum
{
    HENKA_MATERIAL_OVERRIDE_METALLIC = 1U << 0,
    HENKA_MATERIAL_OVERRIDE_ROUGHNESS = 1U << 1,
    HENKA_MATERIAL_OVERRIDE_SPECULAR_FACTOR = 1U << 2,
    HENKA_MATERIAL_OVERRIDE_IOR = 1U << 3,
    HENKA_MATERIAL_OVERRIDE_TRANSMISSION = 1U << 4,
    HENKA_MATERIAL_OVERRIDE_NORMAL_SCALE = 1U << 5,
    HENKA_MATERIAL_OVERRIDE_OCCLUSION_STRENGTH = 1U << 6,
    HENKA_MATERIAL_OVERRIDE_EMISSIVE_STRENGTH = 1U << 7,
    HENKA_MATERIAL_OVERRIDE_CLEARCOAT = 1U << 8,
    HENKA_MATERIAL_OVERRIDE_CLEARCOAT_ROUGHNESS = 1U << 9,
    HENKA_MATERIAL_OVERRIDE_ALPHA_CUTOFF = 1U << 10,
    HENKA_MATERIAL_OVERRIDE_SHEEN_ROUGHNESS = 1U << 11,
    HENKA_MATERIAL_OVERRIDE_BASE_COLOR = 1U << 12,
    HENKA_MATERIAL_OVERRIDE_EMISSIVE_COLOR = 1U << 13,
    HENKA_MATERIAL_OVERRIDE_SPECULAR_COLOR = 1U << 14,
    HENKA_MATERIAL_OVERRIDE_SHEEN_COLOR = 1U << 15,
    HENKA_MATERIAL_OVERRIDE_USE_LIGHTING = 1U << 16,
    HENKA_MATERIAL_OVERRIDE_DEPTH_TEST = 1U << 17,
    HENKA_MATERIAL_OVERRIDE_DOUBLE_SIDED = 1U << 18,
    HENKA_MATERIAL_OVERRIDE_CAST_SHADOWS = 1U << 19,
    HENKA_MATERIAL_OVERRIDE_RECEIVE_SHADOWS = 1U << 20,
    HENKA_MATERIAL_OVERRIDE_ALPHA_MODE = 1U << 21,
    HENKA_MATERIAL_OVERRIDE_THICKNESS = 1U << 22,
    HENKA_MATERIAL_OVERRIDE_ATTENUATION_DISTANCE = 1U << 23,
    HENKA_MATERIAL_OVERRIDE_ATTENUATION_COLOR = 1U << 24,
    HENKA_MATERIAL_OVERRIDE_BASE_COLOR_TEXTURE = 1U << 25,
    HENKA_MATERIAL_OVERRIDE_NORMAL_TEXTURE = 1U << 26,
    HENKA_MATERIAL_OVERRIDE_METALLIC_ROUGHNESS_TEXTURE = 1U << 27,
    HENKA_MATERIAL_OVERRIDE_OCCLUSION_TEXTURE = 1U << 28,
    HENKA_MATERIAL_OVERRIDE_EMISSIVE_TEXTURE = 1U << 29
};

static uint32_t henka_material_instance_override_bit(
    henka_material_instance_parameter parameter)
{
    static const uint32_t bits[] =
    {
        HENKA_MATERIAL_OVERRIDE_METALLIC,
        HENKA_MATERIAL_OVERRIDE_ROUGHNESS,
        HENKA_MATERIAL_OVERRIDE_SPECULAR_FACTOR,
        HENKA_MATERIAL_OVERRIDE_IOR,
        HENKA_MATERIAL_OVERRIDE_TRANSMISSION,
        HENKA_MATERIAL_OVERRIDE_NORMAL_SCALE,
        HENKA_MATERIAL_OVERRIDE_OCCLUSION_STRENGTH,
        HENKA_MATERIAL_OVERRIDE_EMISSIVE_STRENGTH,
        HENKA_MATERIAL_OVERRIDE_CLEARCOAT,
        HENKA_MATERIAL_OVERRIDE_CLEARCOAT_ROUGHNESS,
        HENKA_MATERIAL_OVERRIDE_ALPHA_CUTOFF,
        HENKA_MATERIAL_OVERRIDE_SHEEN_ROUGHNESS,
        HENKA_MATERIAL_OVERRIDE_BASE_COLOR,
        HENKA_MATERIAL_OVERRIDE_EMISSIVE_COLOR,
        HENKA_MATERIAL_OVERRIDE_SPECULAR_COLOR,
        HENKA_MATERIAL_OVERRIDE_SHEEN_COLOR,
        HENKA_MATERIAL_OVERRIDE_USE_LIGHTING,
        HENKA_MATERIAL_OVERRIDE_DEPTH_TEST,
        HENKA_MATERIAL_OVERRIDE_DOUBLE_SIDED,
        HENKA_MATERIAL_OVERRIDE_CAST_SHADOWS,
        HENKA_MATERIAL_OVERRIDE_RECEIVE_SHADOWS,
        HENKA_MATERIAL_OVERRIDE_ALPHA_MODE,
        HENKA_MATERIAL_OVERRIDE_THICKNESS,
        HENKA_MATERIAL_OVERRIDE_ATTENUATION_DISTANCE,
        HENKA_MATERIAL_OVERRIDE_ATTENUATION_COLOR,
        HENKA_MATERIAL_OVERRIDE_BASE_COLOR_TEXTURE,
        HENKA_MATERIAL_OVERRIDE_NORMAL_TEXTURE,
        HENKA_MATERIAL_OVERRIDE_METALLIC_ROUGHNESS_TEXTURE,
        HENKA_MATERIAL_OVERRIDE_OCCLUSION_TEXTURE,
        HENKA_MATERIAL_OVERRIDE_EMISSIVE_TEXTURE
    };
    return parameter >= 0 &&
        (size_t)parameter < sizeof(bits) / sizeof(bits[0]) ? bits[parameter] : 0U;
}

henka_result henka_assets_get_material_asset_revision(
    const henka_material_asset* asset,
    uint64_t* out_revision)
{
    if (out_revision != NULL) *out_revision = 0U;
    if (asset == NULL || out_revision == NULL) return HENKA_ERROR_INVALID_ARGUMENT;
    *out_revision = asset->revision;
    return HENKA_SUCCESS;
}

henka_result henka_assets_get_material_asset_dependencies(
    const henka_material_asset* asset,
    henka_material_dependency_info* out_dependencies)
{
    const henka_material* material;

    if (out_dependencies != NULL) memset(out_dependencies, 0, sizeof(*out_dependencies));
    if (asset == NULL || out_dependencies == NULL) return HENKA_ERROR_INVALID_ARGUMENT;
    material = &asset->material;
    out_dependencies->definition_revision = asset->revision;
#define HENKA_ADD_MATERIAL_DEPENDENCY(slot_value, usage_value, texture_value) \
    do { \
        if ((texture_value) != NULL && out_dependencies->dependency_count < HENKA_MATERIAL_MAX_TEXTURE_DEPENDENCIES) { \
            henka_material_dependency* dependency = &out_dependencies->dependencies[out_dependencies->dependency_count++]; \
            dependency->slot = (slot_value); \
            dependency->usage = (usage_value); \
            dependency->texture = (texture_value); \
        } \
    } while (0)
    HENKA_ADD_MATERIAL_DEPENDENCY(HENKA_MATERIAL_TEXTURE_SLOT_BASE_COLOR, HENKA_TEXTURE_USAGE_COLOR, material->base_color_texture);
    HENKA_ADD_MATERIAL_DEPENDENCY(HENKA_MATERIAL_TEXTURE_SLOT_NORMAL, HENKA_TEXTURE_USAGE_NORMAL, material->normal_texture);
    HENKA_ADD_MATERIAL_DEPENDENCY(HENKA_MATERIAL_TEXTURE_SLOT_METALLIC_ROUGHNESS, HENKA_TEXTURE_USAGE_METALLIC_ROUGHNESS, material->metallic_roughness_texture);
    HENKA_ADD_MATERIAL_DEPENDENCY(HENKA_MATERIAL_TEXTURE_SLOT_OCCLUSION, HENKA_TEXTURE_USAGE_OCCLUSION, material->occlusion_texture);
    HENKA_ADD_MATERIAL_DEPENDENCY(HENKA_MATERIAL_TEXTURE_SLOT_EMISSIVE, HENKA_TEXTURE_USAGE_EMISSIVE, material->emissive_texture);
    {
        static const henka_material_texture_slot base_color_slots[HENKA_MATERIAL_TERRAIN_LAYER_COUNT] = {
            HENKA_MATERIAL_TEXTURE_SLOT_TERRAIN_LAYER0_BASE_COLOR,
            HENKA_MATERIAL_TEXTURE_SLOT_TERRAIN_LAYER1_BASE_COLOR,
            HENKA_MATERIAL_TEXTURE_SLOT_TERRAIN_LAYER2_BASE_COLOR,
            HENKA_MATERIAL_TEXTURE_SLOT_TERRAIN_LAYER3_BASE_COLOR};
        static const henka_material_texture_slot normal_slots[HENKA_MATERIAL_TERRAIN_LAYER_COUNT] = {
            HENKA_MATERIAL_TEXTURE_SLOT_TERRAIN_LAYER0_NORMAL,
            HENKA_MATERIAL_TEXTURE_SLOT_TERRAIN_LAYER1_NORMAL,
            HENKA_MATERIAL_TEXTURE_SLOT_TERRAIN_LAYER2_NORMAL,
            HENKA_MATERIAL_TEXTURE_SLOT_TERRAIN_LAYER3_NORMAL};
        static const henka_material_texture_slot metallic_roughness_slots[HENKA_MATERIAL_TERRAIN_LAYER_COUNT] = {
            HENKA_MATERIAL_TEXTURE_SLOT_TERRAIN_LAYER0_METALLIC_ROUGHNESS,
            HENKA_MATERIAL_TEXTURE_SLOT_TERRAIN_LAYER1_METALLIC_ROUGHNESS,
            HENKA_MATERIAL_TEXTURE_SLOT_TERRAIN_LAYER2_METALLIC_ROUGHNESS,
            HENKA_MATERIAL_TEXTURE_SLOT_TERRAIN_LAYER3_METALLIC_ROUGHNESS};
        size_t layer_index;
        for (layer_index = 0U; layer_index < HENKA_MATERIAL_TERRAIN_LAYER_COUNT; ++layer_index)
        {
            const henka_material_layer* layer = &material->terrain_layers[layer_index];
            HENKA_ADD_MATERIAL_DEPENDENCY(
                base_color_slots[layer_index], HENKA_TEXTURE_USAGE_COLOR, layer->base_color_texture);
            HENKA_ADD_MATERIAL_DEPENDENCY(
                normal_slots[layer_index], HENKA_TEXTURE_USAGE_NORMAL, layer->normal_texture);
            HENKA_ADD_MATERIAL_DEPENDENCY(
                metallic_roughness_slots[layer_index],
                HENKA_TEXTURE_USAGE_METALLIC_ROUGHNESS,
                layer->metallic_roughness_texture);
        }
    }
#undef HENKA_ADD_MATERIAL_DEPENDENCY
    return HENKA_SUCCESS;
}

henka_result henka_assets_get_material_instance_dependencies(
    const henka_material_instance* instance,
    henka_material_dependency_info* out_dependencies)
{
    const henka_material* material;

    if (out_dependencies != NULL) memset(out_dependencies, 0, sizeof(*out_dependencies));
    if (instance == NULL || instance->definition == NULL || out_dependencies == NULL ||
        henka_material_validate(&instance->material) != HENKA_SUCCESS)
        return HENKA_ERROR_INVALID_ARGUMENT;
    material = &instance->material;
    out_dependencies->definition_revision = instance->definition_revision;
#define HENKA_ADD_INSTANCE_DEPENDENCY(slot_value, usage_value, texture_value) \
    do { \
        if ((texture_value) != NULL && out_dependencies->dependency_count < HENKA_MATERIAL_MAX_TEXTURE_DEPENDENCIES) { \
            henka_material_dependency* dependency = &out_dependencies->dependencies[out_dependencies->dependency_count++]; \
            dependency->slot = (slot_value); \
            dependency->usage = (usage_value); \
            dependency->texture = (texture_value); \
        } \
    } while (0)
    HENKA_ADD_INSTANCE_DEPENDENCY(HENKA_MATERIAL_TEXTURE_SLOT_BASE_COLOR, HENKA_TEXTURE_USAGE_COLOR, material->base_color_texture);
    HENKA_ADD_INSTANCE_DEPENDENCY(HENKA_MATERIAL_TEXTURE_SLOT_NORMAL, HENKA_TEXTURE_USAGE_NORMAL, material->normal_texture);
    HENKA_ADD_INSTANCE_DEPENDENCY(HENKA_MATERIAL_TEXTURE_SLOT_METALLIC_ROUGHNESS, HENKA_TEXTURE_USAGE_METALLIC_ROUGHNESS, material->metallic_roughness_texture);
    HENKA_ADD_INSTANCE_DEPENDENCY(HENKA_MATERIAL_TEXTURE_SLOT_OCCLUSION, HENKA_TEXTURE_USAGE_OCCLUSION, material->occlusion_texture);
    HENKA_ADD_INSTANCE_DEPENDENCY(HENKA_MATERIAL_TEXTURE_SLOT_EMISSIVE, HENKA_TEXTURE_USAGE_EMISSIVE, material->emissive_texture);
    {
        static const henka_material_texture_slot base_color_slots[HENKA_MATERIAL_TERRAIN_LAYER_COUNT] = {
            HENKA_MATERIAL_TEXTURE_SLOT_TERRAIN_LAYER0_BASE_COLOR,
            HENKA_MATERIAL_TEXTURE_SLOT_TERRAIN_LAYER1_BASE_COLOR,
            HENKA_MATERIAL_TEXTURE_SLOT_TERRAIN_LAYER2_BASE_COLOR,
            HENKA_MATERIAL_TEXTURE_SLOT_TERRAIN_LAYER3_BASE_COLOR};
        static const henka_material_texture_slot normal_slots[HENKA_MATERIAL_TERRAIN_LAYER_COUNT] = {
            HENKA_MATERIAL_TEXTURE_SLOT_TERRAIN_LAYER0_NORMAL,
            HENKA_MATERIAL_TEXTURE_SLOT_TERRAIN_LAYER1_NORMAL,
            HENKA_MATERIAL_TEXTURE_SLOT_TERRAIN_LAYER2_NORMAL,
            HENKA_MATERIAL_TEXTURE_SLOT_TERRAIN_LAYER3_NORMAL};
        static const henka_material_texture_slot metallic_roughness_slots[HENKA_MATERIAL_TERRAIN_LAYER_COUNT] = {
            HENKA_MATERIAL_TEXTURE_SLOT_TERRAIN_LAYER0_METALLIC_ROUGHNESS,
            HENKA_MATERIAL_TEXTURE_SLOT_TERRAIN_LAYER1_METALLIC_ROUGHNESS,
            HENKA_MATERIAL_TEXTURE_SLOT_TERRAIN_LAYER2_METALLIC_ROUGHNESS,
            HENKA_MATERIAL_TEXTURE_SLOT_TERRAIN_LAYER3_METALLIC_ROUGHNESS};
        size_t layer_index;
        for (layer_index = 0U; layer_index < HENKA_MATERIAL_TERRAIN_LAYER_COUNT; ++layer_index)
        {
            const henka_material_layer* layer = &material->terrain_layers[layer_index];
            HENKA_ADD_INSTANCE_DEPENDENCY(
                base_color_slots[layer_index], HENKA_TEXTURE_USAGE_COLOR, layer->base_color_texture);
            HENKA_ADD_INSTANCE_DEPENDENCY(
                normal_slots[layer_index], HENKA_TEXTURE_USAGE_NORMAL, layer->normal_texture);
            HENKA_ADD_INSTANCE_DEPENDENCY(
                metallic_roughness_slots[layer_index],
                HENKA_TEXTURE_USAGE_METALLIC_ROUGHNESS,
                layer->metallic_roughness_texture);
        }
    }
#undef HENKA_ADD_INSTANCE_DEPENDENCY
    return HENKA_SUCCESS;
}

henka_result henka_assets_create_material_instance(
    const henka_material_asset* asset,
    henka_material_instance* out_instance)
{
    if (out_instance != NULL) memset(out_instance, 0, sizeof(*out_instance));
    if (asset == NULL || out_instance == NULL ||
        henka_material_validate(&asset->material) != HENKA_SUCCESS)
        return HENKA_ERROR_INVALID_ARGUMENT;
    out_instance->definition = asset;
    out_instance->material = asset->material;
    out_instance->definition_revision = asset->revision;
    return HENKA_SUCCESS;
}

henka_result henka_assets_refresh_material_instance(
    henka_material_instance* instance)
{
    henka_material candidate;
    const henka_material* previous;

    if (instance == NULL || instance->definition == NULL) return HENKA_ERROR_INVALID_ARGUMENT;
    candidate = instance->definition->material;
    previous = &instance->material;
#define HENKA_PRESERVE_MATERIAL_OVERRIDE(bit, field) \
    do { if ((instance->override_mask & (bit)) != 0U) candidate.field = previous->field; } while (0)
    HENKA_PRESERVE_MATERIAL_OVERRIDE(HENKA_MATERIAL_OVERRIDE_METALLIC, metallic);
    HENKA_PRESERVE_MATERIAL_OVERRIDE(HENKA_MATERIAL_OVERRIDE_ROUGHNESS, roughness);
    HENKA_PRESERVE_MATERIAL_OVERRIDE(HENKA_MATERIAL_OVERRIDE_SPECULAR_FACTOR, specular_factor);
    HENKA_PRESERVE_MATERIAL_OVERRIDE(HENKA_MATERIAL_OVERRIDE_IOR, ior);
    HENKA_PRESERVE_MATERIAL_OVERRIDE(HENKA_MATERIAL_OVERRIDE_TRANSMISSION, transmission);
    HENKA_PRESERVE_MATERIAL_OVERRIDE(HENKA_MATERIAL_OVERRIDE_NORMAL_SCALE, normal_scale);
    HENKA_PRESERVE_MATERIAL_OVERRIDE(HENKA_MATERIAL_OVERRIDE_OCCLUSION_STRENGTH, occlusion_strength);
    HENKA_PRESERVE_MATERIAL_OVERRIDE(HENKA_MATERIAL_OVERRIDE_EMISSIVE_STRENGTH, emissive_strength);
    HENKA_PRESERVE_MATERIAL_OVERRIDE(HENKA_MATERIAL_OVERRIDE_CLEARCOAT, clearcoat);
    HENKA_PRESERVE_MATERIAL_OVERRIDE(HENKA_MATERIAL_OVERRIDE_CLEARCOAT_ROUGHNESS, clearcoat_roughness);
    HENKA_PRESERVE_MATERIAL_OVERRIDE(HENKA_MATERIAL_OVERRIDE_ALPHA_CUTOFF, alpha_cutoff);
    HENKA_PRESERVE_MATERIAL_OVERRIDE(HENKA_MATERIAL_OVERRIDE_SHEEN_ROUGHNESS, sheen_roughness);
    HENKA_PRESERVE_MATERIAL_OVERRIDE(HENKA_MATERIAL_OVERRIDE_BASE_COLOR, base_color);
    HENKA_PRESERVE_MATERIAL_OVERRIDE(HENKA_MATERIAL_OVERRIDE_EMISSIVE_COLOR, emissive_color);
    HENKA_PRESERVE_MATERIAL_OVERRIDE(HENKA_MATERIAL_OVERRIDE_SPECULAR_COLOR, specular_color);
    HENKA_PRESERVE_MATERIAL_OVERRIDE(HENKA_MATERIAL_OVERRIDE_SHEEN_COLOR, sheen_color);
    HENKA_PRESERVE_MATERIAL_OVERRIDE(HENKA_MATERIAL_OVERRIDE_USE_LIGHTING, use_lighting);
    HENKA_PRESERVE_MATERIAL_OVERRIDE(HENKA_MATERIAL_OVERRIDE_DEPTH_TEST, depth_test);
    HENKA_PRESERVE_MATERIAL_OVERRIDE(HENKA_MATERIAL_OVERRIDE_DOUBLE_SIDED, double_sided);
    HENKA_PRESERVE_MATERIAL_OVERRIDE(HENKA_MATERIAL_OVERRIDE_CAST_SHADOWS, cast_shadows);
    HENKA_PRESERVE_MATERIAL_OVERRIDE(HENKA_MATERIAL_OVERRIDE_RECEIVE_SHADOWS, receive_shadows);
    HENKA_PRESERVE_MATERIAL_OVERRIDE(HENKA_MATERIAL_OVERRIDE_ALPHA_MODE, alpha_mode);
    HENKA_PRESERVE_MATERIAL_OVERRIDE(HENKA_MATERIAL_OVERRIDE_THICKNESS, thickness);
    HENKA_PRESERVE_MATERIAL_OVERRIDE(HENKA_MATERIAL_OVERRIDE_ATTENUATION_DISTANCE, attenuation_distance);
    HENKA_PRESERVE_MATERIAL_OVERRIDE(HENKA_MATERIAL_OVERRIDE_ATTENUATION_COLOR, attenuation_color);
    HENKA_PRESERVE_MATERIAL_OVERRIDE(HENKA_MATERIAL_OVERRIDE_NORMAL_TEXTURE, normal_texture);
    HENKA_PRESERVE_MATERIAL_OVERRIDE(HENKA_MATERIAL_OVERRIDE_METALLIC_ROUGHNESS_TEXTURE, metallic_roughness_texture);
    HENKA_PRESERVE_MATERIAL_OVERRIDE(HENKA_MATERIAL_OVERRIDE_OCCLUSION_TEXTURE, occlusion_texture);
    HENKA_PRESERVE_MATERIAL_OVERRIDE(HENKA_MATERIAL_OVERRIDE_EMISSIVE_TEXTURE, emissive_texture);
    if ((instance->override_mask & HENKA_MATERIAL_OVERRIDE_BASE_COLOR_TEXTURE) != 0U)
    {
        candidate.base_color_texture = previous->base_color_texture;
        candidate.use_texture = previous->use_texture;
    }
#undef HENKA_PRESERVE_MATERIAL_OVERRIDE
    if (henka_material_validate(&candidate) != HENKA_SUCCESS) return HENKA_ERROR_ASSET_SOURCE;
    instance->material = candidate;
    instance->definition_revision = instance->definition->revision;
    return HENKA_SUCCESS;
}

henka_result henka_assets_get_material_instance_material(
    const henka_material_instance* instance,
    henka_material* out_material)
{
    if (out_material != NULL) *out_material = henka_material_default();
    if (instance == NULL || out_material == NULL ||
        henka_material_validate(&instance->material) != HENKA_SUCCESS)
        return HENKA_ERROR_INVALID_ARGUMENT;
    *out_material = instance->material;
    return HENKA_SUCCESS;
}

henka_result henka_assets_apply_material_instance_to_entity(
    const henka_material_instance* instance,
    henka_scene* scene,
    henka_entity entity)
{
    henka_material material;
    henka_result result;

    if (instance == NULL || scene == NULL || entity == HENKA_INVALID_ENTITY ||
        henka_assets_get_material_instance_material(instance, &material) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_scene_set_entity_material(scene, entity, material);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    return henka_scene_set_entity_material_asset(
        scene, entity, instance->definition);
}

static henka_result henka_material_instance_commit(
    henka_material_instance* instance,
    henka_material candidate,
    henka_material_instance_parameter parameter)
{
    uint32_t bit;

    if (instance == NULL || instance->definition == NULL) return HENKA_ERROR_INVALID_ARGUMENT;
    if (henka_material_validate(&candidate) != HENKA_SUCCESS) return HENKA_ERROR_INVALID_ARGUMENT;
    bit = henka_material_instance_override_bit(parameter);
    if (bit == 0U) return HENKA_ERROR_INVALID_ARGUMENT;
    instance->material = candidate;
    instance->override_mask |= bit;
    instance->definition_revision = instance->definition->revision;
    return HENKA_SUCCESS;
}

static bool henka_material_instance_restore_definition_field(
    henka_material* candidate,
    const henka_material* definition,
    henka_material_instance_parameter parameter)
{
    if (candidate == NULL || definition == NULL)
        return false;
#define HENKA_RESTORE_MATERIAL_FIELD(parameter_value, field_value) \
    case parameter_value: candidate->field_value = definition->field_value; break
    switch (parameter)
    {
        HENKA_RESTORE_MATERIAL_FIELD(HENKA_MATERIAL_INSTANCE_METALLIC, metallic);
        HENKA_RESTORE_MATERIAL_FIELD(HENKA_MATERIAL_INSTANCE_ROUGHNESS, roughness);
        HENKA_RESTORE_MATERIAL_FIELD(HENKA_MATERIAL_INSTANCE_SPECULAR_FACTOR, specular_factor);
        HENKA_RESTORE_MATERIAL_FIELD(HENKA_MATERIAL_INSTANCE_IOR, ior);
        HENKA_RESTORE_MATERIAL_FIELD(HENKA_MATERIAL_INSTANCE_TRANSMISSION, transmission);
        HENKA_RESTORE_MATERIAL_FIELD(HENKA_MATERIAL_INSTANCE_NORMAL_SCALE, normal_scale);
        HENKA_RESTORE_MATERIAL_FIELD(HENKA_MATERIAL_INSTANCE_OCCLUSION_STRENGTH, occlusion_strength);
        HENKA_RESTORE_MATERIAL_FIELD(HENKA_MATERIAL_INSTANCE_EMISSIVE_STRENGTH, emissive_strength);
        HENKA_RESTORE_MATERIAL_FIELD(HENKA_MATERIAL_INSTANCE_CLEARCOAT, clearcoat);
        HENKA_RESTORE_MATERIAL_FIELD(HENKA_MATERIAL_INSTANCE_CLEARCOAT_ROUGHNESS, clearcoat_roughness);
        HENKA_RESTORE_MATERIAL_FIELD(HENKA_MATERIAL_INSTANCE_ALPHA_CUTOFF, alpha_cutoff);
        HENKA_RESTORE_MATERIAL_FIELD(HENKA_MATERIAL_INSTANCE_SHEEN_ROUGHNESS, sheen_roughness);
        HENKA_RESTORE_MATERIAL_FIELD(HENKA_MATERIAL_INSTANCE_BASE_COLOR, base_color);
        HENKA_RESTORE_MATERIAL_FIELD(HENKA_MATERIAL_INSTANCE_EMISSIVE_COLOR, emissive_color);
        HENKA_RESTORE_MATERIAL_FIELD(HENKA_MATERIAL_INSTANCE_SPECULAR_COLOR, specular_color);
        HENKA_RESTORE_MATERIAL_FIELD(HENKA_MATERIAL_INSTANCE_SHEEN_COLOR, sheen_color);
        HENKA_RESTORE_MATERIAL_FIELD(HENKA_MATERIAL_INSTANCE_USE_LIGHTING, use_lighting);
        HENKA_RESTORE_MATERIAL_FIELD(HENKA_MATERIAL_INSTANCE_DEPTH_TEST, depth_test);
        HENKA_RESTORE_MATERIAL_FIELD(HENKA_MATERIAL_INSTANCE_DOUBLE_SIDED, double_sided);
        HENKA_RESTORE_MATERIAL_FIELD(HENKA_MATERIAL_INSTANCE_CAST_SHADOWS, cast_shadows);
        HENKA_RESTORE_MATERIAL_FIELD(HENKA_MATERIAL_INSTANCE_RECEIVE_SHADOWS, receive_shadows);
        HENKA_RESTORE_MATERIAL_FIELD(HENKA_MATERIAL_INSTANCE_ALPHA_MODE, alpha_mode);
        HENKA_RESTORE_MATERIAL_FIELD(HENKA_MATERIAL_INSTANCE_THICKNESS, thickness);
        HENKA_RESTORE_MATERIAL_FIELD(HENKA_MATERIAL_INSTANCE_ATTENUATION_DISTANCE, attenuation_distance);
        HENKA_RESTORE_MATERIAL_FIELD(HENKA_MATERIAL_INSTANCE_ATTENUATION_COLOR, attenuation_color);
        case HENKA_MATERIAL_INSTANCE_BASE_COLOR_TEXTURE:
            candidate->base_color_texture = definition->base_color_texture;
            candidate->use_texture = definition->use_texture;
            break;
        HENKA_RESTORE_MATERIAL_FIELD(HENKA_MATERIAL_INSTANCE_NORMAL_TEXTURE, normal_texture);
        HENKA_RESTORE_MATERIAL_FIELD(HENKA_MATERIAL_INSTANCE_METALLIC_ROUGHNESS_TEXTURE, metallic_roughness_texture);
        HENKA_RESTORE_MATERIAL_FIELD(HENKA_MATERIAL_INSTANCE_OCCLUSION_TEXTURE, occlusion_texture);
        HENKA_RESTORE_MATERIAL_FIELD(HENKA_MATERIAL_INSTANCE_EMISSIVE_TEXTURE, emissive_texture);
        default:
            return false;
    }
#undef HENKA_RESTORE_MATERIAL_FIELD
    return true;
}

henka_result henka_assets_material_instance_reset_override(
    henka_material_instance* instance,
    henka_material_instance_parameter parameter)
{
    henka_material candidate;
    uint32_t bit;

    if (instance == NULL || instance->definition == NULL ||
        parameter < 0 || parameter >= HENKA_MATERIAL_INSTANCE_PARAMETER_COUNT)
        return HENKA_ERROR_INVALID_ARGUMENT;
    bit = henka_material_instance_override_bit(parameter);
    if (bit == 0U || (instance->override_mask & bit) == 0U)
        return HENKA_SUCCESS;
    candidate = instance->material;
    if (!henka_material_instance_restore_definition_field(
            &candidate, &instance->definition->material, parameter) ||
        henka_material_validate(&candidate) != HENKA_SUCCESS)
        return HENKA_ERROR_ASSET_SOURCE;
    instance->material = candidate;
    instance->override_mask &= ~bit;
    instance->definition_revision = instance->definition->revision;
    return HENKA_SUCCESS;
}

henka_result henka_assets_material_instance_reset_overrides(
    henka_material_instance* instance)
{
    henka_material candidate;

    if (instance == NULL || instance->definition == NULL)
        return HENKA_ERROR_INVALID_ARGUMENT;
    candidate = instance->definition->material;
    if (henka_material_validate(&candidate) != HENKA_SUCCESS)
        return HENKA_ERROR_ASSET_SOURCE;
    instance->material = candidate;
    instance->override_mask = 0U;
    instance->definition_revision = instance->definition->revision;
    return HENKA_SUCCESS;
}

henka_result henka_assets_material_instance_set_float(
    henka_material_instance* instance,
    henka_material_instance_parameter parameter,
    float value)
{
    henka_material candidate;
    if (instance == NULL || instance->definition == NULL || !isfinite(value)) return HENKA_ERROR_INVALID_ARGUMENT;
    candidate = instance->material;
    switch (parameter)
    {
        case HENKA_MATERIAL_INSTANCE_METALLIC: candidate.metallic = value; break;
        case HENKA_MATERIAL_INSTANCE_ROUGHNESS: candidate.roughness = value; break;
        case HENKA_MATERIAL_INSTANCE_SPECULAR_FACTOR: candidate.specular_factor = value; break;
        case HENKA_MATERIAL_INSTANCE_IOR: candidate.ior = value; break;
        case HENKA_MATERIAL_INSTANCE_TRANSMISSION: candidate.transmission = value; break;
        case HENKA_MATERIAL_INSTANCE_NORMAL_SCALE: candidate.normal_scale = value; break;
        case HENKA_MATERIAL_INSTANCE_OCCLUSION_STRENGTH: candidate.occlusion_strength = value; break;
        case HENKA_MATERIAL_INSTANCE_EMISSIVE_STRENGTH: candidate.emissive_strength = value; break;
        case HENKA_MATERIAL_INSTANCE_CLEARCOAT: candidate.clearcoat = value; break;
        case HENKA_MATERIAL_INSTANCE_CLEARCOAT_ROUGHNESS: candidate.clearcoat_roughness = value; break;
        case HENKA_MATERIAL_INSTANCE_ALPHA_CUTOFF: candidate.alpha_cutoff = value; break;
        case HENKA_MATERIAL_INSTANCE_SHEEN_ROUGHNESS: candidate.sheen_roughness = value; break;
        case HENKA_MATERIAL_INSTANCE_THICKNESS: candidate.thickness = value; break;
        case HENKA_MATERIAL_INSTANCE_ATTENUATION_DISTANCE: candidate.attenuation_distance = value; break;
        default: return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return henka_material_instance_commit(instance, candidate, parameter);
}

henka_result henka_assets_material_instance_set_vec3(
    henka_material_instance* instance,
    henka_material_instance_parameter parameter,
    henka_vec3 value)
{
    henka_material candidate;
    if (instance == NULL || instance->definition == NULL) return HENKA_ERROR_INVALID_ARGUMENT;
    candidate = instance->material;
    switch (parameter)
    {
        case HENKA_MATERIAL_INSTANCE_EMISSIVE_COLOR: candidate.emissive_color = value; break;
        case HENKA_MATERIAL_INSTANCE_SPECULAR_COLOR: candidate.specular_color = value; break;
        case HENKA_MATERIAL_INSTANCE_SHEEN_COLOR: candidate.sheen_color = value; break;
        case HENKA_MATERIAL_INSTANCE_ATTENUATION_COLOR: candidate.attenuation_color = value; break;
        default: return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return henka_material_instance_commit(instance, candidate, parameter);
}

henka_result henka_assets_material_instance_set_vec4(
    henka_material_instance* instance,
    henka_material_instance_parameter parameter,
    henka_vec4 value)
{
    henka_material candidate;
    if (instance == NULL || instance->definition == NULL) return HENKA_ERROR_INVALID_ARGUMENT;
    candidate = instance->material;
    if (parameter != HENKA_MATERIAL_INSTANCE_BASE_COLOR) return HENKA_ERROR_INVALID_ARGUMENT;
    candidate.base_color = value;
    return henka_material_instance_commit(instance, candidate, parameter);
}

henka_result henka_assets_material_instance_set_bool(
    henka_material_instance* instance,
    henka_material_instance_parameter parameter,
    bool value)
{
    henka_material candidate;
    if (instance == NULL || instance->definition == NULL) return HENKA_ERROR_INVALID_ARGUMENT;
    candidate = instance->material;
    switch (parameter)
    {
        case HENKA_MATERIAL_INSTANCE_USE_LIGHTING: candidate.use_lighting = value; break;
        case HENKA_MATERIAL_INSTANCE_DEPTH_TEST: candidate.depth_test = value; break;
        case HENKA_MATERIAL_INSTANCE_DOUBLE_SIDED: candidate.double_sided = value; break;
        case HENKA_MATERIAL_INSTANCE_CAST_SHADOWS: candidate.cast_shadows = value; break;
        case HENKA_MATERIAL_INSTANCE_RECEIVE_SHADOWS: candidate.receive_shadows = value; break;
        default: return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return henka_material_instance_commit(instance, candidate, parameter);
}

henka_result henka_assets_material_instance_set_alpha_mode(
    henka_material_instance* instance,
    henka_material_alpha_mode mode)
{
    henka_material candidate;
    if (instance == NULL || instance->definition == NULL || mode > HENKA_MATERIAL_ALPHA_BLENDED)
        return HENKA_ERROR_INVALID_ARGUMENT;
    candidate = instance->material;
    candidate.alpha_mode = mode;
    return henka_material_instance_commit(instance, candidate, HENKA_MATERIAL_INSTANCE_ALPHA_MODE);
}

henka_result henka_assets_material_instance_set_texture(
    henka_material_instance* instance,
    henka_material_texture_slot slot,
    henka_texture* texture)
{
    henka_material candidate;
    henka_material_instance_parameter parameter;

    if (instance == NULL || instance->definition == NULL || slot > HENKA_MATERIAL_TEXTURE_SLOT_EMISSIVE)
        return HENKA_ERROR_INVALID_ARGUMENT;
    candidate = instance->material;
    switch (slot)
    {
        case HENKA_MATERIAL_TEXTURE_SLOT_BASE_COLOR:
            candidate.base_color_texture = texture;
            candidate.use_texture = texture != NULL;
            parameter = HENKA_MATERIAL_INSTANCE_BASE_COLOR_TEXTURE;
            break;
        case HENKA_MATERIAL_TEXTURE_SLOT_NORMAL:
            candidate.normal_texture = texture;
            parameter = HENKA_MATERIAL_INSTANCE_NORMAL_TEXTURE;
            break;
        case HENKA_MATERIAL_TEXTURE_SLOT_METALLIC_ROUGHNESS:
            candidate.metallic_roughness_texture = texture;
            parameter = HENKA_MATERIAL_INSTANCE_METALLIC_ROUGHNESS_TEXTURE;
            break;
        case HENKA_MATERIAL_TEXTURE_SLOT_OCCLUSION:
            candidate.occlusion_texture = texture;
            parameter = HENKA_MATERIAL_INSTANCE_OCCLUSION_TEXTURE;
            break;
        case HENKA_MATERIAL_TEXTURE_SLOT_EMISSIVE:
            candidate.emissive_texture = texture;
            parameter = HENKA_MATERIAL_INSTANCE_EMISSIVE_TEXTURE;
            break;
        default:
            return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return henka_material_instance_commit(instance, candidate, parameter);
}

henka_result henka_assets_reload_gltf_material_asset(
    henka_asset_manager* manager,
    const char* path,
    henka_material_asset** out_asset)
{
    char* key = NULL;
    henka_material_asset* asset;
    henka_material candidate;
    henka_mesh* mesh = NULL;
    henka_result result;

    if (out_asset != NULL) *out_asset = NULL;
    if (manager == NULL || path == NULL || out_asset == NULL) return HENKA_ERROR_INVALID_ARGUMENT;
    result = henka_assets_make_canonical_key(path, &key);
    if (result != HENKA_SUCCESS) return result;
    asset = henka_asset_manager_find_material_entry(manager, key);
    henka_free(key);
    if (asset == NULL) return HENKA_ERROR_INVALID_ARGUMENT;

    result = henka_assets_build_gltf_material_instance(
        manager, asset->source_path, asset->material.shader, &mesh, &candidate);
    if (result != HENKA_SUCCESS) return result;
    asset->material = candidate;
    asset->revision += 1U;
    asset->metadata.loaded = true;
    asset->metadata.fallback = false;
    henka_asset_set_summary(&asset->metadata,
        "glTF material asset reloaded transactionally while preserving the stable asset identity.",
        "");
    *out_asset = asset;
    return HENKA_SUCCESS;
}

henka_result henka_assets_reload_material_asset(
    henka_asset_manager* manager,
    const henka_material_asset* asset,
    henka_material_asset** out_asset)
{
    size_t index;
    size_t material_index;

    if (out_asset != NULL)
    {
        *out_asset = NULL;
    }
    if (manager == NULL || asset == NULL || out_asset == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    for (index = 0U; index < manager->material_count; ++index)
    {
        henka_material_asset* entry = manager->material_entries[index];

        if (entry != asset)
        {
            continue;
        }
        if (entry->source_path == NULL || entry->source_path[0] == '\0')
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        return henka_assets_reload_gltf_material_asset(
            manager, entry->source_path, out_asset);
    }

    for (index = 0U; index < manager->gltf_scene_count; ++index)
    {
        henka_gltf_scene_asset* scene = manager->gltf_scene_entries[index];

        for (material_index = 0U;
             material_index < scene->data.material_count;
             ++material_index)
        {
            if (&scene->material_assets[material_index] != asset)
            {
                continue;
            }
            if (scene->source_path == NULL || scene->source_path[0] == '\0')
            {
                return HENKA_ERROR_INVALID_ARGUMENT;
            }
            {
                henka_result result = henka_assets_reload_gltf_scene_asset(
                    manager, scene->source_path, &scene);
                if (result == HENKA_SUCCESS)
                {
                    *out_asset = &scene->material_assets[material_index];
                }
                return result;
            }
        }
    }

    return HENKA_ERROR_INVALID_ARGUMENT;
}

henka_result henka_assets_load_gltf_mesh_with_material(
    henka_asset_manager* manager,
    const char* path,
    henka_shader* shader,
    henka_mesh** out_mesh,
    henka_material* out_material)
{
    henka_material_asset* asset = NULL;
    henka_result result;

    if (out_mesh != NULL) *out_mesh = NULL;
    if (out_material != NULL) *out_material = henka_material_default();
    if (manager == NULL || path == NULL || shader == NULL || out_mesh == NULL || out_material == NULL)
        return HENKA_ERROR_INVALID_ARGUMENT;
    result = henka_assets_load_gltf_material_asset(manager, path, shader, &asset);
    if (result != HENKA_SUCCESS) return result;
    result = henka_assets_load_gltf_mesh(manager, path, out_mesh);
    if (result == HENKA_SUCCESS) result = henka_assets_get_material_asset_material(asset, out_material);
    if (result != HENKA_SUCCESS) *out_mesh = NULL;
    return result;
}

static henka_result henka_assets_build_gltf_scene_payload(
    henka_asset_manager* manager,
    const char* source_path,
    henka_shader* shader,
    henka_gltf_scene_asset* out_asset)
{
    char* resolved_path = NULL;
    henka_model_data primitive_model;
    henka_result result;
    size_t index;

    if (manager == NULL || source_path == NULL || shader == NULL || out_asset == NULL) return HENKA_ERROR_INVALID_ARGUMENT;
    result = henka_assets_resolve_path(
        henka_engine_get_asset_base_path(manager->engine), source_path, &resolved_path);
    if (result == HENKA_SUCCESS) result = henka_model_scene_data_load_gltf(resolved_path, &out_asset->data);
    henka_free(resolved_path);
    if (result != HENKA_SUCCESS) return result;
    out_asset->shader = shader;

    for (index = 0U; index < out_asset->data.material_count; ++index)
    {
        if (!out_asset->data.material_present[index]) continue;
        result = henka_assets_resolve_gltf_material_source(
            manager, source_path, shader, &out_asset->data.materials[index], &out_asset->materials[index]);
        if (result != HENKA_SUCCESS)
        {
            henka_assets_destroy_gltf_scene_payload(out_asset);
            return result;
        }
        out_asset->material_assets[index].material = out_asset->materials[index];
        out_asset->material_assets[index].revision = 1U;
        out_asset->material_ready[index] = true;
    }
    for (index = 0U; index < out_asset->data.primitive_count; ++index)
    {
        memset(&primitive_model, 0, sizeof(primitive_model));
        primitive_model.vertices = out_asset->data.primitives[index].vertices;
        primitive_model.vertex_count = out_asset->data.primitives[index].vertex_count;
        primitive_model.indices = out_asset->data.primitives[index].indices;
        primitive_model.index_count = out_asset->data.primitives[index].index_count;
        result = henka_mesh_create_from_model_data(manager->engine, &primitive_model, &out_asset->primitive_meshes[index]);
        if (result != HENKA_SUCCESS)
        {
            henka_assets_destroy_gltf_scene_payload(out_asset);
            return result;
        }
        out_asset->primitive_meshes[index]->asset_manager_owned = true;
    }
    return HENKA_SUCCESS;
}

henka_result henka_assets_load_gltf_scene_asset(
    henka_asset_manager* manager,
    const char* path,
    henka_shader* shader,
    henka_gltf_scene_asset** out_asset)
{
    char* key = NULL;
    char* source_path = NULL;
    char* display_name = NULL;
    henka_gltf_scene_asset* asset = NULL;
    henka_result result;

    if (out_asset != NULL) *out_asset = NULL;
    if (manager == NULL || path == NULL || shader == NULL || out_asset == NULL) return HENKA_ERROR_INVALID_ARGUMENT;
    result = henka_assets_make_canonical_key(path, &key);
    if (result != HENKA_SUCCESS) return result;
    result = henka_assets_normalize_source_path(path, &source_path);
    if (result != HENKA_SUCCESS) { henka_free(key); return result; }
    asset = henka_asset_manager_find_gltf_scene_entry(manager, key);
    if (asset != NULL)
    {
        *out_asset = asset;
        henka_free(key);
        henka_free(source_path);
        return HENKA_SUCCESS;
    }
    asset = henka_calloc(1U, sizeof(*asset));
    if (asset == NULL)
    {
        henka_free(key);
        henka_free(source_path);
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    result = henka_assets_build_gltf_scene_payload(manager, source_path, shader, asset);
    if (result != HENKA_SUCCESS)
    {
        henka_free(asset);
        henka_free(key);
        henka_free(source_path);
        return result;
    }
    display_name = henka_asset_copy_display_name(source_path);
    if (display_name == NULL)
    {
        henka_assets_destroy_gltf_scene_payload(asset);
        henka_free(asset);
        henka_free(key);
        henka_free(source_path);
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    if (manager->gltf_scene_count == manager->gltf_scene_capacity)
    {
        result = henka_asset_manager_grow_gltf_scenes(manager);
        if (result != HENKA_SUCCESS)
        {
            henka_assets_destroy_gltf_scene_payload(asset);
            henka_free(display_name);
            henka_free(asset);
            henka_free(key);
            henka_free(source_path);
            return result;
        }
    }
    asset->key = key;
    asset->source_path = source_path;
    asset->display_name = display_name;
    asset->revision = 1U;
    asset->metadata.type = HENKA_ASSET_TYPE_GLTF_SCENE;
    asset->metadata.source_path = source_path;
    asset->metadata.display_name = display_name;
    asset->metadata.loaded = true;
    asset->metadata.fallback = false;
    asset->metadata.reload_supported = true;
    henka_asset_set_summary(&asset->metadata,
        "glTF scene imported with manager-owned primitive meshes and semantic material dependencies.", "");
    manager->gltf_scene_entries[manager->gltf_scene_count++] = asset;
    *out_asset = asset;
    return HENKA_SUCCESS;
}

henka_result henka_assets_reload_gltf_scene_asset(
    henka_asset_manager* manager,
    const char* path,
    henka_gltf_scene_asset** out_asset)
{
    char* key = NULL;
    henka_gltf_scene_asset* asset;
    henka_gltf_scene_asset* candidate = NULL;
    henka_model_scene_data* old_data = NULL;
    henka_mesh** old_meshes = NULL;
    henka_material* old_materials = NULL;
    henka_material_asset* old_material_assets = NULL;
    bool* old_material_ready = NULL;
    henka_result result;

    if (out_asset != NULL) *out_asset = NULL;
    if (manager == NULL || path == NULL || out_asset == NULL) return HENKA_ERROR_INVALID_ARGUMENT;
    result = henka_assets_make_canonical_key(path, &key);
    if (result != HENKA_SUCCESS) return result;
    asset = henka_asset_manager_find_gltf_scene_entry(manager, key);
    henka_free(key);
    if (asset == NULL) return HENKA_ERROR_INVALID_ARGUMENT;
    candidate = henka_calloc(1U, sizeof(*candidate));
    old_data = henka_calloc(1U, sizeof(*old_data));
    old_meshes = henka_calloc(HENKA_MODEL_MAX_SCENE_ITEMS, sizeof(*old_meshes));
    old_materials = henka_calloc(HENKA_MODEL_MAX_SCENE_ITEMS, sizeof(*old_materials));
    old_material_assets = henka_calloc(HENKA_MODEL_MAX_SCENE_ITEMS, sizeof(*old_material_assets));
    old_material_ready = henka_calloc(HENKA_MODEL_MAX_SCENE_ITEMS, sizeof(*old_material_ready));
    if (candidate == NULL || old_data == NULL || old_meshes == NULL ||
        old_materials == NULL || old_material_assets == NULL || old_material_ready == NULL)
    {
        henka_free(candidate);
        henka_free(old_data);
        henka_free(old_meshes);
        henka_free(old_materials);
        henka_free(old_material_assets);
        henka_free(old_material_ready);
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    result = henka_assets_build_gltf_scene_payload(manager, asset->source_path, asset->shader, candidate);
    if (result != HENKA_SUCCESS)
    {
        henka_free(candidate);
        henka_free(old_data);
        henka_free(old_meshes);
        henka_free(old_materials);
        henka_free(old_material_assets);
        henka_free(old_material_ready);
        return result;
    }

    *old_data = asset->data;
    memcpy(old_meshes, asset->primitive_meshes, sizeof(asset->primitive_meshes));
    memcpy(old_materials, asset->materials, sizeof(asset->materials));
    memcpy(old_material_assets, asset->material_assets, sizeof(asset->material_assets));
    memcpy(old_material_ready, asset->material_ready, sizeof(asset->material_ready));
    asset->data = candidate->data;
    memcpy(asset->primitive_meshes, candidate->primitive_meshes, sizeof(asset->primitive_meshes));
    memcpy(asset->materials, candidate->materials, sizeof(asset->materials));
    memcpy(asset->material_assets, candidate->material_assets, sizeof(asset->material_assets));
    memcpy(asset->material_ready, candidate->material_ready, sizeof(asset->material_ready));
    {
        size_t material_index;
        for (material_index = 0U; material_index < asset->data.material_count; ++material_index)
        {
            if (asset->material_ready[material_index])
            {
                uint64_t previous_revision = old_material_assets[material_index].revision;
                asset->material_assets[material_index].revision =
                    previous_revision == UINT64_MAX ? 1U : previous_revision + 1U;
            }
        }
    }
    candidate->data = *old_data;
    memcpy(candidate->primitive_meshes, old_meshes, sizeof(candidate->primitive_meshes));
    memcpy(candidate->materials, old_materials, sizeof(candidate->materials));
    memcpy(candidate->material_assets, old_material_assets, sizeof(candidate->material_assets));
    memcpy(candidate->material_ready, old_material_ready, sizeof(candidate->material_ready));
    henka_assets_destroy_gltf_scene_payload(candidate);
    henka_free(candidate);
    henka_free(old_data);
    henka_free(old_meshes);
    henka_free(old_materials);
    henka_free(old_material_assets);
    henka_free(old_material_ready);
    asset->revision += 1U;
    henka_asset_set_summary(&asset->metadata,
        "glTF scene reloaded transactionally while preserving stable scene identity.", "");
    *out_asset = asset;
    return HENKA_SUCCESS;
}

henka_result henka_assets_set_gltf_scene_active_scene(
    henka_gltf_scene_asset* asset,
    size_t scene_index)
{
    if (asset == NULL) return HENKA_ERROR_INVALID_ARGUMENT;
    return henka_model_scene_data_set_active_scene(&asset->data, scene_index);
}

static henka_result henka_assets_instantiate_gltf_scene_node(
    henka_asset_manager* manager,
    const henka_gltf_scene_asset* asset,
    henka_scene* target_scene,
    int node_index,
    const char* name_prefix,
    henka_entity* created,
    size_t* inout_count)
{
    const henka_model_scene_node* node;
    size_t primitive_index;
    size_t child_index;
    char name[HENKA_MAX_SCENE_TEXT_BYTES];

    (void)manager;
    if (asset == NULL || target_scene == NULL || inout_count == NULL || node_index < 0 ||
        (size_t)node_index >= asset->data.node_count) return HENKA_ERROR_INVALID_ARGUMENT;
    node = &asset->data.nodes[node_index];
    for (primitive_index = 0U; primitive_index < asset->data.primitive_count; ++primitive_index)
    {
        henka_entity entity;
        henka_material material;
        const henka_material_asset* material_asset = NULL;
        int written = snprintf(name, sizeof(name), "%s%s%s", name_prefix == NULL ? "" : name_prefix,
            node->name == NULL ? "Node" : node->name,
            asset->data.primitive_count > 1U ? " Primitive" : "");
        if (asset->data.primitives[primitive_index].mesh_index != (uint32_t)node->mesh_index) continue;
        if (*inout_count >= HENKA_MODEL_MAX_SCENE_ITEMS || written < 0 || (size_t)written >= sizeof(name)) return HENKA_ERROR_OUT_OF_MEMORY;
        entity = henka_scene_create_entity_named(target_scene, name);
        if (entity == HENKA_INVALID_ENTITY) return HENKA_ERROR_OUT_OF_MEMORY;
        created[(*inout_count)++] = entity;
        if (henka_scene_set_entity_mesh(target_scene, entity, asset->primitive_meshes[primitive_index]) != HENKA_SUCCESS) return HENKA_ERROR_UNKNOWN;
        material = henka_material_default();
        material.shader = asset->shader;
        if (asset->data.primitives[primitive_index].material_index >= 0 &&
            (size_t)asset->data.primitives[primitive_index].material_index < asset->data.material_count &&
            asset->material_ready[asset->data.primitives[primitive_index].material_index])
        {
            material = asset->materials[asset->data.primitives[primitive_index].material_index];
            material_asset = &asset->material_assets[asset->data.primitives[primitive_index].material_index];
        }
        if (henka_scene_set_entity_material(target_scene, entity, material) != HENKA_SUCCESS ||
            henka_scene_set_entity_material_asset(target_scene, entity, material_asset) != HENKA_SUCCESS ||
            henka_scene_set_entity_transform(target_scene, entity, node->world_transform) != HENKA_SUCCESS) return HENKA_ERROR_UNKNOWN;
    }
    for (child_index = 0U; child_index < asset->data.node_count; ++child_index)
        if (asset->data.nodes[child_index].parent_index == node_index)
        {
            henka_result result = henka_assets_instantiate_gltf_scene_node(manager, asset, target_scene,
                (int)child_index, name_prefix, created, inout_count);
            if (result != HENKA_SUCCESS) return result;
        }
    return HENKA_SUCCESS;
}

static bool henka_assets_gltf_node_is_active(
    const henka_gltf_scene_asset* asset,
    int node_index)
{
    size_t root_index;
    size_t guard;
    int current;

    if (asset == NULL || node_index < 0 || (size_t)node_index >= asset->data.node_count ||
        asset->data.active_scene_index >= asset->data.scene_count) return false;
    current = node_index;
    for (guard = 0U; guard < asset->data.node_count; ++guard)
    {
        int parent = asset->data.nodes[current].parent_index;
        if (parent < 0)
        {
            for (root_index = 0U; root_index < asset->data.scene_root_counts[asset->data.active_scene_index]; ++root_index)
                if (asset->data.scene_root_nodes[asset->data.scene_root_offsets[asset->data.active_scene_index] + root_index] == current)
                    return true;
            return false;
        }
        if ((size_t)parent >= asset->data.node_count) return false;
        current = parent;
    }
    return false;
}

static void henka_assets_apply_gltf_scene_bindings(
    const henka_gltf_scene_asset* asset,
    henka_scene* target_scene)
{
    bool camera_applied = false;
    size_t node_index;

    if (asset == NULL || target_scene == NULL) return;
    for (node_index = 0U; node_index < asset->data.node_count; ++node_index)
    {
        const henka_model_scene_node* node = &asset->data.nodes[node_index];
        if (!henka_assets_gltf_node_is_active(asset, (int)node_index)) continue;
        if (!camera_applied && node->camera_index >= 0 &&
            (size_t)node->camera_index < asset->data.camera_count)
        {
            henka_camera camera = asset->data.cameras[node->camera_index].camera;
            henka_vec3 forward = {
                -node->world_matrix.m[8],
                -node->world_matrix.m[9],
                -node->world_matrix.m[10]};
            float length = henka_vec3_length(forward);
            if (isfinite(length) && length > 0.000001f)
            {
                forward = henka_vec3_scale(forward, 1.0f / length);
                camera.position = (henka_vec3){
                    node->world_matrix.m[12], node->world_matrix.m[13], node->world_matrix.m[14]};
                camera.yaw_radians = atan2f(forward.z, forward.x);
                camera.pitch_radians = asinf(fmaxf(-1.0f, fminf(1.0f, forward.y)));
                if (henka_scene_set_camera(target_scene, &camera) == HENKA_SUCCESS) camera_applied = true;
            }
        }
        if (node->light_index >= 0 && (size_t)node->light_index < asset->data.light_count)
        {
            const henka_model_scene_light* source = &asset->data.lights[node->light_index];
            henka_vec3 direction = {
                -node->world_matrix.m[8],
                -node->world_matrix.m[9],
                -node->world_matrix.m[10]};
            henka_scene_light_desc light = {
                source->type == HENKA_MODEL_SCENE_LIGHT_SPOT ? HENKA_SCENE_LIGHT_SPOT : HENKA_SCENE_LIGHT_POINT,
                {node->world_matrix.m[12], node->world_matrix.m[13], node->world_matrix.m[14]},
                direction,
                source->color,
                source->intensity,
                source->range > 0.0f ? source->range : 10000.0f,
                source->inner_cone_cosine,
                source->outer_cone_cosine,
                true};
            if (source->type == HENKA_MODEL_SCENE_LIGHT_DIRECTIONAL)
            {
                henka_scene_set_light_color(target_scene, source->color);
                henka_scene_set_light_intensity(target_scene, source->intensity);
                henka_scene_set_light_direction(target_scene, direction);
            }
            else
            {
                /* The scene contract is intentionally bounded; excess local lights are ignored safely. */
                (void)henka_scene_add_light(target_scene, light, &(uint32_t){UINT32_MAX});
            }
        }
    }
}

henka_result henka_assets_instantiate_gltf_scene(
    henka_asset_manager* manager,
    const henka_gltf_scene_asset* asset,
    henka_scene* target_scene,
    const char* name_prefix,
    size_t* out_entity_count)
{
    henka_entity created[HENKA_MODEL_MAX_SCENE_ITEMS];
    size_t created_count = 0U;
    size_t root_index;
    henka_result result = HENKA_SUCCESS;

    if (out_entity_count != NULL) *out_entity_count = 0U;
    if (manager == NULL || asset == NULL || target_scene == NULL || out_entity_count == NULL ||
        asset->data.active_scene_index >= asset->data.scene_count) return HENKA_ERROR_INVALID_ARGUMENT;
    for (root_index = 0U; root_index < asset->data.scene_root_counts[asset->data.active_scene_index]; ++root_index)
    {
        int node_index = asset->data.scene_root_nodes[
            asset->data.scene_root_offsets[asset->data.active_scene_index] + root_index];
        result = henka_assets_instantiate_gltf_scene_node(manager, asset, target_scene, node_index,
            name_prefix, created, &created_count);
        if (result != HENKA_SUCCESS) break;
    }
    if (result != HENKA_SUCCESS)
    {
        while (created_count > 0U) henka_scene_destroy_entity(target_scene, created[--created_count]);
        return result;
    }
    henka_assets_apply_gltf_scene_bindings(asset, target_scene);
    *out_entity_count = created_count;
    return HENKA_SUCCESS;
}

henka_result henka_assets_retry_failed_texture(
    henka_asset_manager* manager,
    const char* path,
    henka_texture** out_texture)
{
    henka_asset_texture_entry* entry;
    char* key;
    char* resolved_path;
    henka_texture* replacement;
    henka_texture_info replacement_info;
    henka_result result;

    if (out_texture != NULL)
    {
        *out_texture = NULL;
    }

    if (manager == NULL ||
        path == NULL ||
        out_texture == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    key = NULL;
    result = henka_assets_make_canonical_key(
        path,
        &key);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    entry = henka_asset_manager_find_texture_entry(
        manager,
        key);
    henka_free(key);
    if (entry == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    if (!entry->metadata.fallback)
    {
        *out_texture = entry->texture;
        return HENKA_SUCCESS;
    }

    resolved_path = NULL;
    result = henka_assets_resolve_path(
        henka_engine_get_asset_base_path(
            manager->engine),
        entry->source_path,
        &resolved_path);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    replacement = NULL;
    result = henka_texture_create_from_file(
        manager->engine,
        resolved_path,
        &replacement);
    henka_free(resolved_path);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    memset(&replacement_info, 0, sizeof(replacement_info));
    if (henka_texture_get_info(replacement, &replacement_info) != HENKA_SUCCESS ||
        ((manager->texture_residency_budget_bytes != 0U &&
            (replacement_info.resident_gpu_bytes > manager->texture_residency_budget_bytes ||
                manager->texture_resident_bytes >
                    manager->texture_residency_budget_bytes - replacement_info.resident_gpu_bytes)) ||
            manager->texture_resident_bytes > UINT64_MAX - replacement_info.resident_gpu_bytes))
    {
        if (manager->texture_budget_rejection_count < UINT32_MAX)
            ++manager->texture_budget_rejection_count;
        henka_texture_destroy(replacement);
        return HENKA_ERROR_LIMIT;
    }

    result = henka_texture_adopt_owned_payload(
        entry->texture,
        replacement);
    if (result != HENKA_SUCCESS)
    {
        henka_texture_destroy(replacement);
        return result;
    }

    entry->owns_texture = true;
    manager->texture_resident_bytes += replacement_info.resident_gpu_bytes;
    entry->resident_gpu_bytes = replacement_info.resident_gpu_bytes;
    entry->metadata.loaded = true;
    entry->metadata.fallback = false;
    entry->metadata.reload_supported = false;
    henka_asset_set_summary(
        &entry->metadata,
        "Texture loaded after a transactional fallback retry while preserving the borrowed texture identity.",
        "");
    *out_texture = entry->texture;
    return HENKA_SUCCESS;
}

henka_result henka_assets_retry_failed_obj_mesh(
    henka_asset_manager* manager,
    const char* path,
    henka_mesh** out_mesh)
{
    henka_asset_mesh_entry* entry;
    char* key;
    char* resolved_path;
    henka_mesh* replacement;
    henka_result result;

    if (out_mesh != NULL)
    {
        *out_mesh = NULL;
    }

    if (manager == NULL || path == NULL || out_mesh == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    key = NULL;
    result = henka_assets_make_canonical_key(path, &key);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    entry = henka_asset_manager_find_mesh_entry(manager, key);
    henka_free(key);
    if (entry == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    if (!entry->metadata.fallback)
    {
        *out_mesh = entry->mesh;
        return HENKA_SUCCESS;
    }

    resolved_path = NULL;
    result = henka_assets_resolve_path(
        henka_engine_get_asset_base_path(manager->engine),
        entry->source_path,
        &resolved_path);
    if (result != HENKA_SUCCESS)
    {
        *out_mesh = entry->mesh;
        return result;
    }

    replacement = NULL;
    result = henka_mesh_create_from_obj(
        manager->engine,
        resolved_path,
        &replacement);
    henka_free(resolved_path);
    if (result != HENKA_SUCCESS)
    {
        *out_mesh = entry->mesh;
        return result;
    }

    replacement->asset_manager_owned = true;
    entry->mesh = replacement;
    entry->owns_mesh = true;
    entry->metadata.loaded = true;
    entry->metadata.fallback = false;
    entry->metadata.reload_supported = false;
    henka_asset_set_summary(
        &entry->metadata,
        "Mesh loaded after a transactional fallback retry.",
        "");
    *out_mesh = replacement;
    return HENKA_SUCCESS;
}

henka_result henka_assets_retry_failed_gltf_mesh(
    henka_asset_manager* manager,
    const char* path,
    henka_mesh** out_mesh)
{
    henka_asset_mesh_entry* entry;
    char* key = NULL;
    char* resolved_path = NULL;
    henka_mesh* replacement = NULL;
    henka_result result;

    if (out_mesh != NULL) *out_mesh = NULL;
    if (manager == NULL || path == NULL || out_mesh == NULL) return HENKA_ERROR_INVALID_ARGUMENT;
    result = henka_assets_make_canonical_key(path, &key);
    if (result != HENKA_SUCCESS) return result;
    entry = henka_asset_manager_find_mesh_entry(manager, key);
    henka_free(key);
    if (entry == NULL) return HENKA_ERROR_INVALID_ARGUMENT;
    if (!entry->metadata.fallback) { *out_mesh = entry->mesh; return HENKA_SUCCESS; }
    result = henka_assets_resolve_path(henka_engine_get_asset_base_path(manager->engine), entry->source_path, &resolved_path);
    if (result != HENKA_SUCCESS) { *out_mesh = entry->mesh; return result; }
    result = henka_mesh_create_from_gltf(manager->engine, resolved_path, &replacement);
    henka_free(resolved_path);
    if (result != HENKA_SUCCESS) { *out_mesh = entry->mesh; return result; }
    replacement->asset_manager_owned = true;
    entry->mesh = replacement;
    entry->owns_mesh = true;
    entry->metadata.loaded = true;
    entry->metadata.fallback = false;
    entry->metadata.reload_supported = false;
    henka_asset_set_summary(&entry->metadata, "glTF mesh loaded after a transactional fallback retry.", "");
    *out_mesh = replacement;
    return HENKA_SUCCESS;
}

size_t henka_assets_get_metadata_count(const henka_asset_manager* manager)
{
    if (manager == NULL)
    {
        return 0U;
    }

    return manager->shader_count + manager->texture_count + manager->mesh_count + manager->material_count + manager->gltf_scene_count;
}

henka_result henka_assets_get_metadata_at_index(
    const henka_asset_manager* manager,
    size_t index,
    henka_asset_metadata* out_metadata)
{
    if (out_metadata != NULL)
    {
        memset(out_metadata, 0, sizeof(*out_metadata));
    }

    if (manager == NULL || out_metadata == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    if (index < manager->shader_count)
    {
        *out_metadata = manager->shader_entries[index].metadata;
        return HENKA_SUCCESS;
    }

    index -= manager->shader_count;
    if (index < manager->texture_count)
    {
        *out_metadata = manager->texture_entries[index].metadata;
        return HENKA_SUCCESS;
    }

    index -= manager->texture_count;
    if (index < manager->mesh_count)
    {
        *out_metadata = manager->mesh_entries[index].metadata;
        return HENKA_SUCCESS;
    }

    index -= manager->mesh_count;
    if (index < manager->material_count)
    {
        *out_metadata = manager->material_entries[index]->metadata;
        return HENKA_SUCCESS;
    }

    index -= manager->material_count;
    if (index < manager->gltf_scene_count)
    {
        *out_metadata = manager->gltf_scene_entries[index]->metadata;
        return HENKA_SUCCESS;
    }

    return HENKA_ERROR_INVALID_ARGUMENT;
}

henka_result henka_assets_get_shader_metadata(
    const henka_asset_manager* manager,
    const henka_shader* shader,
    henka_asset_metadata* out_metadata)
{
    size_t index;

    if (out_metadata != NULL)
    {
        memset(out_metadata, 0, sizeof(*out_metadata));
    }

    if (manager == NULL || shader == NULL || out_metadata == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    for (index = 0U; index < manager->shader_count; ++index)
    {
        if (manager->shader_entries[index].shader == shader)
        {
            *out_metadata = manager->shader_entries[index].metadata;
            return HENKA_SUCCESS;
        }
    }

    return HENKA_ERROR_UNKNOWN;
}

henka_result henka_assets_get_texture_metadata(
    const henka_asset_manager* manager,
    const henka_texture* texture,
    henka_asset_metadata* out_metadata)
{
    size_t index;

    if (out_metadata != NULL)
    {
        memset(out_metadata, 0, sizeof(*out_metadata));
    }

    if (manager == NULL || texture == NULL || out_metadata == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    if (texture == manager->error_texture)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    for (index = 0U; index < manager->texture_count; ++index)
    {
        if (manager->texture_entries[index].texture == texture)
        {
            *out_metadata = manager->texture_entries[index].metadata;
            return HENKA_SUCCESS;
        }
    }

    return HENKA_ERROR_UNKNOWN;
}

henka_result henka_assets_get_mesh_metadata(
    const henka_asset_manager* manager,
    const henka_mesh* mesh,
    henka_asset_metadata* out_metadata)
{
    size_t index;

    if (out_metadata != NULL)
    {
        memset(out_metadata, 0, sizeof(*out_metadata));
    }

    if (manager == NULL || mesh == NULL || out_metadata == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    if (mesh == manager->fallback_mesh)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    for (index = 0U; index < manager->mesh_count; ++index)
    {
        if (manager->mesh_entries[index].mesh == mesh)
        {
            *out_metadata = manager->mesh_entries[index].metadata;
            return HENKA_SUCCESS;
        }
    }

    return HENKA_ERROR_UNKNOWN;
}

henka_result henka_assets_get_texture_metadata_for_path(
    const henka_asset_manager* manager,
    const char* path,
    henka_asset_metadata* out_metadata)
{
    char* key;
    const henka_asset_texture_entry* entry;
    henka_result result;

    if (out_metadata != NULL)
    {
        memset(out_metadata, 0, sizeof(*out_metadata));
    }

    if (manager == NULL || path == NULL || out_metadata == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    key = NULL;
    result = henka_assets_make_canonical_key(path, &key);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    entry = henka_asset_manager_find_texture_entry_const(
        manager,
        key);
    henka_free(key);
    if (entry == NULL)
    {
        return HENKA_ERROR_UNKNOWN;
    }

    *out_metadata = entry->metadata;
    return HENKA_SUCCESS;
}

henka_result henka_assets_get_mesh_metadata_for_path(
    const henka_asset_manager* manager,
    const char* path,
    henka_asset_metadata* out_metadata)
{
    char* key;
    const henka_asset_mesh_entry* entry;
    henka_result result;

    if (out_metadata != NULL)
    {
        memset(out_metadata, 0, sizeof(*out_metadata));
    }

    if (manager == NULL || path == NULL || out_metadata == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    key = NULL;
    result = henka_assets_make_canonical_key(path, &key);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    entry = henka_asset_manager_find_mesh_entry_const(
        manager,
        key);
    henka_free(key);
    if (entry == NULL)
    {
        return HENKA_ERROR_UNKNOWN;
    }

    *out_metadata = entry->metadata;
    return HENKA_SUCCESS;
}

henka_texture* henka_assets_get_white_texture(henka_asset_manager* manager)
{
    if (manager == NULL)
    {
        return NULL;
    }

    return manager->white_texture;
}

henka_texture* henka_assets_get_error_texture(henka_asset_manager* manager)
{
    if (manager == NULL)
    {
        return NULL;
    }

    return manager->error_texture;
}

henka_mesh* henka_assets_get_fallback_mesh(henka_asset_manager* manager)
{
    if (manager == NULL)
    {
        return NULL;
    }

    return manager->fallback_mesh;
}
