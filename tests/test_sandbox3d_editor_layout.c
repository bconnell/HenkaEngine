#include "test_suite.h"

#include <string.h>

#include "../examples/sandbox3d/editor_layout.h"
#include "../examples/sandbox3d/view_compass.h"

extern henka_viewport sandbox3d_editor_frame_layout_navigation_viewport(
    const sandbox3d_editor_frame_layout* layout,
    bool authoring_available);

static bool henka_test_rects_overlap(henka_ui_rect left, henka_ui_rect right)
{
    return left.x < right.x + right.width &&
        left.x + left.width > right.x &&
        left.y < right.y + right.height &&
        left.y + left.height > right.y;
}

static void henka_test_modeling_orientation_labels_fit_wide_display(void)
{
    static const char* const labels[] = {"World", "Local", "Norm."};
    const float prior_selector_width = 142.0f;
    const float selector_width =
        SANDBOX3D_EDITOR_MODELING_ORIENTATION_SELECTOR_WIDTH;
    henka_ui_context* ui = NULL;
    henka_ui_frame_desc frame_desc = {0};
    float required_segment_width = 0.0f;
    size_t index;

    HENKA_TEST_ASSERT(henka_ui_create(&ui) == HENKA_SUCCESS);
    frame_desc.framebuffer_width = 2560;
    frame_desc.framebuffer_height = 1440;
    HENKA_TEST_ASSERT(henka_ui_begin_frame(ui, &frame_desc) == HENKA_SUCCESS);
    for (index = 0U; index < sizeof(labels) / sizeof(labels[0]); ++index)
    {
        int label_width = 0;
        int label_height = 0;

        HENKA_TEST_ASSERT(
            henka_ui_measure_text_for_context(
                ui,
                labels[index],
                1.0f,
                &label_width,
                &label_height) == HENKA_SUCCESS);
        if ((float)label_width + 16.0f > required_segment_width)
        {
            required_segment_width = (float)label_width + 16.0f;
        }
    }
    HENKA_TEST_ASSERT(required_segment_width > prior_selector_width / 3.0f);
    HENKA_TEST_ASSERT(required_segment_width <= selector_width / 3.0f);
    HENKA_TEST_ASSERT(henka_ui_end_frame(ui) == HENKA_SUCCESS);

    frame_desc.framebuffer_width = 1280;
    frame_desc.framebuffer_height = 720;
    HENKA_TEST_ASSERT(henka_ui_begin_frame(ui, &frame_desc) == HENKA_SUCCESS);
    for (index = 0U; index < sizeof(labels) / sizeof(labels[0]); ++index)
    {
        int label_width = 0;
        int label_height = 0;

        HENKA_TEST_ASSERT(
            henka_ui_measure_text_for_context(
                ui,
                labels[index],
                1.0f,
                &label_width,
                &label_height) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT((float)label_width + 16.0f <= selector_width / 3.0f);
    }
    HENKA_TEST_ASSERT(henka_ui_end_frame(ui) == HENKA_SUCCESS);
    henka_ui_destroy(ui);
}

static void henka_test_modeling_selection_and_pivot_labels_fit_wide_display(void)
{
    static const char* const selection_labels[] = {"Vertex", "Edge", "Face"};
    static const char* const pivot_labels[] = {"Median", "Active", "Indiv."};
    static const char* const control_ids[] = {
        "modeling_label_fit_selection",
        "modeling_label_fit_pivot"};
    static const int framebuffer_sizes[][2] = {
        {1280, 720},
        {1920, 1080},
        {2560, 1440}};
    const float production_selector_width =
        SANDBOX3D_EDITOR_MODELING_SELECTOR_WIDTH;
    henka_ui_context* ui = NULL;
    henka_ui_frame_desc frame_desc = {0};
    size_t size_index;

    HENKA_TEST_ASSERT(henka_ui_create(&ui) == HENKA_SUCCESS);
    henka_ui_set_visible(ui, true);
    for (size_index = 0U;
        size_index < sizeof(framebuffer_sizes) / sizeof(framebuffer_sizes[0]);
        ++size_index)
    {
        const char* const* label_groups[] = {selection_labels, pivot_labels};
        const size_t label_counts[] = {
            sizeof(selection_labels) / sizeof(selection_labels[0]),
            sizeof(pivot_labels) / sizeof(pivot_labels[0])};
        size_t group_index;

        frame_desc.framebuffer_width = framebuffer_sizes[size_index][0];
        frame_desc.framebuffer_height = framebuffer_sizes[size_index][1];
        HENKA_TEST_ASSERT(henka_ui_begin_frame(ui, &frame_desc) == HENKA_SUCCESS);
        for (group_index = 0U;
            group_index < sizeof(label_groups) / sizeof(label_groups[0]);
            ++group_index)
        {
            float required_segment_width = 0.0f;
            size_t label_index;

            for (label_index = 0U; label_index < label_counts[group_index]; ++label_index)
            {
                int label_width = 0;
                int label_height = 0;

                HENKA_TEST_ASSERT(
                    henka_ui_measure_text_for_context(
                        ui,
                        label_groups[group_index][label_index],
                        1.0f,
                        &label_width,
                        &label_height) == HENKA_SUCCESS);
                if ((float)label_width + 16.0f > required_segment_width)
                {
                    required_segment_width = (float)label_width + 16.0f;
                }
            }
            HENKA_TEST_ASSERT(
                required_segment_width * (float)label_counts[group_index] <=
                production_selector_width);
            {
                size_t selected_index = 0U;
                bool changed = false;
                HENKA_TEST_ASSERT(
                    henka_ui_segmented_select(
                        ui,
                        control_ids[group_index],
                        (henka_ui_rect){
                            (float)group_index * (production_selector_width + 8.0f),
                            24.0f,
                            production_selector_width,
                            22.0f},
                        label_groups[group_index],
                        label_counts[group_index],
                        &selected_index,
                        &changed) == HENKA_SUCCESS);
            }
        }
        HENKA_TEST_ASSERT(henka_ui_end_frame(ui) == HENKA_SUCCESS);
    }
    henka_ui_destroy(ui);
}

static void henka_test_compact_modeling_toolbar_breakpoint_preserves_label_clearance(void)
{
    sandbox3d_editor_frame_layout layout = {0};
    henka_ui_rect toolbar;
    const float required_horizontal_width =
        SANDBOX3D_EDITOR_MODELING_WIDE_ORIENTATION_X +
        SANDBOX3D_EDITOR_MODELING_ORIENTATION_SELECTOR_WIDTH + 16.0f;

    HENKA_TEST_ASSERT(
        SANDBOX3D_EDITOR_MODELING_TOOLBAR_COMPACT_ROW_WIDTH >=
        required_horizontal_width);
    /* The 1280x720 workspace leaves exactly 576 px for this toolbar. Keep
     * both complete selectors on the shared row instead of spending another
     * 30 px of Scene View height on the narrow stacked arrangement. */
    HENKA_TEST_ASSERT(required_horizontal_width <= 576.0f);

    layout.scene_frame = (henka_ui_rect){335.0f, 47.0f, 596.0f, 620.0f};
    layout.scene_viewport = (henka_viewport){343, 85, 580, 582};
    toolbar = sandbox3d_editor_layout_modeling_toolbar_bounds(&layout, true);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(toolbar.width, 576.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(toolbar.height, 136.0f, 0.0001f);

    layout.scene_frame.width = 608.0f;
    layout.scene_viewport.width = 592;
    toolbar = sandbox3d_editor_layout_modeling_toolbar_bounds(&layout, true);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(toolbar.width, 588.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(toolbar.height, 136.0f, 0.0001f);
}

static void henka_test_scene_view_title_clears_wide_header_controls(void)
{
    henka_ui_context* ui = NULL;
    henka_ui_frame_desc frame_desc = {0};
    int title_width = 0;
    int title_height = 0;
    float controls_start = -1.0f;

    HENKA_TEST_ASSERT(henka_ui_create(&ui) == HENKA_SUCCESS);
    frame_desc.framebuffer_width = 2560;
    frame_desc.framebuffer_height = 1440;
    HENKA_TEST_ASSERT(henka_ui_begin_frame(ui, &frame_desc) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(
        henka_ui_measure_text_for_context(
            ui,
            "Scene View",
            1.25f,
            &title_width,
            &title_height) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(
        sandbox3d_editor_layout_nonoverlapping_horizontal_offset(
            1280.0f,
            112.0f,
            12.0f,
            (float)title_width,
            8.0f,
            362.0f,
            &controls_start) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(controls_start >= (float)title_width + 20.0f);
    HENKA_TEST_ASSERT(controls_start >= 112.0f);
    {
        float unchanged_offset = 7.0f;
        HENKA_TEST_ASSERT(
            sandbox3d_editor_layout_nonoverlapping_horizontal_offset(
                controls_start + 361.0f,
                112.0f,
                12.0f,
                (float)title_width,
                8.0f,
                362.0f,
                &unchanged_offset) == HENKA_ERROR_NUMERIC_RANGE);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(unchanged_offset, 7.0f, 0.0001f);
    }
    HENKA_TEST_ASSERT(henka_ui_end_frame(ui) == HENKA_SUCCESS);
    henka_ui_destroy(ui);
}

static void henka_test_modeling_summary_clears_wide_empty_state_prompt(void)
{
    henka_ui_context* ui = NULL;
    henka_ui_frame_desc frame_desc = {0};
    int summary_width = 0;
    int summary_height = 0;
    int prompt_width = 0;
    int prompt_height = 0;
    const float legacy_prompt_offset = 302.0f;
    const float available_width = 1200.0f;
    float prompt_offset = -1.0f;

    HENKA_TEST_ASSERT(henka_ui_create(&ui) == HENKA_SUCCESS);
    frame_desc.framebuffer_width = 2560;
    frame_desc.framebuffer_height = 1440;
    HENKA_TEST_ASSERT(henka_ui_begin_frame(ui, &frame_desc) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(
        henka_ui_measure_text_for_context(
            ui,
            "Face ? Select ? World ? Median ? 0 selected",
            0.82f,
            &summary_width,
            &summary_height) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(
        henka_ui_measure_text_for_context(
            ui,
            "Select an editable asset to begin.",
            0.78f,
            &prompt_width,
            &prompt_height) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(
        (float)summary_width + 12.0f > legacy_prompt_offset);
    HENKA_TEST_ASSERT(
        sandbox3d_editor_layout_nonoverlapping_horizontal_offset(
            available_width,
            legacy_prompt_offset,
            0.0f,
            (float)summary_width,
            12.0f,
            (float)prompt_width,
            &prompt_offset) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(prompt_offset >= (float)summary_width + 12.0f);
    HENKA_TEST_ASSERT(prompt_offset + (float)prompt_width <= available_width);
    {
        float unchanged_offset = 41.0f;

        HENKA_TEST_ASSERT(
            sandbox3d_editor_layout_nonoverlapping_horizontal_offset(
                prompt_offset + (float)prompt_width - 1.0f,
                legacy_prompt_offset,
                0.0f,
                (float)summary_width,
                12.0f,
                (float)prompt_width,
                &unchanged_offset) == HENKA_ERROR_NUMERIC_RANGE);
        HENKA_TEST_ASSERT(unchanged_offset == 41.0f);
    }
    HENKA_TEST_ASSERT(henka_ui_end_frame(ui) == HENKA_SUCCESS);
    henka_ui_destroy(ui);
}

static void henka_test_wide_viewport_shading_tabs_fit_scaled_text(void)
{
    static const char* const labels[] = {
        "Wire", "Solid", "Material", "Rendered"};
    henka_ui_context* ui = NULL;
    henka_ui_frame_desc frame_desc = {0};
    henka_ui_rect controls[4] = {{0}};
    size_t control_count = 0U;
    float minimum_width = 0.0f;
    float row_width = 0.0f;
    size_t index;

    HENKA_TEST_ASSERT(henka_ui_create(&ui) == HENKA_SUCCESS);
    frame_desc.framebuffer_width = 2560;
    frame_desc.framebuffer_height = 1440;
    HENKA_TEST_ASSERT(henka_ui_begin_frame(ui, &frame_desc) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(
        sandbox3d_editor_layout_text_control_row_minimum_width_for_context(
            ui,
            labels,
            4U,
            1.0f,
            8.0f,
            3.0f,
            &minimum_width) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(minimum_width > 0.0f);
    row_width = fmaxf(360.0f, minimum_width);
    HENKA_TEST_ASSERT(
        sandbox3d_editor_layout_text_control_row_for_context(
            ui,
            (henka_ui_rect){0.0f, 0.0f, row_width, 22.0f},
            labels,
            4U,
            1.0f,
            8.0f,
            3.0f,
            controls,
            4U,
            &control_count) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(control_count == 4U);
    for (index = 0U; index < control_count; ++index)
    {
        int label_width = 0;
        int label_height = 0;

        HENKA_TEST_ASSERT(
            henka_ui_measure_text_for_context(
                ui,
                labels[index],
                1.0f,
                &label_width,
                &label_height) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(controls[index].width >= (float)label_width + 16.0f);
    }
    {
        henka_ui_rect unchanged[4];

        memcpy(unchanged, controls, sizeof(unchanged));
        control_count = 99U;
        HENKA_TEST_ASSERT(
            sandbox3d_editor_layout_text_control_row_for_context(
                ui,
                (henka_ui_rect){0.0f, 0.0f, minimum_width - 1.0f, 22.0f},
                labels,
                4U,
                1.0f,
                8.0f,
                3.0f,
                controls,
                4U,
                &control_count) == HENKA_ERROR_NUMERIC_RANGE);
        HENKA_TEST_ASSERT(control_count == 0U);
        HENKA_TEST_ASSERT(memcmp(controls, unchanged, sizeof(unchanged)) == 0);
    }
    HENKA_TEST_ASSERT(henka_ui_end_frame(ui) == HENKA_SUCCESS);
    henka_ui_destroy(ui);
}

static void henka_test_selection_status_avoids_compact_scene_header(void)
{
    const henka_viewport viewport = {343, 85, 564, 580};
    const henka_ui_rect compact_header = {335.0f, 47.0f, 573.0f, 68.0f};
    henka_ui_rect selection_status = {9.0f, 9.0f, 9.0f, 9.0f};
    henka_ui_context* ui = NULL;
    henka_ui_frame_desc frame_desc = {0};
    int status_text_width = 0;
    int status_text_height = 0;
    int compact_status_text_width = 0;

    HENKA_TEST_ASSERT(
        sandbox3d_editor_layout_authoring_selection_status_bounds(
            viewport,
            compact_header,
            &selection_status) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(selection_status.x, 355.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(selection_status.y, 123.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(selection_status.width, 320.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(selection_status.height, 28.0f, 0.0001f);
    HENKA_TEST_ASSERT(!henka_test_rects_overlap(selection_status, compact_header));

    HENKA_TEST_ASSERT(henka_ui_create(&ui) == HENKA_SUCCESS);
    frame_desc.framebuffer_width = 1280;
    frame_desc.framebuffer_height = 720;
    HENKA_TEST_ASSERT(henka_ui_begin_frame(ui, &frame_desc) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(
        henka_ui_measure_text_for_context(
            ui,
            "EDIT FACE  NO ACTIVE  0 SELECTED",
            1.0f,
            &status_text_width,
            &status_text_height) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(
        henka_ui_measure_text_for_context(
            ui,
            "FACE: NONE (0)",
            1.0f,
            &compact_status_text_width,
            &status_text_height) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_ui_end_frame(ui) == HENKA_SUCCESS);
    henka_ui_destroy(ui);
    HENKA_TEST_ASSERT(
        (float)status_text_width + 24.0f <= selection_status.width);
    HENKA_TEST_ASSERT(
        (float)compact_status_text_width + 24.0f <= 216.0f);

    HENKA_TEST_ASSERT(
        sandbox3d_editor_layout_authoring_selection_status_bounds(
            viewport,
            (henka_ui_rect){335.0f, 47.0f, 573.0f, 30.0f},
            &selection_status) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(selection_status.y, 97.0f, 0.0001f);

    HENKA_TEST_ASSERT(
        sandbox3d_editor_layout_authoring_selection_status_bounds(
            (henka_viewport){0, 0, 300, 100},
            (henka_ui_rect){0.0f, 0.0f, 0.0f, 0.0f},
            &selection_status) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(selection_status.y, 12.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(selection_status.width, 276.0f, 0.0001f);

    selection_status = (henka_ui_rect){9.0f, 9.0f, 9.0f, 9.0f};
    HENKA_TEST_ASSERT(
        sandbox3d_editor_layout_authoring_selection_status_bounds(
            (henka_viewport){343, 85, 564, 64},
            compact_header,
            &selection_status) == HENKA_ERROR_NUMERIC_RANGE);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(selection_status.x, 9.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(selection_status.y, 9.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(selection_status.width, 9.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(selection_status.height, 9.0f, 0.0001f);
}

static void henka_test_authoring_toolbar_follows_status_controls(void)
{
    sandbox3d_editor_frame_layout layout = {0};
    henka_ui_rect toolbar;
    henka_viewport navigation_viewport;
    const henka_ui_rect compact_header = {335.0f, 47.0f, 573.0f, 68.0f};
    henka_ui_rect selection_status = {0.0f, 0.0f, 0.0f, 0.0f};
    henka_ui_rect topology_toggle = {0.0f, 0.0f, 0.0f, 0.0f};

    layout.scene_frame = (henka_ui_rect){335.0f, 47.0f, 573.0f, 620.0f};
    layout.scene_viewport = (henka_viewport){343, 85, 564, 582};

    HENKA_TEST_ASSERT(
        sandbox3d_editor_layout_authoring_selection_status_bounds(
            layout.scene_viewport, compact_header, &selection_status) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(
        sandbox3d_editor_layout_authoring_topology_toggle_bounds(
            layout.scene_viewport,
            compact_header,
            selection_status,
            &topology_toggle) == HENKA_SUCCESS);
    toolbar = sandbox3d_editor_layout_modeling_toolbar_bounds(&layout, true);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(toolbar.y, 191.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(toolbar.width, 553.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(toolbar.height, 166.0f, 0.0001f);
    HENKA_TEST_ASSERT(!henka_test_rects_overlap(toolbar, selection_status));
    HENKA_TEST_ASSERT(!henka_test_rects_overlap(toolbar, topology_toggle));
    navigation_viewport =
        sandbox3d_editor_frame_layout_navigation_viewport(&layout, true);
    HENKA_TEST_ASSERT(navigation_viewport.y == 365);
    HENKA_TEST_ASSERT(navigation_viewport.height == 302);

    layout.scene_frame = (henka_ui_rect){335.0f, 47.0f, 567.0f, 620.0f};
    layout.scene_viewport = (henka_viewport){343, 85, 558, 582};
    toolbar = sandbox3d_editor_layout_modeling_toolbar_bounds(&layout, true);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(toolbar.width, 547.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(toolbar.height, 166.0f, 0.0001f);

    layout.scene_frame = (henka_ui_rect){336.0f, 46.0f, 944.0f, 640.0f};
    layout.scene_viewport = (henka_viewport){344, 84, 928, 594};
    HENKA_TEST_ASSERT(
        sandbox3d_editor_layout_authoring_selection_status_bounds(
            layout.scene_viewport,
            (henka_ui_rect){336.0f, 46.0f, 944.0f, 30.0f},
            &selection_status) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(
        sandbox3d_editor_layout_authoring_topology_toggle_bounds(
            layout.scene_viewport,
            (henka_ui_rect){336.0f, 46.0f, 944.0f, 30.0f},
            selection_status,
            &topology_toggle) == HENKA_SUCCESS);
    toolbar = sandbox3d_editor_layout_modeling_toolbar_bounds(&layout, true);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(toolbar.y, 164.0f, 0.0001f);
    HENKA_TEST_ASSERT(!henka_test_rects_overlap(toolbar, selection_status));
    HENKA_TEST_ASSERT(!henka_test_rects_overlap(toolbar, topology_toggle));
}

static void henka_test_authoring_topology_toggle_bounds(void)
{
    const henka_viewport viewport = {336, 46, 944, 640};
    const henka_ui_rect selection_status = {348.0f, 58.0f, 214.0f, 28.0f};
    henka_ui_rect header = {336.0f, 46.0f, 944.0f, 30.0f};
    henka_ui_rect toggle_bounds = {9.0f, 9.0f, 9.0f, 9.0f};

    HENKA_TEST_ASSERT(
        sandbox3d_editor_layout_authoring_topology_toggle_bounds(
            viewport,
            header,
            selection_status,
            &toggle_bounds) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(toggle_bounds.x, 348.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(toggle_bounds.y, 90.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(toggle_bounds.width, 214.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(toggle_bounds.height, 28.0f, 0.0001f);
    HENKA_TEST_ASSERT(!henka_test_rects_overlap(toggle_bounds, header));
    HENKA_TEST_ASSERT(!henka_test_rects_overlap(toggle_bounds, selection_status));

    header.height = 68.0f;
    HENKA_TEST_ASSERT(
        sandbox3d_editor_layout_authoring_topology_toggle_bounds(
            viewport,
            header,
            selection_status,
            &toggle_bounds) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(toggle_bounds.y, 118.0f, 0.0001f);
    HENKA_TEST_ASSERT(!henka_test_rects_overlap(toggle_bounds, header));
    HENKA_TEST_ASSERT(!henka_test_rects_overlap(toggle_bounds, selection_status));

    toggle_bounds = (henka_ui_rect){9.0f, 9.0f, 9.0f, 9.0f};
    HENKA_TEST_ASSERT(
        sandbox3d_editor_layout_authoring_topology_toggle_bounds(
            (henka_viewport){336, 46, 944, 64},
            (henka_ui_rect){336.0f, 46.0f, 944.0f, 30.0f},
            selection_status,
            &toggle_bounds) == HENKA_ERROR_NUMERIC_RANGE);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(toggle_bounds.x, 9.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(toggle_bounds.y, 9.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(toggle_bounds.width, 9.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(toggle_bounds.height, 9.0f, 0.0001f);
}

static void henka_test_scene_object_label_width(void)
{
    henka_ui_context* ui = NULL;
    henka_ui_frame_desc frame_desc = {0};
    const char* long_name =
        "Highlands Hardware Main Entrance Exterior Wall Assembly East Wing";
    char narrow[256] = "unchanged";
    char wide[256] = "unchanged";
    char hidden[256] = "unchanged";
    char too_small[8] = "keep";
    bool requires_wrapping = false;
    int rendered_width = 0;
    int rendered_height = 0;
    float narrow_wrapped_height = 0.0f;
    float wide_wrapped_height = 0.0f;

    HENKA_TEST_ASSERT(henka_ui_create(&ui) == HENKA_SUCCESS);
    frame_desc.framebuffer_width = 1280;
    frame_desc.framebuffer_height = 720;
    HENKA_TEST_ASSERT(henka_ui_begin_frame(ui, &frame_desc) == HENKA_SUCCESS);

    HENKA_TEST_ASSERT(
        sandbox3d_editor_layout_scene_object_label(
            ui,
            long_name,
            0U,
            false,
            220.0f,
            narrow,
            sizeof(narrow),
            &requires_wrapping) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(requires_wrapping);
    HENKA_TEST_ASSERT(strcmp(narrow, long_name) == 0);
    HENKA_TEST_ASSERT(
        henka_ui_measure_text_for_context(
            ui,
            narrow,
            1.0f,
            &rendered_width,
            &rendered_height) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT((float)rendered_width > 220.0f);
    HENKA_TEST_ASSERT(
        henka_ui_label_wrapped(
            ui,
            (henka_ui_rect){0.0f, 0.0f, 220.0f, 1.0f},
            1.0f,
            narrow,
            HENKA_UI_COLOR_NORMAL,
            &narrow_wrapped_height) == HENKA_ERROR_LIMIT);
    HENKA_TEST_ASSERT(narrow_wrapped_height > 1.0f);
    HENKA_TEST_ASSERT(
        henka_ui_label_wrapped(
            ui,
            (henka_ui_rect){0.0f, 0.0f, 220.0f, narrow_wrapped_height + 1.0f},
            1.0f,
            narrow,
            HENKA_UI_COLOR_NORMAL,
            &narrow_wrapped_height) == HENKA_SUCCESS);

    HENKA_TEST_ASSERT(
        sandbox3d_editor_layout_scene_object_label(
            ui,
            long_name,
            0U,
            false,
            360.0f,
            wide,
            sizeof(wide),
            &requires_wrapping) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(requires_wrapping);
    HENKA_TEST_ASSERT(strcmp(wide, long_name) == 0);
    HENKA_TEST_ASSERT(
        henka_ui_measure_text_for_context(
            ui,
            wide,
            1.0f,
            &rendered_width,
            &rendered_height) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT((float)rendered_width > 360.0f);
    HENKA_TEST_ASSERT(
        henka_ui_label_wrapped(
            ui,
            (henka_ui_rect){0.0f, 0.0f, 360.0f, 1.0f},
            1.0f,
            wide,
            HENKA_UI_COLOR_NORMAL,
            &wide_wrapped_height) == HENKA_ERROR_LIMIT);
    HENKA_TEST_ASSERT(wide_wrapped_height > 1.0f);
    HENKA_TEST_ASSERT(
        henka_ui_label_wrapped(
            ui,
            (henka_ui_rect){0.0f, 0.0f, 360.0f, wide_wrapped_height + 1.0f},
            1.0f,
            wide,
            HENKA_UI_COLOR_NORMAL,
            &wide_wrapped_height) == HENKA_SUCCESS);

    HENKA_TEST_ASSERT(
        sandbox3d_editor_layout_scene_object_label(
            ui,
            "Cube",
            0U,
            true,
            220.0f,
            hidden,
            sizeof(hidden),
            &requires_wrapping) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(!requires_wrapping);
    HENKA_TEST_ASSERT(strcmp(hidden, "Cube  - Hidden") == 0);

    snprintf(too_small, sizeof(too_small), "%s", "keep");
    requires_wrapping = false;
    HENKA_TEST_ASSERT(
        sandbox3d_editor_layout_scene_object_label(
            ui,
            long_name,
            0U,
            true,
            220.0f,
            too_small,
            sizeof(too_small),
            &requires_wrapping) == HENKA_ERROR_LIMIT);
    HENKA_TEST_ASSERT(strcmp(too_small, "keep") == 0);
    HENKA_TEST_ASSERT(!requires_wrapping);

    HENKA_TEST_ASSERT(henka_ui_end_frame(ui) == HENKA_SUCCESS);
    henka_ui_destroy(ui);
}

static void henka_test_variable_scene_row_pages(void)
{
    const float heights[] = {28.0f, 50.0f, 28.0f};
    size_t starts[4] = {91U, 92U, 93U, 94U};
    size_t page_count = 95U;

    HENKA_TEST_ASSERT(
        sandbox3d_editor_layout_variable_row_pages(
            heights,
            3U,
            84.0f,
            6.0f,
            starts,
            4U,
            &page_count) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(page_count == 2U);
    HENKA_TEST_ASSERT(starts[0] == 0U);
    HENKA_TEST_ASSERT(starts[1] == 2U);
    HENKA_TEST_ASSERT(starts[2] == 3U);

    starts[0] = 91U;
    starts[1] = 92U;
    starts[2] = 93U;
    starts[3] = 94U;
    page_count = 95U;
    HENKA_TEST_ASSERT(
        sandbox3d_editor_layout_variable_row_pages(
            heights,
            3U,
            80.0f,
            6.0f,
            starts,
            4U,
            &page_count) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(page_count == 3U);
    HENKA_TEST_ASSERT(starts[0] == 0U);
    HENKA_TEST_ASSERT(starts[1] == 1U);
    HENKA_TEST_ASSERT(starts[2] == 2U);
    HENKA_TEST_ASSERT(starts[3] == 3U);

    starts[0] = 91U;
    starts[1] = 92U;
    starts[2] = 93U;
    starts[3] = 94U;
    page_count = 95U;
    HENKA_TEST_ASSERT(
        sandbox3d_editor_layout_variable_row_pages(
            heights,
            3U,
            49.0f,
            6.0f,
            starts,
            4U,
            &page_count) == HENKA_ERROR_LIMIT);
    HENKA_TEST_ASSERT(starts[0] == 91U);
    HENKA_TEST_ASSERT(starts[1] == 92U);
    HENKA_TEST_ASSERT(starts[2] == 93U);
    HENKA_TEST_ASSERT(starts[3] == 94U);
    HENKA_TEST_ASSERT(page_count == 95U);

    HENKA_TEST_ASSERT(
        sandbox3d_editor_layout_variable_row_pages(
            NULL,
            0U,
            49.0f,
            6.0f,
            starts,
            4U,
            &page_count) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(page_count == 1U);
    HENKA_TEST_ASSERT(starts[0] == 0U);
    HENKA_TEST_ASSERT(starts[1] == 0U);
}

void henka_test_sandbox3d_editor_layout(void)
{
    sandbox3d_editor_layout_metrics metrics;
    henka_ui_rect row[4];
    size_t row_count;
    float expanded_sidebar_width;
    float expanded_utility_width;

    henka_test_modeling_orientation_labels_fit_wide_display();
    henka_test_modeling_selection_and_pivot_labels_fit_wide_display();
    henka_test_compact_modeling_toolbar_breakpoint_preserves_label_clearance();
    henka_test_scene_view_title_clears_wide_header_controls();
    henka_test_modeling_summary_clears_wide_empty_state_prompt();
    henka_test_wide_viewport_shading_tabs_fit_scaled_text();
    henka_test_selection_status_avoids_compact_scene_header();
    henka_test_authoring_toolbar_follows_status_controls();
    henka_test_authoring_topology_toggle_bounds();
    henka_test_scene_object_label_width();
    henka_test_variable_scene_row_pages();

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
    HENKA_TEST_ASSERT(metrics.sidebar_width >= 300.0f);
    HENKA_TEST_ASSERT(metrics.utility_width >= 344.0f);

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
    HENKA_TEST_ASSERT(metrics.sidebar_width > 304.0f);
    HENKA_TEST_ASSERT(metrics.sidebar_width <= 480.0f);
    HENKA_TEST_ASSERT(metrics.utility_width > 344.0f);
    HENKA_TEST_ASSERT(metrics.utility_width <= 560.0f);
    expanded_sidebar_width = metrics.sidebar_width;
    expanded_utility_width = metrics.utility_width;

    HENKA_TEST_ASSERT(
        sandbox3d_editor_layout_metrics_for_framebuffer(
            1920, 1080, &metrics) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(expanded_sidebar_width >= metrics.sidebar_width + 48.0f);
    HENKA_TEST_ASSERT(expanded_utility_width >= metrics.utility_width + 96.0f);
    HENKA_TEST_ASSERT(
        metrics.breakpoint == SANDBOX3D_EDITOR_LAYOUT_WIDE);

    {
        const char* labels[] = {"Diagnostics", "Transform QA", "Physics QA"};
        const int framebuffer_widths[] = {1280, 1920, 2560};
        const int framebuffer_heights[] = {720, 1080, 1440};
        henka_ui_rect controls[3];
        henka_ui_rect unchanged_controls[3] = {
            {91.0f, 92.0f, 93.0f, 94.0f},
            {81.0f, 82.0f, 83.0f, 84.0f},
            {71.0f, 72.0f, 73.0f, 74.0f}};
        henka_ui_rect before_controls[3];
        henka_ui_context* text_context = NULL;
        henka_ui_frame_desc frame_desc = {0};
        int required_text_widths[3];
        int label_height;
        double required_total;
        size_t width_index;
        size_t label_index;

        HENKA_TEST_ASSERT(henka_ui_create(&text_context) == HENKA_SUCCESS);
        henka_ui_set_visible(text_context, true);

        for (width_index = 0U; width_index < 3U; ++width_index)
        {
            float row_width;

            frame_desc.framebuffer_width = framebuffer_widths[width_index];
            frame_desc.framebuffer_height = framebuffer_heights[width_index];
            HENKA_TEST_ASSERT(
                henka_ui_begin_frame(text_context, &frame_desc) == HENKA_SUCCESS);

            HENKA_TEST_ASSERT(
                sandbox3d_editor_layout_metrics_for_framebuffer(
                    framebuffer_widths[width_index],
                    framebuffer_heights[width_index],
                    &metrics) == HENKA_SUCCESS);
            row_width = metrics.utility_width - 28.0f;
            required_total = 3.0;
            for (label_index = 0U; label_index < 3U; ++label_index)
            {
                HENKA_TEST_ASSERT(
                    henka_ui_measure_text_for_context(
                        text_context,
                        labels[label_index],
                        1.0f,
                        &required_text_widths[label_index],
                        &label_height) == HENKA_SUCCESS);
            required_total +=
                    (double)required_text_widths[label_index] + 16.0;
            }
            HENKA_TEST_ASSERT(
                sandbox3d_editor_layout_text_control_row_for_context(
                    text_context,
                    (henka_ui_rect){14.0f, 60.0f, row_width, 24.0f},
                    labels,
                    3U,
                    1.0f,
                    8.0f,
                    1.5f,
                    controls,
                    3U,
                    &row_count) == HENKA_SUCCESS);
            HENKA_TEST_ASSERT(row_count == 3U);
            for (label_index = 0U; label_index < 3U; ++label_index)
            {
                HENKA_TEST_ASSERT(
                    controls[label_index].width >=
                        (float)required_text_widths[label_index] + 16.0f);
                HENKA_TEST_ASSERT(controls[label_index].x >= 14.0f);
                HENKA_TEST_ASSERT(
                    controls[label_index].x + controls[label_index].width <=
                        14.0f + row_width + 0.001f);
                if (label_index + 1U < 3U)
                {
                    HENKA_TEST_ASSERT(
                        controls[label_index].x + controls[label_index].width + 1.5f <=
                            controls[label_index + 1U].x + 0.001f);
                }
            }

            if (width_index == 0U)
            {
                memcpy(before_controls, unchanged_controls, sizeof(before_controls));
                row_count = 99U;
                HENKA_TEST_ASSERT(
                    sandbox3d_editor_layout_text_control_row_for_context(
                        text_context,
                        (henka_ui_rect){
                            14.0f,
                            60.0f,
                            (float)required_total - 1.0f,
                            24.0f},
                        labels,
                        3U,
                        1.0f,
                        8.0f,
                        1.5f,
                        unchanged_controls,
                        3U,
                        &row_count) == HENKA_ERROR_NUMERIC_RANGE);
                HENKA_TEST_ASSERT(row_count == 0U);
                HENKA_TEST_ASSERT(
                    memcmp(
                        unchanged_controls,
                        before_controls,
                        sizeof(before_controls)) == 0);
            }
            HENKA_TEST_ASSERT(henka_ui_end_frame(text_context) == HENKA_SUCCESS);
        }
        henka_ui_destroy(text_context);
    }

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
        const char* labels[] = {
            "Preview Extrude", "Inset", "Push Face +0.1", "Pull Face -0.1"};
        henka_ui_rect controls[4] = {{0}};
        henka_ui_rect too_short_controls[4];
        unsigned char too_short_before[sizeof(too_short_controls)];
        unsigned char crowded_before[sizeof(controls)];
        size_t control_index;
        int label_width;
        int label_height;

        HENKA_TEST_ASSERT(
            sandbox3d_editor_layout_text_control_grid(
                (henka_ui_rect){934.0f, 100.0f, 346.0f, 56.0f},
                labels,
                4U,
                2U,
                24.0f,
                1.0f,
                12.0f,
                8.0f,
                4.0f,
                controls,
                4U,
                &row_count) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(row_count == 4U);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(controls[0].y, 100.0f, 0.0001f);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(controls[1].y, 100.0f, 0.0001f);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(controls[2].y, 128.0f, 0.0001f);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(controls[3].y, 128.0f, 0.0001f);
        for (control_index = 0U; control_index < 4U; ++control_index)
        {
            HENKA_TEST_ASSERT(
                henka_ui_measure_text(
                    labels[control_index],
                    1.0f,
                    &label_width,
                    &label_height) == HENKA_SUCCESS);
            HENKA_TEST_ASSERT(
                controls[control_index].width >= (float)label_width + 24.0f);
            HENKA_TEST_ASSERT(
                controls[control_index].x + controls[control_index].width <= 1280.0f);
        }

        HENKA_TEST_ASSERT(
            sandbox3d_editor_layout_text_control_grid(
                (henka_ui_rect){20.0f, 40.0f, 220.0f, 108.0f},
                labels,
                4U,
                1U,
                24.0f,
                1.0f,
                12.0f,
                8.0f,
                4.0f,
                controls,
                4U,
                &row_count) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(row_count == 4U);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(controls[0].y, 40.0f, 0.0001f);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(controls[1].y, 68.0f, 0.0001f);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(controls[2].y, 96.0f, 0.0001f);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(controls[3].y, 124.0f, 0.0001f);
        for (control_index = 0U; control_index < 4U; ++control_index)
        {
            HENKA_TEST_ASSERT(
                henka_ui_measure_text(
                    labels[control_index],
                    1.0f,
                    &label_width,
                    &label_height) == HENKA_SUCCESS);
            HENKA_TEST_ASSERT(
                controls[control_index].width >= (float)label_width + 24.0f);
            HENKA_TEST_ASSERT(
                controls[control_index].x + controls[control_index].width <= 240.0f);
        }

        memset(too_short_controls, 0x5a, sizeof(too_short_controls));
        memcpy(too_short_before, too_short_controls, sizeof(too_short_controls));
        HENKA_TEST_ASSERT(
            sandbox3d_editor_layout_text_control_grid(
                (henka_ui_rect){934.0f, 100.0f, 346.0f, 24.0f},
                labels,
                4U,
                2U,
                24.0f,
                1.0f,
                12.0f,
                8.0f,
                4.0f,
                too_short_controls,
                4U,
                &row_count) == HENKA_ERROR_NUMERIC_RANGE);
        HENKA_TEST_ASSERT(row_count == 0U);
        HENKA_TEST_ASSERT(
            memcmp(too_short_controls, too_short_before, sizeof(too_short_controls)) == 0);

        memset(controls, 0x5a, sizeof(controls));
        memcpy(crowded_before, controls, sizeof(controls));
        HENKA_TEST_ASSERT(
            sandbox3d_editor_layout_text_control_grid(
                (henka_ui_rect){934.0f, 100.0f, 346.0f, 24.0f},
                labels,
                4U,
                4U,
                24.0f,
                1.0f,
                12.0f,
                8.0f,
                0.0f,
                controls,
                4U,
                &row_count) == HENKA_ERROR_NUMERIC_RANGE);
        HENKA_TEST_ASSERT(row_count == 0U);
        HENKA_TEST_ASSERT(memcmp(controls, crowded_before, sizeof(controls)) == 0);
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
        HENKA_TEST_ASSERT(
            sandbox3d_editor_layout_metrics_for_framebuffer(
                1280, 720, &metrics) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(
            frame.left_dock.width, metrics.sidebar_width, 0.0001f);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(
            frame.right_dock.width, metrics.utility_width, 0.0001f);

        HENKA_TEST_ASSERT(
            sandbox3d_editor_frame_layout_build(
                &workspace,
                &visibility,
                1920,
                1080,
                &frame) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(
            sandbox3d_editor_layout_metrics_for_framebuffer(
                1920, 1080, &metrics) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(
            frame.left_dock.width, metrics.sidebar_width, 0.0001f);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(
            frame.right_dock.width, metrics.utility_width, 0.0001f);
        HENKA_TEST_ASSERT(frame.scene_frame.width > 0.0f);

        HENKA_TEST_ASSERT(
            sandbox3d_editor_frame_layout_build(
                &workspace,
                &visibility,
                2560,
                1440,
                &frame) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(
            sandbox3d_editor_layout_metrics_for_framebuffer(
                2560, 1440, &metrics) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(
            frame.left_dock.width, metrics.sidebar_width, 0.0001f);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(
            frame.right_dock.width, metrics.utility_width, 0.0001f);

        sandbox3d_workspace_begin_dock_resize(
            &workspace,
            SANDBOX3D_WORKSPACE_RESIZE_LEFT_DOCK,
            (henka_vec2){100.0f, 100.0f},
            frame.left_dock.width);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(
            workspace.resize_start_width, frame.left_dock.width, 0.0001f);
        sandbox3d_workspace_update_dock_resize(
            &workspace,
            (henka_vec2){112.0f, 100.0f},
            2560,
            620.0f,
            300.0f,
            frame.right_dock.width);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(
            workspace.left_dock_width,
            expanded_sidebar_width + 12.0f,
            0.0001f);

        workspace.left_dock_width = 384.0f;
        workspace.right_dock_width = 416.0f;
        HENKA_TEST_ASSERT(
            sandbox3d_editor_frame_layout_build(
                &workspace,
                &visibility,
                1280,
                720,
                &frame) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(frame.left_dock.width < workspace.left_dock_width);
        HENKA_TEST_ASSERT(frame.right_dock.width < workspace.right_dock_width);
        HENKA_TEST_ASSERT(
            sandbox3d_editor_frame_layout_build(
                &workspace,
                &visibility,
                1920,
                1080,
                &frame) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(frame.left_dock.width, 384.0f, 0.0001f);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(frame.right_dock.width, 416.0f, 0.0001f);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(workspace.left_dock_width, 384.0f, 0.0001f);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(workspace.right_dock_width, 416.0f, 0.0001f);

        {
            sandbox3d_view_compass_preferences compass_preferences;
            sandbox3d_view_compass_layout compass_layout;
            const henka_viewport navigation_viewport =
                sandbox3d_editor_frame_layout_navigation_viewport(&frame, false);
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
