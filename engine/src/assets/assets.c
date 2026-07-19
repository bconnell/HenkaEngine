#include "henka_internal.h"

#include <stdio.h>
#include <string.h>

#include <henka/log.h>
#include <henka/memory.h>

static char* henka_duplicate_string(const char* value)
{
    char* copy;
    size_t length;

    if (value == NULL)
    {
        return NULL;
    }

    length = strlen(value);
    copy = henka_malloc(length + 1U);
    if (copy == NULL)
    {
        return NULL;
    }

    memcpy(copy, value, length + 1U);
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
    henka_asset_shader_entry* entries;
    size_t new_capacity;

    new_capacity = manager->shader_capacity == 0U ? 8U : manager->shader_capacity * 2U;
    entries = henka_realloc(manager->shader_entries, new_capacity * sizeof(*entries));
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
    henka_asset_texture_entry* entries;
    size_t new_capacity;

    new_capacity = manager->texture_capacity == 0U ? 8U : manager->texture_capacity * 2U;
    entries = henka_realloc(manager->texture_entries, new_capacity * sizeof(*entries));
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
    henka_asset_mesh_entry* entries;
    size_t new_capacity;

    new_capacity = manager->mesh_capacity == 0U ? 8U : manager->mesh_capacity * 2U;
    entries = henka_realloc(manager->mesh_entries, new_capacity * sizeof(*entries));
    if (entries == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }

    manager->mesh_entries = entries;
    manager->mesh_capacity = new_capacity;
    return HENKA_SUCCESS;
}

static henka_shader* henka_asset_manager_find_shader(henka_asset_manager* manager, const char* key)
{
    size_t index;

    for (index = 0U; index < manager->shader_count; ++index)
    {
        if (strcmp(manager->shader_entries[index].key, key) == 0)
        {
            return manager->shader_entries[index].shader;
        }
    }

    return NULL;
}

static henka_texture* henka_asset_manager_find_texture(henka_asset_manager* manager, const char* key)
{
    size_t index;

    for (index = 0U; index < manager->texture_count; ++index)
    {
        if (strcmp(manager->texture_entries[index].key, key) == 0)
        {
            return manager->texture_entries[index].texture;
        }
    }

    return NULL;
}

static henka_asset_mesh_entry* henka_asset_manager_find_mesh_entry(
    henka_asset_manager* manager,
    const char* key,
    size_t* out_index)
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
            if (out_index != NULL)
            {
                *out_index = index;
            }
            return &manager->mesh_entries[index];
        }
    }

    return NULL;
}

static henka_mesh* henka_asset_manager_find_mesh(henka_asset_manager* manager, const char* key)
{
    henka_asset_mesh_entry* entry;

    entry = henka_asset_manager_find_mesh_entry(manager, key, NULL);
    return entry != NULL ? entry->mesh : NULL;
}

static void henka_asset_manager_remove_mesh_entry_at(henka_asset_manager* manager, size_t index)
{
    henka_asset_mesh_entry* entry;

    if (manager == NULL || index >= manager->mesh_count)
    {
        return;
    }

    entry = &manager->mesh_entries[index];
    if (entry->owns_mesh)
    {
        henka_mesh_destroy(entry->mesh);
    }
    henka_free(entry->key);

    if (index + 1U < manager->mesh_count)
    {
        memmove(
            &manager->mesh_entries[index],
            &manager->mesh_entries[index + 1U],
            (manager->mesh_count - index - 1U) * sizeof(*manager->mesh_entries));
    }

    manager->mesh_count -= 1U;
}

static henka_result henka_asset_manager_create_fallback_textures(henka_asset_manager* manager)
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

    result = henka_texture_create_from_rgba8(manager->engine, 1, 1, white_pixels, &manager->white_texture);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    result = henka_texture_create_from_rgba8(manager->engine, 2, 2, error_pixels, &manager->error_texture);
    if (result != HENKA_SUCCESS)
    {
        henka_texture_destroy(manager->white_texture);
        manager->white_texture = NULL;
        return result;
    }

    return HENKA_SUCCESS;
}

static henka_result henka_asset_manager_create_fallback_mesh(henka_asset_manager* manager)
{
    return henka_mesh_create_cube(manager->engine, &manager->fallback_mesh);
}

henka_result henka_asset_manager_create(struct henka_engine* engine, struct henka_asset_manager** out_manager)
{
    henka_asset_manager* manager;
    henka_result result;

    if (engine == NULL || out_manager == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    *out_manager = NULL;

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
        henka_texture_destroy(manager->error_texture);
        henka_texture_destroy(manager->white_texture);
        henka_free(manager);
        return result;
    }

    *out_manager = manager;
    return HENKA_SUCCESS;
}

