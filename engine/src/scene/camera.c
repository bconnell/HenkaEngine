#include <henka/camera.h>

#include <math.h>

#include <henka/core.h>
#if !defined(HENKA_RUNTIME_HEADLESS)
#include <henka/engine.h>
#include <henka/input.h>
#endif

static float henka_clamp_pitch(float pitch_radians)
{
    const float max_pitch = 1.55334306f;

    if (pitch_radians > max_pitch)
    {
        return max_pitch;
    }

    if (pitch_radians < -max_pitch)
    {
        return -max_pitch;
    }

    return pitch_radians;
}

static void henka_mat4_multiply_vec4(const henka_mat4* matrix, const float input[4], float output[4])
{
    output[0] =
        matrix->m[0] * input[0] +
        matrix->m[4] * input[1] +
        matrix->m[8] * input[2] +
        matrix->m[12] * input[3];
    output[1] =
        matrix->m[1] * input[0] +
        matrix->m[5] * input[1] +
        matrix->m[9] * input[2] +
        matrix->m[13] * input[3];
    output[2] =
        matrix->m[2] * input[0] +
        matrix->m[6] * input[1] +
        matrix->m[10] * input[2] +
        matrix->m[14] * input[3];
    output[3] =
        matrix->m[3] * input[0] +
        matrix->m[7] * input[1] +
        matrix->m[11] * input[2] +
        matrix->m[15] * input[3];
}

static const float g_henka_camera_default_field_of_view = 60.0f * HENKA_DEG_TO_RAD;
static const float g_henka_camera_default_aspect_ratio = 1.0f;
static const float g_henka_camera_default_near_plane = 0.1f;
static const float g_henka_camera_default_far_plane = 100.0f;
static const float g_henka_camera_default_orthographic_height = 6.0f;
static const float g_henka_camera_default_movement_speed = 4.0f;
static const float g_henka_camera_default_fast_movement_multiplier = 2.5f;
static const float g_henka_camera_minimum_direction_length = 0.000001f;

static float henka_max_float(float left, float right)
{
    return left > right ? left : right;
}

static bool henka_vec2_is_finite(henka_vec2 value)
{
    return isfinite(value.x) && isfinite(value.y);
}

static bool henka_vec3_is_finite(henka_vec3 value)
{
    return isfinite(value.x) && isfinite(value.y) && isfinite(value.z);
}

static bool henka_bounds_are_valid(henka_bounds bounds)
{
    return henka_vec3_is_finite(bounds.center) &&
        henka_vec3_is_finite(bounds.extents) &&
        bounds.extents.x >= 0.0f &&
        bounds.extents.y >= 0.0f &&
        bounds.extents.z >= 0.0f;
}

static bool henka_camera_pose_is_valid(const henka_camera* camera)
{
    return camera != NULL &&
        henka_vec3_is_finite(camera->position) &&
        isfinite(camera->yaw_radians) &&
        isfinite(camera->pitch_radians) &&
        isfinite(camera->roll_radians) &&
        camera->pitch_radians >= -HENKA_PI * 0.5f - 0.0001f &&
        camera->pitch_radians <= HENKA_PI * 0.5f + 0.0001f;
}

static bool henka_camera_projection_is_valid(const henka_camera* camera)
{
    if (camera == NULL ||
        camera->projection_mode < HENKA_CAMERA_PROJECTION_PERSPECTIVE ||
        camera->projection_mode > HENKA_CAMERA_PROJECTION_ORTHOGRAPHIC ||
        !isfinite(camera->aspect_ratio) ||
        !isfinite(camera->near_plane) ||
        !isfinite(camera->far_plane) ||
        camera->aspect_ratio <= 0.0f ||
        camera->near_plane <= 0.0f ||
        camera->far_plane <= camera->near_plane)
    {
        return false;
    }

    if (camera->projection_mode == HENKA_CAMERA_PROJECTION_ORTHOGRAPHIC)
    {
        return isfinite(camera->orthographic_height) &&
            camera->orthographic_height > 0.0f;
    }

    return isfinite(camera->field_of_view_radians) &&
        camera->field_of_view_radians > 0.001f &&
        camera->field_of_view_radians < HENKA_PI - 0.001f;
}

bool henka_camera_is_valid(const henka_camera* camera)
{
    return henka_camera_pose_is_valid(camera) &&
        henka_camera_projection_is_valid(camera) &&
        isfinite(camera->movement_speed) &&
        isfinite(camera->fast_movement_multiplier) &&
        camera->movement_speed >= 0.0f &&
        camera->fast_movement_multiplier > 0.0f;
}

