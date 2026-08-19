#include "test_suite.h"

#include <henka/camera.h>
#include <henka/core.h>

#include "../engine/src/renderer/temporal_camera_policy.h"

#include <math.h>

static void camera_hardening_side_is_world_x(void)
{
    henka_camera camera =
        henka_camera_create_perspective(
            60.0f * HENKA_DEG_TO_RAD,
            16.0f / 9.0f,
            0.1f,
            500.0f);

    henka_vec3 forward;

    HENKA_TEST_ASSERT(
        henka_camera_apply_preset(
            &camera,
            HENKA_CAMERA_PRESET_SIDE_2_5D,
            (henka_vec3){0.0f, 0.0f, 0.0f}) ==
        HENKA_SUCCESS);

    forward =
        henka_camera_get_forward(&camera);

    HENKA_TEST_ASSERT(
        fabsf(fabsf(forward.x) - 1.0f) <=
        0.0001f);

    HENKA_TEST_ASSERT(
        fabsf(forward.y) <= 0.0001f);

    HENKA_TEST_ASSERT(
        fabsf(forward.z) <= 0.0001f);
}

static void camera_hardening_ortho_fit_is_view_aware(void)
{
    henka_camera camera =
        henka_camera_create_orthographic(
            100.0f,
            1.0f,
            0.1f,
            500.0f);

    const henka_bounds bounds =
    {
        {0.0f, 0.0f, 0.0f},
        {8.0f, 1.0f, 1.0f}
    };

    HENKA_TEST_ASSERT(
        henka_camera_frame_bounds(
            &camera,
            bounds,
            0.0f,
            0.0f));

    HENKA_TEST_ASSERT(
        camera.orthographic_height > 2.0f);

    HENKA_TEST_ASSERT(
        camera.orthographic_height < 5.0f);
}

static void camera_hardening_ortho_focus_reframes(void)
{
    henka_camera camera =
        henka_camera_create_orthographic(
            100.0f,
            1.0f,
            0.1f,
            500.0f);

    const henka_bounds bounds =
    {
        {3.0f, 2.0f, -4.0f},
        {8.0f, 1.0f, 1.0f}
    };

    camera.yaw_radians = 0.0f;
    camera.pitch_radians = 0.0f;

    HENKA_TEST_ASSERT(
        henka_camera_focus_on_bounds(
            &camera,
            bounds));

    HENKA_TEST_ASSERT(
        camera.orthographic_height < 5.0f);
}

static void camera_hardening_orbit_yaw_is_bounded(void)
{
    henka_camera camera =
        henka_camera_create_perspective(
            60.0f * HENKA_DEG_TO_RAD,
            1.0f,
            0.1f,
            500.0f);

    HENKA_TEST_ASSERT(
        henka_camera_orbit_target(
            &camera,
            (henka_vec3){0.0f, 0.0f, 0.0f},
            HENKA_PI * 1000.0f + 0.37f,
            0.0f));

    HENKA_TEST_ASSERT(
        camera.yaw_radians >= -HENKA_PI);

    HENKA_TEST_ASSERT(
        camera.yaw_radians <= HENKA_PI);
}

static void camera_hardening_mouse_yaw_is_bounded(void)
{
    henka_camera camera =
        henka_camera_create_perspective(
            60.0f * HENKA_DEG_TO_RAD,
            1.0f,
            0.1f,
            500.0f);

    henka_camera_apply_mouse_look(
        &camera,
        HENKA_PI * 1000.0f + 0.27f,
        0.0f);

    HENKA_TEST_ASSERT(
        camera.yaw_radians >= -HENKA_PI);

    HENKA_TEST_ASSERT(
        camera.yaw_radians <= HENKA_PI);
}

