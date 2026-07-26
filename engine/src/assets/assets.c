#include "henka_internal.h"

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

char* henka_asset_copy_display_name(const char* path)
{
    return henka_duplicate_string(henka_asset_display_name(path));
}

henka_result henka_assets_make_canonical_key(
    const char* path,
    char** out_key)
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

    if (out_key != NULL)
    {
        *out_key = NULL;
    }

    if (path == NULL || out_key == NULL ||
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
        out_key);
    henka_free(normalized);
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

static const henka_asset_shader_entry*
henka_asset_manager_find_shader_entry_const(
    const henka_asset_manager* manager,
    const char* vertex_key,
    const char* fragment_key)
{
    size_t index;

    if (manager == NULL ||
        vertex_key == NULL ||
        fragment_key == NULL)
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
                fragment_key) == 0)
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
    const char* fragment_key)
{
    return (henka_asset_shader_entry*)
        henka_asset_manager_find_shader_entry_const(
            manager,
            vertex_key,
            fragment_key);
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
        henka_free(manager->mesh_entries[index].display_name);
    }

    henka_free(manager->shader_entries);
    henka_free(manager->texture_entries);
    henka_free(manager->mesh_entries);
    henka_mesh_destroy_owned(manager->fallback_mesh);
    henka_texture_destroy_owned(manager->white_texture);
    henka_texture_destroy_owned(manager->error_texture);
    henka_free(manager);
}