henka_camera henka_camera_create_perspective(float field_of_view_radians, float aspect_ratio, float near_plane, float far_plane)
{
    henka_camera camera;

    if (!isfinite(field_of_view_radians) ||
        field_of_view_radians <= 0.001f ||
        field_of_view_radians >= HENKA_PI - 0.001f)
    {
        field_of_view_radians = g_henka_camera_default_field_of_view;
    }
    if (!isfinite(aspect_ratio) || aspect_ratio <= 0.0f)
    {
        aspect_ratio = g_henka_camera_default_aspect_ratio;
    }
    if (!isfinite(near_plane) || near_plane <= 0.0f)
    {
        near_plane = g_henka_camera_default_near_plane;
    }
    if (!isfinite(far_plane) || far_plane <= near_plane)
    {
        far_plane = near_plane + g_henka_camera_default_far_plane;
    }

    camera.position.x = 0.0f;
    camera.position.y = 1.5f;
    camera.position.z = 4.5f;
    camera.yaw_radians = -HENKA_PI * 0.5f;
    camera.pitch_radians = -0.25f;
    camera.roll_radians = 0.0f;
    camera.projection_mode = HENKA_CAMERA_PROJECTION_PERSPECTIVE;
    camera.field_of_view_radians = field_of_view_radians;
    camera.orthographic_height = g_henka_camera_default_orthographic_height;
    camera.near_plane = near_plane;
    camera.far_plane = far_plane;
    camera.aspect_ratio = aspect_ratio;
    camera.movement_speed = g_henka_camera_default_movement_speed;
    camera.fast_movement_multiplier = g_henka_camera_default_fast_movement_multiplier;
    return camera;
}

henka_camera henka_camera_create_orthographic(float orthographic_height, float aspect_ratio, float near_plane, float far_plane)
{
    henka_camera camera;

    camera = henka_camera_create_perspective(
        g_henka_camera_default_field_of_view,
        aspect_ratio,
        near_plane,
        far_plane);
    camera.projection_mode = HENKA_CAMERA_PROJECTION_ORTHOGRAPHIC;
    camera.orthographic_height =
        isfinite(orthographic_height) && orthographic_height > 0.0f
            ? orthographic_height
            : g_henka_camera_default_orthographic_height;
    return camera;
}

const char* henka_camera_preset_get_label(henka_camera_preset preset)
{
    switch (preset)
    {
        case HENKA_CAMERA_PRESET_PERSPECTIVE_3D:
            return "Perspective 3D";
        case HENKA_CAMERA_PRESET_SIDE_2_5D:
            return "Side 2.5D";
        case HENKA_CAMERA_PRESET_TOP_DOWN_2_5D:
            return "Top-down 2.5D";
        case HENKA_CAMERA_PRESET_ISOMETRIC_2_5D:
            return "Isometric 2.5D";
        case HENKA_CAMERA_PRESET_COUNT:
        default:
            return "Unknown";
    }
}

henka_result henka_camera_apply_preset(henka_camera* camera, henka_camera_preset preset, henka_vec3 target)
{
    float distance;
    henka_vec3 forward;

    if (camera == NULL ||
        !henka_vec3_is_finite(target) ||
        preset < HENKA_CAMERA_PRESET_PERSPECTIVE_3D ||
        preset >= HENKA_CAMERA_PRESET_COUNT)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    if (!isfinite(camera->aspect_ratio) || camera->aspect_ratio <= 0.0f)
    {
        camera->aspect_ratio = 1.0f;
    }
    if (!isfinite(camera->near_plane) || camera->near_plane <= 0.0f)
    {
        camera->near_plane = 0.1f;
    }
    if (!isfinite(camera->far_plane) || camera->far_plane <= camera->near_plane)
    {
        camera->far_plane = camera->near_plane + 100.0f;
    }
    if (!isfinite(camera->field_of_view_radians) || camera->field_of_view_radians <= 0.0f)
    {
        camera->field_of_view_radians = 60.0f * HENKA_DEG_TO_RAD;
    }
    if (!isfinite(camera->movement_speed) || camera->movement_speed <= 0.0f)
    {
        camera->movement_speed = 4.0f;
    }
    if (!isfinite(camera->fast_movement_multiplier) || camera->fast_movement_multiplier <= 0.0f)
    {
        camera->fast_movement_multiplier = 2.5f;
    }
    camera->roll_radians = 0.0f;

    switch (preset)
    {
        case HENKA_CAMERA_PRESET_PERSPECTIVE_3D:
            camera->projection_mode = HENKA_CAMERA_PROJECTION_PERSPECTIVE;
            camera->yaw_radians = -HENKA_PI * 0.5f;
            camera->pitch_radians = -0.22f;
            camera->orthographic_height = 6.0f;
            distance = 8.6f;
            break;

        case HENKA_CAMERA_PRESET_SIDE_2_5D:
            camera->projection_mode = HENKA_CAMERA_PROJECTION_ORTHOGRAPHIC;
            camera->yaw_radians = -HENKA_PI * 0.5f;
            camera->pitch_radians = 0.0f;
            camera->orthographic_height = 8.0f;
            distance = 10.0f;
            break;

        case HENKA_CAMERA_PRESET_TOP_DOWN_2_5D:
            camera->projection_mode = HENKA_CAMERA_PROJECTION_ORTHOGRAPHIC;
            camera->yaw_radians = -HENKA_PI * 0.5f;
            camera->pitch_radians = -HENKA_PI * 0.5f;
            camera->orthographic_height = 10.0f;
            distance = 12.0f;
            break;

        case HENKA_CAMERA_PRESET_ISOMETRIC_2_5D:
            camera->projection_mode = HENKA_CAMERA_PROJECTION_ORTHOGRAPHIC;
            camera->yaw_radians = -HENKA_PI * 0.75f;
            camera->pitch_radians = -0.6154797087f;
            camera->orthographic_height = 10.0f;
            distance = 12.0f;
            break;

        case HENKA_CAMERA_PRESET_COUNT:
        default:
            return HENKA_ERROR_INVALID_ARGUMENT;
    }

    forward = henka_camera_get_forward(camera);
    camera->position = henka_vec3_subtract(target, henka_vec3_scale(forward, distance));
    if (!henka_camera_is_valid(camera))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    return HENKA_SUCCESS;
}

