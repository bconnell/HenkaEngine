#ifndef HENKA_CAMERA_H
#define HENKA_CAMERA_H

#include <stdbool.h>

#include <henka/math.h>
#include <henka/result.h>

struct henka_engine;

typedef enum henka_camera_projection_mode
{
    HENKA_CAMERA_PROJECTION_PERSPECTIVE = 0,
    HENKA_CAMERA_PROJECTION_ORTHOGRAPHIC
} henka_camera_projection_mode;

typedef struct henka_ray
{
    henka_vec3 origin;
    henka_vec3 direction;
} henka_ray;

typedef struct henka_bounds
{
    henka_vec3 center;
    henka_vec3 extents;
} henka_bounds;

typedef struct henka_camera
{
    henka_vec3 position;
    float yaw_radians;
    float pitch_radians;
    henka_camera_projection_mode projection_mode;
    float field_of_view_radians;
    float orthographic_height;
    float near_plane;
    float far_plane;
    float aspect_ratio;
    float movement_speed;
    float fast_movement_multiplier;
} henka_camera;

henka_camera henka_camera_create_perspective(float field_of_view_radians, float aspect_ratio, float near_plane, float far_plane);
henka_camera henka_camera_create_orthographic(float orthographic_height, float aspect_ratio, float near_plane, float far_plane);
henka_mat4 henka_camera_get_view_matrix(const henka_camera* camera);
henka_mat4 henka_camera_get_projection_matrix(const henka_camera* camera);
henka_vec3 henka_camera_get_forward(const henka_camera* camera);
henka_vec3 henka_camera_get_right(const henka_camera* camera);
void henka_camera_set_aspect_ratio(henka_camera* camera, float aspect_ratio);
float henka_camera_clamp_pitch(float pitch_radians);
void henka_camera_reset(henka_camera* camera, const henka_camera* source);
void henka_camera_move_relative(henka_camera* camera, henka_vec3 local_direction, float distance);
bool henka_camera_focus_on_bounds(henka_camera* camera, henka_bounds bounds);
henka_result henka_camera_screen_point_to_ray(
    const henka_camera* camera,
    int framebuffer_width,
    int framebuffer_height,
    henka_vec2 screen_position,
    henka_ray* out_ray);
henka_result henka_camera_world_to_screen(
    const henka_camera* camera,
    int framebuffer_width,
    int framebuffer_height,
    henka_vec3 world_position,
    henka_vec2* out_screen_position,
    float* out_depth);
void henka_camera_move_fly(henka_camera* camera, const struct henka_engine* engine, double delta_seconds);
void henka_camera_apply_mouse_look(henka_camera* camera, float delta_yaw_radians, float delta_pitch_radians);

#endif
