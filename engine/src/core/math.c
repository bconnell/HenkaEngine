#include <henka/math.h>

#include <float.h>
#include <math.h>

henka_vec3 henka_vec3_add(henka_vec3 left, henka_vec3 right)
{
    henka_vec3 result;

    result.x = left.x + right.x;
    result.y = left.y + right.y;
    result.z = left.z + right.z;
    return result;
}

henka_vec3 henka_vec3_subtract(henka_vec3 left, henka_vec3 right)
{
    henka_vec3 result;

    result.x = left.x - right.x;
    result.y = left.y - right.y;
    result.z = left.z - right.z;
    return result;
}

henka_vec3 henka_vec3_scale(henka_vec3 value, float scalar)
{
    henka_vec3 result;

    result.x = value.x * scalar;
    result.y = value.y * scalar;
    result.z = value.z * scalar;
    return result;
}

static float henka_max_abs3(float x, float y, float z)
{
    float maximum;

    maximum = fmaxf(fabsf(x), fabsf(y));
    return fmaxf(maximum, fabsf(z));
}

static henka_quat henka_quat_normalize_components(double x, double y, double z, double w)
{
    double magnitude;
    double maximum;
    double scaled_x;
    double scaled_y;
    double scaled_z;
    double scaled_w;

    maximum = fmax(fmax(fabs(x), fabs(y)), fmax(fabs(z), fabs(w)));
    if (!isfinite(maximum) || maximum <= 0.0)
    {
        return henka_quat_identity();
    }

    scaled_x = x / maximum;
    scaled_y = y / maximum;
    scaled_z = z / maximum;
    scaled_w = w / maximum;
    magnitude = maximum * sqrt(
        scaled_x * scaled_x +
        scaled_y * scaled_y +
        scaled_z * scaled_z +
        scaled_w * scaled_w);
    if (!isfinite(magnitude) || magnitude <= 0.000001)
    {
        return henka_quat_identity();
    }

    return (henka_quat){
        (float)(x / magnitude),
        (float)(y / magnitude),
        (float)(z / magnitude),
        (float)(w / magnitude)};
}

float henka_vec3_length(henka_vec3 value)
{
    double length;
    double scaled_x;
    double scaled_y;
    double scaled_z;
    float maximum;

    if (!isfinite(value.x) || !isfinite(value.y) || !isfinite(value.z))
    {
        return NAN;
    }

    maximum = henka_max_abs3(value.x, value.y, value.z);
    if (maximum <= 0.0f)
    {
        return 0.0f;
    }

    scaled_x = (double)value.x / (double)maximum;
    scaled_y = (double)value.y / (double)maximum;
    scaled_z = (double)value.z / (double)maximum;
    length = (double)maximum * sqrt(
        scaled_x * scaled_x +
        scaled_y * scaled_y +
        scaled_z * scaled_z);
    if (!isfinite(length) || length > (double)FLT_MAX)
    {
        return INFINITY;
    }

    return (float)length;
}

henka_vec3 henka_vec3_normalize(henka_vec3 value)
{
    double scaled_length;
    double scaled_x;
    double scaled_y;
    double scaled_z;
    float maximum;

    if (!isfinite(value.x) || !isfinite(value.y) || !isfinite(value.z))
    {
        return (henka_vec3){0.0f, 0.0f, 0.0f};
    }

    maximum = henka_max_abs3(value.x, value.y, value.z);
    if (maximum <= 0.000001f)
    {
        return value;
    }

    scaled_x = (double)value.x / (double)maximum;
    scaled_y = (double)value.y / (double)maximum;
    scaled_z = (double)value.z / (double)maximum;
    scaled_length = sqrt(
        scaled_x * scaled_x +
        scaled_y * scaled_y +
        scaled_z * scaled_z);
    if (!isfinite(scaled_length) || scaled_length <= 0.0)
    {
        return (henka_vec3){0.0f, 0.0f, 0.0f};
    }

    return (henka_vec3){
        (float)(scaled_x / scaled_length),
        (float)(scaled_y / scaled_length),
        (float)(scaled_z / scaled_length)};
}

float henka_vec3_dot(henka_vec3 left, henka_vec3 right)
{
    return (left.x * right.x) + (left.y * right.y) + (left.z * right.z);
}

henka_vec3 henka_vec3_cross(henka_vec3 left, henka_vec3 right)
{
    henka_vec3 result;

    result.x = (left.y * right.z) - (left.z * right.y);
    result.y = (left.z * right.x) - (left.x * right.z);
    result.z = (left.x * right.y) - (left.y * right.x);
    return result;
}

