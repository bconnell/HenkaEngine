#ifndef HENKA_MATH_H
#define HENKA_MATH_H

typedef struct henka_vec2
{
    float x;
    float y;
} henka_vec2;

typedef struct henka_vec3
{
    float x;
    float y;
    float z;
} henka_vec3;

typedef struct henka_vec4
{
    float x;
    float y;
    float z;
    float w;
} henka_vec4;

typedef struct henka_quat
{
    float x;
    float y;
    float z;
    float w;
} henka_quat;

typedef struct henka_mat4
{
    /* Column-major storage for direct upload to OpenGL-style APIs. */
    float m[16];
} henka_mat4;

typedef struct henka_transform
{
    henka_vec3 position;
    henka_quat rotation;
    henka_vec3 scale;
} henka_transform;

henka_vec3 henka_vec3_add(henka_vec3 left, henka_vec3 right);
henka_vec3 henka_vec3_subtract(henka_vec3 left, henka_vec3 right);
henka_vec3 henka_vec3_scale(henka_vec3 value, float scalar);
float henka_vec3_length(henka_vec3 value);
henka_vec3 henka_vec3_normalize(henka_vec3 value);
float henka_vec3_dot(henka_vec3 left, henka_vec3 right);
henka_vec3 henka_vec3_cross(henka_vec3 left, henka_vec3 right);

henka_quat henka_quat_identity(void);
henka_quat henka_quat_from_axis_angle(henka_vec3 axis, float angle_radians);
henka_quat henka_quat_from_euler(float pitch_radians, float yaw_radians, float roll_radians);
henka_quat henka_quat_multiply(henka_quat left, henka_quat right);
henka_quat henka_quat_normalize(henka_quat value);
henka_vec3 henka_quat_rotate_vec3(henka_quat rotation, henka_vec3 value);

henka_mat4 henka_mat4_identity(void);
henka_mat4 henka_mat4_multiply(henka_mat4 left, henka_mat4 right);
henka_mat4 henka_mat4_translation(henka_vec3 translation);
henka_mat4 henka_mat4_rotation(henka_quat rotation);
henka_mat4 henka_mat4_scale(henka_vec3 scale);
henka_mat4 henka_mat4_perspective(float field_of_view_radians, float aspect_ratio, float near_plane, float far_plane);
henka_mat4 henka_mat4_orthographic(float left, float right, float bottom, float top, float near_plane, float far_plane);
henka_mat4 henka_mat4_look_at(henka_vec3 eye, henka_vec3 target, henka_vec3 up);
henka_mat4 henka_transform_to_mat4(henka_transform transform);

henka_transform henka_transform_identity(void);

#endif
