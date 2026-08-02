#include "henka_internal.h"

#include <henka/model.h>

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <henka/log.h>
#include <henka/memory.h>

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

    henka_free(manager->shader_entries);
    henka_free(manager->texture_entries);
    henka_free(manager->mesh_entries);
    henka_free(manager->material_entries);
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
    henka_free(resolved_path);
    if (result == HENKA_ERROR_ASSET_SOURCE)
    {
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
    *out_texture = texture;
    return HENKA_SUCCESS;
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
    henka_texture_descriptor descriptor;
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

    candidate = model.has_material ? model.material_source.material : henka_material_default();
    candidate.shader = shader;
    /* Material instances do not borrow the temporary parser name. */
    candidate.name = "glTF Material";
    candidate.base_color_texture = NULL;
    candidate.normal_texture = NULL;
    candidate.metallic_roughness_texture = NULL;
    candidate.occlusion_texture = NULL;
    candidate.emissive_texture = NULL;
    candidate.use_texture = false;

    descriptor = henka_texture_descriptor_default_color();
    result = henka_assets_resolve_gltf_material_texture(
        manager, source_path, model.material_source.base_color_uri, descriptor, &candidate.base_color_texture);
    if (result == HENKA_SUCCESS && candidate.base_color_texture != NULL) candidate.use_texture = true;

    descriptor = henka_texture_descriptor_default_normal();
    if (result == HENKA_SUCCESS) result = henka_assets_resolve_gltf_material_texture(
        manager, source_path, model.material_source.normal_uri, descriptor, &candidate.normal_texture);

    descriptor = henka_texture_descriptor_default_data();
    descriptor.usage = HENKA_TEXTURE_USAGE_METALLIC_ROUGHNESS;
    if (result == HENKA_SUCCESS) result = henka_assets_resolve_gltf_material_texture(
        manager, source_path, model.material_source.metallic_roughness_uri, descriptor, &candidate.metallic_roughness_texture);

    descriptor = henka_texture_descriptor_default_data();
    descriptor.usage = HENKA_TEXTURE_USAGE_OCCLUSION;
    if (result == HENKA_SUCCESS) result = henka_assets_resolve_gltf_material_texture(
        manager, source_path, model.material_source.occlusion_uri, descriptor, &candidate.occlusion_texture);

    descriptor = henka_texture_descriptor_default_color();
    descriptor.usage = HENKA_TEXTURE_USAGE_EMISSIVE;
    if (result == HENKA_SUCCESS) result = henka_assets_resolve_gltf_material_texture(
        manager, source_path, model.material_source.emissive_uri, descriptor, &candidate.emissive_texture);

    if (result == HENKA_SUCCESS) result = henka_material_validate(&candidate);
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

henka_result henka_assets_retry_failed_texture(
    henka_asset_manager* manager,
    const char* path,
    henka_texture** out_texture)
{
    henka_asset_texture_entry* entry;
    char* key;
    char* resolved_path;
    henka_texture* replacement;
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

    result = henka_texture_adopt_owned_payload(
        entry->texture,
        replacement);
    if (result != HENKA_SUCCESS)
    {
        henka_texture_destroy(replacement);
        return result;
    }

    entry->owns_texture = true;
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

    return manager->shader_count + manager->texture_count + manager->mesh_count + manager->material_count;
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
