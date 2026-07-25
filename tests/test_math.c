#include "test_suite.h"

#include <float.h>

#include <henka/core.h>
#include <henka/math.h>

static int henka_test_math_float_is_finite(float value)
{
    return value == value && value >= -FLT_MAX && value <= FLT_MAX;
}

void henka_test_math(void)
{
    henka_vec3 added;
    henka_vec3 normalized;
    henka_vec3 rotated;
    henka_vec3 crossed;
    henka_mat4 identity;
    henka_mat4 translation;
    henka_mat4 scale;
    henka_mat4 combined;
    henka_mat4 perspective;
    henka_mat4 look_at;
    henka_transform transform;
    henka_mat4 transform_matrix;
    henka_quat rotation;

    added = henka_vec3_add((henka_vec3){1.0f, 2.0f, 3.0f}, (henka_vec3){4.0f, 5.0f, 6.0f});
    HENKA_TEST_ASSERT_FLOAT_CLOSE(added.x, 5.0f, 0.0001);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(added.y, 7.0f, 0.0001);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(added.z, 9.0f, 0.0001);

    normalized = henka_vec3_normalize((henka_vec3){0.0f, 3.0f, 4.0f});
    HENKA_TEST_ASSERT_FLOAT_CLOSE(normalized.y, 0.6f, 0.0001);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(normalized.z, 0.8f, 0.0001);
    normalized = henka_vec3_normalize((henka_vec3){FLT_MAX, FLT_MAX, 0.0f});
    HENKA_TEST_ASSERT(henka_test_math_float_is_finite(normalized.x));
    HENKA_TEST_ASSERT(henka_test_math_float_is_finite(normalized.y));
    HENKA_TEST_ASSERT_FLOAT_CLOSE(normalized.x, 0.7071067f, 0.0002f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(normalized.y, 0.7071067f, 0.0002f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(henka_vec3_length(normalized), 1.0f, 0.0002f);
    added = (henka_vec3){FLT_MAX, FLT_MAX, 0.0f};
    HENKA_TEST_ASSERT(henka_vec3_length(added) == INFINITY);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(henka_vec3_dot((henka_vec3){1.0f, 0.0f, 0.0f}, (henka_vec3){0.0f, 1.0f, 0.0f}), 0.0f, 0.0001);

    crossed = henka_vec3_cross((henka_vec3){1.0f, 0.0f, 0.0f}, (henka_vec3){0.0f, 1.0f, 0.0f});
    HENKA_TEST_ASSERT_FLOAT_CLOSE(crossed.z, 1.0f, 0.0001);
    rotation = henka_quat_from_axis_angle((henka_vec3){0.0f, 1.0f, 0.0f}, 90.0f * HENKA_DEG_TO_RAD);
    rotated = henka_quat_rotate_vec3(rotation, (henka_vec3){1.0f, 0.0f, 0.0f});
    HENKA_TEST_ASSERT_FLOAT_CLOSE(rotated.x, 0.0f, 0.0002f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(rotated.z, -1.0f, 0.0002f);
    rotation = henka_quat_multiply(henka_quat_identity(), rotation);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(rotation.w, henka_quat_normalize(rotation).w, 0.0001f);
    rotation = henka_quat_normalize((henka_quat){FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX});
    HENKA_TEST_ASSERT(henka_test_math_float_is_finite(rotation.x));
    HENKA_TEST_ASSERT(henka_test_math_float_is_finite(rotation.y));
    HENKA_TEST_ASSERT(henka_test_math_float_is_finite(rotation.z));
    HENKA_TEST_ASSERT(henka_test_math_float_is_finite(rotation.w));
    HENKA_TEST_ASSERT_FLOAT_CLOSE(rotation.x, 0.5f, 0.0002f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(rotation.w, 0.5f, 0.0002f);
    rotation = henka_quat_multiply(
        (henka_quat){FLT_MAX, 0.0f, 0.0f, FLT_MAX},
        (henka_quat){0.0f, FLT_MAX, 0.0f, FLT_MAX});
    HENKA_TEST_ASSERT(henka_test_math_float_is_finite(rotation.x));
    HENKA_TEST_ASSERT(henka_test_math_float_is_finite(rotation.y));
    HENKA_TEST_ASSERT(henka_test_math_float_is_finite(rotation.z));
    HENKA_TEST_ASSERT(henka_test_math_float_is_finite(rotation.w));
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        sqrtf(
            rotation.x * rotation.x +
            rotation.y * rotation.y +
            rotation.z * rotation.z +
            rotation.w * rotation.w),
        1.0f,
        0.0002f);

    identity = henka_mat4_identity();
    HENKA_TEST_ASSERT_FLOAT_CLOSE(identity.m[0], 1.0f, 0.0001);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(identity.m[5], 1.0f, 0.0001);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(identity.m[10], 1.0f, 0.0001);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(identity.m[15], 1.0f, 0.0001);

    translation = henka_mat4_translation((henka_vec3){2.0f, 3.0f, 4.0f});
    scale = henka_mat4_scale((henka_vec3){2.0f, 2.0f, 2.0f});
    combined = henka_mat4_multiply(translation, scale);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(combined.m[0], 2.0f, 0.0001);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(combined.m[12], 2.0f, 0.0001);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(combined.m[13], 3.0f, 0.0001);

    perspective = henka_mat4_perspective(60.0f * HENKA_DEG_TO_RAD, 16.0f / 9.0f, 0.1f, 100.0f);
    HENKA_TEST_ASSERT(perspective.m[0] > 0.0f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(perspective.m[11], -1.0f, 0.0001);

    look_at = henka_mat4_look_at((henka_vec3){0.0f, 0.0f, 5.0f}, (henka_vec3){0.0f, 0.0f, 0.0f}, (henka_vec3){0.0f, 1.0f, 0.0f});
    HENKA_TEST_ASSERT_FLOAT_CLOSE(look_at.m[14], -5.0f, 0.0001);

    transform = henka_transform_identity();
    HENKA_TEST_ASSERT_FLOAT_CLOSE(transform.position.x, 0.0f, 0.0001);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(transform.position.y, 0.0f, 0.0001);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(transform.position.z, 0.0f, 0.0001);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(transform.scale.x, 1.0f, 0.0001);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(transform.scale.y, 1.0f, 0.0001);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(transform.scale.z, 1.0f, 0.0001);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(transform.rotation.w, 1.0f, 0.0001);
    transform.position = (henka_vec3){1.0f, 2.0f, 3.0f};
    transform.scale = (henka_vec3){2.0f, 2.0f, 2.0f};
    transform_matrix = henka_transform_to_mat4(transform);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(transform_matrix.m[0], 2.0f, 0.0001);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(transform_matrix.m[12], 1.0f, 0.0001);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(transform_matrix.m[13], 2.0f, 0.0001);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(transform_matrix.m[14], 3.0f, 0.0001);
}