void henka_asset_manager_destroy(struct henka_asset_manager* manager)
{
    size_t index;

    if (manager == NULL)
    {
        return;
    }

    for (index = 0U; index < manager->shader_count; ++index)
    {
        henka_shader_destroy(manager->shader_entries[index].shader);
        henka_free(manager->shader_entries[index].key);
    }

    for (index = 0U; index < manager->texture_count; ++index)
    {
        if (manager->texture_entries[index].owns_texture)
        {
            henka_texture_destroy(manager->texture_entries[index].texture);
        }
        henka_free(manager->texture_entries[index].key);
    }

    for (index = 0U; index < manager->mesh_count; ++index)
    {
        if (manager->mesh_entries[index].owns_mesh)
        {
            henka_mesh_destroy(manager->mesh_entries[index].mesh);
        }
        henka_free(manager->mesh_entries[index].key);
    }

    henka_free(manager->shader_entries);
    henka_free(manager->texture_entries);
    henka_free(manager->mesh_entries);
    henka_mesh_destroy(manager->fallback_mesh);
    henka_texture_destroy(manager->white_texture);
    henka_texture_destroy(manager->error_texture);
    henka_free(manager);
}

henka_result henka_assets_load_shader(
    henka_asset_manager* manager,
    const char* vertex_path,
    const char* fragment_path,
    henka_shader** out_shader)
{
    char* key;
    char* resolved_fragment_path;
    char* resolved_vertex_path;
    henka_shader* shader;
    henka_result result;
    size_t key_length;

    if (manager == NULL || vertex_path == NULL || fragment_path == NULL || out_shader == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    key_length = strlen(vertex_path) + strlen(fragment_path) + 2U;
    key = henka_malloc(key_length);
    if (key == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }

    strcpy_s(key, key_length, vertex_path);
    strcat_s(key, key_length, "|");
    strcat_s(key, key_length, fragment_path);

    shader = henka_asset_manager_find_shader(manager, key);
    if (shader != NULL)
    {
        *out_shader = shader;
        henka_free(key);
        return HENKA_SUCCESS;
    }

    result = henka_assets_resolve_path(henka_engine_get_asset_base_path(manager->engine), vertex_path, &resolved_vertex_path);
    if (result != HENKA_SUCCESS)
    {
        henka_free(key);
        return result;
    }

    result = henka_assets_resolve_path(henka_engine_get_asset_base_path(manager->engine), fragment_path, &resolved_fragment_path);
    if (result != HENKA_SUCCESS)
    {
        henka_free(resolved_vertex_path);
        henka_free(key);
        return result;
    }

    result = henka_shader_create_from_files(manager->engine, resolved_vertex_path, resolved_fragment_path, &shader);
    henka_free(resolved_vertex_path);
    henka_free(resolved_fragment_path);
    if (result != HENKA_SUCCESS)
    {
        henka_free(key);
        return result;
    }

    if (manager->shader_count == manager->shader_capacity)
    {
        result = henka_asset_manager_grow_shaders(manager);
        if (result != HENKA_SUCCESS)
        {
            henka_shader_destroy(shader);
            henka_free(key);
            return result;
        }
    }

    manager->shader_entries[manager->shader_count].key = key;
    manager->shader_entries[manager->shader_count].shader = shader;
    manager->shader_entries[manager->shader_count].metadata.type = HENKA_ASSET_TYPE_SHADER;
    manager->shader_entries[manager->shader_count].metadata.source_path = manager->shader_entries[manager->shader_count].key;
    manager->shader_entries[manager->shader_count].metadata.display_name = henka_asset_display_name(vertex_path);
    manager->shader_entries[manager->shader_count].metadata.loaded = true;
    manager->shader_entries[manager->shader_count].metadata.fallback = false;
    manager->shader_entries[manager->shader_count].metadata.reload_supported = true;
    henka_asset_set_summary(&manager->shader_entries[manager->shader_count].metadata, "Shader loaded from vertex and fragment sources.", "");
    manager->shader_count += 1U;
    *out_shader = shader;
    return HENKA_SUCCESS;
}

henka_result henka_assets_load_texture(henka_asset_manager* manager, const char* path, henka_texture** out_texture)
{
    char* key;
    char* resolved_path;
    henka_texture* texture;
    henka_result result;

    if (manager == NULL || path == NULL || out_texture == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    texture = henka_asset_manager_find_texture(manager, path);
    if (texture != NULL)
    {
        *out_texture = texture;
        return HENKA_SUCCESS;
    }

    result = henka_assets_resolve_path(henka_engine_get_asset_base_path(manager->engine), path, &resolved_path);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    result = henka_texture_create_from_file(manager->engine, resolved_path, &texture);
    henka_free(resolved_path);
    if (result != HENKA_SUCCESS)
    {
        HENKA_LOG_ERROR("Using the error texture because '%s' could not be loaded", path);
        texture = manager->error_texture;
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
            return result;
        }
    }

    key = henka_duplicate_string(path);
    if (key == NULL)
    {
        if (texture != manager->error_texture)
        {
            henka_texture_destroy(texture);
        }
        return HENKA_ERROR_OUT_OF_MEMORY;
    }

    manager->texture_entries[manager->texture_count].key = key;
    manager->texture_entries[manager->texture_count].texture = texture;
    manager->texture_entries[manager->texture_count].owns_texture = texture != manager->error_texture;
    manager->texture_entries[manager->texture_count].metadata.type = HENKA_ASSET_TYPE_TEXTURE;
    manager->texture_entries[manager->texture_count].metadata.source_path = manager->texture_entries[manager->texture_count].key;
    manager->texture_entries[manager->texture_count].metadata.display_name = henka_asset_display_name(path);
    manager->texture_entries[manager->texture_count].metadata.loaded = texture != manager->error_texture;
    manager->texture_entries[manager->texture_count].metadata.fallback = texture == manager->error_texture;
    manager->texture_entries[manager->texture_count].metadata.reload_supported = true;
    henka_asset_set_summary(
        &manager->texture_entries[manager->texture_count].metadata,
        texture == manager->error_texture ? "Texture fallback is active." : "Texture loaded from the asset path.",
        texture == manager->error_texture ? "Texture load failed and the error texture fallback was used." : "");
    manager->texture_count += 1U;
    *out_texture = texture;
    return HENKA_SUCCESS;
}

henka_result henka_assets_load_obj_mesh(henka_asset_manager* manager, const char* path, henka_mesh** out_mesh)
{
    char* key;
    char* resolved_path;
    henka_mesh* mesh;
    henka_result result;

    if (manager == NULL || path == NULL || out_mesh == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    mesh = henka_asset_manager_find_mesh(manager, path);
    if (mesh != NULL)
    {
        *out_mesh = mesh;
        return HENKA_SUCCESS;
    }

    result = henka_assets_resolve_path(henka_engine_get_asset_base_path(manager->engine), path, &resolved_path);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    result = henka_mesh_create_from_obj(manager->engine, resolved_path, &mesh);
    henka_free(resolved_path);
    if (result != HENKA_SUCCESS)
    {
        HENKA_LOG_ERROR("Using the fallback mesh because '%s' could not be loaded", path);
        mesh = manager->fallback_mesh;
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
            return result;
        }
    }

    key = henka_duplicate_string(path);
    if (key == NULL)
    {
        if (mesh != manager->fallback_mesh)
        {
            henka_mesh_destroy(mesh);
        }
        return HENKA_ERROR_OUT_OF_MEMORY;
    }

    manager->mesh_entries[manager->mesh_count].key = key;
    manager->mesh_entries[manager->mesh_count].mesh = mesh;
    manager->mesh_entries[manager->mesh_count].owns_mesh = mesh != manager->fallback_mesh;
    manager->mesh_entries[manager->mesh_count].metadata.type = HENKA_ASSET_TYPE_MESH;
    manager->mesh_entries[manager->mesh_count].metadata.source_path = manager->mesh_entries[manager->mesh_count].key;
    manager->mesh_entries[manager->mesh_count].metadata.display_name = henka_asset_display_name(path);
    manager->mesh_entries[manager->mesh_count].metadata.loaded = mesh != manager->fallback_mesh;
    manager->mesh_entries[manager->mesh_count].metadata.fallback = mesh == manager->fallback_mesh;
    manager->mesh_entries[manager->mesh_count].metadata.reload_supported = true;
    henka_asset_set_summary(
        &manager->mesh_entries[manager->mesh_count].metadata,
        mesh == manager->fallback_mesh ? "Mesh fallback is active and can be retried." : "Mesh loaded from the asset path.",
        mesh == manager->fallback_mesh ? "Mesh load failed and the fallback mesh was used. Retry after fixing the source asset." : "");
    manager->mesh_count += 1U;
    *out_mesh = mesh;
    return HENKA_SUCCESS;
}

henka_result henka_assets_retry_failed_obj_mesh(henka_asset_manager* manager, const char* path, henka_mesh** out_mesh)
{
    henka_asset_mesh_entry* entry;
    size_t entry_index;

    if (manager == NULL || path == NULL || out_mesh == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    entry_index = 0U;
    entry = henka_asset_manager_find_mesh_entry(manager, path, &entry_index);
    if (entry != NULL && !entry->metadata.fallback)
    {
        *out_mesh = entry->mesh;
        return HENKA_SUCCESS;
    }

    if (entry != NULL)
    {
        henka_asset_manager_remove_mesh_entry_at(manager, entry_index);
    }

    return henka_assets_load_obj_mesh(manager, path, out_mesh);
}

size_t henka_assets_get_metadata_count(const henka_asset_manager* manager)
{
    if (manager == NULL)
    {
        return 0U;
    }

    return manager->shader_count + manager->texture_count + manager->mesh_count;
}

henka_result henka_assets_get_metadata_at_index(const henka_asset_manager* manager, size_t index, henka_asset_metadata* out_metadata)
{
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

henka_result henka_assets_get_shader_metadata(const henka_asset_manager* manager, const henka_shader* shader, henka_asset_metadata* out_metadata)
{
    size_t index;

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

henka_result henka_assets_get_texture_metadata(const henka_asset_manager* manager, const henka_texture* texture, henka_asset_metadata* out_metadata)
{
    size_t index;

    if (manager == NULL || texture == NULL || out_metadata == NULL)
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

henka_result henka_assets_get_mesh_metadata(const henka_asset_manager* manager, const henka_mesh* mesh, henka_asset_metadata* out_metadata)
{
    size_t index;

    if (manager == NULL || mesh == NULL || out_metadata == NULL)
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