henka_vec3 henka_camera_get_forward(const henka_camera* camera)
{
    henka_vec3 forward;

    if (camera == NULL ||
        !isfinite(camera->yaw_radians) ||
        !isfinite(camera->pitch_radians))
    {
        forward.x = 0.0f;
        forward.y = 0.0f;
        forward.z = -1.0f;
        return forward;
    }

    forward.x = cosf(camera->pitch_radians) * cosf(camera->yaw_radians);
    forward.y = sinf(camera->pitch_radians);
    forward.z = cosf(camera->pitch_radians) * sinf(camera->yaw_radians);
    return henka_vec3_normalize(forward);
}

henka_vec3 henka_camera_get_right(const henka_camera* camera)
{
    henka_vec3 forward;
    henka_vec3 right;
    henka_vec3 base_up;
    henka_vec3 world_up;
    float roll_cosine;
    float roll_sine;

    world_up = (henka_vec3){0.0f, 1.0f, 0.0f};
    if (camera == NULL)
    {
        return (henka_vec3){1.0f, 0.0f, 0.0f};
    }

    forward = henka_camera_get_forward(camera);
    right = henka_vec3_cross(forward, world_up);
    if (!henka_vec3_is_finite(right) || henka_vec3_length(right) <= 0.0001f)
    {
        right = (henka_vec3)
        {
            -sinf(camera->yaw_radians),
            0.0f,
            cosf(camera->yaw_radians)
        };
    }

    if (!henka_vec3_is_finite(right) || henka_vec3_length(right) <= 0.0001f)
    {
        return (henka_vec3){1.0f, 0.0f, 0.0f};
    }

    right = henka_vec3_normalize(right);
    base_up = henka_vec3_normalize(henka_vec3_cross(right, forward));
    if (!henka_vec3_is_finite(base_up) || henka_vec3_length(base_up) <= 0.0001f)
    {
        return right;
    }
    roll_cosine = cosf(camera->roll_radians);
    roll_sine = sinf(camera->roll_radians);
    return henka_vec3_normalize(henka_vec3_subtract(
        henka_vec3_scale(right, roll_cosine),
        henka_vec3_scale(base_up, roll_sine)));
}

henka_vec3 henka_camera_get_up(const henka_camera* camera)
{
    henka_vec3 forward;
    henka_vec3 right;
    henka_vec3 up;

    if (camera == NULL)
    {
        return (henka_vec3){0.0f, 1.0f, 0.0f};
    }

    forward = henka_camera_get_forward(camera);
    right = henka_camera_get_right(camera);
    up = henka_vec3_cross(right, forward);
    if (!henka_vec3_is_finite(up) || henka_vec3_length(up) <= 0.0001f)
    {
        return (henka_vec3){0.0f, 1.0f, 0.0f};
    }

    return henka_vec3_normalize(up);
}

henka_mat4 henka_camera_get_view_matrix(const henka_camera* camera)
{
    henka_vec3 target;

    if (!henka_camera_pose_is_valid(camera))
    {
        return henka_mat4_identity();
    }

    target = henka_vec3_add(camera->position, henka_camera_get_forward(camera));
    return henka_mat4_look_at(camera->position, target, henka_camera_get_up(camera));
}

