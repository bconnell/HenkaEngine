#include "test_suite.h"

#include <math.h>
#include <string.h>

#include <henka/camera.h>
#include <henka/persistence.h>
#include <henka/ui.h>

#include "../examples/sandbox3d/editor_preferences.h"
#include "../examples/sandbox3d/view_compass.h"

static bool henka_test_vec3_close(henka_vec3 actual, henka_vec3 expected, float epsilon)
{
    return fabsf(actual.x - expected.x) <= epsilon &&
        fabsf(actual.y - expected.y) <= epsilon &&
        fabsf(actual.z - expected.z) <= epsilon;
}

static void henka_test_view_compass_preferences(void)
{
    henka_settings* settings = NULL;
    sandbox3d_view_compass_preferences preferences;
    sandbox3d_view_compass_preferences candidate;

    sandbox3d_view_compass_preferences_defaults(&preferences);
    HENKA_TEST_ASSERT(preferences.visible);
    HENKA_TEST_ASSERT(preferences.side == SANDBOX3D_VIEW_COMPASS_SIDE_RIGHT);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(preferences.scale, 1.0f, 0.0001f);
    HENKA_TEST_ASSERT(sandbox3d_view_compass_preferences_validate(&preferences));
    HENKA_TEST_ASSERT(sandbox3d_view_compass_scale_index(0.85f) == 0U);
    HENKA_TEST_ASSERT(sandbox3d_view_compass_scale_index(1.15f) == 2U);

    HENKA_TEST_ASSERT(henka_settings_create(&settings) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_settings_set_float(settings, "viewport.compass.scale", 2.0f) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_settings_set_string(settings, "viewport.compass.side", "invalid") == HENKA_SUCCESS);
    sandbox3d_view_compass_preferences_load(settings, &preferences);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(preferences.scale, 1.0f, 0.0001f);
    HENKA_TEST_ASSERT(preferences.side == SANDBOX3D_VIEW_COMPASS_SIDE_RIGHT);

    candidate = preferences;
    candidate.side = SANDBOX3D_VIEW_COMPASS_SIDE_LEFT;
    candidate.scale = 1.15f;
    HENKA_TEST_ASSERT(
        sandbox3d_view_compass_preferences_commit(
            settings,
            "build/test_tmp/view_compass.settings",
            &preferences,
            &candidate) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(preferences.side == SANDBOX3D_VIEW_COMPASS_SIDE_LEFT);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        henka_settings_get_float(settings, "viewport.compass.scale", 0.0f),
        1.15f,
        0.0001f);
    henka_settings_destroy(settings);
}

static void henka_test_view_compass_camera_math(void)
{
    const henka_vec3 target = {1.0f, 2.0f, 3.0f};
    henka_camera camera = henka_camera_create_perspective(60.0f * HENKA_DEG_TO_RAD, 1.5f, 0.1f, 100.0f);
    henka_camera candidate;
    henka_vec2 point;
    henka_vec2 center = {100.0f, 100.0f};
    bool front;
    float heading;
    float distance;
    size_t view;

    camera.position = (henka_vec3){7.0f, 5.0f, 9.0f};
    HENKA_TEST_ASSERT(henka_camera_look_at(&camera, target));
    HENKA_TEST_ASSERT(sandbox3d_view_compass_get_heading_degrees(&camera, &(sandbox3d_view_compass_state){0}, &heading));
    HENKA_TEST_ASSERT(isfinite(heading));
    HENKA_TEST_ASSERT(sandbox3d_view_compass_project_direction(&camera, (henka_vec3){1.0f, 0.0f, 0.0f}, 40.0f, center, &point, &front));
    HENKA_TEST_ASSERT(isfinite(point.x) && isfinite(point.y));

    distance = henka_vec3_length(henka_vec3_subtract(target, camera.position));
    for (view = 0U; view < SANDBOX3D_VIEW_COMPASS_AXIS_VIEW_COUNT; ++view)
    {
        HENKA_TEST_ASSERT(
            sandbox3d_view_compass_build_axis_camera(
                &camera,
                target,
                (sandbox3d_view_compass_axis_view)view,
                &candidate));
        HENKA_TEST_ASSERT(henka_camera_is_valid(&candidate));
        HENKA_TEST_ASSERT_FLOAT_CLOSE(
            henka_vec3_length(henka_vec3_subtract(target, candidate.position)),
            distance,
            0.0001f);
    }

    HENKA_TEST_ASSERT(sandbox3d_view_compass_toggle_projection(&camera, target, true));
    HENKA_TEST_ASSERT(camera.projection_mode == HENKA_CAMERA_PROJECTION_ORTHOGRAPHIC);
    HENKA_TEST_ASSERT(sandbox3d_view_compass_toggle_projection(&camera, target, true));
    HENKA_TEST_ASSERT(camera.projection_mode == HENKA_CAMERA_PROJECTION_PERSPECTIVE);
}

