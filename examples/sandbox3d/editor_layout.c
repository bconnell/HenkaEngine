#include "editor_layout.h"

#include <float.h>
#include <math.h>
#include <string.h>

#define SANDBOX3D_EDITOR_LAYOUT_MAX_TOOL_ITEMS 64U
#define SANDBOX3D_EDITOR_LAYOUT_MAX_FRAMEBUFFER_DIMENSION 32768

static bool sandbox3d_editor_layout_float_is_valid(float value)
{
    return isfinite((double)value) != 0;
}

sandbox3d_editor_layout_breakpoint sandbox3d_editor_layout_breakpoint_for_width(
    int framebuffer_width)
{
    if (framebuffer_width < 1200)
    {
        return SANDBOX3D_EDITOR_LAYOUT_NARROW;
    }
    if (framebuffer_width < 1600)
    {
        return SANDBOX3D_EDITOR_LAYOUT_MEDIUM;
    }
    return SANDBOX3D_EDITOR_LAYOUT_WIDE;
}

henka_result sandbox3d_editor_layout_metrics_for_framebuffer(
    int framebuffer_width,
    int framebuffer_height,
    sandbox3d_editor_layout_metrics* out_metrics)
{
    sandbox3d_editor_layout_breakpoint breakpoint;
    sandbox3d_editor_layout_metrics metrics;

    if (out_metrics == NULL || framebuffer_width <= 0 || framebuffer_height <= 0)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (framebuffer_width > SANDBOX3D_EDITOR_LAYOUT_MAX_FRAMEBUFFER_DIMENSION ||
        framebuffer_height > SANDBOX3D_EDITOR_LAYOUT_MAX_FRAMEBUFFER_DIMENSION)
    {
        return HENKA_ERROR_NUMERIC_RANGE;
    }

    breakpoint = sandbox3d_editor_layout_breakpoint_for_width(framebuffer_width);
    memset(&metrics, 0, sizeof(metrics));
    metrics.breakpoint = breakpoint;
    metrics.minimum_hit_target = 32.0f;

    switch (breakpoint)
    {
        case SANDBOX3D_EDITOR_LAYOUT_NARROW:
            metrics.outer_margin = 12.0f;
            metrics.panel_gap = 8.0f;
            metrics.toolbar_height = 40.0f;
            metrics.sidebar_width = 220.0f;
            metrics.utility_width = 244.0f;
            metrics.stack_sidebars = true;
            break;
        case SANDBOX3D_EDITOR_LAYOUT_MEDIUM:
            metrics.outer_margin = 16.0f;
            metrics.panel_gap = 12.0f;
            metrics.toolbar_height = 44.0f;
            metrics.sidebar_width = 260.0f;
            metrics.utility_width = 292.0f;
            metrics.stack_sidebars = false;
            break;
        case SANDBOX3D_EDITOR_LAYOUT_WIDE:
            metrics.outer_margin = 20.0f;
            metrics.panel_gap = 16.0f;
            metrics.toolbar_height = 48.0f;
            metrics.sidebar_width = 304.0f;
            metrics.utility_width = 344.0f;
            metrics.stack_sidebars = false;
            break;
        case SANDBOX3D_EDITOR_LAYOUT_BREAKPOINT_COUNT:
        default:
            return HENKA_ERROR_INVALID_ARGUMENT;
    }

    *out_metrics = metrics;
    return HENKA_SUCCESS;
}

henka_result sandbox3d_editor_layout_tool_row(
    henka_ui_rect bounds,
    size_t item_count,
    float minimum_item_width,
    float gap,
    henka_ui_rect* out_items,
    size_t item_capacity,
    size_t* out_item_count)
{
    double total_gap;
    double available_width;
    double item_width;
    size_t item_index;

    if (out_item_count == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_item_count = 0U;

    if (!sandbox3d_editor_layout_float_is_valid(bounds.x) ||
        !sandbox3d_editor_layout_float_is_valid(bounds.y) ||
        !sandbox3d_editor_layout_float_is_valid(bounds.width) ||
        !sandbox3d_editor_layout_float_is_valid(bounds.height) ||
        bounds.width <= 0.0f || bounds.height <= 0.0f ||
        !sandbox3d_editor_layout_float_is_valid(minimum_item_width) ||
        minimum_item_width <= 0.0f ||
        !sandbox3d_editor_layout_float_is_valid(gap) || gap < 0.0f ||
        item_count > SANDBOX3D_EDITOR_LAYOUT_MAX_TOOL_ITEMS ||
        (item_count > 0U && (out_items == NULL || item_capacity < item_count)))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (item_count == 0U)
    {
        return HENKA_SUCCESS;
    }

    total_gap = (double)gap * (double)(item_count - 1U);
    available_width = (double)bounds.width - total_gap;
    if (!isfinite(total_gap) || !isfinite(available_width) || available_width <= 0.0)
    {
        return HENKA_ERROR_NUMERIC_RANGE;
    }

    item_width = available_width / (double)item_count;
    if (!isfinite(item_width) || item_width < (double)minimum_item_width ||
        item_width > (double)FLT_MAX)
    {
        return HENKA_ERROR_NUMERIC_RANGE;
    }

    for (item_index = 0U; item_index < item_count; ++item_index)
    {
        const double item_x =
            (double)bounds.x + ((double)item_index * (item_width + (double)gap));
        if (!isfinite(item_x) || item_x < -(double)FLT_MAX || item_x > (double)FLT_MAX)
        {
            return HENKA_ERROR_NUMERIC_RANGE;
        }
    }

    for (item_index = 0U; item_index < item_count; ++item_index)
    {
        const double item_x =
            (double)bounds.x + ((double)item_index * (item_width + (double)gap));
        out_items[item_index] = (henka_ui_rect){
            (float)item_x,
            bounds.y,
            (float)item_width,
            bounds.height};
    }
    *out_item_count = item_count;
    return HENKA_SUCCESS;
}
