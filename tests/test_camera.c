#include "test_suite.h"

#include <henka/camera.h>
#include <henka/core.h>

void henka_test_camera(void)
{
    henka_camera camera;

    camera = henka_camera_create_perspective(60.0f * HENKA_DEG_TO_RAD, 16.0f / 9.0f, 0.1f, 100.0f);
    henka_camera_apply_mouse_look(&camera, 1.25f, 99.0f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(camera.yaw_radians, (-HENKA_PI * 0.5f) + 1.25f, 0.0001);
    HENKA_TEST_ASSERT(camera.pitch_radians < 1.56f);

    henka_camera_apply_mouse_look(&camera, 0.0f, -999.0f);
    HENKA_TEST_ASSERT(camera.pitch_radians > -1.56f);
}
