#ifndef SANDBOX3D_EDITOR_LAYOUT_H
#define SANDBOX3D_EDITOR_LAYOUT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <henka/result.h>
#include <henka/ui.h>
#include <henka/workspace.h>

#include "workspace_tools.h"

typedef enum sandbox3d_editor_layout_breakpoint
{
    SANDBOX3D_EDITOR_LAYOUT_NARROW = 0,
    SANDBOX3D_EDITOR_LAYOUT_MEDIUM,
    SANDBOX3D_EDITOR_LAYOUT_WIDE,
    SANDBOX3D_EDITOR_LAYOUT_BREAKPOINT_COUNT
} sandbox3d_editor_layout_breakpoint;

typedef struct sandbox3d_editor_layout_metrics
{
    sandbox3d_editor_layout_breakpoint breakpoint;
    float outer_margin;
    float panel_gap;
    float minimum_hit_target;
    float toolbar_height;
    float sidebar_width;
    float utility_width;
    bool stack_sidebars;
} sandbox3d_editor_layout_metrics;

/* Frame-level layout is editor presentation state, not application state.
 * The builder consumes the workspace model plus explicit visibility inputs so
 * main.c does not own panel geometry and docking composition. */
typedef struct sandbox3d_editor_layout_visibility
{
    bool docked_content_visible;
    bool scene_objects_panel_visible;
    bool object_details_panel_visible;
    bool tools_panel_visible;
    bool utility_panel_visible;
    bool debug_strip_visible;
} sandbox3d_editor_layout_visibility;

typedef struct sandbox3d_editor_scene_view_header_layout
{
    henka_ui_rect context_selector;
    henka_ui_rect context_buttons[3];
    size_t context_button_count;
    henka_ui_rect tools_button;
    henka_ui_rect focus_button;
    henka_ui_rect shading_buttons[4];
    size_t shading_button_count;
    float height;
    bool context_uses_buttons;
    bool compact;
    bool narrow;
} sandbox3d_editor_scene_view_header_layout;

typedef struct sandbox3d_editor_frame_layout
{
    float outer_margin;
    float panel_gap;
    henka_ui_rect left_dock;
    henka_ui_rect scene_frame;
    sandbox3d_editor_scene_view_header_layout scene_header;
    henka_ui_rect right_dock;
    henka_ui_rect controls_panel;
    henka_ui_rect scene_objects_panel;
    henka_ui_rect object_details_panel;
    henka_ui_rect utility_panel;
    henka_viewport scene_viewport;
    henka_ui_rect debug_strip;
    henka_ui_rect left_splitter;
    henka_ui_rect right_splitter;
    sandbox3d_workspace_topology_layout left_topology;
    sandbox3d_workspace_topology_layout right_topology;
} sandbox3d_editor_frame_layout;

typedef struct sandbox3d_editor_controls_status_layout
{
    henka_ui_rect scroll_viewport;
    henka_ui_rect footer_save_settings;
    henka_ui_rect footer_reset_layout;
    henka_ui_rect footer_reset_settings;
    henka_ui_rect footer_open_legend;
} sandbox3d_editor_controls_status_layout;

typedef struct sandbox3d_editor_scene_objects_layout
{
    henka_ui_rect actions_viewport;
    henka_ui_rect hierarchy_viewport;
    henka_ui_rect page_footer;
} sandbox3d_editor_scene_objects_layout;

/* Convert a content-space Scene View rectangle to framebuffer pixels for the
 * renderer and scene-picking path. The output is bounded to the real drawable
 * size to absorb content-size rounding at fractional display scales. */
henka_result sandbox3d_editor_layout_content_viewport_to_framebuffer(
    henka_viewport content_viewport,
    uint16_t app_scale_percent,
    uint32_t display_scale_numerator,
    uint32_t display_scale_denominator,
    int framebuffer_width,
    int framebuffer_height,
    henka_viewport* out_framebuffer_viewport);

/* Convert physical window-content dimensions to the editor's logical layout
 * space. App scale is intentionally separate from OS display scale. */
henka_result sandbox3d_editor_layout_content_size_for_app_scale(
    int content_width,
    int content_height,
    uint16_t app_scale_percent,
    int* out_logical_width,
    int* out_logical_height);