henka_mat4 henka_camera_get_projection_matrix(const henka_camera* camera)
{
    if (!henka_camera_projection_is_valid(camera))
    {
        return henka_mat4_identity();
    }

    if (camera->projection_mode == HENKA_CAMERA_PROJECTION_ORTHOGRAPHIC)
    {
        const float half_height = camera->orthographic_height * 0.5f;
        const float half_width = half_height * camera->aspect_ratio;
        return henka_mat4_orthographic(-half_width, half_width, -half_height, half_height, camera->near_plane, camera->far_plane);
    }

    return henka_mat4_perspective(camera->field_of_view_radians, camera->aspect_ratio, camera->near_plane, camera->far_plane);
}

void henka_camera_set_aspect_ratio(henka_camera* camera, float aspect_ratio)
{
    if (camera != NULL && isfinite(aspect_ratio) && aspect_ratio > 0.0f)
    {
        camera->aspect_ratio = aspect_ratio;
    }
}

henka_result henka_camera_zoom_orthographic(
    henka_camera* camera,
    float zoom_factor,
    float minimum_height,
    float maximum_height)
{
    float next_height;

    if (camera == NULL ||
        camera->projection_mode != HENKA_CAMERA_PROJECTION_ORTHOGRAPHIC ||
        !isfinite(camera->orthographic_height) ||
        camera->orthographic_height <= 0.0f ||
        !isfinite(zoom_factor) ||
        zoom_factor <= 0.0f ||
        !isfinite(minimum_height) ||
        !isfinite(maximum_height) ||
        minimum_height <= 0.0f ||
        maximum_height < minimum_height)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    next_height = camera->orthographic_height * zoom_factor;
    if (!isfinite(next_height))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    if (next_height < minimum_height)
    {
        next_height = minimum_height;
    }
    if (next_height > maximum_height)
    {
        next_height = maximum_height;
    }

    camera->orthographic_height = next_height;
    return HENKA_SUCCESS;
}

float henka_camera_clamp_pitch(float pitch_radians)
{
    return isfinite(pitch_radians) ? henka_clamp_pitch(pitch_radians) : 0.0f;
}

void henka_camera_reset(henka_camera* camera, const henka_camera* source)
{
    if (camera == NULL || !henka_camera_is_valid(source))
    {
        return;
    }

    *camera = *source;
}

bool henka_camera_look_at(henka_camera* camera, henka_vec3 target)
{
    henka_vec3 direction;
    float horizontal_length;

    if (!henka_camera_is_valid(camera) || !henka_vec3_is_finite(target))
    {
        return false;
    }

    direction = henka_vec3_subtract(target, camera->position);
    if (henka_vec3_length(direction) <= 0.0001f)
    {
        return false;
    }

    direction = henka_vec3_normalize(direction);
    horizontal_length = sqrtf(direction.x * direction.x + direction.z * direction.z);
    camera->yaw_radians = atan2f(direction.z, direction.x);
    camera->pitch_radians = henka_clamp_pitch(atan2f(direction.y, horizontal_length));
    camera->roll_radians = 0.0f;
    return true;
}

bool henka_camera_look_at_with_up(
    henka_camera* camera,
    henka_vec3 target,
    henka_vec3 up)
{
    henka_vec3 direction;
    henka_vec3 base_right;
    henka_vec3 base_up;
    henka_vec3 desired_up;
    float horizontal_length;
    float up_length;

    if (!henka_camera_is_valid(camera) ||
        !henka_vec3_is_finite(target) ||
        !henka_vec3_is_finite(up))
    {
        return false;
    }
    direction = henka_vec3_subtract(target, camera->position);
    if (henka_vec3_length(direction) <= 0.0001f)
    {
        return false;
    }
    direction = henka_vec3_normalize(direction);
    up_length = henka_vec3_length(up);
    if (!isfinite(up_length) || up_length <= 0.0001f)
    {
        return false;
    }
    desired_up = henka_vec3_normalize(up);
    desired_up = henka_vec3_subtract(
        desired_up,
        henka_vec3_scale(direction, henka_vec3_dot(desired_up, direction)));
    if (!henka_vec3_is_finite(desired_up) || henka_vec3_length(desired_up) <= 0.0001f)
    {
        return false;
    }
    desired_up = henka_vec3_normalize(desired_up);
    horizontal_length = sqrtf(direction.x * direction.x + direction.z * direction.z);
    camera->yaw_radians = atan2f(direction.z, direction.x);
    camera->pitch_radians = atan2f(direction.y, horizontal_length);
    /* Derive the new basis without inheriting a previous camera roll. */
    camera->roll_radians = 0.0f;
    base_right = henka_camera_get_right(camera);
    base_up = henka_vec3_normalize(henka_vec3_cross(base_right, direction));
    if (!henka_vec3_is_finite(base_up) || henka_vec3_length(base_up) <= 0.0001f)
    {
        camera->roll_radians = 0.0f;
        return false;
    }
    camera->roll_radians = atan2f(
        henka_vec3_dot(desired_up, base_right),
        henka_vec3_dot(desired_up, base_up));
    return henka_camera_is_valid(camera);
}

