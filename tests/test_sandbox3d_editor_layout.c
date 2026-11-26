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
    sandbox3d_editor_controls_status_layout controls_status_layout;
    sandbox3d_editor_scene_objects_layout scene_objects_layout;
    henka_ui_context* ui = NULL;
    henka_ui_rect row[4];
    size_t row_count;

    HENKA_TEST_ASSERT(henka_ui_create(&ui) == HENKA_SUCCESS);

    {
        henka_ui_context* footer_ui = NULL;
        henka_ui_frame_desc frame_desc = {0};
        sandbox3d_editor_details_footer_layout footer = {0};
        sandbox3d_editor_details_scene_actions_layout scene_actions = {0};
        const henka_ui_rect compact_panel = {
            395.0f, 12.0f, 233.0f, 155.0f};
        const henka_ui_rect wide_panel = {
            300.0f, 12.0f, 340.0f, 155.0f};
        const henka_ui_rect details_panel = {
            100.0f, 40.0f, 320.0f, 280.0f};
        const henka_ui_rect details_viewport = {
            114.0f, 78.0f, 280.0f, 228.0f};

        HENKA_TEST_ASSERT(henka_ui_create(&footer_ui) == HENKA_SUCCESS);
        frame_desc.framebuffer_width = 1280;
        frame_desc.framebuffer_height = 720;
        HENKA_TEST_ASSERT(
            henka_ui_begin_frame(footer_ui, &frame_desc) == HENKA_SUCCESS);

        /* At 200% UI scale, the 1280x720 right dock has about 233 logical
         * units of width. The shared footer layout must stack both full-size
         * project actions instead of dropping them below a width threshold. */
        HENKA_TEST_ASSERT(
            sandbox3d_editor_layout_details_footer_for_context(
                footer_ui,
                compact_panel,
                &footer) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(footer.row_count == 2U);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(footer.save_project.width, 140.0f, 0.01f);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(footer.reload_project.width, 140.0f, 0.01f);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(footer.save_project.height, 24.0f, 0.01f);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(footer.reload_project.height, 24.0f, 0.01f);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(
            footer.save_project.x, footer.reload_project.x, 0.01f);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(
            footer.reload_project.y, footer.save_project.y + 28.0f, 0.01f);
        HENKA_TEST_ASSERT(henka_test_rect_is_contained(
            compact_panel, footer.save_project));
        HENKA_TEST_ASSERT(henka_test_rect_is_contained(
            compact_panel, footer.reload_project));
        HENKA_TEST_ASSERT(!henka_test_rects_overlap(
            footer.save_project, footer.reload_project));
        HENKA_TEST_ASSERT_FLOAT_CLOSE(footer.scroll_reserve, 62.0f, 0.01f);

        HENKA_TEST_ASSERT(
            sandbox3d_editor_layout_details_footer_for_context(
                footer_ui,
                wide_panel,
                &footer) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(footer.row_count == 1U);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(
            footer.save_project.y, footer.reload_project.y, 0.01f);
        HENKA_TEST_ASSERT(
            footer.reload_project.x >= footer.save_project.x +
                footer.save_project.width);
        HENKA_TEST_ASSERT(henka_test_rect_is_contained(
            wide_panel, footer.save_project));
        HENKA_TEST_ASSERT(henka_test_rect_is_contained(
            wide_panel, footer.reload_project));

        HENKA_TEST_ASSERT(
            sandbox3d_editor_layout_details_scene_actions(
                details_panel,
                details_viewport,
                true,
                false,
                &scene_actions) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(scene_actions.visible);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(
            scene_actions.action_row.x, 114.0f, 0.01f);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(
            scene_actions.action_row.y, 78.0f, 0.01f);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(
            scene_actions.action_row.width, 292.0f, 0.01f);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(
            scene_actions.action_row.height, 26.0f, 0.01f);
        HENKA_TEST_ASSERT(!scene_actions.compact);
        HENKA_TEST_ASSERT(henka_test_rect_is_contained(
            scene_actions.action_row, scene_actions.new_scene_button));
        HENKA_TEST_ASSERT(henka_test_rect_is_contained(
            scene_actions.action_row, scene_actions.save_scene_button));
        HENKA_TEST_ASSERT(henka_test_rect_is_contained(
            scene_actions.action_row, scene_actions.save_as_button));
        HENKA_TEST_ASSERT(henka_test_rect_is_contained(
            scene_actions.action_row, scene_actions.reload_scene_button));
        HENKA_TEST_ASSERT(!henka_test_rects_overlap(
            scene_actions.new_scene_button,
            scene_actions.save_scene_button));
        HENKA_TEST_ASSERT(!henka_test_rects_overlap(
            scene_actions.save_scene_button,
            scene_actions.save_as_button));
        HENKA_TEST_ASSERT(!henka_test_rects_overlap(
            scene_actions.save_as_button,
            scene_actions.reload_scene_button));
        HENKA_TEST_ASSERT_FLOAT_CLOSE(
            scene_actions.scroll_viewport.y, 110.0f, 0.01f);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(
            scene_actions.scroll_viewport.height, 196.0f, 0.01f);
        HENKA_TEST_ASSERT(henka_test_rect_is_contained(
            details_panel, scene_actions.action_row));
        HENKA_TEST_ASSERT(henka_test_rect_is_contained(
            details_panel, scene_actions.scroll_viewport));

        HENKA_TEST_ASSERT(
            sandbox3d_editor_layout_details_scene_actions(
                compact_panel,
                (henka_ui_rect){409.0f, 50.0f, 205.0f, 100.0f},
                true,
                false,
                &scene_actions) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(scene_actions.visible);
        HENKA_TEST_ASSERT(scene_actions.compact);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(
            scene_actions.action_row.height, 56.0f, 0.01f);
        HENKA_TEST_ASSERT(scene_actions.new_scene_button.width >= 96.0f);
        HENKA_TEST_ASSERT(scene_actions.save_scene_button.width >= 96.0f);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(
            scene_actions.save_scene_button.y,
            scene_actions.new_scene_button.y,
            0.01f);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(
            scene_actions.save_as_button.y,
            scene_actions.new_scene_button.y + 30.0f,
            0.01f);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(
            scene_actions.reload_scene_button.y,
            scene_actions.save_as_button.y,
            0.01f);
        HENKA_TEST_ASSERT(!henka_test_rects_overlap(
            scene_actions.new_scene_button,
            scene_actions.save_scene_button));
        HENKA_TEST_ASSERT(!henka_test_rects_overlap(
            scene_actions.save_as_button,
            scene_actions.reload_scene_button));
        HENKA_TEST_ASSERT(henka_test_rect_is_contained(
            compact_panel, scene_actions.action_row));
        HENKA_TEST_ASSERT_FLOAT_CLOSE(
            scene_actions.scroll_viewport.y, 112.0f, 0.01f);

        HENKA_TEST_ASSERT(
            sandbox3d_editor_layout_details_scene_actions(
                details_panel,
                details_viewport,
                true,
                true,
                &scene_actions) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(scene_actions.visible);
        HENKA_TEST_ASSERT(scene_actions.save_as_open);
        HENKA_TEST_ASSERT(scene_actions.save_as_form_available);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(
            scene_actions.action_row.height, 56.0f, 0.01f);
        HENKA_TEST_ASSERT(henka_test_rect_is_contained(
            scene_actions.action_row, scene_actions.save_as_path_label));
        HENKA_TEST_ASSERT(henka_test_rect_is_contained(
            scene_actions.action_row, scene_actions.save_as_path_field));
        HENKA_TEST_ASSERT(henka_test_rect_is_contained(
            scene_actions.action_row, scene_actions.save_as_confirm_button));
        HENKA_TEST_ASSERT(henka_test_rect_is_contained(
            scene_actions.action_row, scene_actions.save_as_cancel_button));
        HENKA_TEST_ASSERT(!henka_test_rects_overlap(
            scene_actions.save_as_path_label,
            scene_actions.save_as_path_field));
        HENKA_TEST_ASSERT(!henka_test_rects_overlap(
            scene_actions.save_as_confirm_button,
            scene_actions.save_as_cancel_button));
        HENKA_TEST_ASSERT_FLOAT_CLOSE(
            scene_actions.scroll_viewport.y, 140.0f, 0.01f);

        HENKA_TEST_ASSERT(
            sandbox3d_editor_layout_details_scene_actions(
                details_panel,
                details_viewport,
                false,
                false,
                &scene_actions) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(!scene_actions.visible);
        HENKA_TEST_ASSERT(henka_test_rect_is_empty(
            scene_actions.action_row));
        HENKA_TEST_ASSERT_FLOAT_CLOSE(
            scene_actions.scroll_viewport.y, details_viewport.y, 0.01f);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(
            scene_actions.scroll_viewport.height,
            details_viewport.height,
            0.01f);

        HENKA_TEST_ASSERT(
            sandbox3d_editor_layout_details_scene_actions(
                (henka_ui_rect){100.0f, 40.0f, 320.0f, 70.0f},
                details_viewport,
                true,
                false,
                &scene_actions) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(!scene_actions.visible);
        HENKA_TEST_ASSERT(henka_test_rect_is_empty(
            scene_actions.action_row));

        HENKA_TEST_ASSERT(
            sandbox3d_editor_layout_details_scene_actions(
                details_panel,
                (henka_ui_rect){114.0f, 112.0f, 280.0f, 194.0f},
                true,
                false,
                &scene_actions) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(scene_actions.visible);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(
            scene_actions.scroll_viewport.y, 112.0f, 0.01f);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(
            scene_actions.scroll_viewport.height, 194.0f, 0.01f);

        henka_ui_destroy(footer_ui);
    }

    {
        const henka_ui_rect panel_cases[] = {
            {0.0f, 0.0f, 138.0f, 342.0f},
            {0.0f, 0.0f, 276.0f, 684.0f}
        };
        const float line_heights[] = {24.0f, 12.0f};
        const float scene_name_label_widths[] = {58.0f, 30.0f};
        size_t panel_index;

        for (panel_index = 0U;
             panel_index < sizeof(panel_cases) / sizeof(panel_cases[0]);
             ++panel_index)
        {
            const henka_ui_rect panel = panel_cases[panel_index];
            const henka_ui_rect* actions;
            const henka_ui_rect* hierarchy;
            const henka_ui_rect* footer;
            henka_ui_rect scene_name_controls[3];
            size_t control_index;
            size_t other_control_index;

            HENKA_TEST_ASSERT(
                sandbox3d_editor_layout_scene_objects_regions(
                    panel,
                    line_heights[panel_index],
                    scene_name_label_widths[panel_index],
                    0.0f,
                    &scene_objects_layout) == HENKA_SUCCESS);
            actions = &scene_objects_layout.actions_viewport;
            hierarchy = &scene_objects_layout.hierarchy_viewport;
            footer = &scene_objects_layout.page_footer;
            scene_name_controls[0] = scene_objects_layout.scene_name_label;
            scene_name_controls[1] = scene_objects_layout.scene_name_field;
            scene_name_controls[2] = scene_objects_layout.scene_name_apply;
            HENKA_TEST_ASSERT(henka_test_rect_is_contained(panel, *actions));
            HENKA_TEST_ASSERT(henka_test_rect_is_contained(panel, *hierarchy));
            HENKA_TEST_ASSERT(henka_test_rect_is_contained(panel, *footer));
            for (control_index = 0U;
                 control_index < sizeof(scene_name_controls) /
                    sizeof(scene_name_controls[0]);
                 ++control_index)
            {
                HENKA_TEST_ASSERT(!henka_test_rect_is_empty(
                    scene_name_controls[control_index]));
                HENKA_TEST_ASSERT(henka_test_rect_is_contained(
                    panel, scene_name_controls[control_index]));
                HENKA_TEST_ASSERT(!henka_test_rects_overlap(
                    scene_name_controls[control_index], *actions));
                for (other_control_index = control_index + 1U;
                     other_control_index < sizeof(scene_name_controls) /
                        sizeof(scene_name_controls[0]);
                     ++other_control_index)
                {
                    HENKA_TEST_ASSERT(!henka_test_rects_overlap(
                        scene_name_controls[control_index],
                        scene_name_controls[other_control_index]));
                }
            }
            HENKA_TEST_ASSERT(actions->height >= 48.0f);
            HENKA_TEST_ASSERT(hierarchy->height >= line_heights[panel_index] + 8.0f);
            HENKA_TEST_ASSERT(!henka_test_rects_overlap(*actions, *hierarchy));
            HENKA_TEST_ASSERT(!henka_test_rects_overlap(*hierarchy, *footer));
            HENKA_TEST_ASSERT(!henka_test_rects_overlap(*actions, *footer));
            HENKA_TEST_ASSERT(
                scene_objects_layout.scene_name_apply.y +
                    scene_objects_layout.scene_name_apply.height <=
                actions->y + 0.01f);
            if (panel_index == 0U)
            {
                HENKA_TEST_ASSERT(
                    scene_objects_layout.scene_name_label.y <
                        scene_objects_layout.scene_name_field.y);
                HENKA_TEST_ASSERT(
                    scene_objects_layout.scene_name_field.y <
                        scene_objects_layout.scene_name_apply.y);
            }
            else
            {
                HENKA_TEST_ASSERT_FLOAT_CLOSE(
                    scene_objects_layout.scene_name_label.y,
                    scene_objects_layout.scene_name_field.y,
                    0.01f);
                HENKA_TEST_ASSERT_FLOAT_CLOSE(
                    scene_objects_layout.scene_name_field.y,
                    scene_objects_layout.scene_name_apply.y,
                    0.01f);
            }
        }

        HENKA_TEST_ASSERT(
            sandbox3d_editor_layout_scene_objects_regions(
                (henka_ui_rect){0.0f, 0.0f, 220.0f, 420.0f},
                16.0f,
                60.0f,
                0.0f,
                &scene_objects_layout) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(
            scene_objects_layout.scene_name_label.y <
                scene_objects_layout.scene_name_field.y);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(
            scene_objects_layout.scene_name_field.y,
            scene_objects_layout.scene_name_apply.y,
            0.01f);

        HENKA_TEST_ASSERT(
            sandbox3d_editor_layout_scene_objects_regions(
                (henka_ui_rect){0.0f, 0.0f, 138.0f, 110.0f},
                24.0f,
                58.0f,
                0.0f,
                &scene_objects_layout) == HENKA_ERROR_LIMIT);
        HENKA_TEST_ASSERT(henka_test_rect_is_empty(
            scene_objects_layout.scene_name_label));
        HENKA_TEST_ASSERT(henka_test_rect_is_empty(
            scene_objects_layout.scene_name_field));
        HENKA_TEST_ASSERT(henka_test_rect_is_empty(
            scene_objects_layout.scene_name_apply));
        HENKA_TEST_ASSERT(henka_test_rect_is_empty(
            scene_objects_layout.actions_viewport));
        HENKA_TEST_ASSERT(henka_test_rect_is_empty(
            scene_objects_layout.hierarchy_viewport));
        HENKA_TEST_ASSERT(henka_test_rect_is_empty(
            scene_objects_layout.page_footer));
    }

    {
        const henka_ui_rect tall_panel = {0.0f, 0.0f, 300.0f, 700.0f};
        const float line_height = 14.0f;

        HENKA_TEST_ASSERT(
            sandbox3d_editor_layout_scene_objects_regions(
                tall_panel,
                line_height,
                38.0f,
                0.0f,
                &scene_objects_layout) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(
            scene_objects_layout.actions_viewport.height <=
                line_height * 14.0f + 16.0f);
        HENKA_TEST_ASSERT(
            scene_objects_layout.hierarchy_viewport.y <=
                scene_objects_layout.scene_name_apply.y +
                    scene_objects_layout.scene_name_apply.height + 8.0f +
                    line_height * 14.0f + 22.0f);
        HENKA_TEST_ASSERT(
            scene_objects_layout.hierarchy_viewport.height >
                scene_objects_layout.actions_viewport.height);
        HENKA_TEST_ASSERT(
            sandbox3d_editor_layout_scene_objects_regions(
                tall_panel,
                line_height,
                38.0f,
                180.0f,
                &scene_objects_layout) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(
            scene_objects_layout.actions_viewport.height,
            192.0f,
            0.01f);
        HENKA_TEST_ASSERT(
            scene_objects_layout.hierarchy_viewport.height > 300.0f);
        HENKA_TEST_ASSERT(
            sandbox3d_editor_layout_scene_objects_regions(
                tall_panel,
                line_height,
                38.0f,
                4096.0f,
                &scene_objects_layout) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(scene_objects_layout.hierarchy_viewport.height >= 40.0f);
        HENKA_TEST_ASSERT(
            sandbox3d_editor_layout_scene_objects_regions(
                tall_panel,
                line_height,
                38.0f,
                -1.0f,
                &scene_objects_layout) == HENKA_ERROR_LIMIT);
        HENKA_TEST_ASSERT(henka_test_rect_is_empty(
            scene_objects_layout.actions_viewport));
        HENKA_TEST_ASSERT(
            sandbox3d_editor_layout_scene_objects_regions(
                tall_panel,
                line_height,
                38.0f,
                NAN,
                &scene_objects_layout) == HENKA_ERROR_LIMIT);
        HENKA_TEST_ASSERT(henka_test_rect_is_empty(
            scene_objects_layout.actions_viewport));
    }

    {
        const henka_ui_rect panel_cases[] = {
            {20.0f, 727.5f, 320.0f, 332.5f},
            {832.0f, 442.9f, 432.0f, 261.1f}
        };
        size_t panel_index;

        for (panel_index = 0U;
             panel_index < sizeof(panel_cases) / sizeof(panel_cases[0]);
             ++panel_index)
        {
            const henka_ui_rect panel = panel_cases[panel_index];
            const float panel_bottom = panel.y + panel.height;
            henka_ui_rect footer_actions[4];
            size_t footer_index;

            HENKA_TEST_ASSERT(
                sandbox3d_editor_layout_controls_status_page(
                    panel,
                    &controls_status_layout) == HENKA_SUCCESS);
            HENKA_TEST_ASSERT(henka_test_rect_is_contained(
                panel,
                controls_status_layout.scroll_viewport));
            HENKA_TEST_ASSERT(controls_status_layout.scroll_viewport.y ==
                panel.y + 74.0f);
            HENKA_TEST_ASSERT(controls_status_layout.scroll_viewport.height > 0.0f);
            HENKA_TEST_ASSERT(controls_status_layout.scroll_viewport.y +
                controls_status_layout.scroll_viewport.height <=
                controls_status_layout.footer_save_settings.y - 7.99f);

            footer_actions[0] = controls_status_layout.footer_save_settings;
            footer_actions[1] = controls_status_layout.footer_reset_layout;
            footer_actions[2] = controls_status_layout.footer_reset_settings;
            footer_actions[3] = controls_status_layout.footer_open_legend;
            for (footer_index = 0U; footer_index < 4U; ++footer_index)
            {
                HENKA_TEST_ASSERT(henka_test_rect_is_contained(
                    panel,
                    footer_actions[footer_index]));
                HENKA_TEST_ASSERT(!henka_test_rects_overlap(
                    controls_status_layout.scroll_viewport,
                    footer_actions[footer_index]));
            }
            HENKA_TEST_ASSERT(!henka_test_rects_overlap(
                controls_status_layout.footer_save_settings,
                controls_status_layout.footer_reset_settings));
            HENKA_TEST_ASSERT(!henka_test_rects_overlap(
                controls_status_layout.footer_reset_layout,
                controls_status_layout.footer_open_legend));
            HENKA_TEST_ASSERT(
                controls_status_layout.footer_open_legend.y +
                controls_status_layout.footer_open_legend.height <=
                panel_bottom);
        }

        controls_status_layout.scroll_viewport =
            (henka_ui_rect){7.0f, 8.0f, 9.0f, 10.0f};
        HENKA_TEST_ASSERT(
            sandbox3d_editor_layout_controls_status_page(
                (henka_ui_rect){0.0f, 0.0f, 320.0f, 180.0f},
                &controls_status_layout) == HENKA_ERROR_INVALID_ARGUMENT);
        HENKA_TEST_ASSERT(henka_test_rect_is_empty(
            controls_status_layout.scroll_viewport));
    }

    {
        const henka_viewport content_viewport = {10, 20, 100, 50};
        const struct
        {
            uint32_t numerator;
            uint32_t denominator;
            henka_viewport expected;
        } cases[] = {
            {1U, 1U, {10, 20, 100, 50}},
            {5U, 4U, {13, 25, 125, 63}},
            {3U, 2U, {15, 30, 150, 75}},
            {2U, 1U, {20, 40, 200, 100}}
        };
        size_t case_index;
        henka_viewport converted = {0, 0, 0, 0};

        for (case_index = 0U;
             case_index < sizeof(cases) / sizeof(cases[0]);
             ++case_index)
        {
            HENKA_TEST_ASSERT(
                sandbox3d_editor_layout_content_viewport_to_framebuffer(
                    content_viewport,
                    100U,
                    cases[case_index].numerator,
                    cases[case_index].denominator,
                    2048,
                    1200,
                    &converted) == HENKA_SUCCESS);
            HENKA_TEST_ASSERT(converted.x == cases[case_index].expected.x);
            HENKA_TEST_ASSERT(converted.y == cases[case_index].expected.y);
            HENKA_TEST_ASSERT(converted.width == cases[case_index].expected.width);
            HENKA_TEST_ASSERT(converted.height == cases[case_index].expected.height);
        }

        HENKA_TEST_ASSERT(
            sandbox3d_editor_layout_content_viewport_to_framebuffer(
                (henka_viewport){900, 600, 120, 120},
                100U,
                3U,
                2U,
                1500,
                1000,
                &converted) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(converted.x == 1350 && converted.y == 900);
        HENKA_TEST_ASSERT(converted.width == 150 && converted.height == 100);

        converted = (henka_viewport){7, 8, 9, 10};
        HENKA_TEST_ASSERT(
            sandbox3d_editor_layout_content_viewport_to_framebuffer(
                content_viewport, 100U, 1U, 0U, 2048, 1200, &converted) ==
            HENKA_ERROR_INVALID_ARGUMENT);
        HENKA_TEST_ASSERT(converted.x == 0 && converted.y == 0 &&
            converted.width == 0 && converted.height == 0);
    }

    {
        static const uint16_t scales[] = {100U, 125U, 150U, 200U};
        static const int expected_widths[] = {1280, 1024, 854, 640};
        static const int expected_heights[] = {720, 576, 480, 360};
        size_t scale_index;
        int logical_width = 0;
        int logical_height = 0;
        henka_viewport converted = {0, 0, 0, 0};

        for (scale_index = 0U;
             scale_index < sizeof(scales) / sizeof(scales[0]);
             ++scale_index)
        {
            HENKA_TEST_ASSERT(
                sandbox3d_editor_layout_content_size_for_app_scale(
                    1280,
                    720,
                    scales[scale_index],
                    &logical_width,
                    &logical_height) == HENKA_SUCCESS);
            HENKA_TEST_ASSERT(logical_width == expected_widths[scale_index]);
            HENKA_TEST_ASSERT(logical_height == expected_heights[scale_index]);
        }

        HENKA_TEST_ASSERT(
            sandbox3d_editor_layout_content_viewport_to_framebuffer(
                (henka_viewport){0, 0, 640, 360},
                200U,
                1U,
                1U,
                1280,
                720,
                &converted) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(converted.x == 0 && converted.y == 0 &&
            converted.width == 1280 && converted.height == 720);

        HENKA_TEST_ASSERT(
            sandbox3d_editor_layout_content_viewport_to_framebuffer(
                (henka_viewport){10, 20, 100, 50},
                150U,
                3U,
                2U,
                2048,
                1200,
                &converted) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(converted.x == 23 && converted.y == 45 &&
            converted.width == 225 && converted.height == 113);

        logical_width = 7;
        logical_height = 8;
        HENKA_TEST_ASSERT(
            sandbox3d_editor_layout_content_size_for_app_scale(
                1280, 720, 110U, &logical_width, &logical_height) ==
            HENKA_ERROR_INVALID_ARGUMENT);
        HENKA_TEST_ASSERT(logical_width == 0 && logical_height == 0);
    }

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
                    ui, frame, 100U, true, false, &collapsed) == HENKA_SUCCESS);
            HENKA_TEST_ASSERT(
                sandbox3d_editor_layout_modeling_toolbar_compute(
                    ui, frame, 100U, true, true, &expanded) == HENKA_SUCCESS);
            HENKA_TEST_ASSERT(henka_test_rect_is_contained(frame, collapsed.bounds));
            HENKA_TEST_ASSERT(henka_test_rect_is_contained(frame, expanded.bounds));
            HENKA_TEST_ASSERT(henka_test_rect_is_contained(
                collapsed.bounds, collapsed.selection_mode));
            HENKA_TEST_ASSERT(henka_test_rect_is_contained(
                expanded.bounds, expanded.selection_mode));
            HENKA_TEST_ASSERT(
                collapsed.tool_count == (widths[width_index] < 760.0f ? 4U : 6U));
            HENKA_TEST_ASSERT(expanded.tool_count == 6U);
            HENKA_TEST_ASSERT(henka_test_rect_is_contained(
                collapsed.bounds, collapsed.state_summary));
            HENKA_TEST_ASSERT(henka_test_rect_is_contained(
                expanded.bounds, expanded.state_summary));
            HENKA_TEST_ASSERT(henka_test_rect_is_contained(
                expanded.bounds, expanded.orientation_label));
            HENKA_TEST_ASSERT(henka_test_rect_is_contained(
                expanded.bounds, expanded.pivot_label));
            HENKA_TEST_ASSERT(!henka_test_rects_overlap(
                expanded.orientation_label, expanded.orientation));
            HENKA_TEST_ASSERT(!henka_test_rects_overlap(
                expanded.pivot_label, expanded.pivot));

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
                HENKA_TEST_ASSERT(collapsed.bounds.height > 104.0f);
                HENKA_TEST_ASSERT(expanded.bounds.height > collapsed.bounds.height);
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
                for (size_t other_index = index + 1U;
                     other_index < tool_index;
                     ++other_index)
                {
                    HENKA_TEST_ASSERT(!henka_test_rects_overlap(
                        collapsed.tool_buttons[index],
                        collapsed.tool_buttons[other_index]));
                    HENKA_TEST_ASSERT(!henka_test_rects_overlap(
                        expanded.tool_buttons[index],
                        expanded.tool_buttons[other_index]));
                }
            }
        }

        {
            const uint16_t scales[] = {100U, 125U, 150U, 200U};
            sandbox3d_modeling_toolbar_layout rejected;
            sandbox3d_modeling_toolbar_layout exact_boundary;
            size_t scale_index;

            for (scale_index = 0U;
                 scale_index < sizeof(scales) / sizeof(scales[0]);
                 ++scale_index)
            {
                const uint16_t scale_percent = scales[scale_index];
                const float scale = (float)scale_percent / 100.0f;
                const henka_ui_rect frame = {
                    17.0f, 23.0f, 448.05f / scale, 420.0f};
                sandbox3d_modeling_toolbar_layout compact;
                size_t tool_index;

                HENKA_TEST_ASSERT(
                    sandbox3d_editor_layout_modeling_toolbar_compute(
                        ui, frame, scale_percent, true, false, &compact) ==
                    HENKA_SUCCESS);
                HENKA_TEST_ASSERT(compact.compact);
                HENKA_TEST_ASSERT(henka_test_rect_is_contained(
                    frame, compact.bounds));
                HENKA_TEST_ASSERT(henka_test_rect_is_contained(
                    compact.bounds, compact.selection_mode));
                HENKA_TEST_ASSERT(henka_test_rect_is_contained(
                    compact.bounds, compact.options_toggle));
                HENKA_TEST_ASSERT(compact.tool_count == 4U);
                HENKA_TEST_ASSERT(!henka_test_rects_overlap(
                    compact.options_toggle, compact.selection_mode));
                for (tool_index = 0U; tool_index < 4U; ++tool_index)
                {
                    int label_width = 0;
                    int label_height = 0;

                    HENKA_TEST_ASSERT(henka_test_rect_is_contained(
                        compact.bounds, compact.tool_buttons[tool_index]));
                    HENKA_TEST_ASSERT(
                        henka_ui_measure_text_for_context(
                            ui,
                            sandbox3d_modeling_toolbar_tool_labels[tool_index],
                            1.0f,
                            &label_width,
                            &label_height) == HENKA_SUCCESS);
                    (void)label_height;
                    /* henka_ui_tool_button draws its label at x + 32. Keep
                     * the full active-font advance and a small right inset
                     * inside the product-owned hit/draw rectangle. */
                    HENKA_TEST_ASSERT(
                        compact.tool_buttons[tool_index].width >=
                            36.0f + (float)label_width);
                }
                HENKA_TEST_ASSERT(
                    sandbox3d_editor_layout_modeling_toolbar_compute(
                        ui,
                        (henka_ui_rect){
                            frame.x, frame.y, frame.width - 0.1f, frame.height},
                        scale_percent,
                        true,
                        false,
                        &compact) == HENKA_ERROR_INVALID_ARGUMENT);
            }

            HENKA_TEST_ASSERT(
                sandbox3d_editor_layout_modeling_toolbar_compute(
                    ui,
                    (henka_ui_rect){17.0f, 23.0f, 224.0f, 420.0f},
                    100U,
                    true,
                    false,
                    &rejected) == HENKA_ERROR_INVALID_ARGUMENT);
            HENKA_TEST_ASSERT(
                sandbox3d_editor_layout_modeling_toolbar_compute(
                    ui,
                    (henka_ui_rect){17.0f, 23.0f, 224.0f, 420.0f},
                    200U,
                    true,
                    false,
                    &exact_boundary) == HENKA_SUCCESS);
            HENKA_TEST_ASSERT(!henka_test_rects_overlap(
                exact_boundary.options_toggle,
                exact_boundary.selection_mode));
            HENKA_TEST_ASSERT(exact_boundary.tool_columns == 2U);
            HENKA_TEST_ASSERT(exact_boundary.tool_rows == 2U);
            for (size_t tool_index = 0U; tool_index < 4U; ++tool_index)
            {
                int label_width = 0;
                int label_height = 0;

                HENKA_TEST_ASSERT(
                    henka_ui_measure_text_for_context(
                        ui,
                        sandbox3d_modeling_toolbar_tool_labels[tool_index],
                        1.0f,
                        &label_width,
                        &label_height) == HENKA_SUCCESS);
                (void)label_height;
                HENKA_TEST_ASSERT(
                    exact_boundary.tool_buttons[tool_index].width >=
                        36.0f + (float)label_width);
            }

            {
                sandbox3d_modeling_toolbar_layout expanded_at_200;

                HENKA_TEST_ASSERT(
                    sandbox3d_editor_layout_modeling_toolbar_compute(
                        ui,
                        (henka_ui_rect){17.0f, 23.0f, 224.0f, 420.0f},
                        200U,
                        true,
                        true,
                        &expanded_at_200) == HENKA_SUCCESS);
                HENKA_TEST_ASSERT(!henka_test_rects_overlap(
                    expanded_at_200.options_toggle,
                    expanded_at_200.selection_mode));
                HENKA_TEST_ASSERT(expanded_at_200.tool_columns == 2U);
                HENKA_TEST_ASSERT(expanded_at_200.tool_rows == 3U);
                for (size_t tool_index = 0U; tool_index < 6U; ++tool_index)
                {
                    int label_width = 0;
                    int label_height = 0;

                    HENKA_TEST_ASSERT(
                        henka_ui_measure_text_for_context(
                            ui,
                            sandbox3d_modeling_toolbar_tool_labels[tool_index],
                            1.0f,
                            &label_width,
                            &label_height) == HENKA_SUCCESS);
                    (void)label_height;
                    HENKA_TEST_ASSERT(
                        expanded_at_200.tool_buttons[tool_index].width >=
                            36.0f + (float)label_width);
                }
            }
            HENKA_TEST_ASSERT(
                sandbox3d_editor_layout_modeling_toolbar_compute(
                    ui,
                    (henka_ui_rect){17.0f, 23.0f, 223.99f, 420.0f},
                    200U,
                    true,
                    false,
                    &rejected) == HENKA_ERROR_INVALID_ARGUMENT);
            HENKA_TEST_ASSERT(
                sandbox3d_editor_layout_modeling_toolbar_compute(
                    ui,
                    (henka_ui_rect){17.0f, 23.0f, 800.0f, 420.0f},
                    110U,
                    true,
                    false,
                    &rejected) == HENKA_ERROR_INVALID_ARGUMENT);
        }

        {
            sandbox3d_modeling_toolbar_layout unchanged;
            memset(&unchanged, 0, sizeof(unchanged));
            unchanged.bounds = (henka_ui_rect){1.0f, 2.0f, 3.0f, 4.0f};
            HENKA_TEST_ASSERT(
                sandbox3d_editor_layout_modeling_toolbar_compute(
                    ui,
                    (henka_ui_rect){0.0f, 0.0f, 447.9f, 720.0f},
                    100U,
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
            for (item_index = 0U; item_index < 4U; ++item_index)
            {
                HENKA_TEST_ASSERT(narrow_tabs[item_index].x == narrow_tabs_before[item_index].x);
                HENKA_TEST_ASSERT(narrow_tabs[item_index].y == narrow_tabs_before[item_index].y);
                HENKA_TEST_ASSERT(
                    narrow_tabs[item_index].width == narrow_tabs_before[item_index].width);
                HENKA_TEST_ASSERT(
                    narrow_tabs[item_index].height == narrow_tabs_before[item_index].height);
            }
        }

        {
            const char* utility_labels[] = {
                "Help", "Legend", "Info", "Terrain", "Assets",
                "Paths", "Settings", "Diag", "T QA", "Physics QA"};
            henka_ui_rect utility_tabs[10];
            const henka_ui_rect utility_bounds = {
                12.0f, 38.0f, 204.0f, 100.0f};
            size_t utility_row_count = 99U;
            size_t utility_index;

            memset(utility_tabs, 0, sizeof(utility_tabs));
            HENKA_TEST_ASSERT(
                sandbox3d_editor_layout_text_control_grid_for_context(
                    ui_context,
                    utility_bounds,
                    utility_labels,
                    sizeof(utility_labels) / sizeof(utility_labels[0]),
                    1.0f,
                    0.0f,
                    12.0f,
                    8.0f,
                    24.0f,
                    0.0f,
                    utility_tabs,
                    sizeof(utility_tabs) / sizeof(utility_tabs[0]),
                    &utility_row_count) == HENKA_SUCCESS);
            HENKA_TEST_ASSERT(utility_row_count == 4U);
            for (utility_index = 0U;
                 utility_index < sizeof(utility_tabs) / sizeof(utility_tabs[0]);
                 ++utility_index)
            {
                HENKA_TEST_ASSERT(henka_test_rect_is_contained(
                    utility_bounds, utility_tabs[utility_index]));
                HENKA_TEST_ASSERT_FLOAT_CLOSE(
                    utility_tabs[utility_index].height,
                    24.0f,
                    0.0001f);
                if (utility_index > 0U)
                {
                    HENKA_TEST_ASSERT(!henka_test_rects_overlap(
                        utility_tabs[utility_index - 1U],
                        utility_tabs[utility_index]));
                    HENKA_TEST_ASSERT(
                        utility_tabs[utility_index].y >=
                            utility_tabs[utility_index - 1U].y);
                    if (utility_tabs[utility_index].y ==
                        utility_tabs[utility_index - 1U].y)
                    {
                        HENKA_TEST_ASSERT_FLOAT_CLOSE(
                            utility_tabs[utility_index].x,
                            utility_tabs[utility_index - 1U].x +
                                utility_tabs[utility_index - 1U].width +
                                8.0f,
                            0.0001f);
                    }
                }
            }

            {
                const henka_ui_rect before[10] = {
                    utility_tabs[0], utility_tabs[1], utility_tabs[2],
                    utility_tabs[3], utility_tabs[4], utility_tabs[5],
                    utility_tabs[6], utility_tabs[7], utility_tabs[8],
                    utility_tabs[9]};

                utility_row_count = 99U;
                HENKA_TEST_ASSERT(
                    sandbox3d_editor_layout_text_control_grid_for_context(
                        ui_context,
                        (henka_ui_rect){12.0f, 38.0f, 40.0f, 100.0f},
                        utility_labels,
                        sizeof(utility_labels) / sizeof(utility_labels[0]),
                        1.0f,
                        0.0f,
                        12.0f,
                        8.0f,
                        24.0f,
                        0.0f,
                        utility_tabs,
                        sizeof(utility_tabs) / sizeof(utility_tabs[0]),
                        &utility_row_count) == HENKA_ERROR_NUMERIC_RANGE);
                HENKA_TEST_ASSERT(utility_row_count == 0U);
                for (utility_index = 0U;
                     utility_index < sizeof(utility_tabs) / sizeof(utility_tabs[0]);
                     ++utility_index)
                {
                    HENKA_TEST_ASSERT(utility_tabs[utility_index].x == before[utility_index].x);
                    HENKA_TEST_ASSERT(utility_tabs[utility_index].y == before[utility_index].y);
                    HENKA_TEST_ASSERT(utility_tabs[utility_index].width == before[utility_index].width);
                    HENKA_TEST_ASSERT(utility_tabs[utility_index].height == before[utility_index].height);
                }
            }
        }

        {
            const char* face_priority_labels[] = {
                "Bevel", "Delete Faces", "Flip"};
            const char* face_edit_labels[] = {
                "Preview Extrude", "Inset", "N +", "N -"};
            const henka_ui_rect bounds = {
                48.0f, 36.0f, 230.0f, 88.0f};
            henka_ui_rect face_action_controls[4] = {{0}};
            size_t face_action_row_count = 0U;
            size_t face_action_item_index;
            size_t face_action_prior_index;
            size_t face_action_label_set;
            const char* const* label_sets[] = {
                face_priority_labels,
                face_edit_labels};
            const size_t label_counts[] = {3U, 4U};

            /* The 200% logical panel is narrower than the fixed one-row
             * action groups.  Use measured wrapping so every control remains
             * inside the same production row authority instead of omitting
             * the group or placing its last hit target past the panel edge. */
            for (face_action_label_set = 0U;
                 face_action_label_set < 2U;
                 ++face_action_label_set)
            {
                memset(face_action_controls, 0, sizeof(face_action_controls));
                face_action_row_count = 0U;
                HENKA_TEST_ASSERT(
                    sandbox3d_editor_layout_text_control_grid_for_context(
                        ui_context,
                        bounds,
                        label_sets[face_action_label_set],
                        label_counts[face_action_label_set],
                        2.0f,
                        88.0f,
                        12.0f,
                        8.0f,
                        24.0f,
                        4.0f,
                        face_action_controls,
                        sizeof(face_action_controls) /
                            sizeof(face_action_controls[0]),
                        &face_action_row_count) == HENKA_SUCCESS);
                HENKA_TEST_ASSERT(face_action_row_count > 1U);
                for (face_action_item_index = 0U;
                     face_action_item_index <
                         label_counts[face_action_label_set];
                     ++face_action_item_index)
                {
                    HENKA_TEST_ASSERT(henka_test_rect_is_contained(
                        bounds, face_action_controls[face_action_item_index]));
                    HENKA_TEST_ASSERT(
                        face_action_controls[face_action_item_index].width >=
                        88.0f);
                    if (face_action_item_index > 0U)
                    {
                        HENKA_TEST_ASSERT(
                            face_action_controls[face_action_item_index].y >=
                            face_action_controls[
                                face_action_item_index - 1U].y);
                    }
                    for (face_action_prior_index = 0U;
                         face_action_prior_index < face_action_item_index;
                         ++face_action_prior_index)
                    {
                        HENKA_TEST_ASSERT(!henka_test_rects_overlap(
                            face_action_controls[face_action_prior_index],
                            face_action_controls[face_action_item_index]));
                    }
                }
            }

            {
                const henka_ui_rect flow_bounds = {
                    48.0f, 36.0f, 230.0f, 160.0f};
                const float flow_spacing = 6.0f;
                const float action_row_height = 24.0f;
                const float action_row_gap = 4.0f;
                const float expected_row_step =
                    action_row_height + action_row_gap + flow_spacing;
                henka_ui_flow_desc flow = {0};
                henka_ui_rect visible_controls[4] = {{0}};
                henka_ui_rect scrolled_controls[4] = {{0}};
                size_t visible_rows = 0U;
                size_t scrolled_rows = 0U;
                size_t control_index;
                float visible_content_height = 0.0f;
                float scrolled_content_height = 0.0f;
                bool all_visible = false;

                /* A multi-row action grid consumes the same flow spacing as
                 * later controls. Scroll position may change visibility and
                 * coordinates, but never its reserved content extent. */
                flow.bounds = flow_bounds;
                flow.row_spacing = flow_spacing;
                HENKA_TEST_ASSERT(
                    henka_ui_flow_begin(ui_context, &flow) == HENKA_SUCCESS);
                HENKA_TEST_ASSERT(
                    sandbox3d_editor_layout_flow_text_control_grid_for_context(
                        ui_context,
                        flow_bounds,
                        flow.row_spacing,
                        face_edit_labels,
                        4U,
                        2.0f,
                        88.0f,
                        12.0f,
                        8.0f,
                        action_row_height,
                        action_row_gap,
                        visible_controls,
                        4U,
                        &visible_rows,
                        &all_visible));
                HENKA_TEST_ASSERT(visible_rows > 1U);
                HENKA_TEST_ASSERT(all_visible);
                for (control_index = 0U; control_index < 4U; ++control_index)
                {
                    const float row_offset =
                        visible_controls[control_index].y - flow_bounds.y;
                    const float row_index = roundf(row_offset / expected_row_step);
                    HENKA_TEST_ASSERT_FLOAT_CLOSE(
                        row_offset,
                        row_index * expected_row_step,
                        0.01f);
                    HENKA_TEST_ASSERT(henka_test_rect_is_contained(
                        flow_bounds, visible_controls[control_index]));
                }
                HENKA_TEST_ASSERT(
                    henka_ui_flow_end(
                        ui_context,
                        &visible_content_height) == HENKA_SUCCESS);

                flow.scroll_offset = 40.0f;
                HENKA_TEST_ASSERT(
                    henka_ui_flow_begin(ui_context, &flow) == HENKA_SUCCESS);
                all_visible = true;
                HENKA_TEST_ASSERT(
                    sandbox3d_editor_layout_flow_text_control_grid_for_context(
                        ui_context,
                        flow_bounds,
                        flow.row_spacing,
                        face_edit_labels,
                        4U,
                        2.0f,
                        88.0f,
                        12.0f,
                        8.0f,
                        action_row_height,
                        action_row_gap,
                        scrolled_controls,
                        4U,
                        &scrolled_rows,
                        &all_visible));
                HENKA_TEST_ASSERT(scrolled_rows == visible_rows);
                HENKA_TEST_ASSERT(!all_visible);
                for (control_index = 0U; control_index < 4U; ++control_index)
                {
                    HENKA_TEST_ASSERT_FLOAT_CLOSE(
                        scrolled_controls[control_index].x,
                        visible_controls[control_index].x,
                        0.01f);
                    HENKA_TEST_ASSERT_FLOAT_CLOSE(
                        scrolled_controls[control_index].y,
                        visible_controls[control_index].y -
                            flow.scroll_offset,
                        0.01f);
                    HENKA_TEST_ASSERT_FLOAT_CLOSE(
                        scrolled_controls[control_index].width,
                        visible_controls[control_index].width,
                        0.01f);
                    HENKA_TEST_ASSERT_FLOAT_CLOSE(
                        scrolled_controls[control_index].height,
                        visible_controls[control_index].height,
                        0.01f);
                }
                HENKA_TEST_ASSERT(
                    henka_ui_flow_end(
                        ui_context,
                        &scrolled_content_height) == HENKA_SUCCESS);
                HENKA_TEST_ASSERT_FLOAT_CLOSE(
                    scrolled_content_height,
                    visible_content_height,
                    0.01f);
            }
        }

        {
            const char* const material_labels[6] = {
                "Base Color", "Metallic", "Roughness",
                "Emission", "Texture", "Subsurface"};
            const henka_ui_rect narrow_material_viewport = {
                48.0f, 36.0f, 230.0f, 88.0f};
            const float flow_spacing = 6.0f;
            const float material_row_height = 28.0f;
            const float material_row_gap = 4.0f;
            const float material_row_step =
                material_row_height + material_row_gap + flow_spacing;
            henka_ui_flow_desc flow = {0};
            henka_ui_rect visible_controls[6] = {{0}};
            henka_ui_rect scrolled_controls[6] = {{0}};
            size_t visible_rows = 0U;
            size_t scrolled_rows = 0U;
            size_t visible_control_count = 0U;
            size_t hidden_control_count = 0U;
            size_t control_index;
            int measured_subsurface_width = 0;
            int measured_subsurface_height = 0;
            float visible_content_height = 0.0f;
            float scrolled_content_height = 0.0f;
            bool all_visible = true;

            flow.bounds = narrow_material_viewport;
            flow.row_spacing = flow_spacing;
            HENKA_TEST_ASSERT(
                henka_ui_flow_begin(ui_context, &flow) == HENKA_SUCCESS);
            HENKA_TEST_ASSERT(
                sandbox3d_editor_layout_flow_button_grid_for_context(
                    ui_context,
                    narrow_material_viewport,
                    flow.row_spacing,
                    material_labels,
                    6U,
                    2.0f,
                    80.0f,
                    8.0f,
                    material_row_height,
                    material_row_gap,
                    visible_controls,
                    6U,
                    &visible_rows,
                    &all_visible));
            HENKA_TEST_ASSERT(
                henka_ui_measure_text_for_context(
                    ui_context,
                    material_labels[5],
                    2.0f,
                    &measured_subsurface_width,
                    &measured_subsurface_height) == HENKA_SUCCESS);
            HENKA_TEST_ASSERT(
                visible_controls[5].width + 0.01f >=
                (float)measured_subsurface_width +
                    HENKA_UI_BUTTON_TEXT_HORIZONTAL_PADDING * 2.0f);
            HENKA_TEST_ASSERT(visible_rows > 1U);
            HENKA_TEST_ASSERT(!all_visible);
            for (control_index = 0U; control_index < 6U; ++control_index)
            {
                const henka_ui_rect control = visible_controls[control_index];
                const float row_offset = control.y - narrow_material_viewport.y;
                const float row_index = roundf(row_offset / material_row_step);
                size_t prior_index;

                HENKA_TEST_ASSERT_FLOAT_CLOSE(
                    row_offset,
                    row_index * material_row_step,
                    0.01f);
                HENKA_TEST_ASSERT(control.x >= narrow_material_viewport.x);
                HENKA_TEST_ASSERT(
                    control.x + control.width <=
                    narrow_material_viewport.x + narrow_material_viewport.width + 0.01f);
                HENKA_TEST_ASSERT_FLOAT_CLOSE(
                    control.height,
                    material_row_height,
                    0.01f);
                if (control.y >= narrow_material_viewport.y &&
                    control.y + control.height <=
                        narrow_material_viewport.y + narrow_material_viewport.height)
                {
                    ++visible_control_count;
                }
                else
                {
                    ++hidden_control_count;
                }
                for (prior_index = 0U; prior_index < control_index; ++prior_index)
                {
                    if (fabsf(
                            visible_controls[prior_index].y - control.y) < 0.01f)
                    {
                        HENKA_TEST_ASSERT(
                            visible_controls[prior_index].x +
                                visible_controls[prior_index].width <=
                            control.x + 0.01f);
                    }
                }
            }
            HENKA_TEST_ASSERT(visible_control_count > 0U);
            HENKA_TEST_ASSERT(hidden_control_count > 0U);
            HENKA_TEST_ASSERT(
                henka_ui_flow_end(
                    ui_context,
                    &visible_content_height) == HENKA_SUCCESS);

            flow.scroll_offset = material_row_step;
            HENKA_TEST_ASSERT(
                henka_ui_flow_begin(ui_context, &flow) == HENKA_SUCCESS);
            all_visible = true;
            HENKA_TEST_ASSERT(
                sandbox3d_editor_layout_flow_button_grid_for_context(
                    ui_context,
                    narrow_material_viewport,
                    flow.row_spacing,
                    material_labels,
                    6U,
                    2.0f,
                    80.0f,
                    8.0f,
                    material_row_height,
                    material_row_gap,
                    scrolled_controls,
                    6U,
                    &scrolled_rows,
                    &all_visible));
            HENKA_TEST_ASSERT(scrolled_rows == visible_rows);
            for (control_index = 0U; control_index < 6U; ++control_index)
            {
                HENKA_TEST_ASSERT_FLOAT_CLOSE(
                    scrolled_controls[control_index].x,
                    visible_controls[control_index].x,
                    0.01f);
                HENKA_TEST_ASSERT_FLOAT_CLOSE(
                    scrolled_controls[control_index].y,
                    visible_controls[control_index].y - flow.scroll_offset,
                    0.01f);
                HENKA_TEST_ASSERT_FLOAT_CLOSE(
                    scrolled_controls[control_index].width,
                    visible_controls[control_index].width,
                    0.01f);
            }
            HENKA_TEST_ASSERT(
                henka_ui_flow_end(
                    ui_context,
                    &scrolled_content_height) == HENKA_SUCCESS);
            HENKA_TEST_ASSERT_FLOAT_CLOSE(
                scrolled_content_height,
                visible_content_height,
                0.01f);
        }

        {
            henka_ui_rect narrow_tabs[
                SANDBOX3D_EDITOR_UTILITY_TAB_COUNT] = {{0}};
            henka_ui_rect wide_tabs[
                SANDBOX3D_EDITOR_UTILITY_TAB_COUNT] = {{0}};
            const henka_ui_rect narrow_panel = {
                12.0f, 38.0f, 204.0f, 240.0f};
            const henka_ui_rect wide_panel = {
                12.0f, 38.0f, 420.0f, 240.0f};
            const henka_ui_rect narrow_content = {
                narrow_panel.x + 14.0f,
                narrow_panel.y + 38.0f,
                narrow_panel.width - 28.0f,
                narrow_panel.height - 38.0f};
            const henka_ui_rect wide_content = {
                wide_panel.x + 14.0f,
                wide_panel.y + 38.0f,
                wide_panel.width - 28.0f,
                wide_panel.height - 38.0f};
            size_t narrow_rows = 0U;
            size_t wide_rows = 0U;
            float narrow_row_height = 0.0f;
            float wide_row_height = 0.0f;
            size_t tab_index;

            HENKA_TEST_ASSERT(strcmp(
                sandbox3d_editor_utility_tab_label(1U),
                "Scene Legend") == 0);
            HENKA_TEST_ASSERT(strcmp(
                sandbox3d_editor_utility_tab_label(2U),
                "Object Info") == 0);
            HENKA_TEST_ASSERT(strcmp(
                sandbox3d_editor_utility_tab_label(7U),
                "Diagnostics") == 0);
            HENKA_TEST_ASSERT(strcmp(
                sandbox3d_editor_utility_tab_label(8U),
                "Transform QA") == 0);
            HENKA_TEST_ASSERT(strcmp(
                sandbox3d_editor_utility_tab_label(9U),
                "Physics QA") == 0);
            HENKA_TEST_ASSERT(sandbox3d_editor_utility_tab_label(
                SANDBOX3D_EDITOR_UTILITY_TAB_COUNT) == NULL);

            HENKA_TEST_ASSERT(
                sandbox3d_editor_layout_utility_tabs_for_context(
                    ui_context,
                    narrow_panel,
                    1.0f,
                    narrow_tabs,
                    SANDBOX3D_EDITOR_UTILITY_TAB_COUNT,
                    &narrow_rows,
                    &narrow_row_height) == HENKA_SUCCESS);
            HENKA_TEST_ASSERT(narrow_rows > 0U);
            HENKA_TEST_ASSERT(narrow_rows <
                SANDBOX3D_EDITOR_UTILITY_TAB_COUNT);
            HENKA_TEST_ASSERT_FLOAT_CLOSE(narrow_row_height, 24.0f, 0.0001f);

            HENKA_TEST_ASSERT(
                sandbox3d_editor_layout_utility_tabs_for_context(
                    ui_context,
                    wide_panel,
                    1.0f,
                    wide_tabs,
                    SANDBOX3D_EDITOR_UTILITY_TAB_COUNT,
                    &wide_rows,
                    &wide_row_height) == HENKA_SUCCESS);
            HENKA_TEST_ASSERT(wide_rows > 0U);
            HENKA_TEST_ASSERT(wide_rows < narrow_rows);
            HENKA_TEST_ASSERT_FLOAT_CLOSE(wide_row_height, 24.0f, 0.0001f);

            for (tab_index = 0U;
                 tab_index < SANDBOX3D_EDITOR_UTILITY_TAB_COUNT;
                 ++tab_index)
            {
                int measured_width = 0;
                int utility_label_height = 0;
                size_t previous_index;

                HENKA_TEST_ASSERT(henka_ui_measure_text_for_context(
                    ui_context,
                    sandbox3d_editor_utility_tab_label(tab_index),
                    1.0f,
                    &measured_width,
                    &utility_label_height) == HENKA_SUCCESS);
                HENKA_TEST_ASSERT(measured_width > 0);
                HENKA_TEST_ASSERT(utility_label_height > 0);
                HENKA_TEST_ASSERT_FLOAT_CLOSE(
                    narrow_tabs[tab_index].width,
                    (float)measured_width + 24.0f,
                    0.0001f);
                HENKA_TEST_ASSERT(henka_test_rect_is_contained(
                    narrow_content, narrow_tabs[tab_index]));
                HENKA_TEST_ASSERT(henka_test_rect_is_contained(
                    wide_content, wide_tabs[tab_index]));

                for (previous_index = 0U;
                     previous_index < tab_index;
                     ++previous_index)
                {
                    HENKA_TEST_ASSERT(!henka_test_rects_overlap(
                        narrow_tabs[previous_index], narrow_tabs[tab_index]));
                    HENKA_TEST_ASSERT(!henka_test_rects_overlap(
                        wide_tabs[previous_index], wide_tabs[tab_index]));
                }
            }

            {
                const henka_ui_rect before[
                    SANDBOX3D_EDITOR_UTILITY_TAB_COUNT] = {
                        narrow_tabs[0], narrow_tabs[1], narrow_tabs[2],
                        narrow_tabs[3], narrow_tabs[4], narrow_tabs[5],
                        narrow_tabs[6], narrow_tabs[7], narrow_tabs[8],
                        narrow_tabs[9]};
                size_t failed_rows = 99U;
                float failed_row_height = 99.0f;

                HENKA_TEST_ASSERT(
                    sandbox3d_editor_layout_utility_tabs_for_context(
                        ui_context,
                        (henka_ui_rect){12.0f, 38.0f, 204.0f, 100.0f},
                        1.0f,
                        narrow_tabs,
                        SANDBOX3D_EDITOR_UTILITY_TAB_COUNT,
                        &failed_rows,
                        &failed_row_height) == HENKA_ERROR_NUMERIC_RANGE);
                HENKA_TEST_ASSERT(failed_rows == 0U);
                HENKA_TEST_ASSERT_FLOAT_CLOSE(
                    failed_row_height,
                    0.0f,
                    0.0001f);
                for (tab_index = 0U;
                     tab_index < SANDBOX3D_EDITOR_UTILITY_TAB_COUNT;
                     ++tab_index)
                {
                    HENKA_TEST_ASSERT(narrow_tabs[tab_index].x == before[tab_index].x);
                    HENKA_TEST_ASSERT(narrow_tabs[tab_index].y == before[tab_index].y);
                    HENKA_TEST_ASSERT(
                        narrow_tabs[tab_index].width == before[tab_index].width);
                    HENKA_TEST_ASSERT(
                        narrow_tabs[tab_index].height == before[tab_index].height);
                }
            }

            /* At 200% Editor UI Scale, 1280x720 is laid out at 640x360.
             * The narrow Utility dock must retain enough visible content for
             * the stacked scale control after all ten destinations are laid
             * out. The ordinary 24px rows currently leave only 8px here. */
            {
                henka_ui_rect compact_tabs[
                    SANDBOX3D_EDITOR_UTILITY_TAB_COUNT] = {{0}};
                const henka_ui_rect compact_panel = {
                    0.0f, 0.0f, 225.0f, 180.0f};
                size_t compact_rows = 0U;
                size_t compact_index;
                float compact_row_height = 0.0f;
                float settings_viewport_height;

                HENKA_TEST_ASSERT(
                    sandbox3d_editor_layout_utility_tabs_for_context(
                        ui_context,
                        compact_panel,
                        2.0f,
                        compact_tabs,
                        SANDBOX3D_EDITOR_UTILITY_TAB_COUNT,
                        &compact_rows,
                        &compact_row_height) == HENKA_SUCCESS);
                HENKA_TEST_ASSERT(compact_rows == 5U);
                HENKA_TEST_ASSERT(compact_row_height < 24.0f);
                HENKA_TEST_ASSERT(compact_row_height * 2.0f >= 32.0f);
                settings_viewport_height = compact_panel.height - 38.0f -
                    (float)compact_rows * compact_row_height - 14.0f;
                HENKA_TEST_ASSERT(settings_viewport_height >= 44.0f);
                for (compact_index = 0U;
                     compact_index < SANDBOX3D_EDITOR_UTILITY_TAB_COUNT;
                     ++compact_index)
                {
                    HENKA_TEST_ASSERT_FLOAT_CLOSE(
                        compact_tabs[compact_index].height,
                        compact_row_height,
                        0.0001f);
                    HENKA_TEST_ASSERT(henka_test_rect_is_contained(
                        compact_panel,
                        compact_tabs[compact_index]));
                }

                /* Compact hit targets must remain readable and large enough
                 * after the editor-scale transform. At 100%, fail closed and
                 * preserve the prior published geometry. */
                {
                    const henka_ui_rect before[
                        SANDBOX3D_EDITOR_UTILITY_TAB_COUNT] = {
                            compact_tabs[0], compact_tabs[1], compact_tabs[2],
                            compact_tabs[3], compact_tabs[4], compact_tabs[5],
                            compact_tabs[6], compact_tabs[7], compact_tabs[8],
                            compact_tabs[9]};
                    size_t rejected_rows = 99U;
                    float rejected_row_height = 99.0f;

                    HENKA_TEST_ASSERT(
                        sandbox3d_editor_layout_utility_tabs_for_context(
                            ui_context,
                            compact_panel,
                            1.0f,
                            compact_tabs,
                            SANDBOX3D_EDITOR_UTILITY_TAB_COUNT,
                            &rejected_rows,
                            &rejected_row_height) == HENKA_ERROR_LIMIT);
                    HENKA_TEST_ASSERT(rejected_rows == 0U);
                    HENKA_TEST_ASSERT_FLOAT_CLOSE(
                        rejected_row_height,
                        0.0f,
                        0.0001f);
                    for (compact_index = 0U;
                         compact_index < SANDBOX3D_EDITOR_UTILITY_TAB_COUNT;
                         ++compact_index)
                    {
                        HENKA_TEST_ASSERT(compact_tabs[compact_index].x ==
                            before[compact_index].x);
                        HENKA_TEST_ASSERT(compact_tabs[compact_index].y ==
                            before[compact_index].y);
                        HENKA_TEST_ASSERT(compact_tabs[compact_index].width ==
                            before[compact_index].width);
                        HENKA_TEST_ASSERT(compact_tabs[compact_index].height ==
                            before[compact_index].height);
                    }
                }
            }
        }
        {
            const char* scene_action_labels[] = {
                "Add Cube", "Clone", "Delete"};
            henka_ui_rect scene_action_rects[3] = {{0}};
            size_t scene_action_rows = 0U;
            float first_row_width = 0.0f;
            size_t action_index;

            for (action_index = 0U; action_index < 3U; ++action_index)
            {
                int label_width = 0;
                int label_height = 0;

                HENKA_TEST_ASSERT(
                    henka_ui_measure_text_for_context(
                        ui_context,
                        scene_action_labels[action_index],
                        1.0f,
                        &label_width,
                        &label_height) == HENKA_SUCCESS);
                HENKA_TEST_ASSERT(label_width > 0);
                if (action_index < 2U)
                {
                    first_row_width += (float)label_width + 24.0f;
                    if (action_index > 0U)
                    {
                        first_row_width += 6.0f;
                    }
                }
            }

            scene_action_rows = 99U;
            HENKA_TEST_ASSERT(
                sandbox3d_editor_layout_text_control_grid_for_context(
                    ui_context,
                    (henka_ui_rect){
                        20.0f,
                        40.0f,
                        first_row_width + 0.5f,
                        54.0f},
                    scene_action_labels,
                    3U,
                    1.0f,
                    0.0f,
                    12.0f,
                    6.0f,
                    24.0f,
                    6.0f,
                    scene_action_rects,
                    3U,
                    &scene_action_rows) == HENKA_SUCCESS);
            HENKA_TEST_ASSERT(scene_action_rows == 2U);
            HENKA_TEST_ASSERT_FLOAT_CLOSE(
                scene_action_rects[0].y,
                scene_action_rects[1].y,
                0.0001f);
            HENKA_TEST_ASSERT_FLOAT_CLOSE(
                scene_action_rects[2].y,
                scene_action_rects[0].y + 30.0f,
                0.0001f);
            for (action_index = 0U; action_index < 3U; ++action_index)
            {
                HENKA_TEST_ASSERT(henka_test_rect_is_contained(
                    (henka_ui_rect){
                        20.0f,
                        40.0f,
                        first_row_width + 0.5f,
                        54.0f},
                    scene_action_rects[action_index]));
                if (action_index > 0U)
                {
                    HENKA_TEST_ASSERT(!henka_test_rects_overlap(
                        scene_action_rects[action_index - 1U],
                        scene_action_rects[action_index]));
                }
            }
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
            sandbox3d_editor_layout_text_control_row_for_context(
                ui,
                (henka_ui_rect){100.0f, 40.0f, 360.0f, 32.0f},
                labels,
                3U,
                1.0f,
                0.0f,
                12.0f,
                8.0f,
                controls,
                3U,
                &row_count) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(row_count == 3U);
        HENKA_TEST_ASSERT(
            henka_ui_measure_text_for_context(ui, labels[2], 1.0f, &label_width, &label_height) == HENKA_SUCCESS);
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
            sandbox3d_editor_layout_text_control_row_for_context(
                ui,
                (henka_ui_rect){20.0f, 40.0f, 280.0f, 24.0f},
                labels,
                5U,
                1.0f,
                0.0f,
                8.0f,
                4.0f,
                controls,
                5U,
                &row_count) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(row_count == 5U);
        HENKA_TEST_ASSERT(
            henka_ui_measure_text_for_context(ui, labels[2], 1.0f, &label_width, &label_height) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(controls[2].width >= (float)label_width + 16.0f);
        HENKA_TEST_ASSERT(controls[4].x + controls[4].width <= 300.0f);
        HENKA_TEST_ASSERT(controls[3].x + controls[3].width + 4.0f <= controls[4].x);
    }

    {
        const char* labels[] = {"A very long inspector property label"};
        henka_ui_rect control = {91.0f, 92.0f, 93.0f, 94.0f};

        HENKA_TEST_ASSERT(
            sandbox3d_editor_layout_text_control_row_for_context(
                ui,
                (henka_ui_rect){12.0f, 24.0f, 64.0f, 28.0f},
                labels,
                1U,
                1.0f,
                0.0f,
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
                ui,
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
        HENKA_TEST_ASSERT(frame.scene_header.height >= 30.0f);
        HENKA_TEST_ASSERT(
            frame.scene_viewport.y >= (int)ceilf(
                frame.scene_frame.y + frame.scene_header.height + 8.0f));
        {
            const henka_ui_rect collapsed_toolbar =
                sandbox3d_editor_layout_modeling_toolbar_bounds(
                    ui,
                    frame.scene_frame,
                    100U,
                    true,
                    false);
            HENKA_TEST_ASSERT(collapsed_toolbar.width > 0.0f);
            HENKA_TEST_ASSERT(collapsed_toolbar.height > 0.0f);
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

        /* At 200% app scale a 1280x720 drawable is laid out in 640x360
         * logical units. The narrow Scene View still needs a reserved header
         * tall enough to expose its reflowed context, Tools, and shading rows. */
        HENKA_TEST_ASSERT(
            sandbox3d_editor_frame_layout_build(
                ui,
                &workspace,
                &visibility,
                640,
                360,
                &frame) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(sandbox3d_editor_frame_layout_is_valid(&frame));
        HENKA_TEST_ASSERT(frame.scene_viewport.height > 0);
        HENKA_TEST_ASSERT(frame.scene_header.narrow);
        HENKA_TEST_ASSERT(frame.scene_header.context_uses_buttons);
        HENKA_TEST_ASSERT(frame.scene_header.context_button_count == 3U);
        HENKA_TEST_ASSERT(frame.scene_header.shading_button_count == 4U);
        HENKA_TEST_ASSERT(
            frame.scene_viewport.y >= (int)ceilf(
                frame.scene_frame.y + frame.scene_header.height + 8.0f));
        HENKA_TEST_ASSERT(
            frame.scene_viewport.y + frame.scene_viewport.height <=
                (int)ceilf(frame.scene_frame.y + frame.scene_frame.height));
        {
            henka_ui_rect header_controls[8];
            static const char* const header_labels[8] = {
                "Tools",
                "Build",
                "Game",
                "World",
                "Wire",
                "Solid",
                "Material",
                "Rendered"};
            size_t control_index;
            size_t other_index;
            header_controls[0] = frame.scene_header.tools_button;
            for (control_index = 0U; control_index < 3U; ++control_index)
            {
                header_controls[1U + control_index] =
                    frame.scene_header.context_buttons[control_index];
            }
            for (control_index = 0U; control_index < 4U; ++control_index)
            {
                header_controls[4U + control_index] =
                    frame.scene_header.shading_buttons[control_index];
            }
            for (control_index = 0U; control_index < 8U; ++control_index)
            {
                int measured_width = 0;
                int measured_height = 0;

                HENKA_TEST_ASSERT(henka_test_rect_is_contained(
                    frame.scene_frame, header_controls[control_index]));
                HENKA_TEST_ASSERT(
                    header_controls[control_index].y +
                        header_controls[control_index].height <=
                        frame.scene_frame.y + frame.scene_header.height +
                            0.01f);
                HENKA_TEST_ASSERT(
                    henka_ui_measure_text_for_context(
                        ui,
                        header_labels[control_index],
                        1.0f,
                        &measured_width,
                        &measured_height) == HENKA_SUCCESS);
                /* henka_ui_tab fits text inside 10 logical pixels of
                 * horizontal padding on each side. The layout must reserve
                 * that same measured content width, including at scaled UI
                 * sizes, so the renderer does not ellipsize a complete label. */
                HENKA_TEST_ASSERT(
                    header_controls[control_index].width + 0.01f >=
                    (float)measured_width +
                        2.0f * HENKA_UI_TAB_TEXT_HORIZONTAL_PADDING);
                for (other_index = control_index + 1U;
                     other_index < 8U;
                     ++other_index)
                {
                    HENKA_TEST_ASSERT(!henka_test_rects_overlap(
                        header_controls[control_index],
                        header_controls[other_index]));
                }
            }
        }

        HENKA_TEST_ASSERT(
            sandbox3d_editor_frame_layout_build(
                ui,
                &workspace,
                &visibility,
                1280,
                720,
                &frame) == HENKA_SUCCESS);
        {
            sandbox3d_view_compass_preferences compass_preferences;
            sandbox3d_view_compass_layout compass_layout;
            const henka_viewport navigation_viewport =
                sandbox3d_editor_frame_layout_navigation_viewport(
                    ui, &frame, false, false, 100U);
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
                ui,
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
                ui,
                NULL,
                &visibility,
                1280,
                720,
                &frame) == HENKA_ERROR_INVALID_ARGUMENT);
    }

    henka_ui_destroy(ui);
}