typedef struct sandbox3d_modeling_toolbar_layout
{
    henka_ui_rect bounds;
    henka_ui_rect selection_mode;
    henka_ui_rect options_toggle;
    henka_ui_rect orientation;
    henka_ui_rect pivot;
    henka_ui_rect tool_buttons[6];
    henka_ui_rect state_summary;
    bool compact;
    bool options_expanded;
    bool authoring_available;
} sandbox3d_modeling_toolbar_layout;

/*
 * Return the responsive policy for a positive framebuffer width. Invalid
 * widths fail closed to the narrowest policy; callers that need diagnostics
 * should use sandbox3d_editor_layout_metrics_for_framebuffer().
 */
sandbox3d_editor_layout_breakpoint sandbox3d_editor_layout_breakpoint_for_width(
    int framebuffer_width);

henka_result sandbox3d_editor_layout_metrics_for_framebuffer(
    int framebuffer_width,
    int framebuffer_height,
    sandbox3d_editor_layout_metrics* out_metrics);

henka_result sandbox3d_editor_frame_layout_build(
    const henka_ui_context* ui_context,
    const sandbox3d_workspace_model* workspace,
    const sandbox3d_editor_layout_visibility* visibility,
    int framebuffer_width,
    int framebuffer_height,
    sandbox3d_editor_frame_layout* out_layout);

/* Layout the actual Scene View header controls with the active text metrics.
 * The returned bounds are shared by drawing, hit testing, and workspace
 * viewport reservation. */
henka_result sandbox3d_editor_layout_scene_view_header_for_context(
    const henka_ui_context* ui_context,
    henka_ui_rect bounds,
    sandbox3d_editor_scene_view_header_layout* out_layout);

bool sandbox3d_editor_frame_layout_is_valid(
    const sandbox3d_editor_frame_layout* layout);

/* Keep Camera/Status page content in a scrollable viewport above its fixed
 * footer actions. This prevents long page content from overlapping the
 * Settings/Reset controls in compact dock heights. */
henka_result sandbox3d_editor_layout_controls_status_page(
    henka_ui_rect panel_bounds,
    sandbox3d_editor_controls_status_layout* out_layout);

/* Split Scene Objects content into independently usable action and hierarchy
 * regions. The action area may scroll, while the hierarchy retains its own
 * row paging and fixed page footer. */
henka_result sandbox3d_editor_layout_scene_objects_regions(
    henka_ui_rect panel_bounds,
    float measured_line_height,
    sandbox3d_editor_scene_objects_layout* out_layout);

/* Return the editor-owned modeling toolbar bounds for the current scene
 * frame. The navigation overlay consumes the same bounds so it cannot cover
 * the toolbar or its hit targets. */
henka_ui_rect sandbox3d_editor_layout_modeling_toolbar_bounds(
    henka_ui_rect scene_frame,
    uint16_t app_scale_percent,
    bool authoring_available,
    bool options_expanded);

henka_result sandbox3d_editor_layout_modeling_toolbar_compute(
    henka_ui_rect scene_frame,
    uint16_t app_scale_percent,
    bool authoring_available,
    bool options_expanded,
    sandbox3d_modeling_toolbar_layout* out_layout);

/* True only for visible interactive toolbar controls. Empty space and
 * informational summary text remain available for viewport interaction. */
bool sandbox3d_editor_layout_modeling_toolbar_contains_interactive_point(
    const sandbox3d_modeling_toolbar_layout* layout,
    henka_vec2 point);

/* Return the scene viewport region reserved for navigation overlays. This is
 * editor presentation geometry; it does not change the renderer viewport. */
henka_viewport sandbox3d_editor_frame_layout_navigation_viewport(
    const sandbox3d_editor_frame_layout* layout,
    bool authoring_available,
    bool options_expanded,
    uint16_t app_scale_percent);

henka_ui_rect sandbox3d_editor_frame_layout_panel_rect(
    const sandbox3d_editor_frame_layout* layout,
    sandbox3d_workspace_panel_id panel_id);

henka_ui_rect* sandbox3d_editor_frame_layout_panel_rect_slot(
    sandbox3d_editor_frame_layout* layout,
    sandbox3d_workspace_panel_id panel_id);

/*
 * Place a bounded row of equal-width controls without allocating. The output
 * array is caller-owned and is written only after all dimensions validate.
 */
henka_result sandbox3d_editor_layout_tool_row(
    henka_ui_rect bounds,
    size_t item_count,
    float minimum_item_width,
    float gap,
    henka_ui_rect* out_items,
    size_t item_capacity,
    size_t* out_item_count);