void henka_camera_move_relative(henka_camera* camera, henka_vec3 local_direction, float distance)
{
    henka_vec3 forward;
    henka_vec3 move_direction;
    henka_vec3 right;
    henka_vec3 up;

    if (!henka_camera_is_valid(camera) ||
        !henka_vec3_is_finite(local_direction) ||
        !isfinite(distance) ||
        distance == 0.0f)
    {
        return;
    }

    forward = henka_camera_get_forward(camera);
    right = henka_camera_get_right(camera);
    up = henka_camera_get_up(camera);

    move_direction = (henka_vec3){0.0f, 0.0f, 0.0f};
    move_direction = henka_vec3_add(move_direction, henka_vec3_scale(right, local_direction.x));
    move_direction = henka_vec3_add(move_direction, henka_vec3_scale(up, local_direction.y));
    move_direction = henka_vec3_add(move_direction, henka_vec3_scale(forward, local_direction.z));
    if (henka_vec3_length(move_direction) <= 0.0f)
    {
        return;
    }

    move_direction = henka_vec3_normalize(move_direction);
    move_direction = henka_vec3_scale(move_direction, distance);
    if (!henka_vec3_is_finite(move_direction))
    {
        return;
    }

    move_direction = henka_vec3_add(camera->position, move_direction);
    if (henka_vec3_is_finite(move_direction))
    {
        camera->position = move_direction;
    }
}

bool henka_camera_focus_on_bounds(henka_camera* camera, henka_bounds bounds)
{
    henka_vec3 forward;
    float radius;
    float distance;
    float vertical_distance;
    float horizontal_distance;
    float horizontal_fov;

    if (!henka_camera_is_valid(camera) || !henka_bounds_are_valid(bounds))
    {
        return false;
    }

    radius = henka_vec3_length(bounds.extents);
    if (radius <= 0.0f)
    {
        radius = 0.5f;
    }

    forward = henka_camera_get_forward(camera);
    if (camera->projection_mode == HENKA_CAMERA_PROJECTION_ORTHOGRAPHIC)
    {
        henka_vec3 next_position = henka_vec3_subtract(
            bounds.center,
            henka_vec3_scale(forward, radius + camera->near_plane + 1.0f));
        if (!henka_vec3_is_finite(next_position))
        {
            return false;
        }

        camera->position = next_position;
        return true;
    }

    vertical_distance = radius / tanf(camera->field_of_view_radians * 0.5f);
    horizontal_fov = 2.0f * atanf(tanf(camera->field_of_view_radians * 0.5f) * camera->aspect_ratio);
    horizontal_distance = radius / tanf(horizontal_fov * 0.5f);
    distance = vertical_distance > horizontal_distance ? vertical_distance : horizontal_distance;
    distance += radius * 0.6f;
    if (distance < camera->near_plane + radius)
    {
        distance = camera->near_plane + radius + 0.5f;
    }

    if (!isfinite(distance))
    {
        return false;
    }

    {
        henka_vec3 next_position = henka_vec3_subtract(
            bounds.center,
            henka_vec3_scale(forward, distance));
        if (!henka_vec3_is_finite(next_position))
        {
            return false;
        }

        camera->position = next_position;
    }
    return true;
}

