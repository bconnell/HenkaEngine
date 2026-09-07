#include "scene_hierarchy_projection.h"

static henka_result sandbox3d_scene_hierarchy_projection_append_children(
    const henka_scene* scene,
    henka_entity parent,
    size_t depth,
    size_t entity_limit,
    sandbox3d_scene_hierarchy_include_fn include,
    void* context,
    sandbox3d_scene_hierarchy_row* rows,
    size_t row_capacity,
    size_t* in_out_row_count)
{
    size_t child_count;
    size_t child_index;

    if (scene == NULL || rows == NULL || in_out_row_count == NULL ||
        include == NULL || depth > entity_limit)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (henka_scene_get_entity_child_count(scene, parent, &child_count) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    for (child_index = 0U; child_index < child_count; ++child_index)
    {
        henka_entity child;
        bool included;
        henka_result result;

        if (henka_scene_get_entity_child_at_index(
                scene, parent, child_index, &child) != HENKA_SUCCESS ||
            child == HENKA_INVALID_ENTITY)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        included = include(scene, child, context);
        if (included)
        {
            if (*in_out_row_count >= row_capacity)
            {
                return HENKA_ERROR_LIMIT;
            }
            rows[*in_out_row_count].entity = child;
            rows[*in_out_row_count].depth = depth;
            ++(*in_out_row_count);
        }
        result = sandbox3d_scene_hierarchy_projection_append_children(
            scene,
            child,
            included ? depth + 1U : depth,
            entity_limit,
            include,
            context,
            rows,
            row_capacity,
            in_out_row_count);
        if (result != HENKA_SUCCESS)
        {
            return result;
        }
    }
    return HENKA_SUCCESS;
}

henka_result sandbox3d_scene_hierarchy_projection_build(
    const henka_scene* scene,
    sandbox3d_scene_hierarchy_include_fn include,
    void* context,
    sandbox3d_scene_hierarchy_row* rows,
    size_t row_capacity,
    size_t* out_row_count)
{
    size_t entity_count;

    if (scene == NULL || include == NULL || rows == NULL || out_row_count == NULL ||
        row_capacity == 0U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_row_count = 0U;
    entity_count = henka_scene_get_entity_count(scene);
    return sandbox3d_scene_hierarchy_projection_append_children(
        scene,
        HENKA_INVALID_ENTITY,
        0U,
        entity_count,
        include,
        context,
        rows,
        row_capacity,
        out_row_count);
}
