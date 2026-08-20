#ifndef SANDBOX3D_EDITOR_LAYOUT_H
#define SANDBOX3D_EDITOR_LAYOUT_H

#include <stdbool.h>
#include <stddef.h>

#include <henka/result.h>
#include <henka/ui.h>

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

#endif