bool henka_camera_frame_bounds(henka_camera* camera, henka_bounds bounds, float yaw_radians, float pitch_radians)
{
    float distance;
    float effective_aspect;
    float framed_height;
    float horizontal_distance;
    float horizontal_fov;
    float radius;
    float vertical_distance;
    henka_camera next_camera;
    henka_vec3 forward;
    henka_vec3 next_position;

    if (!henka_camera_is_valid(camera) ||
        !henka_bounds_are_valid(bounds) ||
        !isfinite(yaw_radians) ||
        !isfinite(pitch_radians))
    {
        return false;
    }

    radius = henka_vec3_length(bounds.extents);
    if (!isfinite(radius) || radius <= 0.0f)
    {
        radius = 0.5f;
    }

    next_camera = *camera;
    next_camera.yaw_radians = yaw_radians;
    next_camera.pitch_radians =
        next_camera.projection_mode == HENKA_CAMERA_PROJECTION_ORTHOGRAPHIC &&
        fabsf(fabsf(pitch_radians) - HENKA_PI * 0.5f) <= 0.0001f
            ? pitch_radians
            : henka_clamp_pitch(pitch_radians);
    forward = henka_camera_get_forward(&next_camera);

    if (next_camera.projection_mode == HENKA_CAMERA_PROJECTION_ORTHOGRAPHIC)
    {
        effective_aspect = next_camera.aspect_ratio;
        framed_height = radius * 2.4f;
        if (effective_aspect < 1.0f)
        {
            framed_height /= effective_aspect;
        }

        next_camera.orthographic_height = henka_max_float(framed_height, 0.5f);
        next_position = henka_vec3_subtract(
            bounds.center,
            henka_vec3_scale(forward, radius + next_camera.near_plane + 2.0f));
        if (!henka_vec3_is_finite(next_position))
        {
            return false;
        }

        next_camera.position = next_position;
        if (!henka_camera_is_valid(&next_camera))
        {
            return false;
        }

        *camera = next_camera;
        return true;
    }

    vertical_distance = radius / tanf(next_camera.field_of_view_radians * 0.5f);
    horizontal_fov =
        2.0f * atanf(tanf(next_camera.field_of_view_radians * 0.5f) * next_camera.aspect_ratio);
    horizontal_distance = radius / tanf(horizontal_fov * 0.5f);
    distance = henka_max_float(vertical_distance, horizontal_distance) + radius * 0.9f;
    distance = henka_max_float(distance, next_camera.near_plane + radius + 0.75f);
    if (!isfinite(distance))
    {
        return false;
    }

    next_position = henka_vec3_subtract(bounds.center, henka_vec3_scale(forward, distance));
    if (!henka_vec3_is_finite(next_position))
    {
        return false;
    }

    next_camera.position = next_position;
    if (!henka_camera_is_valid(&next_camera))
    {
        return false;
    }

    *camera = next_camera;
    return true;
}

bool henka_camera_orbit_target(henka_camera* camera, henka_vec3 target, float delta_yaw_radians, float delta_pitch_radians)
{
    float distance;
    henka_camera next_camera;
    henka_vec3 forward;
    henka_vec3 next_position;

    if (!henka_camera_is_valid(camera) ||
        !henka_vec3_is_finite(target) ||
        !isfinite(delta_yaw_radians) ||
        !isfinite(delta_pitch_radians))
    {
        return false;
    }

    distance = henka_vec3_length(henka_vec3_subtract(target, camera->position));
    if (!isfinite(distance))
    {
        return false;
    }
    if (distance <= 0.0001f)
    {
        distance = 1.0f;
    }

    next_camera = *camera;
    next_camera.yaw_radians += delta_yaw_radians;
    next_camera.pitch_radians = henka_clamp_pitch(next_camera.pitch_radians + delta_pitch_radians);
    if (!isfinite(next_camera.yaw_radians) || !isfinite(next_camera.pitch_radians))
    {
        return false;
    }

    forward = henka_camera_get_forward(&next_camera);
    next_position = henka_vec3_subtract(target, henka_vec3_scale(forward, distance));
    if (!henka_vec3_is_finite(next_position))
    {
        return false;
    }

    next_camera.position = next_position;
    if (!henka_camera_is_valid(&next_camera))
    {
        return false;
    }

    *camera = next_camera;
    return true;
}

bool henka_camera_pan_target(henka_camera* camera, henka_vec3* target, float delta_right, float delta_up)
{
    henka_vec3 next_position;
    henka_vec3 next_target;
    henka_vec3 offset;
    henka_vec3 right;
    henka_vec3 up;

    if (!henka_camera_is_valid(camera) ||
        target == NULL ||
        !henka_vec3_is_finite(*target) ||
        !isfinite(delta_right) ||
        !isfinite(delta_up))
    {
        return false;
    }

    right = henka_camera_get_right(camera);
    up = henka_camera_get_up(camera);
    offset = henka_vec3_add(
        henka_vec3_scale(right, delta_right),
        henka_vec3_scale(up, delta_up));
    next_position = henka_vec3_add(camera->position, offset);
    next_target = henka_vec3_add(*target, offset);
    if (!henka_vec3_is_finite(next_position) || !henka_vec3_is_finite(next_target))
    {
        return false;
    }

    camera->position = next_position;
    *target = next_target;
    return true;
}

