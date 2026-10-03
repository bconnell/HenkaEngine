#ifndef SANDBOX3D_EDITOR_LAYOUT_H
#define SANDBOX3D_EDITOR_LAYOUT_H

#include <stdbool.h>
#include <stddef.h>

#include <henka/result.h>
#include <henka/ui.h>
#include <henka/workspace.h>

#include "workspace_tools.h"

/* Width required by the three Modeling orientation options to keep their
 * complete labels readable at the maximum supported display scale. */
#define SANDBOX3D_EDITOR_MODELING_ORIENTATION_SELECTOR_WIDTH 216.0f
/* Width required for three six-character segments at the maximum supported
 * readability scale, including the segmented-control text padding. */
#define SANDBOX3D_EDITOR_MODELING_SELECTOR_WIDTH 252.0f
#define SANDBOX3D_EDITOR_MODELING_WIDE_ORIENTATION_X 344.0f
/* Preserve a 16 px right inset after the wide orientation selector. */
#define SANDBOX3D_EDITOR_MODELING_TOOLBAR_COMPACT_ROW_WIDTH 576.0f

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
    const sandbox3d_editor_frame_layout* layout,
    bool authoring_available);

/* Place the Edit-mode selection status below the visible Scene View header,
 * including the second row used by compact headers. Returns a range error
 * when the complete status surface does not fit in the viewport. */
henka_result sandbox3d_editor_layout_authoring_selection_status_bounds(
    henka_viewport viewport,
    henka_ui_rect scene_header,
    henka_ui_rect* out_bounds);

/* Place the Edit-mode topology toggle below the scene header and selection
 * status overlay. Returns a range error when the viewport has no vertical
 * space for the complete hit target; output is unchanged on failure. */
henka_result sandbox3d_editor_layout_authoring_topology_toggle_bounds(
    henka_viewport viewport,
    henka_ui_rect scene_header,
    henka_ui_rect selection_status,
    henka_ui_rect* out_bounds);

/* Return the scene viewport region reserved for navigation overlays. This is
 * editor presentation geometry; it does not change the renderer viewport. */
henka_viewport sandbox3d_editor_frame_layout_navigation_viewport(
    const sandbox3d_editor_frame_layout* layout,
    bool authoring_available);

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

/* Places a horizontal control group after a measured leading label while
 * retaining a preferred offset. Outputs remain unchanged when it cannot fit. */
henka_result sandbox3d_editor_layout_nonoverlapping_horizontal_offset(
    float available_width,
    float preferred_offset,
    float leading_text_offset,
    float leading_text_width,
    float gap,
    float item_width,
    float* out_offset);

/* Measures each label with the framebuffer-aware readability scale used by
 * the target UI context before distributing row width. */
henka_result sandbox3d_editor_layout_text_control_row_for_context(
    const henka_ui_context* text_context,
    henka_ui_rect bounds,
    const char* const* labels,
    size_t item_count,
    float scale,
    float horizontal_padding,
    float gap,
    henka_ui_rect* out_items,
    size_t item_capacity,
    size_t* out_item_count);

/* Returns the measured width needed for every label, its horizontal padding,
 * and the requested gaps at the target UI context's readability scale. */
henka_result sandbox3d_editor_layout_text_control_row_minimum_width_for_context(
    const henka_ui_context* text_context,
    const char* const* labels,
    size_t item_count,
    float scale,
    float horizontal_padding,
    float gap,
    float* out_minimum_width);

/* Arrange labeled controls in bounded row-major order. Rows use the measured
 * label widths and the supplied fixed control height; outputs are published
 * only when every row fits. */
henka_result sandbox3d_editor_layout_text_control_grid(
    henka_ui_rect bounds,
    const char* const* labels,
    size_t item_count,
    size_t column_count,
    float control_height,
    float scale,
    float horizontal_padding,
    float column_gap,
    float row_gap,
    henka_ui_rect* out_items,
    size_t item_capacity,
    size_t* out_item_count);

/* Format the complete scene-hierarchy row label. The width is used only to
 * report whether the complete label needs wrapping; the name is never
 * shortened. Outputs remain unchanged when formatting or measurement fails. */
henka_result sandbox3d_editor_layout_scene_object_label(
    const henka_ui_context* ui,
    const char* name,
    size_t hierarchy_depth,
    bool hidden,
    float available_width,
    char* out_text,
    size_t text_capacity,
    bool* out_requires_wrapping);
/* Partition measured scene-object row heights into pages without allowing a
 * row or inter-row gap to exceed the visible list height. `out_page_starts`
 * receives page_count + 1 offsets, including the final row_count sentinel. */
henka_result sandbox3d_editor_layout_variable_row_pages(
    const float* row_heights,
    size_t row_count,
    float available_height,
    float row_gap,
    size_t* out_page_starts,
    size_t page_start_capacity,
    size_t* out_page_count);

#endif
