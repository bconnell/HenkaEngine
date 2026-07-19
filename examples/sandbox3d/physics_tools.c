#include "physics_tools.h"

henka_physics_body_type sandbox3d_physics_initial_body_type(sandbox3d_physics_sample_slot slot)
{
    (void)slot;
    return HENKA_PHYSICS_BODY_STATIC;
}

henka_physics_body_type sandbox3d_physics_demo_body_type(sandbox3d_physics_sample_slot slot)
{
    switch (slot)
    {
        case SANDBOX3D_PHYSICS_SAMPLE_TEXTURED_CUBE:
        case SANDBOX3D_PHYSICS_SAMPLE_COLORED_CUBE:
        case SANDBOX3D_PHYSICS_SAMPLE_OBJ_MARKER:
            return HENKA_PHYSICS_BODY_DYNAMIC;
        case SANDBOX3D_PHYSICS_SAMPLE_GROUND:
        case SANDBOX3D_PHYSICS_SAMPLE_MISSING_TEXTURE:
        case SANDBOX3D_PHYSICS_SAMPLE_MISSING_MODEL:
        case SANDBOX3D_PHYSICS_SAMPLE_DEBUG_GRID:
        case SANDBOX3D_PHYSICS_SAMPLE_COUNT:
        default:
            return HENKA_PHYSICS_BODY_STATIC;
    }
}

henka_result sandbox3d_physics_activate_only_body(
    henka_physics_world* world,
    const henka_physics_body_id* bodies,
    size_t body_count,
    henka_physics_body_id selected_body,
    henka_scene* scene,
    henka_entity selected_entity)
{
    henka_physics_body_state selected_state;
    henka_transform selected_transform;
    size_t index;

    if (world == NULL || bodies == NULL || body_count == 0U ||
        selected_body == HENKA_INVALID_PHYSICS_BODY_ID || scene == NULL ||
        selected_entity == HENKA_INVALID_ENTITY ||
        henka_physics_body_get_state(world, selected_body, &selected_state) != HENKA_SUCCESS ||
        selected_state.collider.shape == HENKA_PHYSICS_SHAPE_PLANE ||
        henka_scene_get_entity_transform(scene, selected_entity, &selected_transform) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    for (index = 0U; index < body_count; ++index)
    {
        henka_physics_body_state state;
        const henka_physics_body_id body = bodies[index];

        if (body == HENKA_INVALID_PHYSICS_BODY_ID || body == selected_body)
        {
            continue;
        }
        if (henka_physics_body_get_state(world, body, &state) != HENKA_SUCCESS)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        if (henka_physics_body_set_type(world, body, HENKA_PHYSICS_BODY_STATIC) != HENKA_SUCCESS ||
            henka_physics_body_clear_velocity(world, body) != HENKA_SUCCESS)
        {
            return HENKA_ERROR_UNKNOWN;
        }
    }

    if (henka_physics_body_set_transform(world, selected_body, selected_transform, true) != HENKA_SUCCESS ||
        henka_physics_body_set_type(world, selected_body, HENKA_PHYSICS_BODY_DYNAMIC) != HENKA_SUCCESS ||
        henka_physics_body_clear_velocity(world, selected_body) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_UNKNOWN;
    }

    return HENKA_SUCCESS;
}
