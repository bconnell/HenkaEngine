#include <henka/camera.h>

#include <math.h>

#include <henka/core.h>
#include <henka/engine.h>
#include <henka/input.h>

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

static float henka_max_float(float left, float right)
{
    return left > right ? left : right;
}

henka_camera henka_camera_create_perspective(float field_of_view_radians, float aspect_ratio, float near_plane, float far_plane)
{
    henka_camera camera;

    camera.position.x = 0.0f;
    camera.position.y = 1.5f;
    camera.position.z = 4.5f;
    camera.yaw_radians = -HENKA_PI * 0.5f;
    camera.pitch_radians = -0.25f;
    camera.projection_mode = HENKA_CAMERA_PROJECTION_PERSPECTIVE;
    camera.field_of_view_radians = field_of_view_radians;
    camera.orthographic_height = 6.0f;
    camera.near_plane = near_plane;
    camera.far_plane = far_plane;
    camera.aspect_ratio = aspect_ratio;
    camera.movement_speed = 4.0f;
    camera.fast_movement_multiplier = 2.5f;
    return camera;
}

henka_camera henka_camera_create_orthographic(float orthographic_height, float aspect_ratio, float near_plane, float far_plane)
{
    henka_camera camera;

    camera = henka_camera_create_perspective(60.0f * HENKA_DEG_TO_RAD, aspect_ratio, near_plane, far_plane);
    camera.projection_mode = HENKA_CAMERA_PROJECTION_ORTHOGRAPHIC;
    camera.orthographic_height = orthographic_height > 0.0f ? orthographic_height : 6.0f;
    return camera;
}

henka_vec3 henka_camera_get_forward(const henka_camera* camera)
{
    henka_vec3 forward;

    if (camera == NULL)
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
    henka_vec3 world_up;
    henka_vec3 right;

    world_up.x = 0.0f;
    world_up.y = 1.0f;
    world_up.z = 0.0f;

    right = henka_vec3_cross(henka_camera_get_forward(camera), world_up);
    return henka_vec3_normalize(right);
}

henka_mat4 henka_camera_get_view_matrix(const henka_camera* camera)
{
    henka_vec3 target;
    henka_vec3 up;

    target = henka_vec3_add(camera->position, henka_camera_get_forward(camera));
    up.x = 0.0f;
    up.y = 1.0f;
    up.z = 0.0f;
    return henka_mat4_look_at(camera->position, target, up);
}

henka_mat4 henka_camera_get_projection_matrix(const henka_camera* camera)
{
    if (camera == NULL)
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
    if (camera != NULL && aspect_ratio > 0.0f)
    {
        camera->aspect_ratio = aspect_ratio;
    }
}

float henka_camera_clamp_pitch(float pitch_radians)
{
    return henka_clamp_pitch(pitch_radians);
}

void henka_camera_reset(henka_camera* camera, const henka_camera* source)
{
    if (camera == NULL || source == NULL)
    {
        return;
    }

    *camera = *source;
}

bool henka_camera_look_at(henka_camera* camera, henka_vec3 target)
{
    henka_vec3 direction;
    float horizontal_length;

    if (camera == NULL)
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
    return true;
}

void henka_camera_move_relative(henka_camera* camera, henka_vec3 local_direction, float distance)
{
    henka_vec3 world_up;
    henka_vec3 move_direction;
    henka_vec3 right;
    henka_vec3 up;
    henka_vec3 forward;

    if (camera == NULL || distance == 0.0f)
    {
        return;
    }

    forward = henka_camera_get_forward(camera);
    right = henka_camera_get_right(camera);
    world_up = (henka_vec3){0.0f, 1.0f, 0.0f};
    up = henka_vec3_normalize(henka_vec3_cross(right, forward));
    if (henka_vec3_length(up) == 0.0f)
    {
        up = world_up;
    }

    move_direction = (henka_vec3){0.0f, 0.0f, 0.0f};
    move_direction = henka_vec3_add(move_direction, henka_vec3_scale(right, local_direction.x));
    move_direction = henka_vec3_add(move_direction, henka_vec3_scale(up, local_direction.y));
    move_direction = henka_vec3_add(move_direction, henka_vec3_scale(forward, local_direction.z));
    if (henka_vec3_length(move_direction) <= 0.0f)
    {
        return;
    }

    move_direction = henka_vec3_normalize(move_direction);
    camera->position = henka_vec3_add(camera->position, henka_vec3_scale(move_direction, distance));
}

