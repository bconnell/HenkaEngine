#include "test_suite.h"

#include "../examples/sandbox3d/editor_layout.h"

void henka_test_sandbox3d_editor_layout(void)
{
    sandbox3d_editor_layout_metrics metrics;
    henka_ui_rect row[4];
    size_t row_count;

    HENKA_TEST_ASSERT(
        sandbox3d_editor_layout_metrics_for_framebuffer(
            1024, 768, &metrics) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(
        metrics.breakpoint == SANDBOX3D_EDITOR_LAYOUT_NARROW);
    HENKA_TEST_ASSERT(metrics.stack_sidebars);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(metrics.minimum_hit_target, 32.0f, 0.0001f);

    HENKA_TEST_ASSERT(
        sandbox3d_editor_layout_metrics_for_framebuffer(
            1280, 720, &metrics) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(
        metrics.breakpoint == SANDBOX3D_EDITOR_LAYOUT_MEDIUM);
    HENKA_TEST_ASSERT(!metrics.stack_sidebars);

    HENKA_TEST_ASSERT(
        sandbox3d_editor_layout_metrics_for_framebuffer(
            1600, 900, &metrics) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(
        metrics.breakpoint == SANDBOX3D_EDITOR_LAYOUT_WIDE);

    HENKA_TEST_ASSERT(
        sandbox3d_editor_layout_metrics_for_framebuffer(
            2560, 1440, &metrics) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(
        metrics.breakpoint == SANDBOX3D_EDITOR_LAYOUT_WIDE);
    HENKA_TEST_ASSERT(metrics.sidebar_width == 304.0f);
    HENKA_TEST_ASSERT(metrics.sidebar_width > 260.0f);

    HENKA_TEST_ASSERT(
        sandbox3d_editor_layout_metrics_for_framebuffer(
            1920, 1080, &metrics) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(
        metrics.breakpoint == SANDBOX3D_EDITOR_LAYOUT_WIDE);

    row_count = 0U;
    HENKA_TEST_ASSERT(
        sandbox3d_editor_layout_tool_row(
            (henka_ui_rect){10.0f, 20.0f, 220.0f, 32.0f},
            4U,
            32.0f,
            4.0f,
            row,
            4U,
            &row_count) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(row_count == 4U);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(row[0].x, 10.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(row[0].width, 52.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(row[3].x, 178.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(row[3].width, 52.0f, 0.0001f);

    HENKA_TEST_ASSERT(
        sandbox3d_editor_layout_tool_row(
            (henka_ui_rect){0.0f, 0.0f, 100.0f, 32.0f},
            4U,
            32.0f,
            4.0f,
            row,
            4U,
            &row_count) == HENKA_ERROR_NUMERIC_RANGE);
    HENKA_TEST_ASSERT(row_count == 0U);

    {
        const char* labels[] = {"Scene objects", "Save Asset", "Reset Settings"};
        henka_ui_rect controls[3] = {{0}};
        int label_width;
        int label_height;

        HENKA_TEST_ASSERT(
            sandbox3d_editor_layout_text_control_row(
                (henka_ui_rect){100.0f, 40.0f, 360.0f, 32.0f},
                labels,
                3U,
                1.0f,
                12.0f,
                8.0f,
                controls,
                3U,
                &row_count) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(row_count == 3U);
        HENKA_TEST_ASSERT(
            henka_ui_measure_text(labels[2], 1.0f, &label_width, &label_height) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(controls[2].width >= (float)label_width + 24.0f);
        HENKA_TEST_ASSERT(controls[2].x + controls[2].width <= 460.0f);
        HENKA_TEST_ASSERT(controls[0].x + controls[0].width + 8.0f <= controls[1].x);
    }

    {
        const char* labels[] = {"Prev", "Next", "Channel", "Restore", "Clear"};
        henka_ui_rect controls[5] = {{0}};
        int label_width;
        int label_height;

        HENKA_TEST_ASSERT(
            sandbox3d_editor_layout_text_control_row(
                (henka_ui_rect){20.0f, 40.0f, 280.0f, 24.0f},
                labels,
                5U,
                1.0f,
                8.0f,
                4.0f,
                controls,
                5U,
                &row_count) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(row_count == 5U);
        HENKA_TEST_ASSERT(
            henka_ui_measure_text(labels[2], 1.0f, &label_width, &label_height) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(controls[2].width >= (float)label_width + 16.0f);
        HENKA_TEST_ASSERT(controls[4].x + controls[4].width <= 300.0f);
        HENKA_TEST_ASSERT(controls[3].x + controls[3].width + 4.0f <= controls[4].x);
    }

    {
        const char* labels[] = {"A very long inspector property label"};
        henka_ui_rect control = {91.0f, 92.0f, 93.0f, 94.0f};

        HENKA_TEST_ASSERT(
            sandbox3d_editor_layout_text_control_row(
                (henka_ui_rect){12.0f, 24.0f, 64.0f, 28.0f},
                labels,
                1U,
                1.0f,
                12.0f,
                0.0f,
                &control,
                1U,
                &row_count) == HENKA_ERROR_NUMERIC_RANGE);
        HENKA_TEST_ASSERT(row_count == 0U);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(control.x, 91.0f, 0.0001f);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(control.width, 93.0f, 0.0001f);
    }

    HENKA_TEST_ASSERT(
        sandbox3d_editor_layout_metrics_for_framebuffer(
            0, 720, &metrics) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(
        sandbox3d_editor_layout_tool_row(
            (henka_ui_rect){0.0f, 0.0f, 200.0f, 32.0f},
            4U,
            32.0f,
            4.0f,
            row,
            3U,
            &row_count) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(row_count == 0U);

    {
        sandbox3d_workspace_model workspace;
        sandbox3d_editor_layout_visibility visibility = {
            true,
            true,
            true,
            true,
            true,
            true};
        sandbox3d_editor_frame_layout frame;
        henka_ui_rect scene_objects_panel;
        henka_ui_rect object_details_panel;

        sandbox3d_workspace_model_reset(&workspace);
        HENKA_TEST_ASSERT(
            sandbox3d_editor_frame_layout_build(
                &workspace,
                &visibility,
                1280,
                720,
                &frame) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(sandbox3d_editor_frame_layout_is_valid(&frame));
        HENKA_TEST_ASSERT(frame.scene_frame.width > 0.0f);
        HENKA_TEST_ASSERT(frame.scene_frame.height > 0.0f);
        HENKA_TEST_ASSERT(frame.scene_viewport.width > 0);
        HENKA_TEST_ASSERT(frame.scene_viewport.height > 0);
        scene_objects_panel = sandbox3d_editor_frame_layout_panel_rect(
            &frame,
            SANDBOX3D_WORKSPACE_PANEL_SCENE_OBJECTS);
        object_details_panel = sandbox3d_editor_frame_layout_panel_rect(
            &frame,
            SANDBOX3D_WORKSPACE_PANEL_OBJECT_DETAILS);
        HENKA_TEST_ASSERT(scene_objects_panel.width > 0.0f);
        HENKA_TEST_ASSERT(object_details_panel.width > 0.0f);
        HENKA_TEST_ASSERT(scene_objects_panel.x + scene_objects_panel.width <= 1280.01f);
        HENKA_TEST_ASSERT(object_details_panel.x + object_details_panel.width <= 1280.01f);

        visibility.docked_content_visible = false;
        HENKA_TEST_ASSERT(
            sandbox3d_editor_frame_layout_build(
                &workspace,
                &visibility,
                1280,
                720,
                &frame) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(frame.scene_frame.width > 0.0f);
        HENKA_TEST_ASSERT(
            sandbox3d_editor_frame_layout_panel_rect(
                &frame,
                SANDBOX3D_WORKSPACE_PANEL_SCENE_OBJECTS).width == 0.0f);
        HENKA_TEST_ASSERT(
            sandbox3d_editor_frame_layout_build(
                NULL,
                &visibility,
                1280,
                720,
                &frame) == HENKA_ERROR_INVALID_ARGUMENT);
    }
}