henka_quat henka_quat_identity(void)
{
    henka_quat result;

    result.x = 0.0f;
    result.y = 0.0f;
    result.z = 0.0f;
    result.w = 1.0f;
    return result;
}

henka_quat henka_quat_from_axis_angle(henka_vec3 axis, float angle_radians)
{
    henka_quat result;
    float half_angle;
    float sine;

    if (!isfinite(angle_radians))
    {
        return henka_quat_identity();
    }

    axis = henka_vec3_normalize(axis);
    if (henka_vec3_length(axis) <= 0.000001f)
    {
        return henka_quat_identity();
    }

    half_angle = angle_radians * 0.5f;
    sine = sinf(half_angle);
    result.x = axis.x * sine;
    result.y = axis.y * sine;
    result.z = axis.z * sine;
    result.w = cosf(half_angle);
    return henka_quat_normalize(result);
}

henka_quat henka_quat_from_euler(float pitch_radians, float yaw_radians, float roll_radians)
{
    float cy;
    float cp;
    float cr;
    float sy;
    float sp;
    float sr;
    henka_quat result;

    if (!isfinite(pitch_radians) || !isfinite(yaw_radians) || !isfinite(roll_radians))
    {
        return henka_quat_identity();
    }

    cy = cosf(yaw_radians * 0.5f);
    sy = sinf(yaw_radians * 0.5f);
    cp = cosf(pitch_radians * 0.5f);
    sp = sinf(pitch_radians * 0.5f);
    cr = cosf(roll_radians * 0.5f);
    sr = sinf(roll_radians * 0.5f);

    result.w = (cr * cp * cy) + (sr * sp * sy);
    result.x = (sr * cp * cy) - (cr * sp * sy);
    result.y = (cr * sp * cy) + (sr * cp * sy);
    result.z = (cr * cp * sy) - (sr * sp * cy);
    return henka_quat_normalize(result);
}

henka_quat henka_quat_multiply(henka_quat left, henka_quat right)
{
    double x;
    double y;
    double z;
    double w;

    x = ((double)left.w * (double)right.x) +
        ((double)left.x * (double)right.w) +
        ((double)left.y * (double)right.z) -
        ((double)left.z * (double)right.y);
    y = ((double)left.w * (double)right.y) -
        ((double)left.x * (double)right.z) +
        ((double)left.y * (double)right.w) +
        ((double)left.z * (double)right.x);
    z = ((double)left.w * (double)right.z) +
        ((double)left.x * (double)right.y) -
        ((double)left.y * (double)right.x) +
        ((double)left.z * (double)right.w);
    w = ((double)left.w * (double)right.w) -
        ((double)left.x * (double)right.x) -
        ((double)left.y * (double)right.y) -
        ((double)left.z * (double)right.z);
    return henka_quat_normalize_components(x, y, z, w);
}

henka_quat henka_quat_normalize(henka_quat value)
{
    return henka_quat_normalize_components(
        (double)value.x,
        (double)value.y,
        (double)value.z,
        (double)value.w);
}

henka_vec3 henka_quat_rotate_vec3(henka_quat rotation, henka_vec3 value)
{
    henka_vec3 qv;
    henka_vec3 uv;
    henka_vec3 uuv;

    rotation = henka_quat_normalize(rotation);
    qv = (henka_vec3){rotation.x, rotation.y, rotation.z};
    uv = henka_vec3_cross(qv, value);
    uuv = henka_vec3_cross(qv, uv);
    uv = henka_vec3_scale(uv, 2.0f * rotation.w);
    uuv = henka_vec3_scale(uuv, 2.0f);
    return henka_vec3_add(value, henka_vec3_add(uv, uuv));
}

henka_mat4 henka_mat4_identity(void)
{
    henka_mat4 result;
    int index;

    for (index = 0; index < 16; ++index)
    {
        result.m[index] = 0.0f;
    }

    result.m[0] = 1.0f;
    result.m[5] = 1.0f;
    result.m[10] = 1.0f;
    result.m[15] = 1.0f;
    return result;
}

henka_mat4 henka_mat4_multiply(henka_mat4 left, henka_mat4 right)
{
    henka_mat4 result;
    int column;
    int row;
    int inner;

    for (column = 0; column < 4; ++column)
    {
        for (row = 0; row < 4; ++row)
        {
            float sum;

            sum = 0.0f;
            for (inner = 0; inner < 4; ++inner)
            {
                sum += left.m[(inner * 4) + row] * right.m[(column * 4) + inner];
            }

            result.m[(column * 4) + row] = sum;
        }
    }

    return result;
}

henka_mat4 henka_mat4_translation(henka_vec3 translation)
{
    henka_mat4 result;

    result = henka_mat4_identity();
    result.m[12] = translation.x;
    result.m[13] = translation.y;
    result.m[14] = translation.z;
    return result;
}