bool henka_camera_focus_on_bounds(henka_camera* camera, henka_bounds bounds)
{
    henka_vec3 forward;
    float radius;
    float distance;
    float vertical_distance;
    float horizontal_distance;
    float horizontal_fov;

    if (camera == NULL)
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
        camera->position = henka_vec3_subtract(bounds.center, henka_vec3_scale(forward, radius + camera->near_plane + 1.0f));
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

    camera->position = henka_vec3_subtract(bounds.center, henka_vec3_scale(forward, distance));
    return true;
}

bool henka_camera_frame_bounds(henka_camera* camera, henka_bounds bounds, float yaw_radians, float pitch_radians)
{
    henka_vec3 forward;
    float radius;
    float distance;
    float horizontal_distance;
    float horizontal_fov;
    float vertical_distance;

    if (camera == NULL)
    {
        return false;
    }

    radius = henka_vec3_length(bounds.extents);
    if (radius <= 0.0f)
    {
        radius = 0.5f;
    }

    camera->yaw_radians = yaw_radians;
    camera->pitch_radians = henka_clamp_pitch(pitch_radians);
    forward = henka_camera_get_forward(camera);
    if (camera->projection_mode == HENKA_CAMERA_PROJECTION_ORTHOGRAPHIC)
    {
        camera->position = henka_vec3_subtract(bounds.center, henka_vec3_scale(forward, radius + camera->near_plane + 2.0f));
        return true;
    }

    vertical_distance = radius / tanf(camera->field_of_view_radians * 0.5f);
    horizontal_fov = 2.0f * atanf(tanf(camera->field_of_view_radians * 0.5f) * camera->aspect_ratio);
    horizontal_distance = radius / tanf(horizontal_fov * 0.5f);
    distance = henka_max_float(vertical_distance, horizontal_distance) + radius * 0.9f;
    distance = henka_max_float(distance, camera->near_plane + radius + 0.75f);
    camera->position = henka_vec3_subtract(bounds.center, henka_vec3_scale(forward, distance));
    return true;
}

bool henka_camera_orbit_target(henka_camera* camera, henka_vec3 target, float delta_yaw_radians, float delta_pitch_radians)
{
    float distance;
    henka_vec3 forward;

    if (camera == NULL)
    {
        return false;
    }

    distance = henka_vec3_length(henka_vec3_subtract(target, camera->position));
    if (distance <= 0.0001f)
    {
        distance = 1.0f;
    }

    camera->yaw_radians += delta_yaw_radians;
    camera->pitch_radians = henka_clamp_pitch(camera->pitch_radians + delta_pitch_radians);
    forward = henka_camera_get_forward(camera);
    camera->position = henka_vec3_subtract(target, henka_vec3_scale(forward, distance));
    return true;
}

bool henka_camera_pan_target(henka_camera* camera, henka_vec3* target, float delta_right, float delta_up)
{
    henka_vec3 offset;
    henka_vec3 right;
    henka_vec3 up;

    if (camera == NULL || target == NULL)
    {
        return false;
    }

    right = henka_camera_get_right(camera);
    up = henka_vec3_normalize(henka_vec3_cross(right, henka_camera_get_forward(camera)));
    offset = henka_vec3_add(henka_vec3_scale(right, delta_right), henka_vec3_scale(up, delta_up));
    camera->position = henka_vec3_add(camera->position, offset);
    *target = henka_vec3_add(*target, offset);
    return true;
}

