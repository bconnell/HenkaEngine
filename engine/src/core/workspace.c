#include <henka/workspace.h>

#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

static float henka_workspace_clamp_float(float value, float minimum, float maximum)
{
    if (value < minimum)
    {
        return minimum;
    }
    if (value > maximum)
    {
        return maximum;
    }
    return value;
}

static bool henka_workspace_float_is_finite(float value)
{
    return isfinite(value) != 0;
}

static bool henka_workspace_vec2_is_finite(henka_vec2 value)
{
    return henka_workspace_float_is_finite(value.x) &&
        henka_workspace_float_is_finite(value.y);
}

static int henka_workspace_clamp_int(int value, int minimum, int maximum)
{
    if (value < minimum)
    {
        return minimum;
    }
    if (value > maximum)
    {
        return maximum;
    }
    return value;
}

bool henka_viewport_is_valid(henka_viewport viewport)
{
    int64_t bottom;
    int64_t right;

    if (viewport.x < 0 || viewport.y < 0 ||
        viewport.width <= 0 || viewport.height <= 0)
    {
        return false;
    }

    right = (int64_t)viewport.x + (int64_t)viewport.width;
    bottom = (int64_t)viewport.y + (int64_t)viewport.height;
    return right <= (int64_t)INT_MAX &&
        bottom <= (int64_t)INT_MAX;
}

float henka_viewport_get_aspect_ratio(henka_viewport viewport)
{
    if (!henka_viewport_is_valid(viewport))
    {
        return 1.0f;
    }

    return (float)viewport.width / (float)viewport.height;
}

bool henka_viewport_contains_point(henka_viewport viewport, henka_vec2 window_point)
{
    int64_t bottom;
    int64_t right;

    if (!henka_viewport_is_valid(viewport) ||
        !henka_workspace_vec2_is_finite(window_point))
    {
        return false;
    }

    right = (int64_t)viewport.x + (int64_t)viewport.width;
    bottom = (int64_t)viewport.y + (int64_t)viewport.height;
    return (double)window_point.x >= (double)viewport.x &&
        (double)window_point.y >= (double)viewport.y &&
        (double)window_point.x < (double)right &&
        (double)window_point.y < (double)bottom;
}

henka_result henka_window_point_to_framebuffer_point(
    int window_width,
    int window_height,
    int framebuffer_width,
    int framebuffer_height,
    henka_vec2 window_point,
    henka_vec2* out_framebuffer_point)
{
    double framebuffer_x;
    double framebuffer_y;

    if (out_framebuffer_point != NULL)
    {
        *out_framebuffer_point = (henka_vec2){0.0f, 0.0f};
    }

    if (out_framebuffer_point == NULL ||
        window_width <= 0 ||
        window_height <= 0 ||
        framebuffer_width <= 0 ||
        framebuffer_height <= 0 ||
        !henka_workspace_vec2_is_finite(window_point))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    framebuffer_x = ((double)window_point.x / (double)window_width) *
        (double)framebuffer_width;
    framebuffer_y = ((double)window_point.y / (double)window_height) *
        (double)framebuffer_height;
    if (!isfinite(framebuffer_x) || !isfinite(framebuffer_y) ||
        framebuffer_x < -(double)FLT_MAX || framebuffer_x > (double)FLT_MAX ||
        framebuffer_y < -(double)FLT_MAX || framebuffer_y > (double)FLT_MAX)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    out_framebuffer_point->x = (float)framebuffer_x;
    out_framebuffer_point->y = (float)framebuffer_y;
    return HENKA_SUCCESS;
}

henka_result henka_viewport_window_to_local(
    henka_viewport viewport,
    henka_vec2 window_point,
    henka_vec2* out_local_point)
{
    double local_x;
    double local_y;

    if (out_local_point != NULL)
    {
        *out_local_point = (henka_vec2){0.0f, 0.0f};
    }

    if (out_local_point == NULL ||
        !henka_viewport_contains_point(viewport, window_point))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    local_x = (double)window_point.x - (double)viewport.x;
    local_y = (double)window_point.y - (double)viewport.y;
    if (!isfinite(local_x) || !isfinite(local_y) ||
        local_x < 0.0 || local_y < 0.0 ||
        local_x > (double)FLT_MAX || local_y > (double)FLT_MAX)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    out_local_point->x = (float)local_x;
    out_local_point->y = (float)local_y;
    return HENKA_SUCCESS;
}