bool henka_camera_dolly_target(henka_camera* camera, henka_vec3 target, float delta_distance, float minimum_distance)
{
    float distance;
    henka_vec3 forward;
    henka_vec3 next_position;

    if (!henka_camera_is_valid(camera) ||
        !henka_vec3_is_finite(target) ||
        !isfinite(delta_distance) ||
        !isfinite(minimum_distance))
    {
        return false;
    }

    if (minimum_distance <= 0.0f)
    {
        minimum_distance = 0.25f;
    }

    distance = henka_vec3_length(henka_vec3_subtract(target, camera->position));
    if (!isfinite(distance))
    {
        return false;
    }
    if (distance <= 0.0001f)
    {
        distance = minimum_distance;
    }

    distance += delta_distance;
    if (!isfinite(distance))
    {
        return false;
    }
    if (distance < minimum_distance)
    {
        distance = minimum_distance;
    }

    forward = henka_camera_get_forward(camera);
    next_position = henka_vec3_subtract(target, henka_vec3_scale(forward, distance));
    if (!henka_vec3_is_finite(next_position))
    {
        return false;
    }

    camera->position = next_position;
    return true;
}

henka_result henka_camera_screen_point_to_ray(
    const henka_camera* camera,
    int framebuffer_width,
    int framebuffer_height,
    henka_vec2 screen_position,
    henka_ray* out_ray)
{
    henka_vec3 right;
    henka_vec3 up;
    henka_vec3 forward;
    henka_vec3 direction;
    henka_vec3 origin;
    float ndc_x;
    float ndc_y;

    if (out_ray == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    out_ray->origin = (henka_vec3){0.0f, 0.0f, 0.0f};
    out_ray->direction = (henka_vec3){0.0f, 0.0f, -1.0f};

    if (!henka_camera_is_valid(camera) ||
        framebuffer_width <= 0 ||
        framebuffer_height <= 0 ||
        !henka_vec2_is_finite(screen_position))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    ndc_x = (2.0f * screen_position.x / (float)framebuffer_width) - 1.0f;
    ndc_y = 1.0f - (2.0f * screen_position.y / (float)framebuffer_height);
    if (!isfinite(ndc_x) || !isfinite(ndc_y))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    forward = henka_camera_get_forward(camera);
    right = henka_camera_get_right(camera);
    up = henka_camera_get_up(camera);
    if (!henka_vec3_is_finite(forward) ||
        !henka_vec3_is_finite(right) ||
        !henka_vec3_is_finite(up))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    origin = camera->position;
    if (camera->projection_mode == HENKA_CAMERA_PROJECTION_ORTHOGRAPHIC)
    {
        const float half_height = camera->orthographic_height * 0.5f;
        const float half_width = half_height * camera->aspect_ratio;
        origin = henka_vec3_add(
            camera->position,
            henka_vec3_add(
                henka_vec3_scale(right, ndc_x * half_width),
                henka_vec3_scale(up, ndc_y * half_height)));
        direction = forward;
    }
    else
    {
        const float tangent = tanf(camera->field_of_view_radians * 0.5f);
        if (!isfinite(tangent) || tangent <= 0.0f)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }

        direction = forward;
        direction = henka_vec3_add(
            direction,
            henka_vec3_scale(right, ndc_x * tangent * camera->aspect_ratio));
        direction = henka_vec3_add(
            direction,
            henka_vec3_scale(up, ndc_y * tangent));
        if (!henka_vec3_is_finite(direction) ||
            henka_vec3_length(direction) <= g_henka_camera_minimum_direction_length)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }

        direction = henka_vec3_normalize(direction);
    }

    if (!henka_vec3_is_finite(origin) ||
        !henka_vec3_is_finite(direction) ||
        henka_vec3_length(direction) <= g_henka_camera_minimum_direction_length)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    out_ray->origin = origin;
    out_ray->direction = direction;
    return HENKA_SUCCESS;
}

