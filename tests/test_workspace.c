#include "test_suite.h"

#include <float.h>
#include <limits.h>
#include <math.h>
#include <string.h>

#include <henka/workspace.h>

void henka_test_workspace(void)
{
    henka_vec2 framebuffer_point;
    henka_result result;
    henka_vec2 local_point;
    henka_workspace_desc desc;
    henka_workspace_layout layout;

    HENKA_TEST_ASSERT(henka_viewport_is_valid((henka_viewport){0, 0, 0, 10}) == false);
    HENKA_TEST_ASSERT(henka_viewport_is_valid((henka_viewport){-1, 0, 10, 10}) == false);
    HENKA_TEST_ASSERT(henka_viewport_is_valid((henka_viewport){0, -1, 10, 10}) == false);
    HENKA_TEST_ASSERT(henka_viewport_is_valid((henka_viewport){INT_MAX - 4, 0, 16, 10}) == false);
    HENKA_TEST_ASSERT(henka_viewport_is_valid((henka_viewport){10, 20, 300, 200}) == true);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(henka_viewport_get_aspect_ratio((henka_viewport){0, 0, 400, 200}), 2.0f, 0.0001f);
    HENKA_TEST_ASSERT(henka_viewport_contains_point((henka_viewport){100, 50, 320, 180}, (henka_vec2){120.0f, 60.0f}) == true);
    HENKA_TEST_ASSERT(henka_viewport_contains_point((henka_viewport){100, 50, 320, 180}, (henka_vec2){90.0f, 60.0f}) == false);
    HENKA_TEST_ASSERT(henka_viewport_contains_point((henka_viewport){100, 50, 320, 180}, (henka_vec2){420.0f, 60.0f}) == false);
    HENKA_TEST_ASSERT(henka_viewport_contains_point((henka_viewport){100, 50, 320, 180}, (henka_vec2){120.0f, 230.0f}) == false);
    HENKA_TEST_ASSERT(henka_viewport_window_to_local((henka_viewport){100, 50, 320, 180}, (henka_vec2){132.0f, 94.0f}, &local_point) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(local_point.x, 32.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(local_point.y, 44.0f, 0.0001f);
    local_point = (henka_vec2){123.0f, 456.0f};
    HENKA_TEST_ASSERT(henka_viewport_window_to_local((henka_viewport){100, 50, 320, 180}, (henka_vec2){40.0f, 94.0f}, &local_point) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(local_point.x, 0.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(local_point.y, 0.0f, 0.0001f);
    HENKA_TEST_ASSERT(henka_window_point_to_framebuffer_point(800, 600, 1200, 900, (henka_vec2){200.0f, 150.0f}, &framebuffer_point) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(framebuffer_point.x, 300.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(framebuffer_point.y, 225.0f, 0.0001f);
    HENKA_TEST_ASSERT(henka_viewport_contains_point((henka_viewport){250, 200, 400, 300}, framebuffer_point));
    HENKA_TEST_ASSERT(henka_window_point_to_framebuffer_point(800, 600, 1200, 900, (henka_vec2){60.0f, 40.0f}, &framebuffer_point) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(!henka_viewport_contains_point((henka_viewport){250, 200, 400, 300}, framebuffer_point));
    framebuffer_point = (henka_vec2){123.0f, 456.0f};
    HENKA_TEST_ASSERT(henka_window_point_to_framebuffer_point(0, 600, 1200, 900, (henka_vec2){200.0f, 150.0f}, &framebuffer_point) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(framebuffer_point.x, 0.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(framebuffer_point.y, 0.0f, 0.0001f);

    desc = (henka_workspace_desc){
        1280,
        720,
        14.0f,
        12.0f,
        300.0f,
        340.0f,
        0.0f,
        30.0f,
        4.0f,
        420,
        280,
        true,
        true,
        false};
    result = henka_workspace_layout_docked(&desc, &layout);
    HENKA_TEST_ASSERT(result == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(layout.left_dock.width > 0.0f);
    HENKA_TEST_ASSERT(layout.right_dock.width > 0.0f);
    HENKA_TEST_ASSERT(layout.scene_frame.width > 0.0f);
    HENKA_TEST_ASSERT(henka_viewport_is_valid(layout.scene_viewport));
    HENKA_TEST_ASSERT(layout.scene_frame.x >= layout.left_dock.x + layout.left_dock.width);
    HENKA_TEST_ASSERT(layout.right_dock.x >= layout.scene_frame.x + layout.scene_frame.width);
    HENKA_TEST_ASSERT(layout.scene_viewport.x >= (int)layout.scene_frame.x);
    HENKA_TEST_ASSERT(layout.scene_viewport.y >= (int)layout.scene_frame.y);
    HENKA_TEST_ASSERT(layout.scene_viewport.x + layout.scene_viewport.width <= (int)(layout.scene_frame.x + layout.scene_frame.width));
    HENKA_TEST_ASSERT(layout.scene_viewport.y + layout.scene_viewport.height <= (int)(layout.scene_frame.y + layout.scene_frame.height));
    HENKA_TEST_ASSERT(henka_viewport_contains_point(layout.scene_viewport, (henka_vec2){layout.left_dock.x + 10.0f, layout.left_dock.y + 10.0f}) == false);
    HENKA_TEST_ASSERT(henka_viewport_contains_point(layout.scene_viewport, (henka_vec2){layout.right_dock.x + 10.0f, layout.right_dock.y + 10.0f}) == false);
    HENKA_TEST_ASSERT(layout.scene_viewport.width >= 400);
    HENKA_TEST_ASSERT(layout.scene_viewport.height >= 250);
    {
        const int default_viewport_width = layout.scene_viewport.width;
        desc.left_dock_width += 48.0f;
        result = henka_workspace_layout_docked(&desc, &layout);
        HENKA_TEST_ASSERT(result == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(layout.scene_viewport.width < default_viewport_width);
        HENKA_TEST_ASSERT(layout.scene_viewport.width >= 400);
        desc.left_dock_width -= 48.0f;
    }

    desc.framebuffer_width = 720;
    desc.framebuffer_height = 420;
    desc.left_dock_width = 320.0f;
    desc.right_dock_width = 360.0f;
    desc.min_scene_width = 220;
    desc.min_scene_height = 160;
    result = henka_workspace_layout_docked(&desc, &layout);
    HENKA_TEST_ASSERT(result == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_viewport_is_valid(layout.scene_viewport));
    HENKA_TEST_ASSERT(layout.scene_viewport.width >= 1);
    HENKA_TEST_ASSERT(layout.scene_viewport.height >= 1);
    HENKA_TEST_ASSERT(layout.scene_frame.x >= 0.0f);
    HENKA_TEST_ASSERT(layout.scene_frame.y >= 0.0f);

    desc.framebuffer_width = 1920;
    desc.framebuffer_height = 1080;
    desc.left_dock_width = 328.0f;
    desc.right_dock_width = 356.0f;
    desc.min_scene_width = 520;
    desc.min_scene_height = 340;
    result = henka_workspace_layout_docked(&desc, &layout);
    HENKA_TEST_ASSERT(result == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(layout.scene_viewport.width >= 520);
    HENKA_TEST_ASSERT(layout.scene_viewport.height >= 340);
    HENKA_TEST_ASSERT(layout.scene_frame.x >= layout.left_dock.x + layout.left_dock.width);
    HENKA_TEST_ASSERT(layout.right_dock.x >= layout.scene_frame.x + layout.scene_frame.width);

    HENKA_TEST_ASSERT(henka_viewport_contains_point(
        (henka_viewport){INT_MAX - 4, 0, 16, 10},
        (henka_vec2){0.0f, 0.0f}) == false);
    HENKA_TEST_ASSERT(henka_viewport_contains_point(
        (henka_viewport){0, 0, 10, 10},
        (henka_vec2){NAN, 0.0f}) == false);
    HENKA_TEST_ASSERT(henka_window_point_to_framebuffer_point(
        800,
        600,
        1200,
        900,
        (henka_vec2){NAN, 10.0f},
        &framebuffer_point) == HENKA_ERROR_INVALID_ARGUMENT);

    desc.margin = NAN;
    memset(&layout, 0x5a, sizeof(layout));
    HENKA_TEST_ASSERT(henka_workspace_layout_docked(&desc, &layout) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(layout.scene_viewport.x == 0);
    HENKA_TEST_ASSERT(layout.scene_viewport.y == 0);
    HENKA_TEST_ASSERT(layout.scene_viewport.width == 0);
    HENKA_TEST_ASSERT(layout.scene_viewport.height == 0);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(layout.scene_frame.width, 0.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(layout.scene_frame.height, 0.0f, 0.0001f);

    desc = (henka_workspace_desc){
        1,
        1,
        100.0f,
        100.0f,
        1000.0f,
        1000.0f,
        1000.0f,
        100.0f,
        100.0f,
        1000,
        1000,
        true,
        true,
        true};
    HENKA_TEST_ASSERT(henka_workspace_layout_docked(&desc, &layout) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(layout.scene_viewport.x == 0);
    HENKA_TEST_ASSERT(layout.scene_viewport.y == 0);
    HENKA_TEST_ASSERT(layout.scene_viewport.width == 1);
    HENKA_TEST_ASSERT(layout.scene_viewport.height == 1);

    desc = (henka_workspace_desc){
        1280,
        720,
        FLT_MAX,
        FLT_MAX,
        FLT_MAX,
        FLT_MAX,
        FLT_MAX,
        FLT_MAX,
        FLT_MAX,
        INT_MAX,
        INT_MAX,
        true,
        true,
        true};
    HENKA_TEST_ASSERT(henka_workspace_layout_docked(&desc, &layout) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_viewport_is_valid(layout.scene_viewport));
    HENKA_TEST_ASSERT(layout.scene_viewport.x >= 0);
    HENKA_TEST_ASSERT(layout.scene_viewport.y >= 0);
    HENKA_TEST_ASSERT(layout.scene_viewport.x + layout.scene_viewport.width <= 1280);
    HENKA_TEST_ASSERT(layout.scene_viewport.y + layout.scene_viewport.height <= 720);
}
