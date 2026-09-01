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
    float acceleration;
    float deceleration;
    float slope_limit_cosine;
    henka_vec3 desired_velocity;
    bool jump_queued;
    bool grounded;
    henka_vec3 ground_normal;
    henka_physics_body_id ground_body;
};

static bool henka_character_controller_vec3_finite(henka_vec3 value)
{
    return isfinite(value.x) && isfinite(value.y) && isfinite(value.z);
}

static bool henka_character_controller_double_fits_float(double value)
{
    return isfinite(value) && value >= -(double)FLT_MAX && value <= (double)FLT_MAX;
}

static bool henka_character_controller_slope_limit_cosine(
    float slope_limit_degrees,
    float* out_cosine)
{
    double radians;

    if (out_cosine == NULL || !isfinite(slope_limit_degrees) ||
        slope_limit_degrees < 0.0f || slope_limit_degrees > 90.0f)
    {
        return false;
    }
    radians = (double)slope_limit_degrees *
        3.14159265358979323846 / 180.0;
    if (!isfinite(radians) || !isfinite(cos(radians)))
    {
        return false;
    }
    *out_cosine = (float)cos(radians);
    return isfinite(*out_cosine);
}

static bool henka_character_controller_find_ground_contact(
    const henka_character_controller* controller,
    bool* out_grounded,
    henka_vec3* out_normal,
    henka_physics_body_id* out_body)
{
    const henka_physics_contact* contacts;
    size_t contact_count;
    float best_y = 0.0f;
    bool found = false;
    size_t index;

    if (controller == NULL || out_grounded == NULL || out_normal == NULL ||
        out_body == NULL)
    {
        return false;
    }
    *out_grounded = false;
    *out_normal = (henka_vec3){0.0f, 0.0f, 0.0f};
    *out_body = HENKA_INVALID_PHYSICS_BODY_ID;
    contacts = henka_physics_world_get_contacts(
        controller->world, &contact_count);
    if (contact_count > 0U && contacts == NULL)
    {
        return false;
    }
    for (index = 0U; index < contact_count; ++index)
    {
        henka_physics_contact contact = contacts[index];
        henka_vec3 support_normal;
        henka_physics_body_id support_body;
        double normal_length;

        if (contact.is_trigger ||
            (contact.body_a != controller->body &&
                contact.body_b != controller->body))
        {
            continue;
        }
        support_normal = contact.body_a == controller->body ?
            henka_vec3_scale(contact.normal, -1.0f) : contact.normal;
        support_body = contact.body_a == controller->body ?
            contact.body_b : contact.body_a;
        if (support_body == HENKA_INVALID_PHYSICS_BODY_ID)
        {
            return false;
        }
        normal_length = hypot(
            hypot((double)support_normal.x, (double)support_normal.y),
            (double)support_normal.z);
        if (!henka_character_controller_vec3_finite(support_normal) ||
            !isfinite(normal_length) || fabs(normal_length - 1.0) > 0.001 ||
            support_normal.y <= 0.0001f ||
            support_normal.y + 0.0001f < controller->slope_limit_cosine)
        {
            continue;
        }
        if (!found || support_normal.y > best_y)
        {
            found = true;
            best_y = support_normal.y;
            *out_normal = support_normal;
            *out_body = support_body;
        }
    }
    *out_grounded = found;
    return true;
}

