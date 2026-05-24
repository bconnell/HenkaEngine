#include "henka_internal.h"

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
        henka_texture_destroy(manager->texture_entries[index].texture);
        henka_free(manager->texture_entries[index].key);
    }

    henka_free(manager->shader_entries);
    henka_free(manager->texture_entries);
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

    result = henka_shader_create_from_files(manager->engine, vertex_path, fragment_path, &shader);
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
    manager->shader_count += 1U;
    *out_shader = shader;
    return HENKA_SUCCESS;
}

henka_result henka_assets_load_texture(henka_asset_manager* manager, const char* path, henka_texture** out_texture)
{
    char* key;
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

    result = henka_texture_create_from_file(manager->engine, path, &texture);
    if (result != HENKA_SUCCESS)
    {
        HENKA_LOG_ERROR("Using the error texture because '%s' could not be loaded", path);
        *out_texture = manager->error_texture;
        return HENKA_SUCCESS;
    }

    if (manager->texture_count == manager->texture_capacity)
    {
        result = henka_asset_manager_grow_textures(manager);
        if (result != HENKA_SUCCESS)
        {
            henka_texture_destroy(texture);
            return result;
        }
    }

    key = henka_duplicate_string(path);
    if (key == NULL)
    {
        henka_texture_destroy(texture);
        return HENKA_ERROR_OUT_OF_MEMORY;
    }

    manager->texture_entries[manager->texture_count].key = key;
    manager->texture_entries[manager->texture_count].texture = texture;
    manager->texture_count += 1U;
    *out_texture = texture;
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
