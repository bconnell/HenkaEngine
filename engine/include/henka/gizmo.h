#ifndef HENKA_GIZMO_H
#define HENKA_GIZMO_H

#include <henka/camera.h>
#include <henka/result.h>
#include <stddef.h>

typedef enum henka_gizmo_axis
{
    HENKA_GIZMO_AXIS_NONE = 0,
    HENKA_GIZMO_AXIS_X,
    HENKA_GIZMO_AXIS_Y,
    HENKA_GIZMO_AXIS_Z,
    HENKA_GIZMO_AXIS_UNIFORM
} henka_gizmo_axis;

henka_result henka_gizmo_get_axis_direction(henka_gizmo_axis axis, henka_vec3* out_direction);
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

#endif