henka_result henka_camera_world_to_screen(
    const henka_camera* camera,
    int framebuffer_width,
    int framebuffer_height,
    henka_vec3 world_position,
    henka_vec2* out_screen_position,
    float* out_depth)
{
    henka_mat4 projection;
    henka_mat4 view;
    henka_mat4 view_projection;
    float clip[4];
    float ndc_x;
    float ndc_y;
    float ndc_z;
    float screen_x;
    float screen_y;
    float world[4];

    if (out_screen_position == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    out_screen_position->x = 0.0f;
    out_screen_position->y = 0.0f;
    if (out_depth != NULL)
    {
        *out_depth = 0.0f;
    }

    if (!henka_camera_is_valid(camera) ||
        framebuffer_width <= 0 ||
        framebuffer_height <= 0 ||
        !henka_vec3_is_finite(world_position))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    world[0] = world_position.x;
    world[1] = world_position.y;
    world[2] = world_position.z;
    world[3] = 1.0f;

    view = henka_camera_get_view_matrix(camera);
    projection = henka_camera_get_projection_matrix(camera);
    view_projection = henka_mat4_multiply(projection, view);
    henka_mat4_multiply_vec4(&view_projection, world, clip);
    if (!isfinite(clip[0]) ||
        !isfinite(clip[1]) ||
        !isfinite(clip[2]) ||
        !isfinite(clip[3]))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (clip[3] <= 0.00001f)
    {
        return HENKA_ERROR_UNKNOWN;
    }

    ndc_x = clip[0] / clip[3];
    ndc_y = clip[1] / clip[3];
    ndc_z = clip[2] / clip[3];
    if (!isfinite(ndc_x) || !isfinite(ndc_y) || !isfinite(ndc_z))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    screen_x = (ndc_x * 0.5f + 0.5f) * (float)framebuffer_width;
    screen_y = (1.0f - (ndc_y * 0.5f + 0.5f)) * (float)framebuffer_height;
    if (!isfinite(screen_x) || !isfinite(screen_y))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    out_screen_position->x = screen_x;
    out_screen_position->y = screen_y;
    if (out_depth != NULL)
    {
        *out_depth = ndc_z;
    }

    return HENKA_SUCCESS;
}

void henka_camera_apply_mouse_look(henka_camera* camera, float delta_yaw_radians, float delta_pitch_radians)
{
    float next_pitch;
    float next_yaw;

    if (!henka_camera_is_valid(camera) ||
        !isfinite(delta_yaw_radians) ||
        !isfinite(delta_pitch_radians))
    {
        return;
    }

    next_yaw = camera->yaw_radians + delta_yaw_radians;
    next_pitch = henka_clamp_pitch(camera->pitch_radians + delta_pitch_radians);
    if (!isfinite(next_yaw) || !isfinite(next_pitch))
    {
        return;
    }

    camera->yaw_radians = next_yaw;
    camera->pitch_radians = next_pitch;
}

void henka_camera_move_fly(henka_camera* camera, const struct henka_engine* engine, double delta_seconds)
{
#if defined(HENKA_RUNTIME_HEADLESS)
    (void)camera;
    (void)engine;
    (void)delta_seconds;
    return;
#else
    float speed;
    float distance;
    henka_vec3 move_direction;
    henka_vec3 horizontal_forward;
    henka_vec3 right;

    if (!henka_camera_is_valid(camera) ||
        engine == NULL ||
        !isfinite(delta_seconds) ||
        delta_seconds < 0.0)
    {
        return;
    }

    speed = camera->movement_speed;
    if (henka_input_is_key_down(engine, HENKA_KEY_LEFT_SHIFT))
    {
        speed *= camera->fast_movement_multiplier;
    }

    distance = speed * (float)delta_seconds;
    if (!isfinite(distance))
    {
        return;
    }
    move_direction.x = 0.0f;
    move_direction.y = 0.0f;
    move_direction.z = 0.0f;

    horizontal_forward = henka_camera_get_forward(camera);
    horizontal_forward.y = 0.0f;
    horizontal_forward = henka_vec3_normalize(horizontal_forward);
    right = henka_camera_get_right(camera);
    right.y = 0.0f;
    right = henka_vec3_normalize(right);

    if (henka_input_action_is_down(engine, HENKA_INPUT_ACTION_MOVE_FORWARD))
    {
        move_direction = henka_vec3_add(move_direction, horizontal_forward);
    }

    if (henka_input_action_is_down(engine, HENKA_INPUT_ACTION_MOVE_BACK))
    {
        move_direction = henka_vec3_subtract(move_direction, horizontal_forward);
    }

    if (henka_input_action_is_down(engine, HENKA_INPUT_ACTION_MOVE_RIGHT))
    {
        move_direction = henka_vec3_add(move_direction, right);
    }

    if (henka_input_action_is_down(engine, HENKA_INPUT_ACTION_MOVE_LEFT))
    {
        move_direction = henka_vec3_subtract(move_direction, right);
    }

    if (henka_input_action_is_down(engine, HENKA_INPUT_ACTION_MOVE_UP))
    {
        move_direction.y += 1.0f;
    }

    if (henka_input_action_is_down(engine, HENKA_INPUT_ACTION_MOVE_DOWN))
    {
        move_direction.y -= 1.0f;
    }

    if (henka_vec3_length(move_direction) > 0.0f)
    {
        henka_vec3 next_position;

        move_direction = henka_vec3_normalize(move_direction);
        next_position = henka_vec3_add(
            camera->position,
            henka_vec3_scale(move_direction, distance));
        if (henka_vec3_is_finite(next_position))
        {
            camera->position = next_position;
        }
    }
#endif
}
