#include "object_authoring_tools.h"

size_t sandbox3d_object_authoring_collect_user_entities(
    const henka_scene* scene,
    henka_entity* out_entities,
    size_t capacity)
{
    size_t count;
    size_t index;

    if (scene == NULL)
    {
        return 0U;
    }

    count = 0U;
    for (index = 0U; index < henka_scene_get_entity_count(scene); ++index)
    {
        henka_entity entity = henka_scene_get_entity_at_index(scene, index);
        if (entity == HENKA_INVALID_ENTITY || henka_scene_is_entity_helper(scene, entity))
        {
            continue;
        }

        if (out_entities != NULL && count < capacity)
        {
            out_entities[count] = entity;
        }
        ++count;
    }

    return count;
}

bool sandbox3d_object_authoring_can_edit_entity(
    const henka_scene* scene,
    henka_entity entity)
{
    return scene != NULL &&
        entity != HENKA_INVALID_ENTITY &&
        henka_scene_is_entity_valid(scene, entity) &&
        !henka_scene_is_entity_helper(scene, entity);
}

henka_result sandbox3d_object_authoring_duplicate_entity(
    henka_scene* scene,
    henka_entity source,
    const char* duplicate_name,
    henka_entity* out_duplicate)
{
    henka_entity duplicate;
    henka_transform transform;
    henka_mesh* mesh;
    henka_material material;
    henka_bounds bounds;
    henka_interaction_desc interaction;
    uint32_t flags;
    const char* tag;
    henka_result result;
    bool has_bounds;

    if (out_duplicate == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_duplicate = HENKA_INVALID_ENTITY;

    if (!sandbox3d_object_authoring_can_edit_entity(scene, source) ||
        duplicate_name == NULL || duplicate_name[0] == '\0')
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    if (henka_scene_get_entity_transform(scene, source, &transform) != HENKA_SUCCESS ||
        henka_scene_get_entity_mesh(scene, source, &mesh) != HENKA_SUCCESS ||
        henka_scene_get_entity_material(scene, source, &material) != HENKA_SUCCESS ||
        henka_scene_get_entity_interaction(scene, source, &interaction) != HENKA_SUCCESS ||
        henka_scene_get_entity_flags(scene, source, &flags) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    tag = henka_scene_get_entity_tag(scene, source);
    has_bounds = henka_scene_get_entity_local_bounds(scene, source, &bounds) == HENKA_SUCCESS;
    duplicate = henka_scene_create_entity_named(scene, duplicate_name);
    if (duplicate == HENKA_INVALID_ENTITY)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }

    result = henka_scene_set_entity_transform(scene, duplicate, transform);
    if (result == HENKA_SUCCESS && mesh != NULL)
    {
        result = henka_scene_set_entity_mesh(scene, duplicate, mesh);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_scene_set_entity_material(scene, duplicate, material);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_scene_set_entity_tag(scene, duplicate, tag);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_scene_set_entity_visible(
            scene,
            duplicate,
            henka_scene_is_entity_visible(scene, source));
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_scene_set_entity_flags(scene, duplicate, flags);
    }
    if (result == HENKA_SUCCESS)
    {
        result = has_bounds
            ? henka_scene_set_entity_local_bounds(scene, duplicate, bounds)
            : henka_scene_clear_entity_local_bounds(scene, duplicate);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_scene_set_entity_interaction(scene, duplicate, &interaction);
    }

    if (result != HENKA_SUCCESS)
    {
        henka_scene_destroy_entity(scene, duplicate);
        return result;
    }

    *out_duplicate = duplicate;
    return HENKA_SUCCESS;
}