static henka_result henka_character_controller_project_planar_velocity(
    const henka_character_controller* controller,
    henka_vec3 target_velocity,
    henka_vec3* out_velocity)
{
    const henka_physics_contact* contacts;
    size_t contact_count;
    size_t index;

    if (controller == NULL || controller->world == NULL ||
        out_velocity == NULL ||
        !henka_character_controller_vec3_finite(target_velocity))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_velocity = target_velocity;
    contacts = henka_physics_world_get_contacts(
        controller->world, &contact_count);
    if (contact_count > 0U && contacts == NULL)
    {
        return HENKA_ERROR_NUMERIC_RANGE;
    }
    for (index = 0U; index < contact_count; ++index)
    {
        henka_physics_contact contact = contacts[index];
        henka_vec3 support_normal;
        double normal_length;
        double horizontal_length;
        double normal_x;
        double normal_z;
        double inward_velocity;
        double next_x;
        double next_z;

        if (contact.is_trigger ||
            (contact.body_a != controller->body &&
                contact.body_b != controller->body))
        {
            continue;
        }
        support_normal = contact.body_a == controller->body ?
            henka_vec3_scale(contact.normal, -1.0f) : contact.normal;
        normal_length = hypot(
            hypot((double)support_normal.x, (double)support_normal.y),
            (double)support_normal.z);
        if (!henka_character_controller_vec3_finite(support_normal) ||
            !isfinite(normal_length) || fabs(normal_length - 1.0) > 0.001)
        {
            return HENKA_ERROR_NUMERIC_RANGE;
        }
        if (support_normal.y + 0.0001f >= controller->slope_limit_cosine)
        {
            continue;
        }
        horizontal_length = hypot(
            (double)support_normal.x, (double)support_normal.z);
        if (!isfinite(horizontal_length))
        {
            return HENKA_ERROR_NUMERIC_RANGE;
        }
        if (horizontal_length <= 0.0001)
        {
            continue;
        }
        normal_x = (double)support_normal.x / horizontal_length;
        normal_z = (double)support_normal.z / horizontal_length;
        inward_velocity = (double)out_velocity->x * normal_x +
            (double)out_velocity->z * normal_z;
        if (!isfinite(inward_velocity) || inward_velocity >= 0.0)
        {
            continue;
        }
        next_x = (double)out_velocity->x - normal_x * inward_velocity;
        next_z = (double)out_velocity->z - normal_z * inward_velocity;
        if (!henka_character_controller_double_fits_float(next_x) ||
            !henka_character_controller_double_fits_float(next_z))
        {
            return HENKA_ERROR_NUMERIC_RANGE;
        }
        out_velocity->x = (float)next_x;
        out_velocity->z = (float)next_z;
    }
    return HENKA_SUCCESS;
}

static henka_result henka_character_controller_approach_planar_velocity(
    henka_vec3 current,
    henka_vec3 target,
    float rate,
    float timestep,
    henka_vec3* out_velocity)
{
    double delta_x;
    double delta_z;
    double delta_length;
    double maximum_delta;
    double scale;
    double next_x;
    double next_z;

    if (out_velocity == NULL || !henka_character_controller_vec3_finite(current) ||
        !henka_character_controller_vec3_finite(target) || !isfinite(rate) ||
        rate < 0.0f || !isfinite(timestep) || timestep <= 0.0f)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    delta_x = (double)target.x - (double)current.x;
    delta_z = (double)target.z - (double)current.z;
    delta_length = hypot(delta_x, delta_z);
    if (!isfinite(delta_length))
    {
        return HENKA_ERROR_NUMERIC_RANGE;
    }
    if (delta_length <= 0.0)
    {
        *out_velocity = (henka_vec3){current.x, 0.0f, current.z};
        return HENKA_SUCCESS;
    }
    if (rate <= 0.0f)
    {
        *out_velocity = (henka_vec3){target.x, 0.0f, target.z};
        return HENKA_SUCCESS;
    }

    maximum_delta = (double)rate * (double)timestep;
    if (!isfinite(maximum_delta))
    {
        return HENKA_ERROR_NUMERIC_RANGE;
    }
    if (maximum_delta >= delta_length)
    {
        *out_velocity = (henka_vec3){target.x, 0.0f, target.z};
        return HENKA_SUCCESS;
    }

    scale = maximum_delta / delta_length;
    next_x = (double)current.x + delta_x * scale;
    next_z = (double)current.z + delta_z * scale;
    if (!henka_character_controller_double_fits_float(next_x) ||
        !henka_character_controller_double_fits_float(next_z))
    {
        return HENKA_ERROR_NUMERIC_RANGE;
    }
    *out_velocity = (henka_vec3){(float)next_x, 0.0f, (float)next_z};
    return HENKA_SUCCESS;
}

