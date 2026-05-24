#ifndef HENKA_CAMERA_H
#define HENKA_CAMERA_H

#include <henka/math.h>

struct henka_engine;

typedef struct henka_camera
{
    henka_vec3 position;
    float yaw_radians;
    float pitch_radians;
    float field_of_view_radians;
    float near_plane;
    float far_plane;
    float aspect_ratio;
    float movement_speed;
    float fast_movement_multiplier;
} henka_camera;

henka_camera henka_camera_create_perspective(float field_of_view_radians, float aspect_ratio, float near_plane, float far_plane);
henka_mat4 henka_camera_get_view_matrix(const henka_camera* camera);
henka_mat4 henka_camera_get_projection_matrix(const henka_camera* camera);
henka_vec3 henka_camera_get_forward(const henka_camera* camera);
henka_vec3 henka_camera_get_right(const henka_camera* camera);
void henka_camera_set_aspect_ratio(henka_camera* camera, float aspect_ratio);
void henka_camera_move_fly(henka_camera* camera, const struct henka_engine* engine, double delta_seconds);
void henka_camera_apply_mouse_look(henka_camera* camera, float delta_yaw_radians, float delta_pitch_radians);

#endif