static void henka_test_view_compass_ui_capture(void)
{
    henka_ui_context* ui = NULL;
    henka_ui_frame_desc frame = {0};
    henka_ui_interaction_state interaction;

    HENKA_TEST_ASSERT(henka_ui_create(&ui) == HENKA_SUCCESS);
    henka_ui_set_visible(ui, true);
    frame.framebuffer_width = 640;
    frame.framebuffer_height = 480;
    frame.mouse_position = (henka_vec2){50.0f, 50.0f};

    HENKA_TEST_ASSERT(henka_ui_begin_frame(ui, &frame) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_ui_custom_interaction(ui, "compass_globe", true, true, &interaction) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(interaction.hovered && !interaction.pressed && !interaction.active);
    HENKA_TEST_ASSERT(henka_ui_get_wants_mouse(ui));
    HENKA_TEST_ASSERT(henka_ui_end_frame(ui) == HENKA_SUCCESS);

    frame.mouse_left_down = true;
    frame.mouse_left_pressed = true;
    HENKA_TEST_ASSERT(henka_ui_begin_frame(ui, &frame) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_ui_custom_interaction(ui, "compass_globe", true, true, &interaction) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(interaction.pressed && interaction.held && interaction.active);
    HENKA_TEST_ASSERT(henka_ui_custom_interaction(ui, "compass_projection", true, true, &(henka_ui_interaction_state){0}) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_ui_end_frame(ui) == HENKA_SUCCESS);

    frame.mouse_left_pressed = false;
    frame.mouse_position = (henka_vec2){500.0f, 400.0f};
    HENKA_TEST_ASSERT(henka_ui_begin_frame(ui, &frame) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_ui_custom_interaction(ui, "compass_globe", false, true, &interaction) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(interaction.held && interaction.active);
    HENKA_TEST_ASSERT(henka_ui_end_frame(ui) == HENKA_SUCCESS);

    frame.mouse_left_down = false;
    frame.mouse_left_released = true;
    HENKA_TEST_ASSERT(henka_ui_begin_frame(ui, &frame) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_ui_custom_interaction(ui, "compass_globe", false, true, &interaction) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(interaction.released && !interaction.held);
    HENKA_TEST_ASSERT(henka_ui_end_frame(ui) == HENKA_SUCCESS);
    henka_ui_destroy(ui);
}

void henka_test_view_compass(void)
{
    sandbox3d_view_compass_layout layout;
    sandbox3d_view_compass_preferences preferences;

    sandbox3d_view_compass_preferences_defaults(&preferences);
    HENKA_TEST_ASSERT(sandbox3d_view_compass_compute_layout((henka_viewport){0, 0, 640, 480}, &preferences, &layout));
    HENKA_TEST_ASSERT(layout.circle_bounds.x > 400.0f);
    preferences.side = SANDBOX3D_VIEW_COMPASS_SIDE_LEFT;
    HENKA_TEST_ASSERT(sandbox3d_view_compass_compute_layout((henka_viewport){0, 0, 640, 480}, &preferences, &layout));
    HENKA_TEST_ASSERT(layout.circle_bounds.x < 100.0f);

    henka_test_view_compass_preferences();
    henka_test_view_compass_camera_math();
    henka_test_view_compass_ui_capture();
}
