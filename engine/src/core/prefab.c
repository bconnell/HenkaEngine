#include <henka/prefab.h>
#include <henka/assets.h>
#include <henka/memory.h>
#include <henka/persistence.h>

#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "checked.h"
#include "../henka_internal.h"

#define HENKA_PREFAB_MAX_HIERARCHY_DEPTH 256U

typedef struct henka_prefab_entry
{
    henka_prefab_source_id source_id;
    henka_entity source_entity;
    henka_entity source_parent;
    henka_entity source_selection_owner;
    size_t parent_index;
    size_t selection_owner_index;
    char* name;
    char* tag;
    char* material_name;
    char* interaction_prompt;
    henka_transform local_transform;
    henka_mesh* mesh;
    henka_material material;
    bool has_explicit_material;
    const henka_material_asset* material_asset;
    uint64_t material_asset_revision;
    bool material_asset_overridden;
    bool visible;
    bool renderer_enabled;
    uint32_t flags;
    bool has_local_bounds;
    henka_bounds local_bounds;
    henka_interaction_desc interaction;
} henka_prefab_entry;

struct henka_prefab
{
    henka_prefab_entry* entries;
    size_t entity_count;
    size_t root_index;
    uint64_t revision;
    henka_prefab_source_id next_source_id;
    char* asset_path;
};

struct henka_prefab_instance
{
    henka_scene* target_scene;
    const henka_prefab* prefab;
    henka_entity* entities;
    henka_prefab_source_id* source_ids;
    henka_transform* base_local_transforms;
    henka_transform* local_transform_overrides;
    bool* transform_override_flags;
    size_t entity_count;
    size_t root_index;
    uint64_t prefab_revision;
};

static bool henka_prefab_transform_equal(
    henka_transform left,
    henka_transform right)
{
    return left.position.x == right.position.x &&
        left.position.y == right.position.y &&
        left.position.z == right.position.z &&
        left.rotation.x == right.rotation.x &&
        left.rotation.y == right.rotation.y &&
        left.rotation.z == right.rotation.z &&
        left.rotation.w == right.rotation.w &&
        left.scale.x == right.scale.x &&
        left.scale.y == right.scale.y &&
        left.scale.z == right.scale.z;
}

