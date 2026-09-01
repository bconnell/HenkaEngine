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
};

struct henka_prefab_instance
{
    henka_scene* target_scene;
    henka_entity* entities;
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
        if (entity == root_entity)
        {
            prefab->root_index = count;
        }
        entry->visible = info.visible;
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

henka_result henka_prefab_refresh_from_scene(
    henka_prefab* prefab,
    const henka_scene* source_scene,
    henka_entity root_entity)
{
    henka_prefab* candidate;
    henka_prefab_entry* old_entries;
    size_t old_entity_count;
    size_t old_root_index;
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
    candidate->revision = prefab->revision + UINT64_C(1);

    old_entries = prefab->entries;
    old_entity_count = prefab->entity_count;
    old_root_index = prefab->root_index;
    prefab->entries = candidate->entries;
    prefab->entity_count = candidate->entity_count;
    prefab->root_index = candidate->root_index;
    prefab->revision = candidate->revision;
    candidate->entries = old_entries;
    candidate->entity_count = old_entity_count;
    candidate->root_index = old_root_index;
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
    henka_free(instance);
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
    size_t count)
{
    while (count > 0U)
    {
        count -= 1U;
        henka_scene_destroy_entity(scene, entities[count]);
    }
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
    henka_prefab_instance* instance;
    size_t allocation_size;
    size_t index;
    size_t created;
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
    if (!henka_checked_size_multiply(prefab->entity_count, sizeof(*entities), &allocation_size))
    {
        return HENKA_ERROR_NUMERIC_RANGE;
    }
    entities = henka_malloc(allocation_size);
    if (entities == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    created = 0U;
    for (index = 0U; index < prefab->entity_count; ++index)
    {
        entities[index] = henka_scene_create_entity_named(
            target_scene,
            prefab->entries[index].name);
        if (entities[index] == HENKA_INVALID_ENTITY)
        {
            henka_prefab_rollback_entities(target_scene, entities, created);
            henka_free(entities);
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
            henka_prefab_rollback_entities(target_scene, entities, created);
            henka_free(entities);
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
            henka_prefab_rollback_entities(target_scene, entities, created);
            henka_free(entities);
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
                henka_prefab_rollback_entities(target_scene, entities, created);
                henka_free(entities);
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
            henka_prefab_rollback_entities(target_scene, entities, created);
            henka_free(entities);
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
            henka_prefab_rollback_entities(target_scene, entities, created);
            henka_free(entities);
            return result;
        }
    }

    instance = NULL;
    if (out_instance != NULL)
    {
        instance = (henka_prefab_instance*)henka_calloc(1U, sizeof(*instance));
        if (instance == NULL)
        {
            henka_prefab_rollback_entities(target_scene, entities, created);
            henka_free(entities);
            return HENKA_ERROR_OUT_OF_MEMORY;
        }
        instance->target_scene = target_scene;
        instance->entities = entities;
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
        henka_free(entities);
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
