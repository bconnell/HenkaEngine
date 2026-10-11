#include "test_suite.h"

#include <math.h>
#include <string.h>

#include "../examples/sandbox3d/editor_layout.h"
#include "../examples/sandbox3d/view_compass.h"

static bool henka_test_rects_overlap(henka_ui_rect left, henka_ui_rect right)
{
    return left.x < right.x + right.width &&
        left.x + left.width > right.x &&
        left.y < right.y + right.height &&
        left.y + left.height > right.y;
}

static bool henka_test_rect_is_empty(henka_ui_rect rect)
{
    return rect.width <= 0.0f || rect.height <= 0.0f;
}

static bool henka_test_rect_is_contained(henka_ui_rect outer, henka_ui_rect inner)
{
    return henka_test_rect_is_empty(inner) ||
        (inner.x >= outer.x && inner.y >= outer.y &&
         inner.x + inner.width <= outer.x + outer.width + 0.01f &&
         inner.y + inner.height <= outer.y + outer.height + 0.01f);
}

void henka_test_sandbox3d_editor_layout(void)
{
    sandbox3d_editor_layout_metrics metrics;
    henka_ui_rect row[4];
    size_t row_count;

    {
        const float widths[] = {500.0f, 720.0f, 759.0f, 760.0f, 1280.0f, 1920.0f, 2560.0f};
        size_t width_index;

        for (width_index = 0U; width_index < sizeof(widths) / sizeof(widths[0]); ++width_index)
        {
            const henka_ui_rect frame = {17.0f, 23.0f, widths[width_index], 900.0f};
            sandbox3d_modeling_toolbar_layout collapsed;
            sandbox3d_modeling_toolbar_layout expanded;
            size_t tool_index;

            HENKA_TEST_ASSERT(
                sandbox3d_editor_layout_modeling_toolbar_compute(
                    frame, true, false, &collapsed) == HENKA_SUCCESS);
            HENKA_TEST_ASSERT(
                sandbox3d_editor_layout_modeling_toolbar_compute(
                    frame, true, true, &expanded) == HENKA_SUCCESS);
            HENKA_TEST_ASSERT(henka_test_rect_is_contained(frame, collapsed.bounds));
            HENKA_TEST_ASSERT(henka_test_rect_is_contained(frame, expanded.bounds));
            HENKA_TEST_ASSERT(henka_test_rect_is_contained(
                collapsed.bounds, collapsed.selection_mode));
            HENKA_TEST_ASSERT(henka_test_rect_is_contained(
                expanded.bounds, expanded.selection_mode));

            if (widths[width_index] < 760.0f)
            {
                const henka_vec2 empty_toolbar_gap = {
                    collapsed.bounds.x + 20.0f,
                    collapsed.bounds.y + 10.0f};
                const henka_vec2 select_mode_center = {
                    collapsed.selection_mode.x + collapsed.selection_mode.width * 0.5f,
                    collapsed.selection_mode.y + collapsed.selection_mode.height * 0.5f};
                const henka_vec2 options_center = {
                    collapsed.options_toggle.x + collapsed.options_toggle.width * 0.5f,
                    collapsed.options_toggle.y + collapsed.options_toggle.height * 0.5f};
                const henka_vec2 summary_center = {
                    collapsed.state_summary.x + collapsed.state_summary.width * 0.5f,
                    collapsed.state_summary.y + collapsed.state_summary.height * 0.5f};
                const henka_vec2 expanded_orientation_center = {
                    expanded.orientation.x + expanded.orientation.width * 0.5f,
                    expanded.orientation.y + expanded.orientation.height * 0.5f};
                const henka_vec2 expanded_pivot_center = {
                    expanded.pivot.x + expanded.pivot.width * 0.5f,
                    expanded.pivot.y + expanded.pivot.height * 0.5f};
                const henka_vec2 expanded_snap_center = {
                    expanded.tool_buttons[4].x + expanded.tool_buttons[4].width * 0.5f,
                    expanded.tool_buttons[4].y + expanded.tool_buttons[4].height * 0.5f};
                const henka_vec2 visible_tool_center = {
                    collapsed.tool_buttons[0].x + collapsed.tool_buttons[0].width * 0.5f,
                    collapsed.tool_buttons[0].y + collapsed.tool_buttons[0].height * 0.5f};

                HENKA_TEST_ASSERT(collapsed.compact);
                HENKA_TEST_ASSERT(collapsed.bounds.height == 104.0f);
                HENKA_TEST_ASSERT(expanded.bounds.height == 166.0f);
                HENKA_TEST_ASSERT(collapsed.options_toggle.width >= 96.0f);
                HENKA_TEST_ASSERT(collapsed.options_toggle.height >= 32.0f);
                HENKA_TEST_ASSERT(henka_test_rect_is_contained(
                    collapsed.bounds, collapsed.options_toggle));
                HENKA_TEST_ASSERT(henka_test_rect_is_empty(collapsed.orientation));
                HENKA_TEST_ASSERT(henka_test_rect_is_empty(collapsed.pivot));
                HENKA_TEST_ASSERT(henka_test_rect_is_empty(collapsed.tool_buttons[4]));
                HENKA_TEST_ASSERT(henka_test_rect_is_empty(collapsed.tool_buttons[5]));
                HENKA_TEST_ASSERT(
                    !sandbox3d_editor_layout_modeling_toolbar_contains_interactive_point(
                        &collapsed, empty_toolbar_gap));
                HENKA_TEST_ASSERT(
                    sandbox3d_editor_layout_modeling_toolbar_contains_interactive_point(
                        &collapsed, select_mode_center));
                HENKA_TEST_ASSERT(
                    sandbox3d_editor_layout_modeling_toolbar_contains_interactive_point(
                        &collapsed, options_center));
                HENKA_TEST_ASSERT(
                    sandbox3d_editor_layout_modeling_toolbar_contains_interactive_point(
                        &collapsed, visible_tool_center));
                HENKA_TEST_ASSERT(
                    !sandbox3d_editor_layout_modeling_toolbar_contains_interactive_point(
                        &collapsed, summary_center));
                HENKA_TEST_ASSERT(
                    !sandbox3d_editor_layout_modeling_toolbar_contains_interactive_point(
                        &collapsed, expanded_snap_center));
                HENKA_TEST_ASSERT(
                    sandbox3d_editor_layout_modeling_toolbar_contains_interactive_point(
                        &expanded, expanded_orientation_center));
                HENKA_TEST_ASSERT(
                    sandbox3d_editor_layout_modeling_toolbar_contains_interactive_point(
                        &expanded, expanded_pivot_center));
                HENKA_TEST_ASSERT(
                    sandbox3d_editor_layout_modeling_toolbar_contains_interactive_point(
                        &expanded, expanded_snap_center));
                HENKA_TEST_ASSERT(!henka_test_rect_is_empty(collapsed.state_summary));
                HENKA_TEST_ASSERT(henka_test_rect_is_contained(
                    collapsed.bounds, collapsed.state_summary));
                HENKA_TEST_ASSERT(henka_test_rect_is_contained(
                    expanded.bounds, expanded.orientation));
                HENKA_TEST_ASSERT(henka_test_rect_is_contained(
                    expanded.bounds, expanded.pivot));
                HENKA_TEST_ASSERT(henka_test_rect_is_empty(expanded.options_toggle) == false);
                HENKA_TEST_ASSERT(expanded.options_expanded);
                HENKA_TEST_ASSERT(expanded.bounds.height > collapsed.bounds.height);
                tool_index = 6U;
            }
            else
            {
                const henka_vec2 orientation_center = {
                    collapsed.orientation.x + collapsed.orientation.width * 0.5f,
                    collapsed.orientation.y + collapsed.orientation.height * 0.5f};
                const henka_vec2 pivot_center = {
                    collapsed.pivot.x + collapsed.pivot.width * 0.5f,
                    collapsed.pivot.y + collapsed.pivot.height * 0.5f};

                HENKA_TEST_ASSERT(!collapsed.compact);
                HENKA_TEST_ASSERT(collapsed.bounds.height == 136.0f);
                HENKA_TEST_ASSERT(henka_test_rect_is_empty(collapsed.options_toggle));
                HENKA_TEST_ASSERT(!henka_test_rect_is_empty(collapsed.orientation));
                HENKA_TEST_ASSERT(!henka_test_rect_is_empty(collapsed.pivot));
                HENKA_TEST_ASSERT(henka_test_rect_is_contained(
                    collapsed.bounds, collapsed.orientation));
                HENKA_TEST_ASSERT(henka_test_rect_is_contained(
                    collapsed.bounds, collapsed.pivot));
                HENKA_TEST_ASSERT(
                    sandbox3d_editor_layout_modeling_toolbar_contains_interactive_point(
                        &collapsed, orientation_center));
                HENKA_TEST_ASSERT(
                    sandbox3d_editor_layout_modeling_toolbar_contains_interactive_point(
                        &collapsed, pivot_center));
                tool_index = 6U;
            }

            for (size_t index = 0U; index < tool_index; ++index)
            {
                HENKA_TEST_ASSERT(henka_test_rect_is_contained(
                    collapsed.bounds, collapsed.tool_buttons[index]));
                HENKA_TEST_ASSERT(henka_test_rect_is_contained(
                    expanded.bounds, expanded.tool_buttons[index]));
            }
        }

        {
            sandbox3d_modeling_toolbar_layout unchanged;
            memset(&unchanged, 0, sizeof(unchanged));
            unchanged.bounds = (henka_ui_rect){1.0f, 2.0f, 3.0f, 4.0f};
            HENKA_TEST_ASSERT(
                sandbox3d_editor_layout_modeling_toolbar_compute(
                    (henka_ui_rect){0.0f, 0.0f, 499.0f, 720.0f},
                    true,
                    false,
                    &unchanged) == HENKA_ERROR_INVALID_ARGUMENT);
            HENKA_TEST_ASSERT(unchanged.bounds.x == 1.0f);
            HENKA_TEST_ASSERT(unchanged.bounds.y == 2.0f);
            HENKA_TEST_ASSERT(unchanged.bounds.width == 3.0f);
            HENKA_TEST_ASSERT(unchanged.bounds.height == 4.0f);
        }
    }

    {
        const char* face_action_labels[] = {"Bevel", "Delete Faces", "Flip"};
        henka_ui_rect controls[3] = {
            {91.0f, 92.0f, 93.0f, 94.0f},
            {95.0f, 96.0f, 97.0f, 98.0f},
            {99.0f, 100.0f, 101.0f, 102.0f}};
        henka_ui_context* ui_context = NULL;
        henka_ui_frame_desc frame_desc = {0};
        int measured_widths[3] = {0};
        int measured_height = 0;
        double required_width = 16.0;
        size_t item_index;

        HENKA_TEST_ASSERT(henka_ui_create(&ui_context) == HENKA_SUCCESS);
        frame_desc.framebuffer_width = 1280;
        frame_desc.framebuffer_height = 720;
        HENKA_TEST_ASSERT(
            henka_ui_begin_frame(ui_context, &frame_desc) == HENKA_SUCCESS);

        for (item_index = 0U; item_index < 3U; ++item_index)
        {
            HENKA_TEST_ASSERT(
                henka_ui_measure_text_for_context(
                    ui_context,
                    face_action_labels[item_index],
                    1.0f,
                    &measured_widths[item_index],
                    &measured_height) == HENKA_SUCCESS);
            required_width += fmax(
                (double)measured_widths[item_index] + 24.0,
                88.0);
        }

        row_count = 99U;
        HENKA_TEST_ASSERT(
            sandbox3d_editor_layout_text_control_row_for_context(
                ui_context,
                (henka_ui_rect){40.0f, 50.0f, (float)(required_width - 0.5), 28.0f},
                face_action_labels,
                3U,
                1.0f,
                88.0f,
                12.0f,
                8.0f,
                controls,
                3U,
                &row_count) == HENKA_ERROR_NUMERIC_RANGE);
        HENKA_TEST_ASSERT(row_count == 0U);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(controls[0].x, 91.0f, 0.0001f);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(controls[1].width, 97.0f, 0.0001f);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(controls[2].y, 100.0f, 0.0001f);

        HENKA_TEST_ASSERT(
            sandbox3d_editor_layout_text_control_row_for_context(
                ui_context,
                (henka_ui_rect){40.0f, 50.0f, (float)(required_width + 64.0), 28.0f},
                face_action_labels,
                3U,
                1.0f,
                88.0f,
                12.0f,
                8.0f,
                controls,
                3U,
                &row_count) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(row_count == 3U);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(controls[0].width, 88.0f, 0.0001f);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(controls[2].width, 88.0f, 0.0001f);
        HENKA_TEST_ASSERT(
            controls[1].width >= (float)measured_widths[1] + 24.0f);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(
            controls[1].x,
            controls[0].x + controls[0].width + 8.0f,
            0.0001f);
        HENKA_TEST_ASSERT(
            controls[0].x + controls[0].width + 8.0f <= controls[1].x);
        HENKA_TEST_ASSERT(
            controls[1].x + controls[1].width + 8.0f <= controls[2].x);
        HENKA_TEST_ASSERT(
            controls[2].x + controls[2].width <=
            40.0f + (float)(required_width + 64.0));

        {
            const char* asset_type_labels[] = {
                "Textures", "Materials", "Meshes", "Prefabs"};
            henka_ui_rect asset_tabs[4] = {
                {11.0f, 12.0f, 13.0f, 14.0f},
                {21.0f, 22.0f, 23.0f, 24.0f},
                {31.0f, 32.0f, 33.0f, 34.0f},
                {41.0f, 42.0f, 43.0f, 44.0f}};
            henka_ui_rect narrow_tabs[4] = {
                {51.0f, 52.0f, 53.0f, 54.0f},
                {61.0f, 62.0f, 63.0f, 64.0f},
                {71.0f, 72.0f, 73.0f, 74.0f},
                {81.0f, 82.0f, 83.0f, 84.0f}};
            const henka_ui_rect narrow_tabs_before[4] = {
                narrow_tabs[0], narrow_tabs[1], narrow_tabs[2], narrow_tabs[3]};
            const henka_ui_rect asset_tab_bounds = {846.0f, 286.0f, 404.0f, 24.0f};
            int label_width = 0;
            int label_height = 0;
            double required_tab_width = 24.0;
            size_t asset_tab_count = 99U;
            size_t narrow_tab_count = 99U;

            for (item_index = 0U; item_index < 4U; ++item_index)
            {
                HENKA_TEST_ASSERT(
                    henka_ui_measure_text_for_context(
                        ui_context,
                        asset_type_labels[item_index],
                        1.0f,
                        &label_width,
                        &label_height) == HENKA_SUCCESS);
                required_tab_width += (double)label_width + 16.0;
            }

            HENKA_TEST_ASSERT(
                sandbox3d_editor_layout_text_control_row_for_context(
                    ui_context,
                    asset_tab_bounds,
                    asset_type_labels,
                    4U,
                    1.0f,
                    0.0f,
                    8.0f,
                    8.0f,
                    asset_tabs,
                    4U,
                    &asset_tab_count) == HENKA_SUCCESS);
            HENKA_TEST_ASSERT(asset_tab_count == 4U);
            for (item_index = 0U; item_index < asset_tab_count; ++item_index)
            {
                HENKA_TEST_ASSERT(
                    henka_test_rect_is_contained(asset_tab_bounds, asset_tabs[item_index]));
                HENKA_TEST_ASSERT_FLOAT_CLOSE(
                    asset_tabs[item_index].height,
                    asset_tab_bounds.height,
                    0.0001f);
                HENKA_TEST_ASSERT(
                    henka_ui_measure_text_for_context(
                        ui_context,
                        asset_type_labels[item_index],
                        1.0f,
                        &label_width,
                        &label_height) == HENKA_SUCCESS);
                HENKA_TEST_ASSERT(
                    asset_tabs[item_index].width >= (float)label_width + 16.0f);
                if (item_index > 0U)
                {
                    HENKA_TEST_ASSERT_FLOAT_CLOSE(
                        asset_tabs[item_index].x,
                        asset_tabs[item_index - 1U].x +
                            asset_tabs[item_index - 1U].width + 8.0f,
                        0.0001f);
                    HENKA_TEST_ASSERT(
                        !henka_test_rects_overlap(
                            asset_tabs[item_index - 1U], asset_tabs[item_index]));
                }
            }

            HENKA_TEST_ASSERT(
                sandbox3d_editor_layout_text_control_row_for_context(
                    ui_context,
                    (henka_ui_rect){
                        asset_tab_bounds.x,
                        asset_tab_bounds.y,
                        (float)(required_tab_width - 0.5),
                        asset_tab_bounds.height},
                    asset_type_labels,
                    4U,
                    1.0f,
                    0.0f,
                    8.0f,
                    8.0f,
                    narrow_tabs,
                    4U,
                    &narrow_tab_count) == HENKA_ERROR_NUMERIC_RANGE);
            HENKA_TEST_ASSERT(narrow_tab_count == 0U);
            HENKA_TEST_ASSERT_FLOAT_CLOSE(narrow_tabs[0].x, narrow_tabs_before[0].x, 0.0001f);
            HENKA_TEST_ASSERT_FLOAT_CLOSE(narrow_tabs[1].width, narrow_tabs_before[1].width, 0.0001f);
            HENKA_TEST_ASSERT_FLOAT_CLOSE(narrow_tabs[2].y, narrow_tabs_before[2].y, 0.0001f);
            HENKA_TEST_ASSERT_FLOAT_CLOSE(narrow_tabs[3].height, narrow_tabs_before[3].height, 0.0001f);
        }

        HENKA_TEST_ASSERT(henka_ui_end_frame(ui_context) == HENKA_SUCCESS);
        henka_ui_destroy(ui_context);
    }

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

    {
        const char* long_name = "Roof Door Hinged";
        const char expected[] = "Roof\nDoor\nHinged";
        const char* long_token = "UnbrokenIdentifier";
        const char expected_token_narrow[] = "Unbro\nkenId\nentif\nier";
        const char expected_token_wide[] = "UnbrokenI\ndentifier";
        char wrapped[64] = "unchanged";
        char wrapped_token[64] = "unchanged";
        char too_small[8] = "keep";
        size_t line_count = 0U;

        HENKA_TEST_ASSERT(
            sandbox3d_editor_layout_measure_wrapped_text(
                long_name,
                6U,
                &line_count) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(line_count == 3U);
        HENKA_TEST_ASSERT(
            sandbox3d_editor_layout_wrap_text(
                long_name,
                6U,
                wrapped,
                sizeof(wrapped),
                &line_count) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(strcmp(wrapped, expected) == 0);
        HENKA_TEST_ASSERT(line_count == 3U);

        HENKA_TEST_ASSERT(
            sandbox3d_editor_layout_wrap_text(
                long_token,
                5U,
                wrapped_token,
                sizeof(wrapped_token),
                &line_count) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(strcmp(wrapped_token, expected_token_narrow) == 0);
        HENKA_TEST_ASSERT(line_count == 4U);

        HENKA_TEST_ASSERT(
            sandbox3d_editor_layout_wrap_text(
                long_token,
                9U,
                wrapped_token,
                sizeof(wrapped_token),
                &line_count) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(strcmp(wrapped_token, expected_token_wide) == 0);
        HENKA_TEST_ASSERT(line_count == 2U);

        line_count = 99U;
        HENKA_TEST_ASSERT(
            sandbox3d_editor_layout_wrap_text(
                "UnbrokenIdentifier",
                5U,
                too_small,
                4U,
                &line_count) == HENKA_ERROR_LIMIT);
        HENKA_TEST_ASSERT(strcmp(too_small, "keep") == 0);
        HENKA_TEST_ASSERT(line_count == 0U);
    }

    {
        char hidden_label[32] = "unchanged";
        char empty_name_label[32] = "unchanged";
        char multiline_name_label[40] = "unchanged";
        char narrow_alpha[24] = "unchanged";
        char narrow_beta[24] = "unchanged";
        char too_narrow[16] = "preserve";
        bool name_truncated = true;
        bool alpha_truncated = false;
        bool beta_truncated = false;

        HENKA_TEST_ASSERT(
            sandbox3d_editor_layout_format_hidden_row_label(
                "Ground",
                24U,
                hidden_label,
                sizeof(hidden_label),
                &name_truncated) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(strcmp(hidden_label, "Ground [Hidden]") == 0);
        HENKA_TEST_ASSERT(!name_truncated);
        HENKA_TEST_ASSERT(strchr(hidden_label, '\n') == NULL);

        name_truncated = true;
        HENKA_TEST_ASSERT(
            sandbox3d_editor_layout_format_hidden_row_label(
                "",
                24U,
                empty_name_label,
                sizeof(empty_name_label),
                &name_truncated) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(
            strcmp(empty_name_label, "(unnamed) [Hidden]") == 0);
        HENKA_TEST_ASSERT(!name_truncated);

        name_truncated = false;
        HENKA_TEST_ASSERT(
            sandbox3d_editor_layout_format_hidden_row_label(
                "",
                10U,
                empty_name_label,
                sizeof(empty_name_label),
                &name_truncated) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(strcmp(empty_name_label, "? [Hidden]") == 0);
        HENKA_TEST_ASSERT(name_truncated);

        name_truncated = true;
        HENKA_TEST_ASSERT(
            sandbox3d_editor_layout_format_hidden_row_label(
                "Gear\r\nAssembly",
                32U,
                multiline_name_label,
                sizeof(multiline_name_label),
                &name_truncated) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(
            strcmp(multiline_name_label, "Gear Assembly [Hidden]") == 0);
        HENKA_TEST_ASSERT(!name_truncated);

        HENKA_TEST_ASSERT(
            sandbox3d_editor_layout_format_hidden_row_label(
                "LongHiddenMeshAlpha",
                16U,
                narrow_alpha,
                sizeof(narrow_alpha),
                &alpha_truncated) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(strcmp(narrow_alpha, "Lo...ha [Hidden]") == 0);
        HENKA_TEST_ASSERT(alpha_truncated);
        HENKA_TEST_ASSERT(strlen(narrow_alpha) <= 16U);
        HENKA_TEST_ASSERT(strchr(narrow_alpha, '\n') == NULL);

        HENKA_TEST_ASSERT(
            sandbox3d_editor_layout_format_hidden_row_label(
                "LongHiddenMeshBeta",
                16U,
                narrow_beta,
                sizeof(narrow_beta),
                &beta_truncated) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(strcmp(narrow_beta, "Lo...ta [Hidden]") == 0);
        HENKA_TEST_ASSERT(beta_truncated);
        HENKA_TEST_ASSERT(strcmp(narrow_alpha, narrow_beta) != 0);

        {
            static const size_t narrow_columns[] = {10U, 11U, 12U};
            static const char* expected_labels[] = {
                "L [Hidden]",
                "L. [Hidden]",
                "Lo. [Hidden]"};
            char narrow_label[16] = "unchanged";
            size_t index;

            for (index = 0U;
                 index < sizeof(narrow_columns) / sizeof(narrow_columns[0]);
                 ++index)
            {
                name_truncated = false;
                HENKA_TEST_ASSERT(
                    sandbox3d_editor_layout_format_hidden_row_label(
                        "LongHiddenMeshAlpha",
                        narrow_columns[index],
                        narrow_label,
                        sizeof(narrow_label),
                        &name_truncated) == HENKA_SUCCESS);
                HENKA_TEST_ASSERT(
                    strcmp(narrow_label, expected_labels[index]) == 0);
                HENKA_TEST_ASSERT(name_truncated);
            }
        }

        name_truncated = true;
        HENKA_TEST_ASSERT(
            sandbox3d_editor_layout_format_hidden_row_label(
                "Ground",
                9U,
                too_narrow,
                sizeof(too_narrow),
                &name_truncated) == HENKA_ERROR_LIMIT);
        HENKA_TEST_ASSERT(strcmp(too_narrow, "preserve") == 0);
        HENKA_TEST_ASSERT(name_truncated);
    }

    {
        const size_t row_line_counts[] = {1U, 4U, 2U, 1U};
        size_t page_index = 99U;
        size_t page_count = 99U;
        size_t first_row = 99U;
        size_t visible_count = 99U;
        float row_height = 0.0f;

        HENKA_TEST_ASSERT(
            sandbox3d_editor_layout_text_row_height(
                1U,
                16.0f,
                12.0f,
                28.0f,
                &row_height) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(row_height, 28.0f, 0.0001f);
        HENKA_TEST_ASSERT(
            sandbox3d_editor_layout_clamp_row_height(
                row_height,
                28.0f,
                28.0f,
                &row_height) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(row_height, 28.0f, 0.0001f);
        HENKA_TEST_ASSERT((size_t)floor(((double)row_height - 12.0) / 16.0) == 1U);

        HENKA_TEST_ASSERT(
            sandbox3d_editor_layout_text_row_height(
                4U,
                8.0f,
                12.0f,
                28.0f,
                &row_height) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(row_height, 44.0f, 0.0001f);
        HENKA_TEST_ASSERT(
            sandbox3d_editor_layout_clamp_row_height(
                332.0f,
                72.0f,
                28.0f,
                &row_height) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(row_height, 72.0f, 0.0001f);
        HENKA_TEST_ASSERT(
            sandbox3d_editor_layout_clamp_row_height(
                44.0f,
                72.0f,
                28.0f,
                &row_height) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(row_height, 44.0f, 0.0001f);
        HENKA_TEST_ASSERT(
            sandbox3d_editor_layout_clamp_row_height(
                44.0f,
                27.0f,
                28.0f,
                &row_height) == HENKA_ERROR_NUMERIC_RANGE);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(row_height, 0.0f, 0.0001f);
        HENKA_TEST_ASSERT(
            sandbox3d_editor_layout_page_variable_rows(
                row_line_counts,
                4U,
                72.0f,
                8.0f,
                12.0f,
                28.0f,
                0U,
                &page_index,
                &page_count,
                &first_row,
                &visible_count) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(page_index == 0U);
        HENKA_TEST_ASSERT(page_count == 2U);
        HENKA_TEST_ASSERT(first_row == 0U);
        HENKA_TEST_ASSERT(visible_count == 2U);

        HENKA_TEST_ASSERT(
            sandbox3d_editor_layout_page_variable_rows(
                row_line_counts,
                4U,
                72.0f,
                8.0f,
                12.0f,
                28.0f,
                20U,
                &page_index,
                &page_count,
                &first_row,
                &visible_count) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(page_index == 1U);
        HENKA_TEST_ASSERT(page_count == 2U);
        HENKA_TEST_ASSERT(first_row == 2U);
        HENKA_TEST_ASSERT(visible_count == 2U);

        HENKA_TEST_ASSERT(
            sandbox3d_editor_layout_page_variable_rows(
                row_line_counts,
                4U,
                27.0f,
                8.0f,
                12.0f,
                28.0f,
                0U,
                &page_index,
                &page_count,
                &first_row,
                &visible_count) == HENKA_ERROR_NUMERIC_RANGE);
        HENKA_TEST_ASSERT(page_index == 0U);
        HENKA_TEST_ASSERT(page_count == 0U);
        HENKA_TEST_ASSERT(first_row == 0U);
        HENKA_TEST_ASSERT(visible_count == 0U);
    }

    {
        char bounded_text[32] = "keep";
        size_t visible_lines = 99U;
        bool truncated = false;

        HENKA_TEST_ASSERT(
            sandbox3d_editor_layout_limit_wrapped_text(
                "root\nchild\ncontinued",
                2U,
                12U,
                bounded_text,
                sizeof(bounded_text),
                &visible_lines,
                &truncated) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(strcmp(bounded_text, "root\n...") == 0);
        HENKA_TEST_ASSERT(visible_lines == 2U);
        HENKA_TEST_ASSERT(truncated);

        HENKA_TEST_ASSERT(
            sandbox3d_editor_layout_limit_wrapped_text(
                "root\nchild",
                2U,
                12U,
                bounded_text,
                sizeof(bounded_text),
                &visible_lines,
                &truncated) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(strcmp(bounded_text, "root\nchild") == 0);
        HENKA_TEST_ASSERT(visible_lines == 2U);
        HENKA_TEST_ASSERT(!truncated);

        (void)memcpy(bounded_text, "keep", sizeof("keep"));
        HENKA_TEST_ASSERT(
            sandbox3d_editor_layout_limit_wrapped_text(
                "root\nchild",
                2U,
                12U,
                bounded_text,
                5U,
                &visible_lines,
                &truncated) == HENKA_ERROR_LIMIT);
        HENKA_TEST_ASSERT(strcmp(bounded_text, "keep") == 0);
        HENKA_TEST_ASSERT(visible_lines == 0U);
        HENKA_TEST_ASSERT(!truncated);

        HENKA_TEST_ASSERT(
            sandbox3d_editor_layout_limit_wrapped_text(
                "ab\ncd\nef",
                2U,
                2U,
                bounded_text,
                sizeof(bounded_text),
                &visible_lines,
                &truncated) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(strcmp(bounded_text, "ab\n..") == 0);
        HENKA_TEST_ASSERT(visible_lines == 2U);
        HENKA_TEST_ASSERT(truncated);
    }

    {
        const size_t row_line_counts[] = {1U, 40U, 2U, 1U};
        size_t page_index = 99U;
        size_t page_count = 99U;
        size_t first_row = 99U;
        size_t visible_count = 99U;

        HENKA_TEST_ASSERT(
            sandbox3d_editor_layout_page_variable_rows(
                row_line_counts,
                4U,
                72.0f,
                8.0f,
                12.0f,
                28.0f,
                0U,
                &page_index,
                &page_count,
                &first_row,
                &visible_count) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(page_index == 0U);
        HENKA_TEST_ASSERT(page_count == 3U);
        HENKA_TEST_ASSERT(first_row == 0U);
        HENKA_TEST_ASSERT(visible_count == 1U);

        HENKA_TEST_ASSERT(
            sandbox3d_editor_layout_page_variable_rows(
                row_line_counts,
                4U,
                72.0f,
                8.0f,
                12.0f,
                28.0f,
                1U,
                &page_index,
                &page_count,
                &first_row,
                &visible_count) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(page_index == 1U);
        HENKA_TEST_ASSERT(page_count == 3U);
        HENKA_TEST_ASSERT(first_row == 1U);
        HENKA_TEST_ASSERT(visible_count == 1U);

        HENKA_TEST_ASSERT(
            sandbox3d_editor_layout_page_variable_rows(
                row_line_counts,
                4U,
                72.0f,
                8.0f,
                12.0f,
                28.0f,
                2U,
                &page_index,
                &page_count,
                &first_row,
                &visible_count) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(page_index == 2U);
        HENKA_TEST_ASSERT(page_count == 3U);
        HENKA_TEST_ASSERT(first_row == 2U);
        HENKA_TEST_ASSERT(visible_count == 2U);
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
        {
            const henka_ui_rect collapsed_toolbar =
                sandbox3d_editor_layout_modeling_toolbar_bounds(
                    frame.scene_frame,
                    true,
                    false);
            HENKA_TEST_ASSERT(collapsed_toolbar.width > 0.0f);
            HENKA_TEST_ASSERT(collapsed_toolbar.height <= 104.0f);
            HENKA_TEST_ASSERT(
                collapsed_toolbar.x >= frame.scene_frame.x &&
                collapsed_toolbar.y >= frame.scene_frame.y &&
                collapsed_toolbar.x + collapsed_toolbar.width <=
                    frame.scene_frame.x + frame.scene_frame.width + 0.01f &&
                collapsed_toolbar.y + collapsed_toolbar.height <=
                    frame.scene_frame.y + frame.scene_frame.height + 0.01f);
        }
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

        {
            sandbox3d_view_compass_preferences compass_preferences;
            sandbox3d_view_compass_layout compass_layout;
            const henka_viewport navigation_viewport =
                sandbox3d_editor_frame_layout_navigation_viewport(&frame, false, false);
            const henka_ui_rect compact_toolbar = {
                frame.scene_frame.x + 10.0f,
                frame.scene_frame.y + 76.0f,
                frame.scene_frame.width - 20.0f,
                52.0f};

            sandbox3d_view_compass_preferences_defaults(&compass_preferences);
            HENKA_TEST_ASSERT(henka_viewport_is_valid(navigation_viewport));
            HENKA_TEST_ASSERT(navigation_viewport.y > frame.scene_viewport.y);
            HENKA_TEST_ASSERT(
                sandbox3d_view_compass_compute_layout(
                    navigation_viewport,
                    &compass_preferences,
                    &compass_layout));
            HENKA_TEST_ASSERT(
                !henka_test_rects_overlap(
                    compass_layout.circle_bounds,
                    compact_toolbar));
            HENKA_TEST_ASSERT(
                !henka_test_rects_overlap(
                    compass_layout.info_bounds,
                    compact_toolbar));
        }

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