static void camera_hardening_all_presets_fit_bounds(void)
{
    const int width = 1280;
    const int height = 720;

    const henka_bounds bounds =
    {
        {2.0f, 4.0f, -3.0f},
        {1.5f, 5.0f, 2.0f}
    };

    int preset_index;

    for (preset_index = 0;
         preset_index <
            (int)HENKA_CAMERA_PRESET_COUNT;
         ++preset_index)
    {
        henka_camera camera =
            henka_camera_create_perspective(
                60.0f * HENKA_DEG_TO_RAD,
                (float)width / (float)height,
                0.1f,
                500.0f);

        henka_vec2 center_screen;
        float center_depth;
        int x_sign;
        int y_sign;
        int z_sign;

        const henka_camera_preset preset =
            (henka_camera_preset)preset_index;

        HENKA_TEST_ASSERT(
            henka_camera_apply_preset(
                &camera,
                preset,
                bounds.center) ==
            HENKA_SUCCESS);

        HENKA_TEST_ASSERT(
            henka_camera_frame_bounds(
                &camera,
                bounds,
                camera.yaw_radians,
                camera.pitch_radians));

        HENKA_TEST_ASSERT(
            henka_camera_world_to_screen(
                &camera,
                width,
                height,
                bounds.center,
                &center_screen,
                &center_depth) ==
            HENKA_SUCCESS);

        HENKA_TEST_ASSERT(
            fabsf(
                center_screen.x -
                (float)width * 0.5f) <=
            1.0f);

        HENKA_TEST_ASSERT(
            fabsf(
                center_screen.y -
                (float)height * 0.5f) <=
            1.0f);

        for (x_sign = -1;
             x_sign <= 1;
             x_sign += 2)
        {
            for (y_sign = -1;
                 y_sign <= 1;
                 y_sign += 2)
            {
                for (z_sign = -1;
                     z_sign <= 1;
                     z_sign += 2)
                {
                    henka_vec3 corner =
                    {
                        bounds.center.x +
                            bounds.extents.x *
                            (float)x_sign,

                        bounds.center.y +
                            bounds.extents.y *
                            (float)y_sign,

                        bounds.center.z +
                            bounds.extents.z *
                            (float)z_sign
                    };

                    henka_vec2 screen;
                    float depth;

                    HENKA_TEST_ASSERT(
                        henka_camera_world_to_screen(
                            &camera,
                            width,
                            height,
                            corner,
                            &screen,
                            &depth) ==
                        HENKA_SUCCESS);

                    HENKA_TEST_ASSERT(
                        screen.x >= 0.0f &&
                        screen.x <= (float)width);

                    HENKA_TEST_ASSERT(
                        screen.y >= 0.0f &&
                        screen.y <= (float)height);
                }
            }
        }
    }
}

static void camera_hardening_temporal_angle_wraps(void)
{
    henka_camera previous =
        henka_camera_create_perspective(
            60.0f * HENKA_DEG_TO_RAD,
            1.0f,
            0.1f,
            500.0f);

    henka_camera current = previous;
    henka_temporal_camera_motion motion;

    previous.yaw_radians =
        HENKA_PI - 0.01f;

    current.yaw_radians =
        -HENKA_PI + 0.01f;

    HENKA_TEST_ASSERT(
        henka_temporal_camera_measure(
            &previous,
            &current,
            &motion));

    HENKA_TEST_ASSERT(
        motion.angle_delta < 0.05f);
}

static void camera_hardening_temporal_detects_roll(void)
{
    henka_camera previous =
        henka_camera_create_perspective(
            60.0f * HENKA_DEG_TO_RAD,
            1.0f,
            0.1f,
            500.0f);

    henka_camera current = previous;
    henka_temporal_camera_motion motion;

    current.roll_radians = 0.2f;

    HENKA_TEST_ASSERT(
        henka_temporal_camera_measure(
            &previous,
            &current,
            &motion));

    HENKA_TEST_ASSERT(
        motion.angle_delta > 0.19f);
}

