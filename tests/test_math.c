#include "test_suite.h"

#include <henka/core.h>
#include <henka/math.h>

void henka_test_math(void)
{
    henka_vec3 added;
    henka_vec3 normalized;
    henka_vec3 crossed;
    henka_mat4 identity;
    henka_mat4 translation;
    henka_mat4 scale;
    henka_mat4 combined;
    henka_mat4 perspective;
    henka_mat4 look_at;
    henka_transform transform;
    henka_mat4 transform_matrix;

    added = henka_vec3_add((henka_vec3){1.0f, 2.0f, 3.0f}, (henka_vec3){4.0f, 5.0f, 6.0f});
    HENKA_TEST_ASSERT_FLOAT_CLOSE(added.x, 5.0f, 0.0001);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(added.y, 7.0f, 0.0001);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(added.z, 9.0f, 0.0001);

    normalized = henka_vec3_normalize((henka_vec3){0.0f, 3.0f, 4.0f});
    HENKA_TEST_ASSERT_FLOAT_CLOSE(normalized.y, 0.6f, 0.0001);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(normalized.z, 0.8f, 0.0001);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(henka_vec3_dot((henka_vec3){1.0f, 0.0f, 0.0f}, (henka_vec3){0.0f, 1.0f, 0.0f}), 0.0f, 0.0001);

    crossed = henka_vec3_cross((henka_vec3){1.0f, 0.0f, 0.0f}, (henka_vec3){0.0f, 1.0f, 0.0f});
    HENKA_TEST_ASSERT_FLOAT_CLOSE(crossed.z, 1.0f, 0.0001);

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
