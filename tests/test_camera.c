#include "test_suite.h"

#include <henka/camera.h>
#include <henka/core.h>

void henka_test_camera(void)
{
    henka_camera camera;
    henka_bounds bounds;
    henka_ray ray;

    camera = henka_camera_create_perspective(60.0f * HENKA_DEG_TO_RAD, 16.0f / 9.0f, 0.1f, 100.0f);
    henka_camera_apply_mouse_look(&camera, 1.25f, 99.0f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(camera.yaw_radians, (-HENKA_PI * 0.5f) + 1.25f, 0.0001);
    HENKA_TEST_ASSERT(camera.pitch_radians < 1.56f);

    henka_camera_apply_mouse_look(&camera, 0.0f, -999.0f);
    HENKA_TEST_ASSERT(camera.pitch_radians > -1.56f);
    HENKA_TEST_ASSERT(henka_camera_clamp_pitch(999.0f) < 1.56f);
    HENKA_TEST_ASSERT(henka_camera_clamp_pitch(-999.0f) > -1.56f);

    bounds = (henka_bounds){{0.0f, 0.5f, 0.0f}, {0.5f, 0.5f, 0.5f}};
    HENKA_TEST_ASSERT(henka_camera_focus_on_bounds(&camera, bounds));
    HENKA_TEST_ASSERT(henka_camera_screen_point_to_ray(&camera, 1280, 720, (henka_vec2){640.0f, 360.0f}, &ray) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(henka_vec3_length(ray.direction), 1.0f, 0.0001f);

    camera = henka_camera_create_orthographic(8.0f, 16.0f / 9.0f, 0.1f, 100.0f);
    HENKA_TEST_ASSERT(camera.projection_mode == HENKA_CAMERA_PROJECTION_ORTHOGRAPHIC);
    HENKA_TEST_ASSERT(henka_camera_screen_point_to_ray(&camera, 1280, 720, (henka_vec2){0.0f, 0.0f}, &ray) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(henka_vec3_length(ray.direction), 1.0f, 0.0001f);
}
