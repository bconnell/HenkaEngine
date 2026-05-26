#include "test_suite.h"

#include <henka/core.h>
#include <henka/gizmo.h>

void henka_test_gizmo(void)
{
    float distance;
    float snapped;
    float value;
    henka_gizmo_axis axis;

    HENKA_TEST_ASSERT(henka_gizmo_get_axis_direction(HENKA_GIZMO_AXIS_X, NULL) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_gizmo_snap_move(1.14f, 0.25f, &snapped) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(snapped, 1.25f, 0.0001f);
    HENKA_TEST_ASSERT(henka_gizmo_snap_rotate(20.0f * HENKA_DEG_TO_RAD, 15.0f * HENKA_DEG_TO_RAD, &snapped) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(snapped, 15.0f * HENKA_DEG_TO_RAD, 0.0001f);
    HENKA_TEST_ASSERT(henka_gizmo_snap_scale(0.02f, 0.1f, 0.05f, &snapped) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(snapped, 0.1f, 0.0001f);

    HENKA_TEST_ASSERT(
        henka_gizmo_hit_test_axis(
            (henka_vec3){0.0f, 0.0f, 0.0f},
            (henka_ray){{0.5f, 0.0f, 3.0f}, {0.0f, 0.0f, -1.0f}},
            1.5f,
            0.25f,
            &axis,
            &distance) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(axis == HENKA_GIZMO_AXIS_X);

    HENKA_TEST_ASSERT(
        henka_gizmo_hit_test_axis(
            (henka_vec3){0.0f, 0.0f, 0.0f},
            (henka_ray){{0.0f, 2.0f, 3.0f}, {0.0f, 0.0f, -1.0f}},
            1.5f,
            0.2f,
            &axis,
            &distance) != HENKA_SUCCESS);

    HENKA_TEST_ASSERT(
        henka_gizmo_hit_test_rotation_rings(
            (henka_vec3){0.0f, 0.0f, 0.0f},
            (henka_ray){{1.0f, 2.0f, 0.0f}, {0.0f, -1.0f, 0.0f}},
            1.0f,
            0.2f,
            &axis,
            &distance) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(axis == HENKA_GIZMO_AXIS_Y);

    HENKA_TEST_ASSERT(
        henka_gizmo_hit_test_uniform_handle(
            (henka_vec3){0.0f, 0.0f, 0.0f},
            (henka_ray){{0.0f, 0.0f, 3.0f}, {0.0f, 0.0f, -1.0f}},
            0.5f,
            &distance) == HENKA_SUCCESS);

    HENKA_TEST_ASSERT(
        henka_gizmo_project_axis_delta(
            (henka_vec3){0.0f, 0.0f, 0.0f},
            (henka_vec3){1.0f, 0.0f, 0.0f},
            (henka_vec3){0.0f, 0.0f, -1.0f},
            (henka_ray){{1.0f, 0.0f, 5.0f}, {0.0f, 0.0f, -1.0f}},
            (henka_ray){{2.0f, 0.0f, 5.0f}, {0.0f, 0.0f, -1.0f}},
            &value) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(value, 1.0f, 0.0001f);

    HENKA_TEST_ASSERT(
        henka_gizmo_project_rotation_delta(
            (henka_vec3){0.0f, 0.0f, 0.0f},
            (henka_vec3){0.0f, 1.0f, 0.0f},
            (henka_ray){{1.0f, 2.0f, 0.0f}, {0.0f, -1.0f, 0.0f}},
            (henka_ray){{0.0f, 2.0f, 1.0f}, {0.0f, -1.0f, 0.0f}},
            &value) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(fabsf(value), 90.0f * HENKA_DEG_TO_RAD, 0.0001f);
}