henka_result henka_assets_load_shader(
    henka_asset_manager* manager,
    const char* vertex_path,
    const char* fragment_path,
    henka_shader** out_shader)
{
    char* display_name;
    char* fragment_key;
    char* resolved_fragment_path;
    char* resolved_vertex_path;
    char* source_path;
    henka_asset_shader_entry* existing_entry;
    henka_shader* shader;
    henka_result result;
    char* vertex_key;

    if (out_shader != NULL)
    {
        *out_shader = NULL;
    }

    if (manager == NULL ||
        vertex_path == NULL ||
        fragment_path == NULL ||
        out_shader == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    vertex_key = NULL;
    fragment_key = NULL;
    result = henka_assets_make_canonical_key(
        vertex_path,
        &vertex_key);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    result = henka_assets_make_canonical_key(
        fragment_path,
        &fragment_key);
    if (result != HENKA_SUCCESS)
    {
        henka_free(vertex_key);
        return result;
    }

    existing_entry = henka_asset_manager_find_shader_entry(
        manager,
        vertex_key,
        fragment_key);
    if (existing_entry != NULL)
    {
        *out_shader = existing_entry->shader;
        henka_free(fragment_key);
        henka_free(vertex_key);
        return HENKA_SUCCESS;
    }

    display_name = henka_asset_copy_display_name(vertex_key);
    source_path = henka_asset_create_shader_source_path(
        vertex_key,
        fragment_key);
    if (display_name == NULL || source_path == NULL)
    {
        henka_free(source_path);
        henka_free(display_name);
        henka_free(fragment_key);
        henka_free(vertex_key);
        return HENKA_ERROR_OUT_OF_MEMORY;
    }

    resolved_vertex_path = NULL;
    resolved_fragment_path = NULL;
    result = henka_assets_resolve_path(
        henka_engine_get_asset_base_path(manager->engine),
        vertex_key,
        &resolved_vertex_path);
    if (result != HENKA_SUCCESS)
    {
        henka_free(source_path);
        henka_free(display_name);
        henka_free(fragment_key);
        henka_free(vertex_key);
        return result;
    }

    result = henka_assets_resolve_path(
        henka_engine_get_asset_base_path(manager->engine),
        fragment_key,
        &resolved_fragment_path);
    if (result != HENKA_SUCCESS)
    {
        henka_free(resolved_vertex_path);
        henka_free(source_path);
        henka_free(display_name);
        henka_free(fragment_key);
        henka_free(vertex_key);
        return result;
    }

    shader = NULL;
    result = henka_shader_create_from_files(
        manager->engine,
        resolved_vertex_path,
        resolved_fragment_path,
        &shader);
    henka_free(resolved_vertex_path);
    henka_free(resolved_fragment_path);
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
    char* display_name;
    henka_asset_texture_entry* existing_entry;
    char* key;
    char* resolved_path;
    henka_texture* texture;
    henka_result result;

    if (out_texture != NULL)
    {
        *out_texture = NULL;
    }

    if (manager == NULL || path == NULL || out_texture == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    key = NULL;
    result = henka_assets_make_canonical_key(path, &key);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    existing_entry = henka_asset_manager_find_texture_entry(
        manager,
        key);
    if (existing_entry != NULL)
    {
        *out_texture = existing_entry->texture;
        henka_free(key);
        return HENKA_SUCCESS;
    }

    resolved_path = NULL;
    result = henka_assets_resolve_path(
        henka_engine_get_asset_base_path(manager->engine),
        key,
        &resolved_path);
    if (result != HENKA_SUCCESS)
    {
        henka_free(key);
        return result;
    }

    texture = NULL;
    result = henka_texture_create_from_file(
        manager->engine,
        resolved_path,
        &texture);
    henka_free(resolved_path);
    if (result != HENKA_SUCCESS)
    {
        HENKA_LOG_ERROR(
            "Using the error texture because '%s' could not be loaded",
            key);
        texture = manager->error_texture;
    }

    display_name = henka_asset_copy_display_name(key);
    if (display_name == NULL)
    {
        if (texture != manager->error_texture)
        {
            henka_texture_destroy(texture);
        }
        henka_free(key);
        return HENKA_ERROR_OUT_OF_MEMORY;
    }

    if (manager->texture_count == manager->texture_capacity)
    {
        result = henka_asset_manager_grow_textures(manager);
        if (result != HENKA_SUCCESS)
        {
            if (texture != manager->error_texture)
            {
                henka_texture_destroy(texture);
            }
            henka_free(display_name);
            henka_free(key);
            return result;
        }
    }

    if (texture != manager->error_texture)
    {
        texture->asset_manager_owned = true;
    }

    manager->texture_entries[manager->texture_count].key = key;
    manager->texture_entries[manager->texture_count].display_name =
        display_name;
    manager->texture_entries[manager->texture_count].texture = texture;
    manager->texture_entries[manager->texture_count].owns_texture =
        texture != manager->error_texture;
    manager->texture_entries[manager->texture_count].metadata.type =
        HENKA_ASSET_TYPE_TEXTURE;
    manager->texture_entries[manager->texture_count].metadata.source_path =
        key;
    manager->texture_entries[manager->texture_count].metadata.display_name =
        display_name;
    manager->texture_entries[manager->texture_count].metadata.loaded =
        texture != manager->error_texture;
    manager->texture_entries[manager->texture_count].metadata.fallback =
        texture == manager->error_texture;
    manager->texture_entries[manager->texture_count].metadata.reload_supported =
        texture == manager->error_texture;
    henka_asset_set_summary(
        &manager->texture_entries[manager->texture_count].metadata,
        texture == manager->error_texture ?
            "Texture fallback is active and can be retried." :
            "Texture loaded from the canonical asset path.",
        texture == manager->error_texture ?
            "Texture load failed and the error texture fallback was used. Retry after fixing the source asset." :
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
    result = henka_assets_make_canonical_key(path, &key);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    existing_entry = henka_asset_manager_find_mesh_entry(
        manager,
        key);
    if (existing_entry != NULL)
    {
        *out_mesh = existing_entry->mesh;
        henka_free(key);
        return HENKA_SUCCESS;
    }

    resolved_path = NULL;
    result = henka_assets_resolve_path(
        henka_engine_get_asset_base_path(manager->engine),
        key,
        &resolved_path);
    if (result != HENKA_SUCCESS)
    {
        henka_free(key);
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
            key);
        mesh = manager->fallback_mesh;
    }

    display_name = henka_asset_copy_display_name(key);
    if (display_name == NULL)
    {
        if (mesh != manager->fallback_mesh)
        {
            henka_mesh_destroy(mesh);
        }
        henka_free(key);
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
            return result;
        }
    }

    if (mesh != manager->fallback_mesh)
    {
        mesh->asset_manager_owned = true;
    }

    manager->mesh_entries[manager->mesh_count].key = key;
    manager->mesh_entries[manager->mesh_count].display_name =
        display_name;
    manager->mesh_entries[manager->mesh_count].mesh = mesh;
    manager->mesh_entries[manager->mesh_count].owns_mesh =
        mesh != manager->fallback_mesh;
    manager->mesh_entries[manager->mesh_count].metadata.type =
        HENKA_ASSET_TYPE_MESH;
    manager->mesh_entries[manager->mesh_count].metadata.source_path =
        key;
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

    if (manager == NULL || path == NULL || out_texture == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    key = NULL;
    result = henka_assets_make_canonical_key(path, &key);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    entry = henka_asset_manager_find_texture_entry(manager, key);
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
        henka_engine_get_asset_base_path(manager->engine),
        entry->key,
        &resolved_path);
    if (result != HENKA_SUCCESS)
    {
        *out_texture = entry->texture;
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
        *out_texture = entry->texture;
        return result;
    }

    replacement->asset_manager_owned = true;
    entry->texture = replacement;
    entry->owns_texture = true;
    entry->metadata.loaded = true;
    entry->metadata.fallback = false;
    entry->metadata.reload_supported = false;
    henka_asset_set_summary(
        &entry->metadata,
        "Texture loaded after a transactional fallback retry.",
        "");
    *out_texture = replacement;
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
        entry->key,
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

size_t henka_assets_get_metadata_count(const henka_asset_manager* manager)
{
    if (manager == NULL)
    {
        return 0U;
    }

    return manager->shader_count + manager->texture_count + manager->mesh_count;
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
