#include "test_suite.h"

#include <string.h>

#include <henka/camera.h>
#include <henka/core.h>

void henka_test_camera(void)
{
    henka_bounds bounds;
    henka_camera camera;
    henka_ray corner_ray;
    henka_ray ray;
    henka_vec2 screen_point;
    henka_vec3 focus_target;
    henka_vec3 forward;
    henka_vec3 preset_target;
    henka_vec3 right;
    henka_vec3 up;

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
    HENKA_TEST_ASSERT(henka_camera_frame_bounds(&camera, bounds, -HENKA_PI * 0.5f, -0.35f));
    HENKA_TEST_ASSERT(henka_camera_screen_point_to_ray(&camera, 1280, 720, (henka_vec2){640.0f, 360.0f}, &ray) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(henka_vec3_length(ray.direction), 1.0f, 0.0001f);
    forward = henka_camera_get_forward(&camera);
    HENKA_TEST_ASSERT(henka_camera_world_to_screen(
        &camera,
        1280,
        720,
        henka_vec3_add(camera.position, henka_vec3_scale(forward, 5.0f)),
        &screen_point,
        NULL) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(screen_point.x, 640.0f, 0.5f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(screen_point.y, 360.0f, 0.5f);
    HENKA_TEST_ASSERT(henka_camera_world_to_screen(
        &camera,
        1280,
        720,
        henka_vec3_subtract(camera.position, henka_vec3_scale(forward, 5.0f)),
        &screen_point,
        NULL) != HENKA_SUCCESS);

    focus_target = bounds.center;
    HENKA_TEST_ASSERT(henka_camera_orbit_target(&camera, focus_target, 0.4f, 0.2f));
    HENKA_TEST_ASSERT(henka_camera_look_at(&camera, focus_target));
    HENKA_TEST_ASSERT(henka_camera_pan_target(&camera, &focus_target, 0.5f, 0.25f));
    HENKA_TEST_ASSERT(henka_camera_dolly_target(&camera, focus_target, -0.75f, 0.5f));
    HENKA_TEST_ASSERT(henka_camera_look_at(&camera, focus_target));
    forward = henka_camera_get_forward(&camera);
    HENKA_TEST_ASSERT(henka_camera_world_to_screen(
        &camera,
        1280,
        720,
        henka_vec3_add(camera.position, henka_vec3_scale(forward, 5.0f)),
        &screen_point,
        NULL) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(screen_point.x, 640.0f, 1.5f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(screen_point.y, 360.0f, 1.5f);

    HENKA_TEST_ASSERT(strcmp(henka_camera_preset_get_label(HENKA_CAMERA_PRESET_PERSPECTIVE_3D), "Perspective 3D") == 0);
    HENKA_TEST_ASSERT(strcmp(henka_camera_preset_get_label(HENKA_CAMERA_PRESET_SIDE_2_5D), "Side 2.5D") == 0);
    HENKA_TEST_ASSERT(strcmp(henka_camera_preset_get_label(HENKA_CAMERA_PRESET_TOP_DOWN_2_5D), "Top-down 2.5D") == 0);
    HENKA_TEST_ASSERT(strcmp(henka_camera_preset_get_label(HENKA_CAMERA_PRESET_ISOMETRIC_2_5D), "Isometric 2.5D") == 0);
    HENKA_TEST_ASSERT(strcmp(henka_camera_preset_get_label(HENKA_CAMERA_PRESET_COUNT), "Unknown") == 0);

    preset_target = (henka_vec3){0.0f, 0.5f, 0.0f};
    HENKA_TEST_ASSERT(henka_camera_apply_preset(NULL, HENKA_CAMERA_PRESET_SIDE_2_5D, preset_target) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_camera_apply_preset(&camera, HENKA_CAMERA_PRESET_COUNT, preset_target) == HENKA_ERROR_INVALID_ARGUMENT);

    camera = henka_camera_create_perspective(60.0f * HENKA_DEG_TO_RAD, 16.0f / 9.0f, 0.1f, 100.0f);
    HENKA_TEST_ASSERT(henka_camera_apply_preset(&camera, HENKA_CAMERA_PRESET_SIDE_2_5D, preset_target) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(camera.projection_mode == HENKA_CAMERA_PROJECTION_ORTHOGRAPHIC);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(camera.pitch_radians, 0.0f, 0.0001f);
    HENKA_TEST_ASSERT(henka_camera_world_to_screen(&camera, 1280, 720, preset_target, &screen_point, NULL) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(screen_point.x, 640.0f, 0.5f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(screen_point.y, 360.0f, 0.5f);

    HENKA_TEST_ASSERT(henka_camera_screen_point_to_ray(&camera, 1280, 720, (henka_vec2){0.0f, 0.0f}, &ray) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_camera_screen_point_to_ray(&camera, 1280, 720, (henka_vec2){1280.0f, 720.0f}, &corner_ray) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(ray.direction.x, corner_ray.direction.x, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(ray.direction.y, corner_ray.direction.y, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(ray.direction.z, corner_ray.direction.z, 0.0001f);
    HENKA_TEST_ASSERT(henka_vec3_length(henka_vec3_subtract(ray.origin, corner_ray.origin)) > 1.0f);

    HENKA_TEST_ASSERT(henka_camera_zoom_orthographic(&camera, 0.5f, 2.0f, 20.0f) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(camera.orthographic_height, 4.0f, 0.0001f);
    HENKA_TEST_ASSERT(henka_camera_zoom_orthographic(&camera, 0.01f, 2.0f, 20.0f) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(camera.orthographic_height, 2.0f, 0.0001f);
    HENKA_TEST_ASSERT(henka_camera_zoom_orthographic(&camera, 100.0f, 2.0f, 20.0f) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(camera.orthographic_height, 20.0f, 0.0001f);
    HENKA_TEST_ASSERT(henka_camera_zoom_orthographic(&camera, 0.0f, 2.0f, 20.0f) == HENKA_ERROR_INVALID_ARGUMENT);

    bounds = (henka_bounds){{1.0f, 2.0f, 3.0f}, {2.0f, 1.0f, 3.0f}};
    HENKA_TEST_ASSERT(henka_camera_frame_bounds(&camera, bounds, camera.yaw_radians, camera.pitch_radians));
    HENKA_TEST_ASSERT(camera.orthographic_height > 8.0f);
    HENKA_TEST_ASSERT(henka_camera_world_to_screen(&camera, 1280, 720, bounds.center, &screen_point, NULL) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(screen_point.x, 640.0f, 0.5f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(screen_point.y, 360.0f, 0.5f);

    HENKA_TEST_ASSERT(henka_camera_apply_preset(&camera, HENKA_CAMERA_PRESET_TOP_DOWN_2_5D, preset_target) == HENKA_SUCCESS);
    forward = henka_camera_get_forward(&camera);
    right = henka_camera_get_right(&camera);
    up = henka_camera_get_up(&camera);
    HENKA_TEST_ASSERT(forward.y < -0.999f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(henka_vec3_length(forward), 1.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(henka_vec3_length(right), 1.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(henka_vec3_length(up), 1.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(henka_vec3_dot(forward, right), 0.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(henka_vec3_dot(forward, up), 0.0f, 0.0001f);
    HENKA_TEST_ASSERT(henka_camera_screen_point_to_ray(&camera, 1280, 720, (henka_vec2){640.0f, 360.0f}, &ray) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(ray.direction.y < -0.999f);
    HENKA_TEST_ASSERT(henka_camera_world_to_screen(&camera, 1280, 720, preset_target, &screen_point, NULL) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(screen_point.x, 640.0f, 0.5f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(screen_point.y, 360.0f, 0.5f);

    HENKA_TEST_ASSERT(henka_camera_apply_preset(&camera, HENKA_CAMERA_PRESET_ISOMETRIC_2_5D, preset_target) == HENKA_SUCCESS);
    forward = henka_camera_get_forward(&camera);
    HENKA_TEST_ASSERT(forward.x < 0.0f);
    HENKA_TEST_ASSERT(forward.y < 0.0f);
    HENKA_TEST_ASSERT(forward.z < 0.0f);
    HENKA_TEST_ASSERT(camera.projection_mode == HENKA_CAMERA_PROJECTION_ORTHOGRAPHIC);

    HENKA_TEST_ASSERT(henka_camera_apply_preset(&camera, HENKA_CAMERA_PRESET_PERSPECTIVE_3D, preset_target) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(camera.projection_mode == HENKA_CAMERA_PROJECTION_PERSPECTIVE);
    HENKA_TEST_ASSERT(henka_camera_zoom_orthographic(&camera, 0.5f, 2.0f, 20.0f) == HENKA_ERROR_INVALID_ARGUMENT);
}
