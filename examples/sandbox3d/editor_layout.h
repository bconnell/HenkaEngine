#ifndef SANDBOX3D_EDITOR_LAYOUT_H
#define SANDBOX3D_EDITOR_LAYOUT_H

#include <stdbool.h>
#include <stddef.h>

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

typedef struct sandbox3d_editor_frame_layout
{
    float outer_margin;
    float panel_gap;
    henka_ui_rect left_dock;
    henka_ui_rect scene_frame;
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
    const sandbox3d_workspace_model* workspace,
    const sandbox3d_editor_layout_visibility* visibility,
    int framebuffer_width,
    int framebuffer_height,
    sandbox3d_editor_frame_layout* out_layout);

bool sandbox3d_editor_frame_layout_is_valid(
    const sandbox3d_editor_frame_layout* layout);

/* Return the editor-owned modeling toolbar bounds for the current scene
 * frame. The navigation overlay consumes the same bounds so it cannot cover
 * the toolbar or its hit targets. */
henka_ui_rect sandbox3d_editor_layout_modeling_toolbar_bounds(
    henka_ui_rect scene_frame,
    bool authoring_available,
    bool options_expanded);

henka_result sandbox3d_editor_layout_modeling_toolbar_compute(
    henka_ui_rect scene_frame,
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
    bool options_expanded);

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

/*
 * Place a bounded row of labeled controls using the engine UI text metrics.
 * Each control receives its measured label width plus horizontal padding;
 * extra space is distributed evenly. The output array is caller-owned and
 * remains unchanged when the row cannot fit or any input is invalid.
 */
henka_result sandbox3d_editor_layout_text_control_row(
    henka_ui_rect bounds,
    const char* const* labels,
    size_t item_count,
    float scale,
    float horizontal_padding,
    float gap,
    henka_ui_rect* out_items,
    size_t item_capacity,
    size_t* out_item_count);

/* Place controls at their measured text width (or a minimum hit width) using
 * the active UI context's DPI-aware text scale. Extra row space remains
 * available to neighboring content instead of stretching the controls. */
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

/* Wrap display text without truncating its non-whitespace content. A long
 * token is split at the column boundary. */
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
