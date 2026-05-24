#include <henka/math.h>

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

float henka_vec3_length(henka_vec3 value)
{
    return sqrtf((value.x * value.x) + (value.y * value.y) + (value.z * value.z));
}

henka_vec3 henka_vec3_normalize(henka_vec3 value)
{
    float length;

    length = henka_vec3_length(value);
    if (length <= 0.000001f)
    {
        return value;
    }

    return henka_vec3_scale(value, 1.0f / length);
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

henka_quat henka_quat_from_euler(float pitch_radians, float yaw_radians, float roll_radians)
{
    float cy;
    float cp;
    float cr;
    float sy;
    float sp;
    float sr;
    henka_quat result;

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

henka_quat henka_quat_normalize(henka_quat value)
{
    float magnitude;
    henka_quat result;

    magnitude = sqrtf((value.x * value.x) + (value.y * value.y) + (value.z * value.z) + (value.w * value.w));
    if (magnitude <= 0.000001f)
    {
        return henka_quat_identity();
    }

    result.x = value.x / magnitude;
    result.y = value.y / magnitude;
    result.z = value.z / magnitude;
    result.w = value.w / magnitude;
    return result;
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