static henka_result henka_prefab_duplicate_text(const char* source, char** out_copy)
{
    size_t length;
    size_t allocation_size;
    char* copy;

    if (out_copy == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_copy = NULL;
    if (source == NULL)
    {
        return HENKA_SUCCESS;
    }
    if (!henka_checked_c_string_length(
            source, HENKA_MAX_SCENE_TEXT_BYTES, &length))
    {
        return HENKA_ERROR_LIMIT;
    }
    if (!henka_checked_size_add(length, 1U, &allocation_size))
    {
        return HENKA_ERROR_NUMERIC_RANGE;
    }
    copy = henka_malloc(allocation_size);
    if (copy == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    memcpy(copy, source, allocation_size);
    *out_copy = copy;
    return HENKA_SUCCESS;
}

static henka_result henka_prefab_duplicate_asset_path(
    const char* source,
    char** out_copy)
{
    char* normalized;

    if (out_copy == NULL || source == NULL || source[0] == '\0')
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_copy = NULL;
    normalized = NULL;
    {
        const henka_result result = henka_path_resolve_confined(
            "", source, &normalized);
        if (result != HENKA_SUCCESS)
        {
            return result;
        }
    }
    if (strlen(normalized) > HENKA_PREFAB_MAX_ASSET_PATH_BYTES)
    {
        henka_free(normalized);
        return HENKA_ERROR_LIMIT;
    }
    *out_copy = normalized;
    return HENKA_SUCCESS;
}

static void henka_prefab_entry_destroy(henka_prefab_entry* entry)
{
    if (entry == NULL)
    {
        return;
    }
    henka_free(entry->name);
    henka_free(entry->tag);
    henka_free(entry->material_name);
    henka_free(entry->interaction_prompt);
    memset(entry, 0, sizeof(*entry));
}

static bool henka_prefab_is_descendant_or_root(
    const henka_scene* scene,
    henka_entity root,
    henka_entity candidate)
{
    henka_entity cursor = candidate;
    size_t depth;

    for (depth = 0U; depth < HENKA_PREFAB_MAX_HIERARCHY_DEPTH; ++depth)
    {
        henka_entity parent;

        if (cursor == root)
        {
            return true;
        }
        if (cursor == HENKA_INVALID_ENTITY ||
            henka_scene_get_entity_parent(scene, cursor, &parent) != HENKA_SUCCESS)
        {
            return false;
        }
        cursor = parent;
    }
    return false;
}

static size_t henka_prefab_find_entry(
    const henka_prefab* prefab,
    henka_entity source_entity)
{
    size_t index;

    for (index = 0U; index < prefab->entity_count; ++index)
    {
        if (prefab->entries[index].source_entity == source_entity)
        {
            return index;
        }
    }
    return SIZE_MAX;
}

henka_result henka_prefab_create_from_scene(
    const henka_scene* source_scene,
    henka_entity root_entity,
    henka_prefab** out_prefab)
{
    henka_prefab* prefab;
    size_t scene_count;
    size_t allocation_size;
    size_t count;
    size_t index;
    henka_result result;

    if (out_prefab == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_prefab = NULL;
    if (source_scene == NULL ||
        !henka_scene_is_entity_valid(source_scene, root_entity))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    scene_count = henka_scene_get_entity_count(source_scene);
    count = 0U;
    for (index = 0U; index < scene_count; ++index)
    {
        const henka_entity entity = henka_scene_get_entity_at_index(source_scene, index);
        if (entity != HENKA_INVALID_ENTITY &&
            henka_prefab_is_descendant_or_root(source_scene, root_entity, entity))
        {
            if (count >= HENKA_MAX_PREFAB_ENTITIES)
            {
                return HENKA_ERROR_LIMIT;
            }
            count += 1U;
        }
    }
    if (count == 0U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    prefab = henka_malloc(sizeof(*prefab));
    if (prefab == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    prefab->entries = NULL;
    prefab->entity_count = count;
    prefab->root_index = SIZE_MAX;
    prefab->revision = UINT64_C(1);
    prefab->next_source_id = HENKA_INVALID_PREFAB_SOURCE_ID;
    prefab->asset_path = NULL;
    if (!henka_checked_size_multiply(count, sizeof(*prefab->entries), &allocation_size))
    {
        henka_free(prefab);
        return HENKA_ERROR_NUMERIC_RANGE;
    }
    prefab->entries = henka_malloc(allocation_size);
    if (prefab->entries == NULL)
    {
        henka_free(prefab);
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    memset(prefab->entries, 0, allocation_size);
    for (index = 0U; index < count; ++index)
    {
        prefab->entries[index].parent_index = SIZE_MAX;
    }

    count = 0U;
    scene_count = henka_scene_get_entity_count(source_scene);
    for (index = 0U; index < scene_count; ++index)
    {
        const henka_entity entity = henka_scene_get_entity_at_index(source_scene, index);
        henka_prefab_entry* entry;
        henka_scene_object_info info;
        henka_material material;
        henka_interaction_desc interaction;
        const henka_material_asset* material_asset;
        uint64_t asset_revision;
        bool asset_overridden;
        henka_entity selection_owner;

        if (entity == HENKA_INVALID_ENTITY ||
            !henka_prefab_is_descendant_or_root(source_scene, root_entity, entity))
        {
            continue;
        }
        entry = &prefab->entries[count];
        result = henka_scene_get_entity_info(source_scene, entity, &info);
        if (result != HENKA_SUCCESS)
        {
            henka_prefab_destroy(prefab);
            return result;
        }
        result = henka_scene_get_entity_local_transform(
            source_scene, entity, &entry->local_transform);
        if (result != HENKA_SUCCESS)
        {
            henka_prefab_destroy(prefab);
            return result;
        }
        result = henka_scene_get_entity_mesh(source_scene, entity, &entry->mesh);
        if (result != HENKA_SUCCESS)
        {
            henka_prefab_destroy(prefab);
            return result;
        }
        result = henka_scene_get_entity_material(source_scene, entity, &material);
        if (result != HENKA_SUCCESS)
        {
            henka_prefab_destroy(prefab);
            return result;
        }
        result = henka_scene_get_entity_material_asset(source_scene, entity, &material_asset);
        if (result != HENKA_SUCCESS)
        {
            henka_prefab_destroy(prefab);
            return result;
        }
        result = henka_scene_get_material_asset_state(
            source_scene, entity, &asset_revision, &asset_overridden);
        if (result != HENKA_SUCCESS)
        {
            henka_prefab_destroy(prefab);
            return result;
        }
        result = henka_scene_get_entity_interaction(source_scene, entity, &interaction);
        if (result != HENKA_SUCCESS)
        {
            henka_prefab_destroy(prefab);
            return result;
        }
        result = henka_scene_get_entity_flags(source_scene, entity, &entry->flags);
        if (result != HENKA_SUCCESS)
        {
            henka_prefab_destroy(prefab);
            return result;
        }
        entry->source_selection_owner = entity;
        result = henka_scene_get_entity_selection_owner(
            source_scene, entity, &selection_owner);
        if (result == HENKA_SUCCESS)
        {
            entry->source_selection_owner = selection_owner;
        }
        else if (result != HENKA_ERROR_UNKNOWN ||
                 !henka_scene_is_entity_helper(source_scene, entity))
        {
            henka_prefab_destroy(prefab);
            return result;
        }
        entry->source_entity = entity;
        entry->source_id = (henka_prefab_source_id)(count + 1U);
        if (entity == root_entity)
        {
            prefab->root_index = count;
        }
        entry->visible = info.visible;
        entry->renderer_enabled = info.renderer_enabled;
        entry->has_local_bounds = info.has_bounds;
        entry->local_bounds = info.local_bounds;
        entry->material = material;
        /* Scene-created entities intentionally carry the engine default
         * material, whose shader handle is not bound until a renderer or
         * asset manager supplies one. It is not valid input to the public
         * material setter, so leave the target's equivalent default intact. */
        entry->has_explicit_material = material.shader != NULL;
        entry->material_asset = material_asset;
        entry->material_asset_revision = asset_revision;
        entry->material_asset_overridden = asset_overridden;
        entry->interaction = interaction;
        result = henka_scene_get_entity_parent(source_scene, entity, &entry->source_parent);
        if (result != HENKA_SUCCESS)
        {
            henka_prefab_destroy(prefab);
            return result;
        }
        result = henka_prefab_duplicate_text(info.name, &entry->name);
        if (result != HENKA_SUCCESS)
        {
            henka_prefab_destroy(prefab);
            return result;
        }
        result = henka_prefab_duplicate_text(info.tag, &entry->tag);
        if (result != HENKA_SUCCESS)
        {
            henka_prefab_destroy(prefab);
            return result;
        }
        result = henka_prefab_duplicate_text(material.name, &entry->material_name);
        if (result != HENKA_SUCCESS)
        {
            henka_prefab_destroy(prefab);
            return result;
        }
        result = henka_prefab_duplicate_text(interaction.prompt, &entry->interaction_prompt);
        if (result != HENKA_SUCCESS)
        {
            henka_prefab_destroy(prefab);
            return result;
        }
        entry->material.name = entry->material_name;
        entry->interaction.prompt = entry->interaction_prompt;
        count += 1U;
    }

    if (prefab->root_index == SIZE_MAX)
    {
        henka_prefab_destroy(prefab);
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    prefab->next_source_id = (henka_prefab_source_id)(prefab->entity_count + 1U);

    for (index = 0U; index < prefab->entity_count; ++index)
    {
        prefab->entries[index].selection_owner_index =
            henka_prefab_find_entry(
                prefab, prefab->entries[index].source_selection_owner);
        /* A logical owner outside the captured subtree must not alias the
         * source scene or another prefab instance. New entities already own
         * themselves, so retain that isolated default in this case. */
        if (prefab->entries[index].source_entity != root_entity)
        {
            prefab->entries[index].parent_index = henka_prefab_find_entry(
                prefab,
                prefab->entries[index].source_parent);
            if (prefab->entries[index].parent_index == SIZE_MAX)
            {
                henka_prefab_destroy(prefab);
                return HENKA_ERROR_INVALID_ARGUMENT;
            }
        }
    }

    *out_prefab = prefab;
    return HENKA_SUCCESS;
}

void henka_prefab_destroy(henka_prefab* prefab)
{
    size_t index;

    if (prefab == NULL)
    {
        return;
    }
    for (index = 0U; index < prefab->entity_count; ++index)
    {
        henka_prefab_entry_destroy(&prefab->entries[index]);
    }
    henka_free(prefab->entries);
    henka_free(prefab->asset_path);
    henka_free(prefab);
}

size_t henka_prefab_get_entity_count(const henka_prefab* prefab)
{
    return prefab == NULL ? 0U : prefab->entity_count;
}

uint64_t henka_prefab_get_revision(const henka_prefab* prefab)
{
    return prefab == NULL ? 0U : prefab->revision;
}

henka_result henka_prefab_set_asset_path(
    henka_prefab* prefab,
    const char* project_relative_path)
{
    char* normalized;
    henka_result result;

    if (prefab == NULL || project_relative_path == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    normalized = NULL;
    result = henka_prefab_duplicate_asset_path(
        project_relative_path, &normalized);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    henka_free(prefab->asset_path);
    prefab->asset_path = normalized;
    return HENKA_SUCCESS;
}

const char* henka_prefab_get_asset_path(const henka_prefab* prefab)
{
    return prefab == NULL ? NULL : prefab->asset_path;
}

static bool henka_prefab_make_key(
    char* buffer,
    size_t buffer_size,
    size_t index,
    const char* suffix)
{
    int written;

    if (buffer == NULL || buffer_size == 0U || suffix == NULL)
    {
        return false;
    }
    written = snprintf(buffer, buffer_size, "entry.%zu.%s", index, suffix);
    return written >= 0 && (size_t)written < buffer_size;
}

static henka_result henka_prefab_settings_set_string(
    henka_settings* settings,
    size_t index,
    const char* suffix,
    const char* value)
{
    char key[128];

    if (!henka_prefab_make_key(key, sizeof(key), index, suffix) ||
        value == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return henka_settings_set_string(settings, key, value);
}

static henka_result henka_prefab_settings_set_float(
    henka_settings* settings,
    size_t index,
    const char* suffix,
    float value)
{
    char key[128];

    if (!henka_prefab_make_key(key, sizeof(key), index, suffix))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return henka_settings_set_float(settings, key, value);
}

static henka_result henka_prefab_settings_set_int(
    henka_settings* settings,
    size_t index,
    const char* suffix,
    int value)
{
    char key[128];

    if (!henka_prefab_make_key(key, sizeof(key), index, suffix))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return henka_settings_set_int(settings, key, value);
}

static henka_result henka_prefab_settings_set_bool(
    henka_settings* settings,
    size_t index,
    const char* suffix,
    bool value)
{
    char key[128];

    if (!henka_prefab_make_key(key, sizeof(key), index, suffix))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return henka_settings_set_bool(settings, key, value);
}

static henka_result henka_prefab_settings_set_u64(
    henka_settings* settings,
    const char* key,
    uint64_t value)
{
    char buffer[32];
    int written;

    if (settings == NULL || key == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    written = snprintf(buffer, sizeof(buffer), "%llu", (unsigned long long)value);
    if (written < 0 || (size_t)written >= sizeof(buffer))
    {
        return HENKA_ERROR_NUMERIC_RANGE;
    }
    return henka_settings_set_string(settings, key, buffer);
}

static henka_result henka_prefab_settings_set_indexed_u64(
    henka_settings* settings,
    size_t index,
    const char* suffix,
    uint64_t value)
{
    char key[128];
    char buffer[32];
    int written;

    if (!henka_prefab_make_key(key, sizeof(key), index, suffix))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    written = snprintf(buffer, sizeof(buffer), "%llu", (unsigned long long)value);
    if (written < 0 || (size_t)written >= sizeof(buffer))
    {
        return HENKA_ERROR_NUMERIC_RANGE;
    }
    return henka_settings_set_string(settings, key, buffer);
}

static bool henka_prefab_settings_get_u64(
    const henka_settings* settings,
    const char* key,
    uint64_t* out_value)
{
    const char* text;
    char* end;
    unsigned long long value;

    if (out_value != NULL)
    {
        *out_value = 0U;
    }
    if (settings == NULL || key == NULL || out_value == NULL ||
        !henka_settings_has_key(settings, key))
    {
        return false;
    }
    text = henka_settings_get_string(settings, key, NULL);
    if (text == NULL || text[0] == '\0')
    {
        return false;
    }
    errno = 0;
    end = NULL;
    value = strtoull(text, &end, 10);
    if (errno == ERANGE || end == text || *end != '\0')
    {
        return false;
    }
    *out_value = (uint64_t)value;
    return true;
}

static bool henka_prefab_settings_get_int(
    const henka_settings* settings,
    const char* key,
    int* out_value)
{
    const char* text;
    char* end;
    long value;

    if (out_value != NULL)
    {
        *out_value = 0;
    }
    if (settings == NULL || key == NULL || out_value == NULL ||
        !henka_settings_has_key(settings, key))
    {
        return false;
    }
    text = henka_settings_get_string(settings, key, NULL);
    if (text == NULL || text[0] == '\0')
    {
        return false;
    }
    errno = 0;
    end = NULL;
    value = strtol(text, &end, 10);
    if (errno == ERANGE || end == text || *end != '\0' ||
        value < INT_MIN || value > INT_MAX)
    {
        return false;
    }
    *out_value = (int)value;
    return true;
}

static bool henka_prefab_settings_get_float(
    const henka_settings* settings,
    const char* key,
    float* out_value)
{
    const char* text;
    char* end;
    float value;

    if (out_value != NULL)
    {
        *out_value = 0.0f;
    }
    if (settings == NULL || key == NULL || out_value == NULL ||
        !henka_settings_has_key(settings, key))
    {
        return false;
    }
    text = henka_settings_get_string(settings, key, NULL);
    if (text == NULL || text[0] == '\0')
    {
        return false;
    }
    errno = 0;
    end = NULL;
    value = strtof(text, &end);
    if (errno == ERANGE || end == text || *end != '\0' || !isfinite(value))
    {
        return false;
    }
    *out_value = value;
    return true;
}

static bool henka_prefab_settings_get_bool(
    const henka_settings* settings,
    const char* key,
    bool* out_value)
{
    const char* text;

    if (out_value != NULL)
    {
        *out_value = false;
    }
    if (settings == NULL || key == NULL || out_value == NULL ||
        !henka_settings_has_key(settings, key))
    {
        return false;
    }
    text = henka_settings_get_string(settings, key, NULL);
    if (text == NULL)
    {
        return false;
    }
    if (strcmp(text, "true") == 0)
    {
        *out_value = true;
        return true;
    }
    if (strcmp(text, "false") == 0)
    {
        *out_value = false;
        return true;
    }
    return false;
}

static bool henka_prefab_transform_is_valid(henka_transform transform)
{
    const float rotation_length_squared =
        transform.rotation.x * transform.rotation.x +
        transform.rotation.y * transform.rotation.y +
        transform.rotation.z * transform.rotation.z +
        transform.rotation.w * transform.rotation.w;

    return isfinite(transform.position.x) &&
        isfinite(transform.position.y) &&
        isfinite(transform.position.z) &&
        isfinite(transform.scale.x) &&
        isfinite(transform.scale.y) &&
        isfinite(transform.scale.z) &&
        isfinite(transform.rotation.x) &&
        isfinite(transform.rotation.y) &&
        isfinite(transform.rotation.z) &&
        isfinite(transform.rotation.w) &&
        transform.scale.x > 0.0f &&
        transform.scale.y > 0.0f &&
        transform.scale.z > 0.0f &&
        isfinite(rotation_length_squared) &&
        rotation_length_squared > FLT_EPSILON;
}

static bool henka_prefab_material_has_borrowed_dependencies(
    const henka_material* material)
{
    size_t index;

    if (material == NULL)
    {
        return true;
    }
    if (material->base_color_texture != NULL ||
        material->normal_texture != NULL ||
        material->metallic_roughness_texture != NULL ||
        material->occlusion_texture != NULL ||
        material->emissive_texture != NULL ||
        material->transmission_texture != NULL ||
        material->thickness_texture != NULL ||
        material->terrain_layers_enabled)
    {
        return true;
    }
    for (index = 0U; index < HENKA_MATERIAL_TERRAIN_LAYER_COUNT; ++index)
    {
        const henka_material_layer* layer = &material->terrain_layers[index];
        if (layer->base_color_texture != NULL ||
            layer->normal_texture != NULL ||
            layer->metallic_roughness_texture != NULL)
        {
            return true;
        }
    }
    return false;
}

static henka_result henka_prefab_save_inline_material(
    henka_settings* settings,
    size_t index,
    const henka_material* material)
{
    henka_result result = HENKA_SUCCESS;

    if (settings == NULL || material == NULL || material->shader == NULL ||
        henka_prefab_material_has_borrowed_dependencies(material) ||
        henka_material_validate_values(material) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
#define HENKA_PREFAB_SAVE_MATERIAL_INT(suffix, field) \
    do { if (result == HENKA_SUCCESS) result = henka_prefab_settings_set_int(settings, index, suffix, (int)(field)); } while (0)
#define HENKA_PREFAB_SAVE_MATERIAL_FLOAT(suffix, field) \
    do { if (result == HENKA_SUCCESS) result = henka_prefab_settings_set_float(settings, index, suffix, (field)); } while (0)
#define HENKA_PREFAB_SAVE_MATERIAL_BOOL(suffix, field) \
    do { if (result == HENKA_SUCCESS) result = henka_prefab_settings_set_bool(settings, index, suffix, (field)); } while (0)
    HENKA_PREFAB_SAVE_MATERIAL_INT("material.type", material->type);
    HENKA_PREFAB_SAVE_MATERIAL_INT("material.base_color_uv_set", material->base_color_uv_set);
    HENKA_PREFAB_SAVE_MATERIAL_INT("material.normal_uv_set", material->normal_uv_set);
    HENKA_PREFAB_SAVE_MATERIAL_INT("material.metallic_roughness_uv_set", material->metallic_roughness_uv_set);
    HENKA_PREFAB_SAVE_MATERIAL_INT("material.occlusion_uv_set", material->occlusion_uv_set);
    HENKA_PREFAB_SAVE_MATERIAL_INT("material.emissive_uv_set", material->emissive_uv_set);
    HENKA_PREFAB_SAVE_MATERIAL_INT("material.transmission_uv_set", material->transmission_uv_set);
    HENKA_PREFAB_SAVE_MATERIAL_INT("material.thickness_uv_set", material->thickness_uv_set);
    HENKA_PREFAB_SAVE_MATERIAL_FLOAT("material.base_color.x", material->base_color.x);
    HENKA_PREFAB_SAVE_MATERIAL_FLOAT("material.base_color.y", material->base_color.y);
    HENKA_PREFAB_SAVE_MATERIAL_FLOAT("material.base_color.z", material->base_color.z);
    HENKA_PREFAB_SAVE_MATERIAL_FLOAT("material.base_color.w", material->base_color.w);
    HENKA_PREFAB_SAVE_MATERIAL_FLOAT("material.emissive_color.x", material->emissive_color.x);
    HENKA_PREFAB_SAVE_MATERIAL_FLOAT("material.emissive_color.y", material->emissive_color.y);
    HENKA_PREFAB_SAVE_MATERIAL_FLOAT("material.emissive_color.z", material->emissive_color.z);
    HENKA_PREFAB_SAVE_MATERIAL_FLOAT("material.metallic", material->metallic);
    HENKA_PREFAB_SAVE_MATERIAL_FLOAT("material.roughness", material->roughness);
    HENKA_PREFAB_SAVE_MATERIAL_FLOAT("material.specular_factor", material->specular_factor);
    HENKA_PREFAB_SAVE_MATERIAL_FLOAT("material.specular_color.x", material->specular_color.x);
    HENKA_PREFAB_SAVE_MATERIAL_FLOAT("material.specular_color.y", material->specular_color.y);
    HENKA_PREFAB_SAVE_MATERIAL_FLOAT("material.specular_color.z", material->specular_color.z);
    HENKA_PREFAB_SAVE_MATERIAL_FLOAT("material.ior", material->ior);
    HENKA_PREFAB_SAVE_MATERIAL_FLOAT("material.transmission", material->transmission);
    HENKA_PREFAB_SAVE_MATERIAL_FLOAT("material.thickness", material->thickness);
    HENKA_PREFAB_SAVE_MATERIAL_FLOAT("material.attenuation_distance", material->attenuation_distance);
    HENKA_PREFAB_SAVE_MATERIAL_FLOAT("material.attenuation_color.x", material->attenuation_color.x);
    HENKA_PREFAB_SAVE_MATERIAL_FLOAT("material.attenuation_color.y", material->attenuation_color.y);
    HENKA_PREFAB_SAVE_MATERIAL_FLOAT("material.attenuation_color.z", material->attenuation_color.z);
    HENKA_PREFAB_SAVE_MATERIAL_FLOAT("material.subsurface", material->subsurface);
    HENKA_PREFAB_SAVE_MATERIAL_FLOAT("material.subsurface_color.x", material->subsurface_color.x);
    HENKA_PREFAB_SAVE_MATERIAL_FLOAT("material.subsurface_color.y", material->subsurface_color.y);
    HENKA_PREFAB_SAVE_MATERIAL_FLOAT("material.subsurface_color.z", material->subsurface_color.z);
    HENKA_PREFAB_SAVE_MATERIAL_FLOAT("material.normal_scale", material->normal_scale);
    HENKA_PREFAB_SAVE_MATERIAL_FLOAT("material.occlusion_strength", material->occlusion_strength);
    HENKA_PREFAB_SAVE_MATERIAL_FLOAT("material.emissive_strength", material->emissive_strength);
    HENKA_PREFAB_SAVE_MATERIAL_FLOAT("material.clearcoat", material->clearcoat);
    HENKA_PREFAB_SAVE_MATERIAL_FLOAT("material.clearcoat_roughness", material->clearcoat_roughness);
    HENKA_PREFAB_SAVE_MATERIAL_FLOAT("material.alpha_cutoff", material->alpha_cutoff);
    HENKA_PREFAB_SAVE_MATERIAL_FLOAT("material.sheen_color.x", material->sheen_color.x);
    HENKA_PREFAB_SAVE_MATERIAL_FLOAT("material.sheen_color.y", material->sheen_color.y);
    HENKA_PREFAB_SAVE_MATERIAL_FLOAT("material.sheen_color.z", material->sheen_color.z);
    HENKA_PREFAB_SAVE_MATERIAL_FLOAT("material.sheen_roughness", material->sheen_roughness);
    HENKA_PREFAB_SAVE_MATERIAL_BOOL("material.use_texture", material->use_texture);
    HENKA_PREFAB_SAVE_MATERIAL_BOOL("material.use_lighting", material->use_lighting);
    HENKA_PREFAB_SAVE_MATERIAL_BOOL("material.depth_test", material->depth_test);
    HENKA_PREFAB_SAVE_MATERIAL_INT("material.alpha_mode", material->alpha_mode);
    HENKA_PREFAB_SAVE_MATERIAL_BOOL("material.double_sided", material->double_sided);
    HENKA_PREFAB_SAVE_MATERIAL_BOOL("material.cast_shadows", material->cast_shadows);
    HENKA_PREFAB_SAVE_MATERIAL_BOOL("material.receive_shadows", material->receive_shadows);
#undef HENKA_PREFAB_SAVE_MATERIAL_BOOL
#undef HENKA_PREFAB_SAVE_MATERIAL_FLOAT
#undef HENKA_PREFAB_SAVE_MATERIAL_INT
    return result;
}

static bool henka_prefab_load_inline_material(
    const henka_settings* settings,
    size_t index,
    henka_shader* shader,
    henka_material* out_material)
{
    int integer;
    float value;
    bool boolean;

    if (settings == NULL || shader == NULL || out_material == NULL)
    {
        return false;
    }
    *out_material = henka_material_default();
    out_material->shader = shader;
#define HENKA_PREFAB_LOAD_MATERIAL_INT(suffix, field) \
    do { char key[128]; if (!henka_prefab_make_key(key, sizeof(key), index, suffix) || !henka_prefab_settings_get_int(settings, key, &integer)) return false; (field) = integer; } while (0)
#define HENKA_PREFAB_LOAD_MATERIAL_FLOAT(suffix, field) \
    do { char key[128]; if (!henka_prefab_make_key(key, sizeof(key), index, suffix) || !henka_prefab_settings_get_float(settings, key, &value)) return false; (field) = value; } while (0)
#define HENKA_PREFAB_LOAD_MATERIAL_BOOL(suffix, field) \
    do { char key[128]; if (!henka_prefab_make_key(key, sizeof(key), index, suffix) || !henka_prefab_settings_get_bool(settings, key, &boolean)) return false; (field) = boolean; } while (0)
    HENKA_PREFAB_LOAD_MATERIAL_INT("material.type", out_material->type);
    HENKA_PREFAB_LOAD_MATERIAL_INT("material.base_color_uv_set", out_material->base_color_uv_set);
    HENKA_PREFAB_LOAD_MATERIAL_INT("material.normal_uv_set", out_material->normal_uv_set);
    HENKA_PREFAB_LOAD_MATERIAL_INT("material.metallic_roughness_uv_set", out_material->metallic_roughness_uv_set);
    HENKA_PREFAB_LOAD_MATERIAL_INT("material.occlusion_uv_set", out_material->occlusion_uv_set);
    HENKA_PREFAB_LOAD_MATERIAL_INT("material.emissive_uv_set", out_material->emissive_uv_set);
    HENKA_PREFAB_LOAD_MATERIAL_INT("material.transmission_uv_set", out_material->transmission_uv_set);
    HENKA_PREFAB_LOAD_MATERIAL_INT("material.thickness_uv_set", out_material->thickness_uv_set);
    HENKA_PREFAB_LOAD_MATERIAL_FLOAT("material.base_color.x", out_material->base_color.x);
    HENKA_PREFAB_LOAD_MATERIAL_FLOAT("material.base_color.y", out_material->base_color.y);
    HENKA_PREFAB_LOAD_MATERIAL_FLOAT("material.base_color.z", out_material->base_color.z);
    HENKA_PREFAB_LOAD_MATERIAL_FLOAT("material.base_color.w", out_material->base_color.w);
    HENKA_PREFAB_LOAD_MATERIAL_FLOAT("material.emissive_color.x", out_material->emissive_color.x);
    HENKA_PREFAB_LOAD_MATERIAL_FLOAT("material.emissive_color.y", out_material->emissive_color.y);
    HENKA_PREFAB_LOAD_MATERIAL_FLOAT("material.emissive_color.z", out_material->emissive_color.z);
    HENKA_PREFAB_LOAD_MATERIAL_FLOAT("material.metallic", out_material->metallic);
    HENKA_PREFAB_LOAD_MATERIAL_FLOAT("material.roughness", out_material->roughness);
    HENKA_PREFAB_LOAD_MATERIAL_FLOAT("material.specular_factor", out_material->specular_factor);
    HENKA_PREFAB_LOAD_MATERIAL_FLOAT("material.specular_color.x", out_material->specular_color.x);
    HENKA_PREFAB_LOAD_MATERIAL_FLOAT("material.specular_color.y", out_material->specular_color.y);
    HENKA_PREFAB_LOAD_MATERIAL_FLOAT("material.specular_color.z", out_material->specular_color.z);
    HENKA_PREFAB_LOAD_MATERIAL_FLOAT("material.ior", out_material->ior);
    HENKA_PREFAB_LOAD_MATERIAL_FLOAT("material.transmission", out_material->transmission);
    HENKA_PREFAB_LOAD_MATERIAL_FLOAT("material.thickness", out_material->thickness);
    HENKA_PREFAB_LOAD_MATERIAL_FLOAT("material.attenuation_distance", out_material->attenuation_distance);
    HENKA_PREFAB_LOAD_MATERIAL_FLOAT("material.attenuation_color.x", out_material->attenuation_color.x);
    HENKA_PREFAB_LOAD_MATERIAL_FLOAT("material.attenuation_color.y", out_material->attenuation_color.y);
    HENKA_PREFAB_LOAD_MATERIAL_FLOAT("material.attenuation_color.z", out_material->attenuation_color.z);
    HENKA_PREFAB_LOAD_MATERIAL_FLOAT("material.subsurface", out_material->subsurface);
    HENKA_PREFAB_LOAD_MATERIAL_FLOAT("material.subsurface_color.x", out_material->subsurface_color.x);
    HENKA_PREFAB_LOAD_MATERIAL_FLOAT("material.subsurface_color.y", out_material->subsurface_color.y);
    HENKA_PREFAB_LOAD_MATERIAL_FLOAT("material.subsurface_color.z", out_material->subsurface_color.z);
    HENKA_PREFAB_LOAD_MATERIAL_FLOAT("material.normal_scale", out_material->normal_scale);
    HENKA_PREFAB_LOAD_MATERIAL_FLOAT("material.occlusion_strength", out_material->occlusion_strength);
    HENKA_PREFAB_LOAD_MATERIAL_FLOAT("material.emissive_strength", out_material->emissive_strength);
    HENKA_PREFAB_LOAD_MATERIAL_FLOAT("material.clearcoat", out_material->clearcoat);
    HENKA_PREFAB_LOAD_MATERIAL_FLOAT("material.clearcoat_roughness", out_material->clearcoat_roughness);
    HENKA_PREFAB_LOAD_MATERIAL_FLOAT("material.alpha_cutoff", out_material->alpha_cutoff);
    HENKA_PREFAB_LOAD_MATERIAL_FLOAT("material.sheen_color.x", out_material->sheen_color.x);
    HENKA_PREFAB_LOAD_MATERIAL_FLOAT("material.sheen_color.y", out_material->sheen_color.y);
    HENKA_PREFAB_LOAD_MATERIAL_FLOAT("material.sheen_color.z", out_material->sheen_color.z);
    HENKA_PREFAB_LOAD_MATERIAL_FLOAT("material.sheen_roughness", out_material->sheen_roughness);
    HENKA_PREFAB_LOAD_MATERIAL_BOOL("material.use_texture", out_material->use_texture);
    HENKA_PREFAB_LOAD_MATERIAL_BOOL("material.use_lighting", out_material->use_lighting);
    HENKA_PREFAB_LOAD_MATERIAL_BOOL("material.depth_test", out_material->depth_test);
    HENKA_PREFAB_LOAD_MATERIAL_INT("material.alpha_mode", out_material->alpha_mode);
    HENKA_PREFAB_LOAD_MATERIAL_BOOL("material.double_sided", out_material->double_sided);
    HENKA_PREFAB_LOAD_MATERIAL_BOOL("material.cast_shadows", out_material->cast_shadows);
    HENKA_PREFAB_LOAD_MATERIAL_BOOL("material.receive_shadows", out_material->receive_shadows);
#undef HENKA_PREFAB_LOAD_MATERIAL_BOOL
#undef HENKA_PREFAB_LOAD_MATERIAL_FLOAT
#undef HENKA_PREFAB_LOAD_MATERIAL_INT
    return henka_material_validate(out_material) == HENKA_SUCCESS;
}

static henka_result henka_prefab_save_optional_text(
    henka_settings* settings,
    size_t index,
    const char* presence_suffix,
    const char* value_suffix,
    const char* value)
{
    henka_result result;

    result = henka_prefab_settings_set_bool(
        settings, index, presence_suffix, value != NULL);
    if (result == HENKA_SUCCESS && value != NULL)
    {
        result = henka_prefab_settings_set_string(
            settings, index, value_suffix, value);
    }
    return result;
}

static henka_result henka_prefab_save_entry(
    const henka_prefab* prefab,
    const henka_asset_manager* asset_manager,
    henka_settings* settings,
    size_t index)
{
    const henka_prefab_entry* entry;
    henka_asset_metadata metadata;
    char* normalized_path;
    henka_result result;
    int material_mode;

    if (prefab == NULL || settings == NULL || index >= prefab->entity_count)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    entry = &prefab->entries[index];
    if (entry->name == NULL || entry->name[0] == '\0' ||
        !henka_prefab_transform_is_valid(entry->local_transform) ||
        (entry->material_asset != NULL && !entry->has_explicit_material))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_prefab_settings_set_indexed_u64(
        settings, index, "source_id", entry->source_id);
    if (result == HENKA_SUCCESS)
    {
        result = henka_prefab_settings_set_string(
            settings, index, "name", entry->name);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_prefab_save_optional_text(
            settings, index, "has_tag", "tag", entry->tag);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_prefab_save_optional_text(
            settings,
            index,
            "has_material_name",
            "material_name",
            (entry->has_explicit_material || entry->material_asset != NULL) ?
                entry->material_name : NULL);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_prefab_save_optional_text(
            settings, index, "has_interaction_prompt", "interaction_prompt", entry->interaction_prompt);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_prefab_settings_set_indexed_u64(
            settings, index, "parent_index", (uint64_t)entry->parent_index);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_prefab_settings_set_indexed_u64(
            settings, index, "selection_owner_index", (uint64_t)entry->selection_owner_index);
    }
#define HENKA_PREFAB_SAVE_ENTRY_FLOAT(suffix, field) \
    do { if (result == HENKA_SUCCESS) result = henka_prefab_settings_set_float(settings, index, suffix, (field)); } while (0)
#define HENKA_PREFAB_SAVE_ENTRY_BOOL(suffix, field) \
    do { if (result == HENKA_SUCCESS) result = henka_prefab_settings_set_bool(settings, index, suffix, (field)); } while (0)
    HENKA_PREFAB_SAVE_ENTRY_FLOAT("transform.position.x", entry->local_transform.position.x);
    HENKA_PREFAB_SAVE_ENTRY_FLOAT("transform.position.y", entry->local_transform.position.y);
    HENKA_PREFAB_SAVE_ENTRY_FLOAT("transform.position.z", entry->local_transform.position.z);
    HENKA_PREFAB_SAVE_ENTRY_FLOAT("transform.rotation.x", entry->local_transform.rotation.x);
    HENKA_PREFAB_SAVE_ENTRY_FLOAT("transform.rotation.y", entry->local_transform.rotation.y);
    HENKA_PREFAB_SAVE_ENTRY_FLOAT("transform.rotation.z", entry->local_transform.rotation.z);
    HENKA_PREFAB_SAVE_ENTRY_FLOAT("transform.rotation.w", entry->local_transform.rotation.w);
    HENKA_PREFAB_SAVE_ENTRY_FLOAT("transform.scale.x", entry->local_transform.scale.x);
    HENKA_PREFAB_SAVE_ENTRY_FLOAT("transform.scale.y", entry->local_transform.scale.y);
    HENKA_PREFAB_SAVE_ENTRY_FLOAT("transform.scale.z", entry->local_transform.scale.z);
    HENKA_PREFAB_SAVE_ENTRY_BOOL("visible", entry->visible);
    HENKA_PREFAB_SAVE_ENTRY_BOOL("renderer_enabled", entry->renderer_enabled);
    HENKA_PREFAB_SAVE_ENTRY_BOOL("has_local_bounds", entry->has_local_bounds);
    HENKA_PREFAB_SAVE_ENTRY_FLOAT("bounds.center.x", entry->local_bounds.center.x);
    HENKA_PREFAB_SAVE_ENTRY_FLOAT("bounds.center.y", entry->local_bounds.center.y);
    HENKA_PREFAB_SAVE_ENTRY_FLOAT("bounds.center.z", entry->local_bounds.center.z);
    HENKA_PREFAB_SAVE_ENTRY_FLOAT("bounds.extents.x", entry->local_bounds.extents.x);
    HENKA_PREFAB_SAVE_ENTRY_FLOAT("bounds.extents.y", entry->local_bounds.extents.y);
    HENKA_PREFAB_SAVE_ENTRY_FLOAT("bounds.extents.z", entry->local_bounds.extents.z);
    HENKA_PREFAB_SAVE_ENTRY_BOOL("interaction.enabled", entry->interaction.enabled);
    HENKA_PREFAB_SAVE_ENTRY_FLOAT("interaction.max_distance", entry->interaction.max_distance);
#undef HENKA_PREFAB_SAVE_ENTRY_BOOL
#undef HENKA_PREFAB_SAVE_ENTRY_FLOAT
    if (result == HENKA_SUCCESS)
    {
        result = henka_prefab_settings_set_indexed_u64(
            settings, index, "flags", (uint64_t)entry->flags);
    }

    normalized_path = NULL;
    if (result == HENKA_SUCCESS)
    {
        result = henka_prefab_settings_set_bool(
            settings, index, "has_mesh", entry->mesh != NULL);
    }
    if (result == HENKA_SUCCESS && entry->mesh != NULL)
    {
        memset(&metadata, 0, sizeof(metadata));
        if (asset_manager == NULL ||
            henka_assets_get_mesh_metadata(asset_manager, entry->mesh, &metadata) != HENKA_SUCCESS ||
            metadata.source_path == NULL || metadata.source_path[0] == '\0')
        {
            result = HENKA_ERROR_ASSET_SOURCE;
        }
        else
        {
            result = henka_prefab_duplicate_asset_path(
                metadata.source_path, &normalized_path);
            if (result == HENKA_SUCCESS)
            {
                result = henka_prefab_settings_set_string(
                    settings, index, "mesh_path", normalized_path);
            }
        }
    }
    henka_free(normalized_path);
    normalized_path = NULL;

    material_mode = entry->has_explicit_material ?
        (entry->material_asset != NULL ? 2 : 1) : 0;
    if (result == HENKA_SUCCESS)
    {
        result = henka_prefab_settings_set_int(
            settings, index, "material_mode", material_mode);
    }
    if (result == HENKA_SUCCESS && material_mode == 2)
    {
        memset(&metadata, 0, sizeof(metadata));
        if (asset_manager == NULL ||
            henka_assets_get_material_metadata(
                asset_manager, entry->material_asset, &metadata) != HENKA_SUCCESS ||
            metadata.source_path == NULL || metadata.source_path[0] == '\0' ||
            !metadata.reload_supported)
        {
            result = HENKA_ERROR_ASSET_SOURCE;
        }
        else
        {
            result = henka_prefab_duplicate_asset_path(
                metadata.source_path, &normalized_path);
            if (result == HENKA_SUCCESS)
            {
                result = henka_prefab_settings_set_string(
                    settings, index, "material_asset_path", normalized_path);
            }
            if (result == HENKA_SUCCESS)
            {
                result = henka_prefab_settings_set_bool(
                    settings, index, "material_asset_overridden", entry->material_asset_overridden);
            }
            if (result == HENKA_SUCCESS)
            {
                result = henka_prefab_settings_set_indexed_u64(
                    settings, index, "material_asset_revision", entry->material_asset_revision);
            }
        }
    }
    else if (result == HENKA_SUCCESS && material_mode == 1)
    {
        result = henka_prefab_save_inline_material(
            settings, index, &entry->material);
    }
    henka_free(normalized_path);
    return result;
}

static bool henka_prefab_path_has_extension(
    const char* path,
    const char* extension)
{
    size_t path_length;
    size_t extension_length;
    size_t index;

    if (path == NULL || extension == NULL)
    {
        return false;
    }
    path_length = strlen(path);
    extension_length = strlen(extension);
    if (path_length < extension_length)
    {
        return false;
    }
    for (index = 0U; index < extension_length; ++index)
    {
        if (tolower((unsigned char)path[path_length - extension_length + index]) !=
            tolower((unsigned char)extension[index]))
        {
            return false;
        }
    }
    return true;
}

static henka_result henka_prefab_load_mesh(
    henka_asset_manager* asset_manager,
    const char* path,
    henka_mesh** out_mesh)
{
    if (out_mesh != NULL)
    {
        *out_mesh = NULL;
    }
    if (asset_manager == NULL || path == NULL || out_mesh == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (henka_prefab_path_has_extension(path, ".gltf") ||
        henka_prefab_path_has_extension(path, ".glb"))
    {
        return henka_assets_load_gltf_mesh(asset_manager, path, out_mesh);
    }
    if (henka_prefab_path_has_extension(path, ".obj"))
    {
        return henka_assets_load_obj_mesh(asset_manager, path, out_mesh);
    }
    return HENKA_ERROR_ASSET_SOURCE;
}

static bool henka_prefab_load_optional_text(
    const henka_settings* settings,
    size_t index,
    const char* presence_suffix,
    const char* value_suffix,
    char** out_value)
{
    char key[128];
    bool present;
    const char* value;

    if (out_value == NULL ||
        !henka_prefab_make_key(key, sizeof(key), index, presence_suffix) ||
        !henka_prefab_settings_get_bool(settings, key, &present))
    {
        return false;
    }
    *out_value = NULL;
    if (!present)
    {
        return true;
    }
    if (!henka_prefab_make_key(key, sizeof(key), index, value_suffix) ||
        !henka_settings_has_key(settings, key))
    {
        return false;
    }
    value = henka_settings_get_string(settings, key, NULL);
    return henka_prefab_duplicate_text(value, out_value) == HENKA_SUCCESS;
}

static bool henka_prefab_load_required_text(
    const henka_settings* settings,
    size_t index,
    const char* suffix,
    char** out_value)
{
    char key[128];
    const char* value;

    if (out_value == NULL ||
        !henka_prefab_make_key(key, sizeof(key), index, suffix) ||
        !henka_settings_has_key(settings, key))
    {
        return false;
    }
    value = henka_settings_get_string(settings, key, NULL);
    if (value == NULL || value[0] == '\0')
    {
        return false;
    }
    return henka_prefab_duplicate_text(value, out_value) == HENKA_SUCCESS;
}

static bool henka_prefab_u64_to_size(uint64_t value, size_t* out_size)
{
    if (out_size == NULL || value > (uint64_t)SIZE_MAX)
    {
        return false;
    }
    *out_size = (size_t)value;
    return true;
}

static henka_result henka_prefab_allocate_loaded(
    size_t entity_count,
    henka_prefab** out_prefab)
{
    henka_prefab* prefab;
    size_t allocation_size;
    size_t index;

    if (out_prefab == NULL || entity_count == 0U ||
        entity_count > HENKA_MAX_PREFAB_ENTITIES ||
        !henka_checked_size_multiply(
            entity_count, sizeof(henka_prefab_entry), &allocation_size))
    {
        return HENKA_ERROR_LIMIT;
    }
    *out_prefab = NULL;
    prefab = (henka_prefab*)henka_calloc(1U, sizeof(*prefab));
    if (prefab == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    prefab->entries = (henka_prefab_entry*)henka_calloc(
        entity_count, sizeof(*prefab->entries));
    if (prefab->entries == NULL)
    {
        henka_free(prefab);
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    prefab->entity_count = entity_count;
    prefab->root_index = SIZE_MAX;
    prefab->revision = 0U;
    prefab->next_source_id = HENKA_INVALID_PREFAB_SOURCE_ID;
    for (index = 0U; index < entity_count; ++index)
    {
        prefab->entries[index].parent_index = SIZE_MAX;
        prefab->entries[index].selection_owner_index = SIZE_MAX;
        prefab->entries[index].source_entity = HENKA_INVALID_ENTITY;
        prefab->entries[index].source_parent = HENKA_INVALID_ENTITY;
        prefab->entries[index].source_selection_owner = HENKA_INVALID_ENTITY;
    }
    *out_prefab = prefab;
    return HENKA_SUCCESS;
}

static bool henka_prefab_load_entry(
    henka_prefab* prefab,
    henka_asset_manager* asset_manager,
    henka_shader* inline_material_shader,
    const henka_settings* settings,
    size_t index)
{
    henka_prefab_entry* entry;
    uint64_t value;
    size_t size_value = SIZE_MAX;
    int integer;
    bool boolean;
    char key[128];
    const char* path;
    char* normalized_path = NULL;
    henka_result result;

    if (prefab == NULL || settings == NULL || index >= prefab->entity_count)
    {
        return false;
    }
    entry = &prefab->entries[index];
    if (!henka_prefab_make_key(key, sizeof(key), index, "source_id") ||
        !henka_prefab_settings_get_u64(settings, key, &value) || value == 0U)
    {
        return false;
    }
    entry->source_id = (henka_prefab_source_id)value;
    if (!henka_prefab_load_required_text(settings, index, "name", &entry->name) ||
        !henka_prefab_load_optional_text(settings, index, "has_tag", "tag", &entry->tag) ||
        !henka_prefab_load_optional_text(settings, index, "has_material_name", "material_name", &entry->material_name) ||
        !henka_prefab_load_optional_text(settings, index, "has_interaction_prompt", "interaction_prompt", &entry->interaction_prompt))
    {
        return false;
    }
    if (!henka_prefab_make_key(key, sizeof(key), index, "parent_index") ||
        !henka_prefab_settings_get_u64(settings, key, &value))
    {
        return false;
    }
    if (value != UINT64_MAX && !henka_prefab_u64_to_size(value, &size_value))
    {
        return false;
    }
    entry->parent_index = value == UINT64_MAX ? SIZE_MAX : size_value;
    if (!henka_prefab_make_key(key, sizeof(key), index, "selection_owner_index") ||
        !henka_prefab_settings_get_u64(settings, key, &value))
    {
        return false;
    }
    if (value != UINT64_MAX && !henka_prefab_u64_to_size(value, &size_value))
    {
        return false;
    }
    entry->selection_owner_index = value == UINT64_MAX ? SIZE_MAX : size_value;

#define HENKA_PREFAB_LOAD_ENTRY_FLOAT(suffix, field) \
    do { char field_key[128]; if (!henka_prefab_make_key(field_key, sizeof(field_key), index, suffix) || !henka_prefab_settings_get_float(settings, field_key, &field)) return false; } while (0)
#define HENKA_PREFAB_LOAD_ENTRY_BOOL(suffix, field) \
    do { char field_key[128]; if (!henka_prefab_make_key(field_key, sizeof(field_key), index, suffix) || !henka_prefab_settings_get_bool(settings, field_key, &field)) return false; } while (0)
    HENKA_PREFAB_LOAD_ENTRY_FLOAT("transform.position.x", entry->local_transform.position.x);
    HENKA_PREFAB_LOAD_ENTRY_FLOAT("transform.position.y", entry->local_transform.position.y);
    HENKA_PREFAB_LOAD_ENTRY_FLOAT("transform.position.z", entry->local_transform.position.z);
    HENKA_PREFAB_LOAD_ENTRY_FLOAT("transform.rotation.x", entry->local_transform.rotation.x);
    HENKA_PREFAB_LOAD_ENTRY_FLOAT("transform.rotation.y", entry->local_transform.rotation.y);
    HENKA_PREFAB_LOAD_ENTRY_FLOAT("transform.rotation.z", entry->local_transform.rotation.z);
    HENKA_PREFAB_LOAD_ENTRY_FLOAT("transform.rotation.w", entry->local_transform.rotation.w);
    HENKA_PREFAB_LOAD_ENTRY_FLOAT("transform.scale.x", entry->local_transform.scale.x);
    HENKA_PREFAB_LOAD_ENTRY_FLOAT("transform.scale.y", entry->local_transform.scale.y);
    HENKA_PREFAB_LOAD_ENTRY_FLOAT("transform.scale.z", entry->local_transform.scale.z);
    HENKA_PREFAB_LOAD_ENTRY_BOOL("visible", entry->visible);
    HENKA_PREFAB_LOAD_ENTRY_BOOL("renderer_enabled", entry->renderer_enabled);
    HENKA_PREFAB_LOAD_ENTRY_BOOL("has_local_bounds", entry->has_local_bounds);
    HENKA_PREFAB_LOAD_ENTRY_FLOAT("bounds.center.x", entry->local_bounds.center.x);
    HENKA_PREFAB_LOAD_ENTRY_FLOAT("bounds.center.y", entry->local_bounds.center.y);
    HENKA_PREFAB_LOAD_ENTRY_FLOAT("bounds.center.z", entry->local_bounds.center.z);
    HENKA_PREFAB_LOAD_ENTRY_FLOAT("bounds.extents.x", entry->local_bounds.extents.x);
    HENKA_PREFAB_LOAD_ENTRY_FLOAT("bounds.extents.y", entry->local_bounds.extents.y);
    HENKA_PREFAB_LOAD_ENTRY_FLOAT("bounds.extents.z", entry->local_bounds.extents.z);
    HENKA_PREFAB_LOAD_ENTRY_BOOL("interaction.enabled", entry->interaction.enabled);
    HENKA_PREFAB_LOAD_ENTRY_FLOAT("interaction.max_distance", entry->interaction.max_distance);
#undef HENKA_PREFAB_LOAD_ENTRY_BOOL
#undef HENKA_PREFAB_LOAD_ENTRY_FLOAT
    if (!henka_prefab_transform_is_valid(entry->local_transform) ||
        !henka_prefab_make_key(key, sizeof(key), index, "flags") ||
        !henka_prefab_settings_get_u64(settings, key, &value) ||
        value > UINT32_MAX ||
        !henka_prefab_make_key(key, sizeof(key), index, "has_mesh") ||
        !henka_prefab_settings_get_bool(settings, key, &boolean))
    {
        return false;
    }
    entry->flags = (uint32_t)value;
    if (boolean)
    {
        if (!henka_prefab_make_key(key, sizeof(key), index, "mesh_path") ||
            !henka_settings_has_key(settings, key))
        {
            return false;
        }
        path = henka_settings_get_string(settings, key, NULL);
        if (path == NULL || path[0] == '\0' ||
            henka_prefab_duplicate_asset_path(path, &normalized_path) != HENKA_SUCCESS)
        {
            henka_free(normalized_path);
            return false;
        }
        result = henka_prefab_load_mesh(
            asset_manager, normalized_path, &entry->mesh);
        henka_free(normalized_path);
        normalized_path = NULL;
        if (result != HENKA_SUCCESS)
        {
            return false;
        }
    }
    if (!henka_prefab_make_key(key, sizeof(key), index, "material_mode") ||
        !henka_prefab_settings_get_int(settings, key, &integer) ||
        integer < 0 || integer > 2)
    {
        return false;
    }
    if (integer == 1)
    {
        if (!henka_prefab_load_inline_material(
                settings, index, inline_material_shader, &entry->material))
        {
            return false;
        }
        entry->has_explicit_material = true;
        if (entry->material_name != NULL)
        {
            entry->material.name = entry->material_name;
        }
    }
    else if (integer == 2)
    {
        const henka_material_asset* asset = NULL;
        if (asset_manager == NULL || inline_material_shader == NULL ||
            !henka_prefab_make_key(key, sizeof(key), index, "material_asset_path") ||
            !henka_settings_has_key(settings, key))
        {
            return false;
        }
        path = henka_settings_get_string(settings, key, NULL);
        if (path == NULL || path[0] == '\0' ||
            henka_assets_load_gltf_material_asset(
                asset_manager, path, inline_material_shader, &asset) != HENKA_SUCCESS ||
            asset == NULL ||
            henka_assets_get_material_asset_material(asset, &entry->material) != HENKA_SUCCESS ||
            !henka_prefab_make_key(key, sizeof(key), index, "material_asset_overridden") ||
            !henka_prefab_settings_get_bool(settings, key, &entry->material_asset_overridden) ||
            !henka_assets_get_material_asset_revision(asset, &entry->material_asset_revision))
        {
            return false;
        }
        entry->material_asset = asset;
        entry->has_explicit_material = true;
        if (entry->material_name != NULL)
        {
            entry->material.name = entry->material_name;
        }
    }
    else
    {
        henka_free(entry->material_name);
        entry->material_name = NULL;
        entry->material = henka_material_default();
        entry->has_explicit_material = false;
    }
    entry->interaction.prompt = entry->interaction_prompt;
    return true;
}

henka_result henka_prefab_save_file(
    const henka_prefab* prefab,
    const henka_asset_manager* asset_manager,
    const char* project_root,
    const char* relative_path)
{
    henka_settings* settings = NULL;
    char* identity_path = NULL;
    char* resolved_path = NULL;
    henka_result result;
    size_t index;

    if (prefab == NULL || project_root == NULL || project_root[0] == '\0' ||
        relative_path == NULL || relative_path[0] == '\0' ||
        prefab->entity_count == 0U || prefab->entity_count > HENKA_MAX_PREFAB_ENTITIES ||
        prefab->root_index >= prefab->entity_count || prefab->revision == 0U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_prefab_duplicate_asset_path(relative_path, &identity_path);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    if (prefab->asset_path != NULL && strcmp(prefab->asset_path, identity_path) != 0)
    {
        henka_free(identity_path);
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_path_resolve_confined(
        project_root, identity_path, &resolved_path);
    if (result == HENKA_SUCCESS)
    {
        result = henka_settings_create(&settings);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_settings_set_int(
            settings, "prefab.format_version", HENKA_PREFAB_ASSET_FORMAT_VERSION);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_settings_set_string(
            settings, "prefab.identity", identity_path);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_prefab_settings_set_u64(
            settings, "prefab.revision", prefab->revision);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_prefab_settings_set_u64(
            settings, "prefab.next_source_id", prefab->next_source_id);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_settings_set_int(
            settings, "prefab.entity_count", (int)prefab->entity_count);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_settings_set_int(
            settings, "prefab.root_index", (int)prefab->root_index);
    }
    for (index = 0U; result == HENKA_SUCCESS && index < prefab->entity_count; ++index)
    {
        const henka_prefab_entry* entry = &prefab->entries[index];
        if (entry->source_id == HENKA_INVALID_PREFAB_SOURCE_ID ||
            (entry->parent_index != SIZE_MAX && entry->parent_index >= prefab->entity_count) ||
            (entry->selection_owner_index != SIZE_MAX &&
                entry->selection_owner_index >= prefab->entity_count))
        {
            result = HENKA_ERROR_INVALID_ARGUMENT;
            break;
        }
        result = henka_prefab_save_entry(
            prefab, asset_manager, settings, index);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_settings_save_file(settings, resolved_path);
    }
    henka_settings_destroy(settings);
    henka_free(resolved_path);
    henka_free(identity_path);
    return result;
}

henka_result henka_prefab_load_file(
    henka_asset_manager* asset_manager,
    henka_shader* inline_material_shader,
    const char* project_root,
    const char* relative_path,
    henka_prefab** out_prefab)
{
    henka_settings* settings = NULL;
    henka_prefab* prefab = NULL;
    char* identity_path = NULL;
    char* resolved_path = NULL;
    const char* stored_identity;
    uint64_t value;
    uint64_t next_source_id;
    size_t entity_count;
    size_t root_index;
    int integer;
    henka_result result;
    size_t index;
    size_t other_index;

    if (out_prefab == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_prefab = NULL;
    if (project_root == NULL || project_root[0] == '\0' ||
        relative_path == NULL || relative_path[0] == '\0')
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_prefab_duplicate_asset_path(relative_path, &identity_path);
    if (result == HENKA_SUCCESS)
    {
        result = henka_path_resolve_confined(
            project_root, identity_path, &resolved_path);
    }
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
        goto load_cleanup;
    }
    if (!henka_settings_has_key(settings, "prefab.format_version") ||
        henka_settings_get_int(settings, "prefab.format_version", 0) !=
            (int)HENKA_PREFAB_ASSET_FORMAT_VERSION ||
        !henka_prefab_settings_get_u64(
            settings, "prefab.revision", &value) || value == 0U ||
        !henka_prefab_settings_get_u64(
            settings, "prefab.next_source_id", &next_source_id) ||
        !henka_prefab_settings_get_int(
            settings, "prefab.entity_count", &integer) || integer <= 0 ||
        (size_t)integer > HENKA_MAX_PREFAB_ENTITIES ||
        !henka_prefab_settings_get_int(
            settings, "prefab.root_index", &integer) || integer < 0)
    {
        result = HENKA_ERROR_INVALID_ARGUMENT;
        goto load_cleanup;
    }
    entity_count = (size_t)henka_settings_get_int(
        settings, "prefab.entity_count", 0);
    root_index = (size_t)integer;
    if (root_index >= entity_count ||
        !henka_settings_has_key(settings, "prefab.identity"))
    {
        result = HENKA_ERROR_INVALID_ARGUMENT;
        goto load_cleanup;
    }
    stored_identity = henka_settings_get_string(
        settings, "prefab.identity", NULL);
    if (stored_identity == NULL || strcmp(stored_identity, identity_path) != 0)
    {
        result = HENKA_ERROR_INVALID_ARGUMENT;
        goto load_cleanup;
    }
    result = henka_prefab_allocate_loaded(entity_count, &prefab);
    if (result != HENKA_SUCCESS)
    {
        goto load_cleanup;
    }
    prefab->revision = value;
    prefab->next_source_id = (henka_prefab_source_id)next_source_id;
    prefab->root_index = root_index;
    result = henka_prefab_duplicate_asset_path(
        stored_identity, &prefab->asset_path);
    if (result != HENKA_SUCCESS)
    {
        goto load_cleanup;
    }
    for (index = 0U; index < entity_count; ++index)
    {
        if (!henka_prefab_load_entry(
                prefab,
                asset_manager,
                inline_material_shader,
                settings,
                index))
        {
            result = HENKA_ERROR_ASSET_SOURCE;
            goto load_cleanup;
        }
        for (other_index = 0U; other_index < index; ++other_index)
        {
            if (prefab->entries[other_index].source_id ==
                prefab->entries[index].source_id)
            {
                result = HENKA_ERROR_INVALID_ARGUMENT;
                goto load_cleanup;
            }
        }
        if (prefab->entries[index].parent_index != SIZE_MAX &&
            prefab->entries[index].parent_index >= entity_count)
        {
            result = HENKA_ERROR_INVALID_ARGUMENT;
            goto load_cleanup;
        }
        if (prefab->entries[index].selection_owner_index != SIZE_MAX &&
            prefab->entries[index].selection_owner_index >= entity_count)
        {
            result = HENKA_ERROR_INVALID_ARGUMENT;
            goto load_cleanup;
        }
    }
    if (next_source_id != HENKA_INVALID_PREFAB_SOURCE_ID)
    {
        for (index = 0U; index < entity_count; ++index)
        {
            if (prefab->entries[index].source_id == next_source_id)
            {
                result = HENKA_ERROR_INVALID_ARGUMENT;
                goto load_cleanup;
            }
        }
    }
    *out_prefab = prefab;
    prefab = NULL;
    result = HENKA_SUCCESS;

load_cleanup:
    henka_prefab_destroy(prefab);
    henka_settings_destroy(settings);
    henka_free(resolved_path);
    henka_free(identity_path);
    return result;
}

henka_result henka_prefab_replace_contents(
    henka_prefab* target,
    henka_prefab* replacement)
{
    henka_prefab_entry* old_entries;
    size_t old_entity_count;
    size_t old_root_index;
    uint64_t old_revision;
    henka_prefab_source_id old_next_source_id;
    char* old_asset_path;

    if (target == NULL || replacement == NULL || target == replacement ||
        target->entries == NULL || replacement->entries == NULL ||
        target->entity_count == 0U || replacement->entity_count == 0U ||
        target->asset_path == NULL || replacement->asset_path == NULL ||
        strcmp(target->asset_path, replacement->asset_path) != 0)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    old_entries = target->entries;
    old_entity_count = target->entity_count;
    old_root_index = target->root_index;
    old_revision = target->revision;
    old_next_source_id = target->next_source_id;
    old_asset_path = target->asset_path;
    target->entries = replacement->entries;
    target->entity_count = replacement->entity_count;
    target->root_index = replacement->root_index;
    target->revision = replacement->revision;
    target->next_source_id = replacement->next_source_id;
    target->asset_path = replacement->asset_path;
    replacement->entries = old_entries;
    replacement->entity_count = old_entity_count;
    replacement->root_index = old_root_index;
    replacement->revision = old_revision;
    replacement->next_source_id = old_next_source_id;
    replacement->asset_path = old_asset_path;
    return HENKA_SUCCESS;
}

henka_result henka_prefab_get_source_id_at(
    const henka_prefab* prefab,
    size_t index,
    henka_prefab_source_id* out_source_id)
{
    if (out_source_id == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_source_id = HENKA_INVALID_PREFAB_SOURCE_ID;
    if (prefab == NULL || index >= prefab->entity_count)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_source_id = prefab->entries[index].source_id;
    return HENKA_SUCCESS;
}

henka_result henka_prefab_find_source_id(
    const henka_prefab* prefab,
    henka_prefab_source_id source_id,
    size_t* out_index)
{
    size_t index;

    if (out_index == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_index = SIZE_MAX;
    if (prefab == NULL || source_id == HENKA_INVALID_PREFAB_SOURCE_ID)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    for (index = 0U; index < prefab->entity_count; ++index)
    {
        if (prefab->entries[index].source_id == source_id)
        {
            *out_index = index;
            return HENKA_SUCCESS;
        }
    }
    return HENKA_ERROR_UNKNOWN;
}

static henka_result henka_prefab_preserve_source_ids(
    const henka_prefab* previous,
    henka_prefab* candidate)
{
    henka_prefab_source_id next_source_id;
    size_t index;

    if (previous == NULL || candidate == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    next_source_id = previous->next_source_id;
    for (index = 0U; index < candidate->entity_count; ++index)
    {
        const size_t previous_index = henka_prefab_find_entry(
            previous, candidate->entries[index].source_entity);
        if (previous_index != SIZE_MAX)
        {
            candidate->entries[index].source_id =
                previous->entries[previous_index].source_id;
            continue;
        }
        if (next_source_id == HENKA_INVALID_PREFAB_SOURCE_ID)
        {
            return HENKA_ERROR_LIMIT;
        }
        candidate->entries[index].source_id = next_source_id;
        if (next_source_id == UINT64_MAX)
        {
            next_source_id = HENKA_INVALID_PREFAB_SOURCE_ID;
        }
        else
        {
            next_source_id += UINT64_C(1);
        }
    }
    candidate->next_source_id = next_source_id;
    return HENKA_SUCCESS;
}

henka_result henka_prefab_refresh_from_scene(
    henka_prefab* prefab,
    const henka_scene* source_scene,
    henka_entity root_entity)
{
    henka_prefab* candidate;
    henka_prefab_entry* old_entries;
    size_t old_entity_count;
    size_t old_root_index;
    henka_prefab_source_id old_next_source_id;
    henka_result result;

    if (prefab == NULL || source_scene == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (prefab->revision == UINT64_MAX)
    {
        return HENKA_ERROR_NUMERIC_RANGE;
    }
    candidate = NULL;
    result = henka_prefab_create_from_scene(
        source_scene, root_entity, &candidate);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    result = henka_prefab_preserve_source_ids(prefab, candidate);
    if (result != HENKA_SUCCESS)
    {
        henka_prefab_destroy(candidate);
        return result;
    }
    candidate->revision = prefab->revision + UINT64_C(1);

    old_entries = prefab->entries;
    old_entity_count = prefab->entity_count;
    old_root_index = prefab->root_index;
    old_next_source_id = prefab->next_source_id;
    prefab->entries = candidate->entries;
    prefab->entity_count = candidate->entity_count;
    prefab->root_index = candidate->root_index;
    prefab->revision = candidate->revision;
    prefab->next_source_id = candidate->next_source_id;
    candidate->entries = old_entries;
    candidate->entity_count = old_entity_count;
    candidate->root_index = old_root_index;
    candidate->next_source_id = old_next_source_id;
    henka_prefab_destroy(candidate);
    return HENKA_SUCCESS;
}

henka_result henka_prefab_find_source_index(
    const henka_prefab* prefab,
    henka_entity source_entity,
    size_t* out_index)
{
    size_t index;

    if (out_index == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_index = SIZE_MAX;
    if (prefab == NULL || source_entity == HENKA_INVALID_ENTITY)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    for (index = 0U; index < prefab->entity_count; ++index)
    {
        if (prefab->entries[index].source_entity == source_entity)
        {
            *out_index = index;
            return HENKA_SUCCESS;
        }
    }
    return HENKA_ERROR_UNKNOWN;
}

void henka_prefab_instance_destroy(henka_prefab_instance* instance)
{
    if (instance == NULL)
    {
        return;
    }
    henka_free(instance->entities);
    henka_free(instance->source_ids);
    henka_free(instance->base_local_transforms);
    henka_free(instance->local_transform_overrides);
    henka_free(instance->transform_override_flags);
    henka_free(instance);
}

static size_t henka_prefab_instance_count_live_entities(
    const henka_prefab_instance* instance)
{
    size_t live_count = 0U;
    size_t index;

    if (instance == NULL || instance->target_scene == NULL ||
        instance->entities == NULL)
    {
        return 0U;
    }
    for (index = 0U; index < instance->entity_count; ++index)
    {
        if (henka_scene_is_entity_valid(
                instance->target_scene,
                instance->entities[index]))
        {
            ++live_count;
        }
    }
    return live_count;
}

henka_result henka_prefab_instance_destroy_entities(
    henka_prefab_instance* instance)
{
    size_t index;
    const size_t live_count =
        henka_prefab_instance_count_live_entities(instance);

    if (instance == NULL || instance->target_scene == NULL ||
        instance->entities == NULL ||
        henka_scene_is_destroyed(instance->target_scene))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (live_count == 0U)
    {
        return HENKA_SUCCESS;
    }
    if (!henka_scene_has_render_revision_capacity(
            instance->target_scene,
            (uint64_t)live_count))
    {
        return HENKA_ERROR_LIMIT;
    }

    /* Destroy in reverse mapping order so the usual parent-before-child
     * capture order does not leave a live child behind. The scene contract
     * promotes children when their parent is retired, so this remains safe
     * even when source storage order differs from hierarchy order. */
    for (index = instance->entity_count; index > 0U; --index)
    {
        const size_t mapping_index = index - 1U;
        const henka_entity entity = instance->entities[mapping_index];

        if (!henka_scene_is_entity_valid(instance->target_scene, entity))
        {
            continue;
        }
        henka_scene_destroy_entity(instance->target_scene, entity);
        instance->entities[mapping_index] = HENKA_INVALID_ENTITY;
    }
    return HENKA_SUCCESS;
}

size_t henka_prefab_instance_get_entity_count(
    const henka_prefab_instance* instance)
{
    return instance == NULL ? 0U : instance->entity_count;
}

uint64_t henka_prefab_instance_get_prefab_revision(
    const henka_prefab_instance* instance)
{
    return instance == NULL ? 0U : instance->prefab_revision;
}

static henka_result henka_prefab_instance_find_source_index(
    const henka_prefab_instance* instance,
    henka_prefab_source_id source_id,
    size_t* out_index)
{
    size_t index;

    if (out_index == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_index = SIZE_MAX;
    if (instance == NULL || instance->target_scene == NULL ||
        instance->entities == NULL || instance->source_ids == NULL ||
        source_id == HENKA_INVALID_PREFAB_SOURCE_ID)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    for (index = 0U; index < instance->entity_count; ++index)
    {
        if (instance->source_ids[index] == source_id)
        {
            if (!henka_scene_is_entity_valid(
                    instance->target_scene, instance->entities[index]))
            {
                return HENKA_ERROR_INVALID_ARGUMENT;
            }
            *out_index = index;
            return HENKA_SUCCESS;
        }
    }
    return HENKA_ERROR_UNKNOWN;
}

henka_result henka_prefab_instance_get_entity_for_source_id(
    const henka_prefab_instance* instance,
    henka_prefab_source_id source_id,
    henka_entity* out_entity)
{
    size_t index;
    henka_result result;

    if (out_entity != NULL)
    {
        *out_entity = HENKA_INVALID_ENTITY;
    }
    if (out_entity == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_prefab_instance_find_source_index(
        instance, source_id, &index);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    *out_entity = instance->entities[index];
    return HENKA_SUCCESS;
}

henka_result henka_prefab_instance_set_local_transform_override(
    henka_prefab_instance* instance,
    henka_prefab_source_id source_id,
    henka_transform transform)
{
    henka_transform actual_transform;
    size_t index;
    henka_result result;

    result = henka_prefab_instance_find_source_index(
        instance, source_id, &index);
    if (result != HENKA_SUCCESS ||
        instance->base_local_transforms == NULL ||
        instance->local_transform_overrides == NULL ||
        instance->transform_override_flags == NULL)
    {
        return result == HENKA_SUCCESS ? HENKA_ERROR_INVALID_ARGUMENT : result;
    }
    result = henka_scene_set_entity_local_transform(
        instance->target_scene, instance->entities[index], transform);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    result = henka_scene_get_entity_local_transform(
        instance->target_scene, instance->entities[index], &actual_transform);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    instance->local_transform_overrides[index] = actual_transform;
    instance->transform_override_flags[index] =
        memcmp(&actual_transform,
            &instance->base_local_transforms[index],
            sizeof(actual_transform)) != 0;
    return HENKA_SUCCESS;
}

henka_result henka_prefab_instance_clear_local_transform_override(
    henka_prefab_instance* instance,
    henka_prefab_source_id source_id)
{
    size_t index;
    henka_result result;

    result = henka_prefab_instance_find_source_index(
        instance, source_id, &index);
    if (result != HENKA_SUCCESS ||
        instance->base_local_transforms == NULL ||
        instance->local_transform_overrides == NULL ||
        instance->transform_override_flags == NULL)
    {
        return result == HENKA_SUCCESS ? HENKA_ERROR_INVALID_ARGUMENT : result;
    }
    if (!instance->transform_override_flags[index])
    {
        return HENKA_SUCCESS;
    }
    result = henka_scene_set_entity_local_transform(
        instance->target_scene,
        instance->entities[index],
        instance->base_local_transforms[index]);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    instance->local_transform_overrides[index] =
        instance->base_local_transforms[index];
    instance->transform_override_flags[index] = false;
    return HENKA_SUCCESS;
}

henka_result henka_prefab_instance_get_local_transform_override(
    const henka_prefab_instance* instance,
    henka_prefab_source_id source_id,
    bool* out_has_override,
    henka_transform* out_transform)
{
    size_t index;
    henka_result result;

    if (out_has_override == NULL || out_transform == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_has_override = false;
    *out_transform = henka_transform_identity();
    result = henka_prefab_instance_find_source_index(
        instance, source_id, &index);
    if (result != HENKA_SUCCESS ||
        instance->base_local_transforms == NULL ||
        instance->local_transform_overrides == NULL ||
        instance->transform_override_flags == NULL)
    {
        return result == HENKA_SUCCESS ? HENKA_ERROR_INVALID_ARGUMENT : result;
    }
    if (index != instance->root_index)
    {
        henka_transform actual_transform;

        if (henka_scene_get_entity_local_transform(
                instance->target_scene,
                instance->entities[index],
                &actual_transform) != HENKA_SUCCESS)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        if (!henka_prefab_transform_equal(
                actual_transform,
                instance->base_local_transforms[index]))
        {
            *out_has_override = true;
            *out_transform = actual_transform;
            return HENKA_SUCCESS;
        }
    }
    *out_has_override = instance->transform_override_flags[index];
    *out_transform = *out_has_override
        ? instance->local_transform_overrides[index]
        : instance->base_local_transforms[index];
    return HENKA_SUCCESS;
}

henka_result henka_prefab_instance_refresh(henka_prefab_instance* instance)
{
    henka_scene_entity_presentation_update* updates;
    henka_scene_entity_mesh_update* mesh_updates;
    henka_scene_entity_material_asset_update* material_asset_updates;
    bool* reconciled_override_flags;
    bool material_asset_updates_present;
    bool refresh_required;
    size_t allocation_size;
    size_t index;
    uint64_t prefab_revision;
    henka_result result;

    if (instance == NULL || instance->target_scene == NULL ||
        instance->prefab == NULL || instance->entities == NULL ||
        instance->source_ids == NULL || instance->base_local_transforms == NULL ||
        instance->local_transform_overrides == NULL ||
        instance->transform_override_flags == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    prefab_revision = instance->prefab->revision;
    if (instance->prefab->entity_count != instance->entity_count ||
        instance->prefab->root_index >= instance->prefab->entity_count)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    material_asset_updates = NULL;
    material_asset_updates_present = false;
    refresh_required = instance->prefab_revision != prefab_revision;
    for (index = 0U; index < instance->entity_count; ++index)
    {
        const henka_prefab_entry* entry = &instance->prefab->entries[index];

        if (instance->source_ids[index] !=
                entry->source_id ||
            !henka_scene_is_entity_valid(
                instance->target_scene,
                instance->entities[index]))
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        if (entry->material_asset != NULL && entry->material_asset_overridden)
        {
            /* A persisted asset-backed override does not yet carry the full
             * override payload required for an atomic refresh. Keep the
             * authority distinction explicit and fail closed. */
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
    }

    for (index = 0U; index < instance->entity_count; ++index)
    {
        const henka_prefab_entry* entry = &instance->prefab->entries[index];

        if (entry->material_asset != NULL)
        {
            henka_material material;
            const henka_material_asset* target_asset;
            uint64_t asset_revision;
            uint64_t target_revision;
            bool target_overridden;

            if (!material_asset_updates_present)
            {
                if (!henka_checked_size_multiply(
                        instance->entity_count,
                        sizeof(*material_asset_updates),
                        &allocation_size))
                {
                    return HENKA_ERROR_NUMERIC_RANGE;
                }
                material_asset_updates =
                    (henka_scene_entity_material_asset_update*)henka_calloc(
                        instance->entity_count,
                        sizeof(*material_asset_updates));
                if (material_asset_updates == NULL)
                {
                    return HENKA_ERROR_OUT_OF_MEMORY;
                }
                material_asset_updates_present = true;
            }
            if (henka_assets_get_material_asset_material(
                    entry->material_asset, &material) != HENKA_SUCCESS ||
                henka_assets_get_material_asset_revision(
                    entry->material_asset, &asset_revision) != HENKA_SUCCESS ||
                asset_revision == 0U ||
                henka_scene_get_entity_material_asset(
                    instance->target_scene,
                    instance->entities[index],
                    &target_asset) != HENKA_SUCCESS ||
                henka_scene_get_material_asset_state(
                    instance->target_scene,
                    instance->entities[index],
                    &target_revision,
                    &target_overridden) != HENKA_SUCCESS)
            {
                henka_free(material_asset_updates);
                return HENKA_ERROR_INVALID_ARGUMENT;
            }
            if (target_overridden)
            {
                /* Keep a live instance override intact for an idempotent
                 * call. If the source snapshot itself changed, the
                 * unsupported combination must fail closed rather than
                 * silently replacing the override with source state. */
                if (refresh_required || target_asset != entry->material_asset)
                {
                    henka_free(material_asset_updates);
                    return HENKA_ERROR_INVALID_ARGUMENT;
                }
                continue;
            }
            material_asset_updates[index].apply_asset = true;
            material_asset_updates[index].asset = entry->material_asset;
            material_asset_updates[index].material = material;
            material_asset_updates[index].revision = asset_revision;
            material_asset_updates[index].overridden = false;
            if (target_revision != asset_revision || target_overridden)
            {
                refresh_required = true;
            }
        }
    }
    if (!refresh_required)
    {
        henka_free(material_asset_updates);
        return HENKA_SUCCESS;
    }

    if (!henka_checked_size_multiply(
        instance->entity_count,
        sizeof(*updates),
            &allocation_size))
    {
        henka_free(material_asset_updates);
        return HENKA_ERROR_NUMERIC_RANGE;
    }
    updates = (henka_scene_entity_presentation_update*)henka_calloc(
        instance->entity_count,
        sizeof(*updates));
    if (updates == NULL)
    {
        henka_free(material_asset_updates);
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    if (!henka_checked_size_multiply(
            instance->entity_count,
            sizeof(*mesh_updates),
            &allocation_size))
    {
        henka_free(updates);
        henka_free(material_asset_updates);
        return HENKA_ERROR_NUMERIC_RANGE;
    }
    mesh_updates = (henka_scene_entity_mesh_update*)henka_calloc(
        instance->entity_count,
        sizeof(*mesh_updates));
    if (mesh_updates == NULL)
    {
        henka_free(updates);
        henka_free(material_asset_updates);
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    if (!henka_checked_size_multiply(
            instance->entity_count,
            sizeof(*reconciled_override_flags),
            &allocation_size))
    {
        henka_free(updates);
        henka_free(mesh_updates);
        henka_free(material_asset_updates);
        return HENKA_ERROR_NUMERIC_RANGE;
    }
    reconciled_override_flags = (bool*)henka_calloc(
        instance->entity_count,
        sizeof(*reconciled_override_flags));
    if (reconciled_override_flags == NULL)
    {
        henka_free(updates);
        henka_free(mesh_updates);
        henka_free(material_asset_updates);
        return HENKA_ERROR_OUT_OF_MEMORY;
    }

    for (index = 0U; index < instance->entity_count; ++index)
    {
        const henka_prefab_entry* entry = &instance->prefab->entries[index];
        henka_transform actual_transform;
        bool has_override;

        /* Scene edits can arrive through the general authoring scene API
         * rather than the prefab-instance convenience setter. Read the
         * canonical local transform without mutating the instance; the
         * corresponding baseline/override arrays are committed only after
         * the target scene transaction succeeds. The root transform is
         * instance placement; a changed non-root transform is an override. */
        if (henka_scene_get_entity_local_transform(
                instance->target_scene,
                instance->entities[index],
                &actual_transform) != HENKA_SUCCESS)
        {
            henka_free(updates);
            henka_free(mesh_updates);
            henka_free(material_asset_updates);
            henka_free(reconciled_override_flags);
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        has_override = index != instance->prefab->root_index &&
            !henka_prefab_transform_equal(
                actual_transform,
                instance->base_local_transforms[index]);
        reconciled_override_flags[index] = has_override;

        updates[index].name = entry->name;
        updates[index].transform = entry->local_transform;
        updates[index].visible = entry->visible;
        updates[index].interaction = entry->interaction;
        updates[index].apply_material = entry->has_explicit_material &&
            entry->material_asset == NULL;
        updates[index].material = entry->material;
        updates[index].apply_renderer_enabled = true;
        updates[index].renderer_enabled = entry->renderer_enabled;
        mesh_updates[index].apply_mesh = true;
        mesh_updates[index].mesh = entry->mesh;
        if (index == instance->prefab->root_index || has_override)
        {
            updates[index].transform = actual_transform;
        }
    }

    result = henka_scene_apply_entity_local_mesh_refresh_with_material_assets_batch(
        instance->target_scene,
        instance->entities,
        updates,
        mesh_updates,
        material_asset_updates,
        instance->entity_count);
    henka_free(updates);
    henka_free(mesh_updates);
    henka_free(material_asset_updates);
    if (result != HENKA_SUCCESS)
    {
        henka_free(reconciled_override_flags);
        return result;
    }

    for (index = 0U; index < instance->entity_count; ++index)
    {
        henka_transform actual_transform;

        if (henka_scene_get_entity_local_transform(
                instance->target_scene,
                instance->entities[index],
                &actual_transform) != HENKA_SUCCESS)
        {
            henka_free(reconciled_override_flags);
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        if (index != instance->prefab->root_index)
        {
            instance->base_local_transforms[index] =
                instance->prefab->entries[index].local_transform;
        }
        instance->transform_override_flags[index] =
            reconciled_override_flags[index];
        if (reconciled_override_flags[index])
        {
            instance->local_transform_overrides[index] = actual_transform;
        }
        else
        {
            instance->base_local_transforms[index] = actual_transform;
            instance->local_transform_overrides[index] = actual_transform;
        }
    }
    henka_free(reconciled_override_flags);
    instance->prefab_revision = prefab_revision;
    return HENKA_SUCCESS;
}

henka_result henka_prefab_instance_get_entity_at(
    const henka_prefab_instance* instance,
    size_t index,
    henka_entity* out_entity)
{
    if (out_entity != NULL)
    {
        *out_entity = HENKA_INVALID_ENTITY;
    }
    if (instance == NULL || instance->target_scene == NULL ||
        instance->entities == NULL || out_entity == NULL ||
        index >= instance->entity_count ||
        !henka_scene_is_entity_valid(instance->target_scene, instance->entities[index]))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_entity = instance->entities[index];
    return HENKA_SUCCESS;
}

henka_result henka_prefab_instance_get_root_entity(
    const henka_prefab_instance* instance,
    henka_entity* out_entity)
{
    if (out_entity != NULL)
    {
        *out_entity = HENKA_INVALID_ENTITY;
    }
    if (instance == NULL || instance->root_index >= instance->entity_count)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return henka_prefab_instance_get_entity_at(
        instance, instance->root_index, out_entity);
}

static void henka_prefab_rollback_entities(
    henka_scene* scene,
    henka_entity* entities,
    size_t count,
    uint64_t render_revision,
    uint64_t content_revision)
{
    while (count > 0U)
    {
        count -= 1U;
        henka_scene_destroy_entity(scene, entities[count]);
    }
    /* The entity destruction above is an internal rollback operation.  It
     * must not consume the caller-visible scene revision budget after the
     * transaction has been rejected. */
    scene->render_revision = render_revision;
    scene->content_revision = content_revision;
}

static void henka_prefab_free_instance_buffers(
    henka_entity* entities,
    henka_prefab_source_id* source_ids,
    henka_transform* base_local_transforms,
    henka_transform* local_transform_overrides,
    bool* transform_override_flags)
{
    henka_free(transform_override_flags);
    henka_free(local_transform_overrides);
    henka_free(base_local_transforms);
    henka_free(source_ids);
    henka_free(entities);
}

static bool henka_prefab_add_mutation_count(
    uint64_t* total,
    uint64_t additional)
{
    if (total == NULL || additional > UINT64_MAX - *total)
    {
        return false;
    }
    *total += additional;
    return true;
}

static bool henka_prefab_get_transaction_mutation_count(
    const henka_prefab* prefab,
    bool has_external_parent,
    uint64_t* out_count)
{
    uint64_t mutation_count;
    size_t index;

    if (prefab == NULL || out_count == NULL)
    {
        return false;
    }

    mutation_count = 0U;
    if (!henka_prefab_add_mutation_count(
            &mutation_count, (uint64_t)prefab->entity_count))
    {
        return false;
    }
    for (index = 0U; index < prefab->entity_count; ++index)
    {
        const henka_prefab_entry* entry = &prefab->entries[index];

        /* Each created entity receives a transform, flags, material-asset
         * state, bounds, and interaction state before hierarchy is linked. */
        if (!henka_prefab_add_mutation_count(&mutation_count, 5U) ||
            (!entry->visible &&
                !henka_prefab_add_mutation_count(&mutation_count, 1U)) ||
            (!entry->renderer_enabled &&
                !henka_prefab_add_mutation_count(&mutation_count, 1U)) ||
            (entry->has_explicit_material &&
                !henka_prefab_add_mutation_count(&mutation_count, 1U)) ||
            (entry->mesh != NULL &&
                !henka_prefab_add_mutation_count(&mutation_count, 1U)) ||
            (entry->selection_owner_index != SIZE_MAX &&
                entry->selection_owner_index != index &&
                !henka_prefab_add_mutation_count(&mutation_count, 1U)) ||
            (entry->parent_index != SIZE_MAX &&
                !henka_prefab_add_mutation_count(&mutation_count, 1U)))
        {
            return false;
        }
    }
    if (has_external_parent &&
        !henka_prefab_add_mutation_count(&mutation_count, 1U))
    {
        return false;
    }

    /* A failed multi-entity operation destroys every entity it created. The
     * rollback itself bumps both scene revision watermarks, so reserve that
     * additional capacity before making the first visible mutation. */
    if (!henka_prefab_add_mutation_count(
            &mutation_count, (uint64_t)prefab->entity_count))
    {
        return false;
    }
    *out_count = mutation_count;
    return true;
}

static henka_result henka_prefab_instantiate_internal(
    const henka_prefab* prefab,
    henka_scene* target_scene,
    henka_entity parent_entity,
    henka_transform root_transform,
    henka_entity* out_root_entity,
    henka_prefab_instance** out_instance,
    bool require_parent)
{
    henka_entity* entities;
    henka_prefab_source_id* source_ids;
    henka_transform* base_local_transforms;
    henka_transform* local_transform_overrides;
    bool* transform_override_flags;
    henka_prefab_instance* instance;
    size_t allocation_size;
    size_t source_id_allocation_size;
    size_t transform_allocation_size;
    size_t index;
    size_t created;
    uint64_t required_mutations;
    uint64_t render_revision_before;
    uint64_t content_revision_before;
    henka_result result;

    if (out_root_entity == NULL && out_instance == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (out_root_entity != NULL)
    {
        *out_root_entity = HENKA_INVALID_ENTITY;
    }
    if (out_instance != NULL)
    {
        *out_instance = NULL;
    }
    if (prefab == NULL || target_scene == NULL ||
        (require_parent && parent_entity == HENKA_INVALID_ENTITY))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (parent_entity != HENKA_INVALID_ENTITY &&
        !henka_scene_is_entity_valid(target_scene, parent_entity))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (prefab->entity_count == 0U ||
        prefab->entity_count > HENKA_MAX_PREFAB_ENTITIES ||
        prefab->root_index >= prefab->entity_count ||
        henka_scene_get_entity_count(target_scene) > HENKA_MAX_SCENE_ENTITIES - prefab->entity_count)
    {
        return HENKA_ERROR_LIMIT;
    }
    if (!henka_prefab_get_transaction_mutation_count(
            prefab, parent_entity != HENKA_INVALID_ENTITY, &required_mutations))
    {
        return HENKA_ERROR_NUMERIC_RANGE;
    }
    if (!henka_scene_has_render_revision_capacity(
            target_scene, required_mutations))
    {
        return HENKA_ERROR_LIMIT;
    }
    render_revision_before = target_scene->render_revision;
    content_revision_before = target_scene->content_revision;
    if (!henka_checked_size_multiply(prefab->entity_count, sizeof(*entities), &allocation_size))
    {
        return HENKA_ERROR_NUMERIC_RANGE;
    }
    entities = henka_malloc(allocation_size);
    if (entities == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    if (!henka_checked_size_multiply(
            prefab->entity_count,
            sizeof(*source_ids),
            &source_id_allocation_size))
    {
        henka_free(entities);
        return HENKA_ERROR_NUMERIC_RANGE;
    }
    source_ids = henka_malloc(source_id_allocation_size);
    if (source_ids == NULL)
    {
        henka_free(entities);
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    for (index = 0U; index < prefab->entity_count; ++index)
    {
        source_ids[index] = prefab->entries[index].source_id;
    }
    base_local_transforms = NULL;
    local_transform_overrides = NULL;
    transform_override_flags = NULL;
    if (out_instance != NULL)
    {
        if (!henka_checked_size_multiply(
                prefab->entity_count,
                sizeof(*base_local_transforms),
                &transform_allocation_size))
        {
            henka_free(source_ids);
            henka_free(entities);
            return HENKA_ERROR_NUMERIC_RANGE;
        }
        base_local_transforms = henka_malloc(transform_allocation_size);
        local_transform_overrides = henka_malloc(transform_allocation_size);
        transform_override_flags = henka_calloc(
            prefab->entity_count, sizeof(*transform_override_flags));
        if (base_local_transforms == NULL ||
            local_transform_overrides == NULL ||
            transform_override_flags == NULL)
        {
            henka_free(transform_override_flags);
            henka_free(local_transform_overrides);
            henka_free(base_local_transforms);
            henka_free(source_ids);
            henka_free(entities);
            return HENKA_ERROR_OUT_OF_MEMORY;
        }
    }
    created = 0U;
    for (index = 0U; index < prefab->entity_count; ++index)
    {
        entities[index] = henka_scene_create_entity_named(
            target_scene,
            prefab->entries[index].name);
        if (entities[index] == HENKA_INVALID_ENTITY)
        {
            henka_prefab_rollback_entities(
                target_scene,
                entities,
                created,
                render_revision_before,
                content_revision_before);
            henka_prefab_free_instance_buffers(
                entities,
                source_ids,
                base_local_transforms,
                local_transform_overrides,
                transform_override_flags);
            return HENKA_ERROR_OUT_OF_MEMORY;
        }
        created += 1U;
    }

    for (index = 0U; index < prefab->entity_count; ++index)
    {
        const henka_prefab_entry* entry = &prefab->entries[index];
        result = index == prefab->root_index
            ? (parent_entity == HENKA_INVALID_ENTITY
                ? henka_scene_set_entity_transform(target_scene, entities[index], root_transform)
                : henka_scene_set_entity_local_transform(target_scene, entities[index], root_transform))
            : henka_scene_set_entity_local_transform(target_scene, entities[index], entry->local_transform);
        if (result == HENKA_SUCCESS)
        {
            result = henka_scene_set_entity_visible(target_scene, entities[index], entry->visible);
        }
        if (result == HENKA_SUCCESS)
        {
            result = henka_scene_set_entity_renderer_enabled(
                target_scene,
                entities[index],
                entry->renderer_enabled);
        }
        if (result == HENKA_SUCCESS)
        {
            result = henka_scene_set_entity_flags(target_scene, entities[index], entry->flags);
        }
        if (result == HENKA_SUCCESS)
        {
            result = henka_scene_set_entity_tag(target_scene, entities[index], entry->tag);
        }
        if (result == HENKA_SUCCESS && entry->has_explicit_material)
        {
            result = henka_scene_set_entity_material(target_scene, entities[index], entry->material);
        }
        if (result == HENKA_SUCCESS)
        {
            result = henka_scene_set_entity_material_asset(target_scene, entities[index], entry->material_asset);
        }
        if (result == HENKA_SUCCESS && entry->mesh != NULL)
        {
            result = henka_scene_set_entity_mesh(target_scene, entities[index], entry->mesh);
        }
        if (result == HENKA_SUCCESS)
        {
            result = entry->has_local_bounds
                ? henka_scene_set_entity_local_bounds(target_scene, entities[index], entry->local_bounds)
                : henka_scene_clear_entity_local_bounds(target_scene, entities[index]);
        }
        if (result == HENKA_SUCCESS)
        {
            result = henka_scene_set_entity_interaction(target_scene, entities[index], &entry->interaction);
        }
        if (result != HENKA_SUCCESS)
        {
            henka_prefab_rollback_entities(
                target_scene,
                entities,
                created,
                render_revision_before,
                content_revision_before);
            henka_prefab_free_instance_buffers(
                entities,
                source_ids,
                base_local_transforms,
                local_transform_overrides,
                transform_override_flags);
            return result;
        }
        result = henka_scene_restore_material_asset_state(
            target_scene,
            entities[index],
            entry->material_asset,
            entry->material_asset_revision,
            entry->material_asset_overridden);
        if (result != HENKA_SUCCESS)
        {
            henka_prefab_rollback_entities(
                target_scene,
                entities,
                created,
                render_revision_before,
                content_revision_before);
            henka_prefab_free_instance_buffers(
                entities,
                source_ids,
                base_local_transforms,
                local_transform_overrides,
                transform_override_flags);
            return result;
        }
    }

    for (index = 0U; index < prefab->entity_count; ++index)
    {
        const size_t owner_index = prefab->entries[index].selection_owner_index;
        if (owner_index != SIZE_MAX && owner_index != index)
        {
            result = henka_scene_set_entity_selection_owner(
                target_scene,
                entities[index],
                entities[owner_index]);
            if (result != HENKA_SUCCESS)
            {
                henka_prefab_rollback_entities(
                    target_scene,
                    entities,
                    created,
                    render_revision_before,
                    content_revision_before);
                henka_prefab_free_instance_buffers(
                    entities,
                    source_ids,
                    base_local_transforms,
                    local_transform_overrides,
                    transform_override_flags);
                return result;
            }
        }
    }

    for (index = 0U; index < prefab->entity_count; ++index)
    {
        const size_t parent_index = prefab->entries[index].parent_index;
        if (parent_index != SIZE_MAX)
        {
            result = henka_scene_set_entity_parent(
                target_scene,
                entities[index],
                entities[parent_index],
                HENKA_SCENE_PARENT_KEEP_LOCAL);
            if (result == HENKA_SUCCESS)
            {
                continue;
            }
            henka_prefab_rollback_entities(
                target_scene,
                entities,
                created,
                render_revision_before,
                content_revision_before);
            henka_prefab_free_instance_buffers(
                entities,
                source_ids,
                base_local_transforms,
                local_transform_overrides,
                transform_override_flags);
            return result;
        }
    }

    if (parent_entity != HENKA_INVALID_ENTITY)
    {
        result = henka_scene_set_entity_parent(
            target_scene,
            entities[prefab->root_index],
            parent_entity,
            HENKA_SCENE_PARENT_KEEP_LOCAL);
        if (result != HENKA_SUCCESS)
        {
            henka_prefab_rollback_entities(
                target_scene,
                entities,
                created,
                render_revision_before,
                content_revision_before);
            henka_prefab_free_instance_buffers(
                entities,
                source_ids,
                base_local_transforms,
                local_transform_overrides,
                transform_override_flags);
            return result;
        }
    }

    if (out_instance != NULL)
    {
        for (index = 0U; index < prefab->entity_count; ++index)
        {
            result = henka_scene_get_entity_local_transform(
                target_scene,
                entities[index],
                &base_local_transforms[index]);
            if (result != HENKA_SUCCESS)
            {
                henka_prefab_rollback_entities(
                    target_scene,
                    entities,
                    created,
                    render_revision_before,
                    content_revision_before);
                henka_prefab_free_instance_buffers(
                    entities,
                    source_ids,
                    base_local_transforms,
                    local_transform_overrides,
                    transform_override_flags);
                return result;
            }
            local_transform_overrides[index] = base_local_transforms[index];
        }
    }

    instance = NULL;
    if (out_instance != NULL)
    {
        instance = (henka_prefab_instance*)henka_calloc(1U, sizeof(*instance));
        if (instance == NULL)
        {
            henka_prefab_rollback_entities(
                target_scene,
                entities,
                created,
                render_revision_before,
                content_revision_before);
            henka_prefab_free_instance_buffers(
                entities,
                source_ids,
                base_local_transforms,
                local_transform_overrides,
                transform_override_flags);
            return HENKA_ERROR_OUT_OF_MEMORY;
        }
        instance->target_scene = target_scene;
        instance->prefab = prefab;
        instance->entities = entities;
        instance->source_ids = source_ids;
        instance->base_local_transforms = base_local_transforms;
        instance->local_transform_overrides = local_transform_overrides;
        instance->transform_override_flags = transform_override_flags;
        instance->entity_count = prefab->entity_count;
        instance->root_index = prefab->root_index;
        instance->prefab_revision = prefab->revision;
        *out_instance = instance;
    }
    if (out_root_entity != NULL)
    {
        *out_root_entity = entities[prefab->root_index];
    }
    if (out_instance == NULL)
    {
        henka_prefab_free_instance_buffers(
            entities,
            source_ids,
            base_local_transforms,
            local_transform_overrides,
            transform_override_flags);
    }
    return HENKA_SUCCESS;
}

henka_result henka_prefab_instantiate(
    const henka_prefab* prefab,
    henka_scene* target_scene,
    henka_transform root_transform,
    henka_entity* out_root_entity)
{
    return henka_prefab_instantiate_internal(
        prefab,
        target_scene,
        HENKA_INVALID_ENTITY,
        root_transform,
        out_root_entity,
        NULL,
        false);
}

henka_result henka_prefab_instantiate_under_parent(
    const henka_prefab* prefab,
    henka_scene* target_scene,
    henka_entity parent_entity,
    henka_transform root_transform,
    henka_entity* out_root_entity)
{
    return henka_prefab_instantiate_internal(
        prefab,
        target_scene,
        parent_entity,
        root_transform,
        out_root_entity,
        NULL,
        true);
}

henka_result henka_prefab_instantiate_with_instance(
    const henka_prefab* prefab,
    henka_scene* target_scene,
    henka_transform root_transform,
    henka_prefab_instance** out_instance)
{
    return henka_prefab_instantiate_internal(
        prefab,
        target_scene,
        HENKA_INVALID_ENTITY,
        root_transform,
        NULL,
        out_instance,
        false);
}

henka_result henka_prefab_instantiate_under_parent_with_instance(
    const henka_prefab* prefab,
    henka_scene* target_scene,
    henka_entity parent_entity,
    henka_transform root_transform,
    henka_prefab_instance** out_instance)
{
    return henka_prefab_instantiate_internal(
        prefab,
        target_scene,
        parent_entity,
        root_transform,
        NULL,
        out_instance,
        true);
}
