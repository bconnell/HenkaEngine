#include <henka/prefab.h>
#include <henka/memory.h>

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
    *out_has_override = instance->transform_override_flags[index];
    *out_transform = *out_has_override
        ? instance->local_transform_overrides[index]
        : instance->base_local_transforms[index];
    return HENKA_SUCCESS;
}

henka_result henka_prefab_instance_refresh(henka_prefab_instance* instance)
{
    henka_scene_entity_presentation_update* updates;
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
    for (index = 0U; index < instance->entity_count; ++index)
    {
        if (instance->source_ids[index] !=
                instance->prefab->entries[index].source_id ||
            !henka_scene_is_entity_valid(
                instance->target_scene,
                instance->entities[index]))
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        /* Refresh currently has an atomic inline-presentation transaction,
         * but no equivalent asset-authority transaction. Do not convert a
         * borrowed material definition into an inline override or leave the
         * live instance pointing at a stale asset state. */
        if (instance->prefab->entries[index].material_asset != NULL)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
    }
    if (instance->prefab_revision == prefab_revision)
    {
        return HENKA_SUCCESS;
    }
    if (!henka_checked_size_multiply(
            instance->entity_count,
            sizeof(*updates),
            &allocation_size))
    {
        return HENKA_ERROR_NUMERIC_RANGE;
    }
    updates = (henka_scene_entity_presentation_update*)henka_calloc(
        instance->entity_count,
        sizeof(*updates));
    if (updates == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }

    for (index = 0U; index < instance->entity_count; ++index)
    {
        const henka_prefab_entry* entry = &instance->prefab->entries[index];

        updates[index].name = entry->name;
        updates[index].transform = entry->local_transform;
        updates[index].visible = entry->visible;
        updates[index].interaction = entry->interaction;
        updates[index].apply_material = entry->has_explicit_material;
        updates[index].material = entry->material;
        updates[index].apply_renderer_enabled = true;
        updates[index].renderer_enabled = entry->renderer_enabled;
        if (index == instance->prefab->root_index &&
            !instance->transform_override_flags[index])
        {
            updates[index].transform = instance->base_local_transforms[index];
        }
        else if (instance->transform_override_flags[index])
        {
            updates[index].transform = instance->local_transform_overrides[index];
        }
    }

    result = henka_scene_apply_entity_local_presentation_batch(
        instance->target_scene,
        instance->entities,
        updates,
        instance->entity_count);
    henka_free(updates);
    if (result != HENKA_SUCCESS)
    {
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
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        if (index != instance->prefab->root_index)
        {
            instance->base_local_transforms[index] =
                instance->prefab->entries[index].local_transform;
        }
        if (instance->transform_override_flags[index])
        {
            instance->local_transform_overrides[index] = actual_transform;
        }
        else
        {
            instance->base_local_transforms[index] = actual_transform;
            instance->local_transform_overrides[index] = actual_transform;
        }
    }
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