henka_mat4 henka_mat4_rotation(henka_quat rotation)
{
    float xx;
    float xy;
    float xz;
    float xw;
    float yy;
    float yz;
    float yw;
    float zz;
    float zw;
    henka_mat4 result;

    rotation = henka_quat_normalize(rotation);

    xx = rotation.x * rotation.x;
    xy = rotation.x * rotation.y;
    xz = rotation.x * rotation.z;
    xw = rotation.x * rotation.w;
    yy = rotation.y * rotation.y;
    yz = rotation.y * rotation.z;
    yw = rotation.y * rotation.w;
    zz = rotation.z * rotation.z;
    zw = rotation.z * rotation.w;

    result = henka_mat4_identity();
    result.m[0] = 1.0f - (2.0f * (yy + zz));
    result.m[1] = 2.0f * (xy + zw);
    result.m[2] = 2.0f * (xz - yw);
    result.m[4] = 2.0f * (xy - zw);
    result.m[5] = 1.0f - (2.0f * (xx + zz));
    result.m[6] = 2.0f * (yz + xw);
    result.m[8] = 2.0f * (xz + yw);
    result.m[9] = 2.0f * (yz - xw);
    result.m[10] = 1.0f - (2.0f * (xx + yy));
    return result;
}

henka_mat4 henka_mat4_scale(henka_vec3 scale)
{
    henka_mat4 result;

    result = henka_mat4_identity();
    result.m[0] = scale.x;
    result.m[5] = scale.y;
    result.m[10] = scale.z;
    return result;
}

henka_mat4 henka_mat4_perspective(float field_of_view_radians, float aspect_ratio, float near_plane, float far_plane)
{
    float focal_length;
    henka_mat4 result;

    result = henka_mat4_identity();
    focal_length = 1.0f / tanf(field_of_view_radians * 0.5f);

    result.m[0] = focal_length / aspect_ratio;
    result.m[5] = focal_length;
    result.m[10] = (far_plane + near_plane) / (near_plane - far_plane);
    result.m[11] = -1.0f;
    result.m[14] = (2.0f * far_plane * near_plane) / (near_plane - far_plane);
    result.m[15] = 0.0f;
    return result;
}

henka_mat4 henka_mat4_orthographic(float left, float right, float bottom, float top, float near_plane, float far_plane)
{
    henka_mat4 result;

    result = henka_mat4_identity();
    result.m[0] = 2.0f / (right - left);
    result.m[5] = 2.0f / (top - bottom);
    result.m[10] = -2.0f / (far_plane - near_plane);
    result.m[12] = -((right + left) / (right - left));
    result.m[13] = -((top + bottom) / (top - bottom));
    result.m[14] = -((far_plane + near_plane) / (far_plane - near_plane));
    return result;
}

henka_mat4 henka_mat4_look_at(henka_vec3 eye, henka_vec3 target, henka_vec3 up)
{
    henka_vec3 forward;
    henka_vec3 right;
    henka_vec3 recalculated_up;
    henka_mat4 result;

    forward = henka_vec3_normalize(henka_vec3_subtract(target, eye));
    right = henka_vec3_normalize(henka_vec3_cross(forward, up));
    recalculated_up = henka_vec3_cross(right, forward);

    result = henka_mat4_identity();
    result.m[0] = right.x;
    result.m[1] = recalculated_up.x;
    result.m[2] = -forward.x;
    result.m[4] = right.y;
    result.m[5] = recalculated_up.y;
    result.m[6] = -forward.y;
    result.m[8] = right.z;
    result.m[9] = recalculated_up.z;
    result.m[10] = -forward.z;
    result.m[12] = -henka_vec3_dot(right, eye);
    result.m[13] = -henka_vec3_dot(recalculated_up, eye);
    result.m[14] = henka_vec3_dot(forward, eye);
    return result;
}

henka_mat4 henka_transform_to_mat4(henka_transform transform)
{
    henka_mat4 translation;
    henka_mat4 rotation;
    henka_mat4 scale;

    translation = henka_mat4_translation(transform.position);
    rotation = henka_mat4_rotation(transform.rotation);
    scale = henka_mat4_scale(transform.scale);
    return henka_mat4_multiply(translation, henka_mat4_multiply(rotation, scale));
}

henka_transform henka_transform_identity(void)
{
    henka_transform result;

    result.position.x = 0.0f;
    result.position.y = 0.0f;
    result.position.z = 0.0f;
    result.rotation = henka_quat_identity();
    result.scale.x = 1.0f;
    result.scale.y = 1.0f;
    result.scale.z = 1.0f;
    return result;
}
