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
}
