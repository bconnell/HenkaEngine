#include <henka/camera_follow.h>

#include <math.h>

static bool henka_camera_follow_vec3_is_finite(henka_vec3 value)
{
    return isfinite(value.x) && isfinite(value.y) && isfinite(value.z);
}

static bool henka_camera_follow_transform_is_valid(henka_transform transform)
{
    return henka_camera_follow_vec3_is_finite(transform.position) &&
        isfinite(transform.rotation.x) &&
        isfinite(transform.rotation.y) &&
        isfinite(transform.rotation.z) &&
        isfinite(transform.rotation.w) &&
        henka_camera_follow_vec3_is_finite(transform.scale) &&
        fabsf(transform.scale.x) >= 0.01f &&
        fabsf(transform.scale.y) >= 0.01f &&
        fabsf(transform.scale.z) >= 0.01f;
}

static bool henka_camera_follow_desc_is_valid(
    const henka_camera_follow_desc* desc)
{
    return desc != NULL &&
        henka_camera_follow_vec3_is_finite(desc->position_offset) &&
        henka_camera_follow_vec3_is_finite(desc->look_at_offset) &&
        isfinite(desc->position_lag_seconds) &&
        desc->position_lag_seconds >= 0.0;
}

static float henka_camera_follow_smoothing_alpha(
    double delta_seconds,
    double lag_seconds)
{
    const double exponent = -delta_seconds / lag_seconds;
    const double alpha = -expm1(exponent);

    return isfinite(alpha) && alpha >= 0.0 && alpha <= 1.0
        ? (float)alpha
        : 0.0f;
}

static bool henka_camera_follow_cameras_equal(
    const henka_camera* left,
    const henka_camera* right)
{
    return left != NULL && right != NULL &&
        left->position.x == right->position.x &&
        left->position.y == right->position.y &&
        left->position.z == right->position.z &&
        left->yaw_radians == right->yaw_radians &&
        left->pitch_radians == right->pitch_radians &&
        left->roll_radians == right->roll_radians &&
        left->projection_mode == right->projection_mode &&
        left->field_of_view_radians == right->field_of_view_radians &&
        left->orthographic_height == right->orthographic_height &&
        left->near_plane == right->near_plane &&
        left->far_plane == right->far_plane &&
        left->aspect_ratio == right->aspect_ratio &&
        left->movement_speed == right->movement_speed &&
        left->fast_movement_multiplier == right->fast_movement_multiplier;
}

henka_camera_follow_desc henka_camera_follow_desc_default(void)
{
    henka_camera_follow_desc desc;

    desc.position_offset = (henka_vec3){0.0f, 0.0f, 0.0f};
    desc.look_at_offset = (henka_vec3){0.0f, 0.0f, 0.0f};
    desc.position_lag_seconds = 0.0;
    return desc;
}

henka_result henka_camera_follow_scene_entity(
    henka_scene* scene,
    henka_entity target,
    const henka_camera_follow_desc* desc,
    double delta_seconds)
{
    henka_camera current_camera;
    henka_camera candidate_camera;
    henka_transform target_transform;
    henka_vec3 camera_offset;
    henka_vec3 look_at_offset;
    henka_vec3 desired_position;
    henka_vec3 desired_look_at;
    float alpha;

    if (scene == NULL ||
        target == HENKA_INVALID_ENTITY ||
        !henka_camera_follow_desc_is_valid(desc) ||
        !isfinite(delta_seconds) ||
        delta_seconds < 0.0 ||
        delta_seconds > 1.0 ||
        henka_scene_get_camera(scene, &current_camera) != HENKA_SUCCESS ||
        henka_scene_get_entity_world_transform(
            scene, target, &target_transform) != HENKA_SUCCESS ||
        !henka_camera_follow_transform_is_valid(target_transform))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    camera_offset = henka_quat_rotate_vec3(
        target_transform.rotation,
        desc->position_offset);
    look_at_offset = henka_quat_rotate_vec3(
        target_transform.rotation,
        desc->look_at_offset);
    desired_position = henka_vec3_add(
        target_transform.position,
        camera_offset);
    desired_look_at = henka_vec3_add(
        target_transform.position,
        look_at_offset);
    if (!henka_camera_follow_vec3_is_finite(desired_position) ||
        !henka_camera_follow_vec3_is_finite(desired_look_at))
    {
        return HENKA_ERROR_NUMERIC_RANGE;
    }

    candidate_camera = current_camera;
    if (desc->position_lag_seconds == 0.0)
    {
        candidate_camera.position = desired_position;
    }
    else
    {
        alpha = henka_camera_follow_smoothing_alpha(
            delta_seconds,
            desc->position_lag_seconds);
        candidate_camera.position = henka_vec3_add(
            current_camera.position,
            henka_vec3_scale(
                henka_vec3_subtract(desired_position, current_camera.position),
                alpha));
    }
    if (!henka_camera_follow_vec3_is_finite(candidate_camera.position) ||
        !henka_camera_look_at(&candidate_camera, desired_look_at) ||
        !henka_camera_is_valid(&candidate_camera))
    {
        return HENKA_ERROR_NUMERIC_RANGE;
    }
    if (henka_camera_follow_cameras_equal(&current_camera, &candidate_camera))
    {
        return HENKA_SUCCESS;
    }

    return henka_scene_set_camera(scene, &candidate_camera);
}