bool henka_camera_dolly_target(henka_camera* camera, henka_vec3 target, float delta_distance, float minimum_distance)
{
    float distance;
    henka_vec3 forward;

    if (camera == NULL)
    {
        return false;
    }

    if (minimum_distance <= 0.0f)
    {
        minimum_distance = 0.25f;
    }

    distance = henka_vec3_length(henka_vec3_subtract(target, camera->position));
    if (distance <= 0.0001f)
    {
        distance = minimum_distance;
    }

    distance += delta_distance;
    if (distance < minimum_distance)
    {
        distance = minimum_distance;
    }

    forward = henka_camera_get_forward(camera);
    camera->position = henka_vec3_subtract(target, henka_vec3_scale(forward, distance));
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
    float ndc_x;
    float ndc_y;

    if (camera == NULL || out_ray == NULL || framebuffer_width <= 0 || framebuffer_height <= 0)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    ndc_x = (2.0f * screen_position.x / (float)framebuffer_width) - 1.0f;
    ndc_y = 1.0f - (2.0f * screen_position.y / (float)framebuffer_height);
    forward = henka_camera_get_forward(camera);
    right = henka_camera_get_right(camera);
    up = henka_vec3_normalize(henka_vec3_cross(right, forward));

    out_ray->origin = camera->position;
    if (camera->projection_mode == HENKA_CAMERA_PROJECTION_ORTHOGRAPHIC)
    {
        const float half_height = camera->orthographic_height * 0.5f;
        const float half_width = half_height * camera->aspect_ratio;
        out_ray->origin = henka_vec3_add(
            camera->position,
            henka_vec3_add(
                henka_vec3_scale(right, ndc_x * half_width),
                henka_vec3_scale(up, ndc_y * half_height)));
        out_ray->direction = forward;
        return HENKA_SUCCESS;
    }

    direction = forward;
    direction = henka_vec3_add(
        direction,
        henka_vec3_scale(right, ndc_x * tanf(camera->field_of_view_radians * 0.5f) * camera->aspect_ratio));
    direction = henka_vec3_add(
        direction,
        henka_vec3_scale(up, ndc_y * tanf(camera->field_of_view_radians * 0.5f)));
    out_ray->direction = henka_vec3_normalize(direction);
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
    float world[4];

    if (camera == NULL ||
        out_screen_position == NULL ||
        framebuffer_width <= 0 ||
        framebuffer_height <= 0)
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
    if (fabsf(clip[3]) <= 0.00001f || clip[3] < 0.0f)
    {
        return HENKA_ERROR_UNKNOWN;
    }

    ndc_x = clip[0] / clip[3];
    ndc_y = clip[1] / clip[3];
    ndc_z = clip[2] / clip[3];

    out_screen_position->x = (ndc_x * 0.5f + 0.5f) * (float)framebuffer_width;
    out_screen_position->y = (1.0f - (ndc_y * 0.5f + 0.5f)) * (float)framebuffer_height;
    if (out_depth != NULL)
    {
        *out_depth = ndc_z;
    }

    return HENKA_SUCCESS;
}

void henka_camera_apply_mouse_look(henka_camera* camera, float delta_yaw_radians, float delta_pitch_radians)
{
    if (camera == NULL)
    {
        return;
    }

    camera->yaw_radians += delta_yaw_radians;
    camera->pitch_radians = henka_clamp_pitch(camera->pitch_radians + delta_pitch_radians);
}

void henka_camera_move_fly(henka_camera* camera, const struct henka_engine* engine, double delta_seconds)
{
    float speed;
    float distance;
    henka_vec3 move_direction;
    henka_vec3 horizontal_forward;
    henka_vec3 right;

    if (camera == NULL || engine == NULL)
    {
        return;
    }

    speed = camera->movement_speed;
    if (henka_input_is_key_down(engine, HENKA_KEY_LEFT_SHIFT))
    {
        speed *= camera->fast_movement_multiplier;
    }

    distance = speed * (float)delta_seconds;
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
        move_direction = henka_vec3_normalize(move_direction);
        camera->position = henka_vec3_add(camera->position, henka_vec3_scale(move_direction, distance));
    }
}