/* Place measured labels using the active UI context's text authority. Each
 * output is caller-owned; an unfit row leaves output geometry unchanged. */
henka_result sandbox3d_editor_layout_text_control_row_for_context(
    const henka_ui_context* ui_context,
    henka_ui_rect bounds,
    const char* const* labels,
    size_t item_count,
    float scale,
    float minimum_item_width,
    float horizontal_padding,
    float gap,
    henka_ui_rect* out_items,
    size_t item_capacity,
    size_t* out_item_count);

/* Pack measured controls into as few full-size rows as fit. The output is
 * published only when every item has a valid, contained hit rectangle. */
henka_result sandbox3d_editor_layout_text_control_grid_for_context(
    const henka_ui_context* ui_context,
    henka_ui_rect bounds,
    const char* const* labels,
    size_t item_count,
    float scale,
    float minimum_item_width,
    float horizontal_padding,
    float horizontal_gap,
    float row_height,
    float row_gap,
    henka_ui_rect* out_items,
    size_t item_capacity,
    size_t* out_row_count);

#define SANDBOX3D_EDITOR_UTILITY_TAB_COUNT 10U

/* Canonical product labels and measured geometry for Utility destinations. */
const char* sandbox3d_editor_utility_tab_label(size_t index);
henka_result sandbox3d_editor_layout_utility_tabs_for_context(
    const henka_ui_context* ui_context,
    henka_ui_rect panel_bounds,
    float app_ui_scale,
    henka_ui_rect* out_tab_rects,
    size_t tab_rect_capacity,
    size_t* out_row_count,
    float* out_row_height);

typedef struct sandbox3d_editor_details_footer_layout
{
    henka_ui_rect save_project;
    henka_ui_rect reload_project;
    float scroll_reserve;
    size_t row_count;
} sandbox3d_editor_details_footer_layout;

henka_result sandbox3d_editor_layout_details_footer_for_context(
    const henka_ui_context* ui_context,
    henka_ui_rect panel_bounds,
    sandbox3d_editor_details_footer_layout* out_layout);

#ifdef HENKA_SANDBOX3D_TEST_SEAM
/* Regression-only coverage for the retired fixed-column display helpers.
 * Product layout must use editor_text_layout's active font metrics. */
henka_result sandbox3d_editor_layout_measure_wrapped_text(
    const char* text,
    size_t max_columns,
    size_t* out_line_count);

henka_result sandbox3d_editor_layout_wrap_text(
    const char* text,
    size_t max_columns,
    char* out_text,
    size_t out_capacity,
    size_t* out_line_count);

/* Bound already-wrapped display text to a visible line and column budget.
 * The final visible line becomes a width-bounded ellipsis when lines are omitted. */
henka_result sandbox3d_editor_layout_limit_wrapped_text(
    const char* wrapped_text,
    size_t maximum_line_count,
    size_t maximum_columns,
    char* out_text,
    size_t out_capacity,
    size_t* out_line_count,
    bool* out_truncated);

/* Keep a hidden Scene Objects row on one line while preserving both the
 * object's display name and a textual state marker. Long names are bounded
 * within the name portion; the canonical scene name is never modified. */
henka_result sandbox3d_editor_layout_format_hidden_row_label(
    const char* object_name,
    size_t maximum_columns,
    char* out_text,
    size_t out_capacity,
    bool* out_name_truncated);
#endif /* HENKA_SANDBOX3D_TEST_SEAM */

/* Row heights and pages share the same text metrics so wrapped rows cannot
 * overlap the next row or the panel footer. */
henka_result sandbox3d_editor_layout_text_row_height(
    size_t line_count,
    float line_height,
    float vertical_padding,
    float minimum_height,
    float* out_height);

henka_result sandbox3d_editor_layout_clamp_row_height(
    float natural_row_height,
    float available_height,
    float minimum_row_height,
    float* out_visible_height);

henka_result sandbox3d_editor_layout_page_variable_rows(
    const size_t* row_line_counts,
    size_t row_count,
    float available_height,
    float line_height,
    float vertical_padding,
    float minimum_row_height,
    size_t requested_page,
    size_t* out_page_index,
    size_t* out_page_count,
    size_t* out_first_row,
    size_t* out_visible_row_count);

#endif