henka_result henka_workspace_layout_docked(
    const henka_workspace_desc* desc,
    henka_workspace_layout* out_layout)
{
    double bottom_height;
    double content_height;
    double content_width;
    double frame_height;
    double frame_width;
    double framebuffer_height;
    double framebuffer_width;
    double gap;
    double left_width;
    double margin;
    double max_margin;
    double minimum_frame_height;
    double minimum_frame_width;
    double padding;
    double right_width;
    double scene_frame_bottom;
    double scene_frame_right;
    double scene_header_height;
    double total_dock_width;
    double total_gap_width;
    double viewport_bottom_value;
    double viewport_left_value;
    double viewport_right_value;
    double viewport_top_value;
    int viewport_bottom;
    int viewport_left;
    int viewport_right;
    int viewport_top;

    if (out_layout != NULL)
    {
        memset(out_layout, 0, sizeof(*out_layout));
    }

    if (desc == NULL || out_layout == NULL ||
        desc->framebuffer_width <= 0 || desc->framebuffer_height <= 0 ||
        !henka_workspace_float_is_finite(desc->margin) ||
        !henka_workspace_float_is_finite(desc->gap) ||
        !henka_workspace_float_is_finite(desc->left_dock_width) ||
        !henka_workspace_float_is_finite(desc->right_dock_width) ||
        !henka_workspace_float_is_finite(desc->bottom_dock_height) ||
        !henka_workspace_float_is_finite(desc->scene_header_height) ||
        !henka_workspace_float_is_finite(desc->scene_padding))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    framebuffer_width = (double)desc->framebuffer_width;
    framebuffer_height = (double)desc->framebuffer_height;
    max_margin = ((framebuffer_width < framebuffer_height ?
        framebuffer_width : framebuffer_height) - 1.0) * 0.5;
    if (max_margin < 0.0)
    {
        max_margin = 0.0;
    }

    margin = (double)desc->margin;
    if (margin < 0.0)
    {
        margin = 0.0;
    }
    if (margin > max_margin)
    {
        margin = max_margin;
    }

    gap = desc->gap > 0.0f ? (double)desc->gap : 0.0;
    padding = desc->scene_padding > 0.0f ?
        (double)desc->scene_padding : 0.0;
    scene_header_height = desc->scene_header_height > 0.0f ?
        (double)desc->scene_header_height : 0.0;

    content_width = framebuffer_width - margin * 2.0;
    content_height = framebuffer_height - margin * 2.0;
    if (content_width < 1.0)
    {
        content_width = 1.0;
    }
    if (content_height < 1.0)
    {
        content_height = 1.0;
    }

    minimum_frame_width =
        (double)(desc->min_scene_width > 0 ? desc->min_scene_width : 1) +
        padding * 2.0;
    minimum_frame_height =
        (double)(desc->min_scene_height > 0 ? desc->min_scene_height : 1) +
        scene_header_height + padding * 2.0;
    if (minimum_frame_width < 1.0)
    {
        minimum_frame_width = 1.0;
    }
    if (minimum_frame_width > content_width)
    {
        minimum_frame_width = content_width;
    }
    if (minimum_frame_height < 1.0)
    {
        minimum_frame_height = 1.0;
    }
    if (minimum_frame_height > content_height)
    {
        minimum_frame_height = content_height;
    }

    left_width = desc->left_dock_visible && desc->left_dock_width > 0.0f ?
        (double)desc->left_dock_width : 0.0;
    right_width = desc->right_dock_visible && desc->right_dock_width > 0.0f ?
        (double)desc->right_dock_width : 0.0;
    bottom_height = desc->bottom_dock_visible &&
        desc->bottom_dock_height > 0.0f ?
        (double)desc->bottom_dock_height : 0.0;

    total_gap_width = (left_width > 0.0 ? gap : 0.0) +
        (right_width > 0.0 ? gap : 0.0);
    if (total_gap_width + minimum_frame_width > content_width)
    {
        gap = 0.0;
        total_gap_width = 0.0;
    }

    total_dock_width = left_width + right_width;
    if (total_dock_width > 0.0 &&
        total_dock_width + total_gap_width + minimum_frame_width >
            content_width)
    {
        const double available_dock_width =
            content_width - minimum_frame_width - total_gap_width;
        const double scale = available_dock_width > 0.0 ?
            available_dock_width / total_dock_width : 0.0;
        left_width *= scale;
        right_width *= scale;
    }

    if (bottom_height > 0.0 &&
        bottom_height + gap + minimum_frame_height > content_height)
    {
        bottom_height = content_height - minimum_frame_height - gap;
        if (bottom_height < 0.0)
        {
            bottom_height = 0.0;
            gap = 0.0;
        }
    }

    frame_width = content_width - left_width - right_width -
        (left_width > 0.0 ? gap : 0.0) -
        (right_width > 0.0 ? gap : 0.0);
    frame_height = content_height - bottom_height -
        (bottom_height > 0.0 ? gap : 0.0);
    if (frame_width < 1.0)
    {
        frame_width = 1.0;
    }
    if (frame_width > content_width)
    {
        frame_width = content_width;
    }
    if (frame_height < 1.0)
    {
        frame_height = 1.0;
    }
    if (frame_height > content_height)
    {
        frame_height = content_height;
    }

    out_layout->left_dock = (henka_ui_rect){
        (float)margin,
        (float)margin,
        (float)left_width,
        (float)frame_height};
    out_layout->scene_frame = (henka_ui_rect){
        (float)(margin + left_width + (left_width > 0.0 ? gap : 0.0)),
        (float)margin,
        (float)frame_width,
        (float)frame_height};

    scene_frame_right =
        (double)out_layout->scene_frame.x +
        (double)out_layout->scene_frame.width;
    if (scene_frame_right > framebuffer_width - margin)
    {
        scene_frame_right = framebuffer_width - margin;
        out_layout->scene_frame.width =
            (float)(scene_frame_right - (double)out_layout->scene_frame.x);
    }
    scene_frame_bottom =
        (double)out_layout->scene_frame.y +
        (double)out_layout->scene_frame.height;
    if (scene_frame_bottom > framebuffer_height - margin)
    {
        scene_frame_bottom = framebuffer_height - margin;
        out_layout->scene_frame.height =
            (float)(scene_frame_bottom - (double)out_layout->scene_frame.y);
    }
    if (out_layout->scene_frame.width < 1.0f)
    {
        out_layout->scene_frame.width = 1.0f;
    }
    if (out_layout->scene_frame.height < 1.0f)
    {
        out_layout->scene_frame.height = 1.0f;
    }

    out_layout->right_dock = (henka_ui_rect){
        (float)((double)out_layout->scene_frame.x +
            (double)out_layout->scene_frame.width +
            (right_width > 0.0 ? gap : 0.0)),
        (float)margin,
        (float)right_width,
        (float)frame_height};
    out_layout->bottom_dock = (henka_ui_rect){
        (float)margin,
        (float)(margin + frame_height +
            (bottom_height > 0.0 ? gap : 0.0)),
        (float)content_width,
        (float)bottom_height};

    viewport_left_value = ceil(
        (double)out_layout->scene_frame.x + padding);
    viewport_top_value = ceil(
        (double)out_layout->scene_frame.y +
        scene_header_height + padding);
    viewport_right_value = floor(
        (double)out_layout->scene_frame.x +
        (double)out_layout->scene_frame.width - padding);
    viewport_bottom_value = floor(
        (double)out_layout->scene_frame.y +
        (double)out_layout->scene_frame.height - padding);

    if (viewport_left_value < 0.0)
    {
        viewport_left_value = 0.0;
    }
    if (viewport_left_value > framebuffer_width - 1.0)
    {
        viewport_left_value = framebuffer_width - 1.0;
    }
    if (viewport_top_value < 0.0)
    {
        viewport_top_value = 0.0;
    }
    if (viewport_top_value > framebuffer_height - 1.0)
    {
        viewport_top_value = framebuffer_height - 1.0;
    }

    if (viewport_right_value < viewport_left_value + 1.0)
    {
        viewport_right_value = viewport_left_value + 1.0;
    }
    if (viewport_right_value > framebuffer_width)
    {
        viewport_right_value = framebuffer_width;
    }
    if (viewport_bottom_value < viewport_top_value + 1.0)
    {
        viewport_bottom_value = viewport_top_value + 1.0;
    }
    if (viewport_bottom_value > framebuffer_height)
    {
        viewport_bottom_value = framebuffer_height;
    }

    viewport_left = (int)viewport_left_value;
    viewport_top = (int)viewport_top_value;
    viewport_right = (int)viewport_right_value;
    viewport_bottom = (int)viewport_bottom_value;

    out_layout->scene_viewport = (henka_viewport){
        viewport_left,
        viewport_top,
        viewport_right - viewport_left,
        viewport_bottom - viewport_top};

    if (!henka_viewport_is_valid(out_layout->scene_viewport))
    {
        memset(out_layout, 0, sizeof(*out_layout));
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    return HENKA_SUCCESS;
}