static bool henka_character_controller_desc_valid(
    const henka_character_controller_desc* desc)
{
    return desc != NULL && isfinite(desc->radius) && desc->radius > 0.0f &&
        isfinite(desc->half_height) && desc->half_height >= 0.0f &&
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
    body_desc.collider = henka_physics_collider_capsule(
        desc->radius, desc->half_height);
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
    controller->acceleration = 0.0f;
    controller->deceleration = 0.0f;
    controller->slope_limit_cosine = cosf(
        45.0f * 3.14159265358979323846f / 180.0f);
    controller->desired_velocity = (henka_vec3){0.0f, 0.0f, 0.0f};
    controller->jump_queued = false;
    controller->grounded = false;
    controller->ground_normal = (henka_vec3){0.0f, 0.0f, 0.0f};
    controller->ground_body = HENKA_INVALID_PHYSICS_BODY_ID;
    *out_controller = controller;
    return HENKA_SUCCESS;
}

henka_result henka_character_controller_set_movement_tuning(
    henka_character_controller* controller,
    float acceleration,
    float deceleration)
{
    henka_physics_body_state body_state;

    if (controller == NULL || controller->world == NULL ||
        !isfinite(acceleration) || acceleration < 0.0f ||
        !isfinite(deceleration) || deceleration < 0.0f ||
        henka_physics_body_get_state(
            controller->world,
            controller->body,
            &body_state) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    controller->acceleration = acceleration;
    controller->deceleration = deceleration;
    return HENKA_SUCCESS;
}

henka_result henka_character_controller_set_slope_limit(
    henka_character_controller* controller,
    float slope_limit_degrees)
{
    float slope_limit_cosine;
    float previous_slope_limit_cosine;
    henka_physics_body_state body_state;
    bool grounded;
    henka_vec3 ground_normal;
    henka_physics_body_id ground_body;

    if (controller == NULL || controller->world == NULL ||
        !henka_character_controller_slope_limit_cosine(
            slope_limit_degrees, &slope_limit_cosine) ||
        henka_physics_body_get_state(
            controller->world,
            controller->body,
            &body_state) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    previous_slope_limit_cosine = controller->slope_limit_cosine;
    controller->slope_limit_cosine = slope_limit_cosine;
    if (!henka_character_controller_find_ground_contact(
            controller, &grounded, &ground_normal, &ground_body))
    {
        controller->slope_limit_cosine = previous_slope_limit_cosine;
        return HENKA_ERROR_NUMERIC_RANGE;
    }
    controller->grounded = grounded;
    controller->ground_normal = ground_normal;
    controller->ground_body = ground_body;
    return HENKA_SUCCESS;
}

henka_result henka_character_controller_teleport(
    henka_character_controller* controller,
    henka_transform transform,
    bool clear_velocity)
{
    henka_result result;

    if (controller == NULL || controller->world == NULL ||
        henka_physics_body_get_state(
            controller->world,
            controller->body,
            &(henka_physics_body_state){0}) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_physics_body_set_transform(
        controller->world,
        controller->body,
        transform,
        clear_velocity);
    if (result == HENKA_SUCCESS)
    {
        controller->grounded = false;
        controller->ground_normal = (henka_vec3){0.0f, 0.0f, 0.0f};
        controller->ground_body = HENKA_INVALID_PHYSICS_BODY_ID;
    }
    return result;
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
    float fixed_timestep;
    henka_vec3 target_planar_velocity;
    henka_vec3 planar_velocity;
    henka_vec3 support_velocity = {0.0f, 0.0f, 0.0f};
    bool apply_jump;
    bool inherit_support_motion = false;
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

    if (controller->grounded &&
        controller->ground_body != HENKA_INVALID_PHYSICS_BODY_ID)
    {
        henka_physics_body_state support_state;

        if (henka_physics_body_get_state(
                controller->world,
                controller->ground_body,
                &support_state) == HENKA_SUCCESS &&
            support_state.id != controller->body &&
            support_state.type == HENKA_PHYSICS_BODY_KINEMATIC)
        {
            if (!henka_character_controller_vec3_finite(
                    support_state.linear_velocity))
            {
                return HENKA_ERROR_NUMERIC_RANGE;
            }
            support_velocity = support_state.linear_velocity;
            inherit_support_motion = true;
        }
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
    if (controller->grounded && vertical_velocity < 0.0)
    {
        vertical_velocity = 0.0;
    }
    apply_jump = controller->jump_queued && controller->grounded;
    if (apply_jump)
    {
        vertical_velocity = (double)controller->jump_speed;
    }
    if (inherit_support_motion)
    {
        vertical_velocity += (double)support_velocity.y;
    }
    if (!henka_character_controller_double_fits_float(vertical_velocity) ||
        !henka_character_controller_double_fits_float(
            (double)controller->desired_velocity.x * planar_scale) ||
        !henka_character_controller_double_fits_float(
            (double)controller->desired_velocity.z * planar_scale))
    {
        return HENKA_ERROR_NUMERIC_RANGE;
    }
    target_planar_velocity = (henka_vec3){
        (float)((double)controller->desired_velocity.x * planar_scale),
        0.0f,
        (float)((double)controller->desired_velocity.z * planar_scale)};
    if (inherit_support_motion)
    {
        double support_x =
            (double)target_planar_velocity.x + (double)support_velocity.x;
        double support_z =
            (double)target_planar_velocity.z + (double)support_velocity.z;

        if (!henka_character_controller_double_fits_float(support_x) ||
            !henka_character_controller_double_fits_float(support_z))
        {
            return HENKA_ERROR_NUMERIC_RANGE;
        }
        target_planar_velocity.x = (float)support_x;
        target_planar_velocity.z = (float)support_z;
    }
    result = henka_character_controller_project_planar_velocity(
        controller,
        target_planar_velocity,
        &target_planar_velocity);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    velocity = target_planar_velocity;
    if (controller->acceleration > 0.0f || controller->deceleration > 0.0f)
    {
        fixed_timestep = henka_physics_world_get_fixed_timestep(controller->world);
        if (!isfinite(fixed_timestep) || fixed_timestep <= 0.0f)
        {
            return HENKA_ERROR_NUMERIC_RANGE;
        }
        result = henka_character_controller_approach_planar_velocity(
            (henka_vec3){body_state.linear_velocity.x, 0.0f, body_state.linear_velocity.z},
            target_planar_velocity,
            target_planar_velocity.x != 0.0f || target_planar_velocity.z != 0.0f ?
                controller->acceleration : controller->deceleration,
            fixed_timestep,
            &planar_velocity);
        if (result != HENKA_SUCCESS)
        {
            return result;
        }
        velocity.x = planar_velocity.x;
        velocity.z = planar_velocity.z;
    }
    velocity.y = (float)vertical_velocity;
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
    bool grounded;
    henka_vec3 ground_normal;
    henka_physics_body_id ground_body;

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
    if (!henka_character_controller_find_ground_contact(
            controller, &grounded, &ground_normal, &ground_body))
    {
        return HENKA_ERROR_NUMERIC_RANGE;
    }
    controller->grounded = grounded;
    controller->ground_normal = ground_normal;
    controller->ground_body = ground_body;
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
        controller->grounded,
        controller->jump_queued,
        controller->ground_normal};
    return HENKA_SUCCESS;
}
