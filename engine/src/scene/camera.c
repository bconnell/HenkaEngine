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

henka_camera henka_camera_create_perspective(float field_of_view_radians, float aspect_ratio, float near_plane, float far_plane)
{
    henka_camera camera;

    camera.position.x = 0.0f;
    camera.position.y = 1.5f;
    camera.position.z = 4.5f;
    camera.yaw_radians = -HENKA_PI * 0.5f;
    camera.pitch_radians = -0.25f;
    camera.field_of_view_radians = field_of_view_radians;
    camera.near_plane = near_plane;
    camera.far_plane = far_plane;
    camera.aspect_ratio = aspect_ratio;
    camera.movement_speed = 4.0f;
    camera.fast_movement_multiplier = 2.5f;
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
    return henka_mat4_perspective(camera->field_of_view_radians, camera->aspect_ratio, camera->near_plane, camera->far_plane);
}

void henka_camera_set_aspect_ratio(henka_camera* camera, float aspect_ratio)
{
    if (camera != NULL && aspect_ratio > 0.0f)
    {
        camera->aspect_ratio = aspect_ratio;
    }
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

    if (henka_input_is_key_down(engine, HENKA_KEY_W))
    {
        move_direction = henka_vec3_add(move_direction, horizontal_forward);
    }

    if (henka_input_is_key_down(engine, HENKA_KEY_S))
    {
        move_direction = henka_vec3_subtract(move_direction, horizontal_forward);
    }

    if (henka_input_is_key_down(engine, HENKA_KEY_D))
    {
        move_direction = henka_vec3_add(move_direction, right);
    }

    if (henka_input_is_key_down(engine, HENKA_KEY_A))
    {
        move_direction = henka_vec3_subtract(move_direction, right);
    }

    if (henka_input_is_key_down(engine, HENKA_KEY_E))
    {
        move_direction.y += 1.0f;
    }

    if (henka_input_is_key_down(engine, HENKA_KEY_Q))
    {
        move_direction.y -= 1.0f;
    }

    if (henka_vec3_length(move_direction) > 0.0f)
    {
        move_direction = henka_vec3_normalize(move_direction);
        camera->position = henka_vec3_add(camera->position, henka_vec3_scale(move_direction, distance));
    }
}
