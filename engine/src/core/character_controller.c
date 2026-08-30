#include <float.h>
#include <math.h>

#include <henka/character_controller.h>
#include <henka/memory.h>

struct henka_character_controller
{
    henka_physics_world* world;
    henka_physics_body_id body;
    float max_speed;
    float jump_speed;
    henka_vec3 desired_velocity;
    bool jump_queued;
};

static bool henka_character_controller_vec3_finite(henka_vec3 value)
{
    return isfinite(value.x) && isfinite(value.y) && isfinite(value.z);
}

static bool henka_character_controller_double_fits_float(double value)
{
    return isfinite(value) && value >= -(double)FLT_MAX && value <= (double)FLT_MAX;
}

static bool henka_character_controller_desc_valid(
    const henka_character_controller_desc* desc)
{
    return desc != NULL && isfinite(desc->radius) && desc->radius > 0.0f &&
        isfinite(desc->max_speed) && desc->max_speed > 0.0f &&
        isfinite(desc->jump_speed) && desc->jump_speed >= 0.0f &&
        desc->layer != 0U;
}

henka_result henka_character_controller_create(
    henka_physics_world* world,
    const henka_character_controller_desc* desc,
    henka_character_controller** out_controller)
{
    henka_character_controller* controller;
    henka_physics_body_desc body_desc;
    henka_result body_result;

    if (out_controller == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_controller = NULL;
    if (world == NULL || !henka_character_controller_desc_valid(desc))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    controller = (henka_character_controller*)henka_malloc(sizeof(*controller));
    if (controller == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    body_desc = (henka_physics_body_desc){0};
    body_desc.type = HENKA_PHYSICS_BODY_DYNAMIC;
    body_desc.transform = desc->transform;
    body_desc.mass = 1.0f;
    body_desc.material = henka_physics_material_default();
    body_desc.collider = henka_physics_collider_sphere(desc->radius);
    body_desc.collider.layer = desc->layer;
    body_desc.collider.mask = desc->mask;
    body_desc.linked_scene = desc->linked_scene;
    body_desc.linked_entity = desc->linked_entity;
    body_result = henka_physics_body_create(world, &body_desc, &controller->body);
    if (body_result != HENKA_SUCCESS)
    {
        henka_free(controller);
        return body_result;
    }

    controller->world = world;
    controller->max_speed = desc->max_speed;
    controller->jump_speed = desc->jump_speed;
    controller->desired_velocity = (henka_vec3){0.0f, 0.0f, 0.0f};
    controller->jump_queued = false;
    *out_controller = controller;
    return HENKA_SUCCESS;
}

henka_result henka_character_controller_destroy(
    henka_character_controller* controller)
{
    henka_result result;

    if (controller == NULL || controller->world == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_physics_body_destroy(controller->world, controller->body);
    if (result == HENKA_ERROR_INVALID_ARGUMENT)
    {
        controller->world = NULL;
        henka_free(controller);
        return HENKA_SUCCESS;
    }
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    controller->world = NULL;
    henka_free(controller);
    return HENKA_SUCCESS;
}

henka_result henka_character_controller_set_planar_velocity(
    henka_character_controller* controller,
    henka_vec3 desired_velocity)
{
    henka_physics_body_state body_state;

    if (controller == NULL || !henka_character_controller_vec3_finite(desired_velocity) ||
        fabsf(desired_velocity.y) > 0.0001f ||
        controller->world == NULL ||
        henka_physics_body_get_state(
            controller->world,
            controller->body,
            &body_state) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    controller->desired_velocity = (henka_vec3){
        desired_velocity.x,
        0.0f,
        desired_velocity.z};
    return HENKA_SUCCESS;
}

henka_result henka_character_controller_queue_jump(
    henka_character_controller* controller)
{
    henka_physics_body_state body_state;

    if (controller == NULL || controller->world == NULL ||
        henka_physics_body_get_state(
            controller->world,
            controller->body,
            &body_state) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    controller->jump_queued = true;
    return HENKA_SUCCESS;
}

henka_result henka_character_controller_prepare_step(
    henka_character_controller* controller)
{
    henka_physics_body_state body_state;
    henka_vec3 velocity;
    double planar_length;
    double planar_scale;
    double vertical_velocity;
    bool apply_jump;
    henka_result result;

    if (controller == NULL || controller->world == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (henka_physics_body_get_state(
            controller->world,
            controller->body,
            &body_state) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    planar_length = hypot(
        (double)controller->desired_velocity.x,
        (double)controller->desired_velocity.z);
    if (!isfinite(planar_length))
    {
        return HENKA_ERROR_NUMERIC_RANGE;
    }
    planar_scale = planar_length > (double)controller->max_speed ?
        (double)controller->max_speed / planar_length : 1.0;
    vertical_velocity = body_state.linear_velocity.y;
    if (body_state.grounded && vertical_velocity < 0.0)
    {
        vertical_velocity = 0.0;
    }
    apply_jump = controller->jump_queued && body_state.grounded;
    if (apply_jump)
    {
        vertical_velocity = (double)controller->jump_speed;
    }
    if (!henka_character_controller_double_fits_float(vertical_velocity) ||
        !henka_character_controller_double_fits_float(
            (double)controller->desired_velocity.x * planar_scale) ||
        !henka_character_controller_double_fits_float(
            (double)controller->desired_velocity.z * planar_scale))
    {
        return HENKA_ERROR_NUMERIC_RANGE;
    }
    velocity = (henka_vec3){
        (float)((double)controller->desired_velocity.x * planar_scale),
        (float)vertical_velocity,
        (float)((double)controller->desired_velocity.z * planar_scale)};
    result = henka_physics_body_set_linear_velocity(
            controller->world,
            controller->body,
            velocity);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    if (apply_jump)
    {
        controller->jump_queued = false;
    }
    return HENKA_SUCCESS;
}

henka_result henka_character_controller_sync_after_step(
    henka_character_controller* controller)
{
    henka_physics_body_state body_state;

    if (controller == NULL || controller->world == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (henka_physics_body_get_state(
            controller->world,
            controller->body,
            &body_state) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return HENKA_SUCCESS;
}

henka_result henka_character_controller_get_state(
    const henka_character_controller* controller,
    henka_character_controller_state* out_state)
{
    henka_physics_body_state body_state;

    if (out_state == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_state = (henka_character_controller_state){0};
    out_state->body = HENKA_INVALID_PHYSICS_BODY_ID;
    if (controller == NULL || controller->world == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (henka_physics_body_get_state(
            controller->world,
            controller->body,
            &body_state) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_state = (henka_character_controller_state){
        body_state.id,
        body_state.transform,
        body_state.linear_velocity,
        body_state.grounded,
        controller->jump_queued};
    return HENKA_SUCCESS;
}