static void camera_hardening_ortho_never_temporal_jitters(void)
{
    HENKA_TEST_ASSERT(
        !henka_temporal_camera_should_jitter(
            true,
            true,
            true,
            HENKA_CAMERA_PROJECTION_ORTHOGRAPHIC,
            true,
            false,
            false));
}

static void camera_hardening_projection_only_change_does_not_jitter(void)
{
    henka_camera previous =
        henka_camera_create_perspective(
            60.0f * HENKA_DEG_TO_RAD,
            1.0f,
            0.1f,
            500.0f);

    henka_camera current = previous;
    henka_temporal_camera_motion motion;

    current.aspect_ratio = 1.001f;

    HENKA_TEST_ASSERT(
        henka_temporal_camera_measure(
            &previous,
            &current,
            &motion));

    HENKA_TEST_ASSERT(
        !henka_temporal_camera_transform_is_moving(
            &motion));

    HENKA_TEST_ASSERT(
        !henka_temporal_camera_should_jitter(
            true,
            true,
            true,
            current.projection_mode,
            false,
            false,
            false));
}

static void camera_hardening_cut_never_jitters(void)
{
    HENKA_TEST_ASSERT(
        !henka_temporal_camera_should_jitter(
            true,
            true,
            true,
            HENKA_CAMERA_PROJECTION_PERSPECTIVE,
            true,
            false,
            true));
}

static void camera_hardening_perspective_motion_can_jitter(void)
{
    HENKA_TEST_ASSERT(
        henka_temporal_camera_should_jitter(
            true,
            true,
            true,
            HENKA_CAMERA_PROJECTION_PERSPECTIVE,
            true,
            false,
            false));
}

static void camera_hardening_inactive_projection_values_are_ignored(void)
{
    henka_camera previous;
    henka_camera current;
    henka_temporal_camera_motion motion;

    previous =
        henka_camera_create_perspective(
            60.0f * HENKA_DEG_TO_RAD,
            1.0f,
            0.1f,
            500.0f);

    current = previous;
    current.orthographic_height += 100.0f;

    HENKA_TEST_ASSERT(
        henka_temporal_camera_measure(
            &previous,
            &current,
            &motion));

    HENKA_TEST_ASSERT(
        motion.projection_delta <= 0.000001f);

    previous =
        henka_camera_create_orthographic(
            8.0f,
            1.0f,
            0.1f,
            500.0f);

    current = previous;
    current.field_of_view_radians += 0.5f;

    HENKA_TEST_ASSERT(
        henka_temporal_camera_measure(
            &previous,
            &current,
            &motion));

    HENKA_TEST_ASSERT(
        motion.projection_delta <= 0.000001f);
}

static void camera_hardening_large_angle_is_cut(void)
{
    henka_camera previous =
        henka_camera_create_perspective(
            60.0f * HENKA_DEG_TO_RAD,
            1.0f,
            0.1f,
            500.0f);

    henka_camera current = previous;
    henka_temporal_camera_motion motion;

    current.yaw_radians += 0.40f;

    HENKA_TEST_ASSERT(
        henka_temporal_camera_measure(
            &previous,
            &current,
            &motion));

    HENKA_TEST_ASSERT(
        henka_temporal_camera_is_cut(
            &motion));
}

void henka_test_camera_hardening(void)
{
    camera_hardening_side_is_world_x();
    camera_hardening_ortho_fit_is_view_aware();
    camera_hardening_ortho_focus_reframes();
    camera_hardening_orbit_yaw_is_bounded();
    camera_hardening_mouse_yaw_is_bounded();
    camera_hardening_all_presets_fit_bounds();

    camera_hardening_temporal_angle_wraps();
    camera_hardening_temporal_detects_roll();
    camera_hardening_inactive_projection_values_are_ignored();
    camera_hardening_large_angle_is_cut();
    camera_hardening_ortho_never_temporal_jitters();
    camera_hardening_projection_only_change_does_not_jitter();
    camera_hardening_cut_never_jitters();
    camera_hardening_perspective_motion_can_jitter();
}