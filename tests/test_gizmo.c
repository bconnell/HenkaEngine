#include "test_suite.h"

#include <henka/core.h>
#include <henka/gizmo.h>

void henka_test_gizmo(void)
{
    float distance;
    float snapped;
    float value;
    henka_gizmo_axis axis;
    henka_vec2 ring_points[5];

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

    HENKA_TEST_ASSERT(
        henka_gizmo_hit_test_segment_2d(
            (henka_vec2){110.0f, 101.0f},
            (henka_vec2){100.0f, 100.0f},
            (henka_vec2){160.0f, 100.0f},
            8.0f,
            &distance) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(
        henka_gizmo_hit_test_rect_2d(
            (henka_vec2){206.0f, 206.0f},
            (henka_vec2){200.0f, 200.0f},
            (henka_vec2){10.0f, 10.0f},
            4.0f) == true);
    HENKA_TEST_ASSERT(
        henka_gizmo_hit_test_rect_2d(
            (henka_vec2){220.5f, 200.0f},
            (henka_vec2){200.0f, 200.0f},
            (henka_vec2){10.0f, 10.0f},
            4.0f) == false);

    ring_points[0] = (henka_vec2){150.0f, 100.0f};
    ring_points[1] = (henka_vec2){100.0f, 150.0f};
    ring_points[2] = (henka_vec2){50.0f, 100.0f};
    ring_points[3] = (henka_vec2){100.0f, 50.0f};
    ring_points[4] = (henka_vec2){150.0f, 100.0f};
    HENKA_TEST_ASSERT(
        henka_gizmo_hit_test_polyline_2d(
            (henka_vec2){148.0f, 102.0f},
            ring_points,
            5U,
            10.0f,
            &distance) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(
        henka_gizmo_hit_test_polyline_2d(
            (henka_vec2){100.0f, 100.0f},
            ring_points,
            5U,
            6.0f,
            &distance) != HENKA_SUCCESS);
}
