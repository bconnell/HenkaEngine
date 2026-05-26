#ifndef HENKA_GIZMO_H
#define HENKA_GIZMO_H

#include <henka/camera.h>
#include <henka/core.h>
#include <henka/result.h>
#include <henka/scene.h>
#include <stddef.h>

typedef enum henka_gizmo_axis
{
    HENKA_GIZMO_AXIS_NONE = 0,
    HENKA_GIZMO_AXIS_X,
    HENKA_GIZMO_AXIS_Y,
    HENKA_GIZMO_AXIS_Z,
    HENKA_GIZMO_AXIS_UNIFORM
} henka_gizmo_axis;

typedef enum henka_gizmo_mode
{
    HENKA_GIZMO_MODE_SELECT = 0,
    HENKA_GIZMO_MODE_MOVE,
    HENKA_GIZMO_MODE_ROTATE,
    HENKA_GIZMO_MODE_SCALE
} henka_gizmo_mode;

typedef enum henka_gizmo_handle_type
{
    HENKA_GIZMO_HANDLE_NONE = 0,
    HENKA_GIZMO_HANDLE_MOVE_AXIS,
    HENKA_GIZMO_HANDLE_MOVE_BOX,
    HENKA_GIZMO_HANDLE_ROTATE_RING,
    HENKA_GIZMO_HANDLE_SCALE_UNIFORM
} henka_gizmo_handle_type;

#define HENKA_GIZMO_RING_SAMPLES 49
#define HENKA_GIZMO_MAX_HANDLES 10

typedef struct henka_gizmo_snap_settings
{
    bool enabled;
    float move_snap_increment;
    float rotate_snap_increment;
    float scale_snap_increment;
    float minimum_scale;
} henka_gizmo_snap_settings;

typedef struct henka_gizmo_handle_model
{
    bool visible;
    henka_gizmo_mode mode;
    henka_gizmo_axis axis;
    henka_gizmo_handle_type type;
    henka_vec3 world_start;
    henka_vec3 world_end;
    henka_vec3 world_center;
    henka_vec2 screen_start;
    henka_vec2 screen_end;
    henka_vec2 screen_center;
    henka_vec2 screen_half_extents;
    float hit_tolerance;
    size_t point_count;
    henka_vec2 points[HENKA_GIZMO_RING_SAMPLES];
} henka_gizmo_handle_model;

typedef struct henka_gizmo_model
{
    bool valid;
    henka_entity target_entity;
    henka_gizmo_mode mode;
    henka_transform target_transform;
    henka_viewport viewport;
    henka_vec2 mouse_framebuffer;
    henka_vec2 mouse_local;
    henka_vec2 screen_center;
    float gizmo_size;
    float dead_zone_radius;
    size_t handle_count;
    henka_gizmo_handle_model handles[HENKA_GIZMO_MAX_HANDLES];
} henka_gizmo_model;

typedef struct henka_gizmo_handle_hit
{
    bool hit;
    bool in_dead_zone;
    henka_gizmo_axis axis;
    henka_gizmo_handle_type type;
    float distance;
} henka_gizmo_handle_hit;

typedef struct henka_gizmo_drag_state
{
    bool dragging;
    henka_entity target_entity;
    henka_vec2 drag_start_mouse_framebuffer;
    henka_vec2 drag_start_mouse_local;
    henka_viewport drag_start_viewport;
    henka_transform drag_start_transform;
    henka_vec2 drag_axis_screen_direction;
    henka_vec2 drag_center_screen;
    float drag_axis_screen_length;
    float gizmo_size;
    float drag_start_angle;
    float drag_start_projection;
    henka_gizmo_axis active_axis;
    henka_gizmo_mode active_mode;
} henka_gizmo_drag_state;

henka_result henka_gizmo_get_axis_direction(henka_gizmo_axis axis, henka_vec3* out_direction);
const char* henka_gizmo_mode_to_string(henka_gizmo_mode mode);
const char* henka_gizmo_axis_to_string(henka_gizmo_axis axis);
henka_result henka_gizmo_snap_move(float value, float increment, float* out_value);
henka_result henka_gizmo_snap_rotate(float angle_radians, float increment, float* out_angle_radians);
henka_result henka_gizmo_snap_scale(float value, float increment, float minimum_scale, float* out_value);
henka_result henka_gizmo_intersect_ray_plane(
    henka_ray ray,
    henka_vec3 plane_origin,
    henka_vec3 plane_normal,
    henka_vec3* out_point,
    float* out_distance);
henka_result henka_gizmo_hit_test_axis(
    henka_vec3 origin,
    henka_ray ray,
    float axis_length,
    float axis_radius,
    henka_gizmo_axis* out_axis,
    float* out_distance);
henka_result henka_gizmo_hit_test_rotation_rings(
    henka_vec3 origin,
    henka_ray ray,
    float ring_radius,
    float ring_thickness,
    henka_gizmo_axis* out_axis,
    float* out_distance);
henka_result henka_gizmo_hit_test_uniform_handle(
    henka_vec3 origin,
    henka_ray ray,
    float radius,
    float* out_distance);
henka_result henka_gizmo_project_axis_delta(
    henka_vec3 origin,
    henka_vec3 axis_direction,
    henka_vec3 camera_forward,
    henka_ray start_ray,
    henka_ray current_ray,
    float* out_delta);
henka_result henka_gizmo_project_rotation_delta(
    henka_vec3 origin,
    henka_vec3 axis_direction,
    henka_ray start_ray,
    henka_ray current_ray,
    float* out_angle_radians);
henka_result henka_gizmo_hit_test_segment_2d(
    henka_vec2 point,
    henka_vec2 start,
    henka_vec2 end,
    float tolerance,
    float* out_distance);
bool henka_gizmo_hit_test_rect_2d(
    henka_vec2 point,
    henka_vec2 center,
    henka_vec2 half_extents,
    float padding);
henka_result henka_gizmo_hit_test_polyline_2d(
    henka_vec2 point,
    const henka_vec2* points,
    size_t point_count,
    float tolerance,
    float* out_distance);
henka_result henka_gizmo_build_model(
    const henka_camera* camera,
    henka_viewport viewport,
    henka_entity target_entity,
    henka_transform target_transform,
    henka_gizmo_mode mode,
    henka_vec2 mouse_framebuffer,
    float gizmo_size,
    henka_gizmo_model* out_model);
henka_result henka_gizmo_hit_test_model(
    const henka_gizmo_model* model,
    henka_gizmo_handle_hit* out_hit);
henka_result henka_gizmo_begin_drag(
    const henka_gizmo_model* model,
    const henka_gizmo_handle_hit* hit,
    henka_gizmo_drag_state* out_drag);
henka_result henka_gizmo_apply_drag_to_transform(
    const henka_gizmo_drag_state* drag,
    henka_vec2 current_mouse_local,
    const henka_gizmo_snap_settings* snap_settings,
    henka_transform* out_transform);

#endif
