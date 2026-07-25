#include <henka/workspace.h>

#include <float.h>
#include <math.h>
#include <stdint.h>

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
    return viewport.width > 0 && viewport.height > 0;
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

    if (out_framebuffer_point == NULL ||
        window_width <= 0 ||
        window_height <= 0 ||
        framebuffer_width <= 0 ||
        framebuffer_height <= 0 ||
        !henka_workspace_vec2_is_finite(window_point))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    framebuffer_x = ((double)window_point.x / (double)window_width) * (double)framebuffer_width;
    framebuffer_y = ((double)window_point.y / (double)window_height) * (double)framebuffer_height;
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

henka_result henka_viewport_window_to_local(henka_viewport viewport, henka_vec2 window_point, henka_vec2* out_local_point)
{
    if (out_local_point == NULL || !henka_viewport_contains_point(viewport, window_point))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    out_local_point->x = window_point.x - (float)viewport.x;
    out_local_point->y = window_point.y - (float)viewport.y;
    return HENKA_SUCCESS;
}

henka_result henka_workspace_layout_docked(const henka_workspace_desc* desc, henka_workspace_layout* out_layout)
{
    float bottom_height;
    float content_height;
    float content_width;
    float frame_height;
    float frame_width;
    float gap;
    float left_width;
    float margin;
    float max_margin;
    float minimum_frame_height;
    float minimum_frame_width;
    float padding;
    float right_width;
    float scene_header_height;
    float total_dock_width;
    float total_gap_width;
    int viewport_bottom;
    int viewport_left;
    int viewport_right;
    int viewport_top;

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

    max_margin = ((float)(desc->framebuffer_width < desc->framebuffer_height ?
        desc->framebuffer_width : desc->framebuffer_height) - 1.0f) * 0.5f;
    if (max_margin < 0.0f)
    {
        max_margin = 0.0f;
    }

    margin = henka_workspace_clamp_float(desc->margin, 0.0f, max_margin);
    gap = desc->gap > 0.0f ? desc->gap : 0.0f;
    padding = desc->scene_padding > 0.0f ? desc->scene_padding : 0.0f;
    scene_header_height = desc->scene_header_height > 0.0f ? desc->scene_header_height : 0.0f;

    content_width = (float)desc->framebuffer_width - margin * 2.0f;
    content_height = (float)desc->framebuffer_height - margin * 2.0f;
    if (content_width < 1.0f)
    {
        content_width = 1.0f;
    }
    if (content_height < 1.0f)
    {
        content_height = 1.0f;
    }

    minimum_frame_width = (float)(desc->min_scene_width > 0 ? desc->min_scene_width : 1) + padding * 2.0f;
    minimum_frame_height = (float)(desc->min_scene_height > 0 ? desc->min_scene_height : 1) +
        scene_header_height + padding * 2.0f;
    minimum_frame_width = henka_workspace_clamp_float(minimum_frame_width, 1.0f, content_width);
    minimum_frame_height = henka_workspace_clamp_float(minimum_frame_height, 1.0f, content_height);

    left_width = desc->left_dock_visible && desc->left_dock_width > 0.0f ?
        desc->left_dock_width : 0.0f;
    right_width = desc->right_dock_visible && desc->right_dock_width > 0.0f ?
        desc->right_dock_width : 0.0f;
    bottom_height = desc->bottom_dock_visible && desc->bottom_dock_height > 0.0f ?
        desc->bottom_dock_height : 0.0f;

    total_gap_width = (left_width > 0.0f ? gap : 0.0f) +
        (right_width > 0.0f ? gap : 0.0f);
    if (total_gap_width + minimum_frame_width > content_width)
    {
        gap = 0.0f;
        total_gap_width = 0.0f;
    }

    total_dock_width = left_width + right_width;
    if (total_dock_width > 0.0f &&
        total_dock_width + total_gap_width + minimum_frame_width > content_width)
    {
        float available_dock_width = content_width - minimum_frame_width - total_gap_width;
        float scale = available_dock_width > 0.0f ? available_dock_width / total_dock_width : 0.0f;
        left_width *= scale;
        right_width *= scale;
    }

    if (bottom_height > 0.0f &&
        bottom_height + gap + minimum_frame_height > content_height)
    {
        bottom_height = content_height - minimum_frame_height - gap;
        if (bottom_height < 0.0f)
        {
            bottom_height = 0.0f;
            gap = 0.0f;
        }
    }

    frame_width = content_width - left_width - right_width -
        (left_width > 0.0f ? gap : 0.0f) -
        (right_width > 0.0f ? gap : 0.0f);
    frame_height = content_height - bottom_height -
        (bottom_height > 0.0f ? gap : 0.0f);
    frame_width = henka_workspace_clamp_float(frame_width, 1.0f, content_width);
    frame_height = henka_workspace_clamp_float(frame_height, 1.0f, content_height);

    out_layout->left_dock = (henka_ui_rect){margin, margin, left_width, frame_height};
    out_layout->scene_frame = (henka_ui_rect)
    {
        margin + left_width + (left_width > 0.0f ? gap : 0.0f),
        margin,
        frame_width,
        frame_height
    };

    if (out_layout->scene_frame.x + out_layout->scene_frame.width > (float)desc->framebuffer_width - margin)
    {
        out_layout->scene_frame.width = (float)desc->framebuffer_width - margin - out_layout->scene_frame.x;
    }
    if (out_layout->scene_frame.y + out_layout->scene_frame.height > (float)desc->framebuffer_height - margin)
    {
        out_layout->scene_frame.height = (float)desc->framebuffer_height - margin - out_layout->scene_frame.y;
    }
    if (out_layout->scene_frame.width < 1.0f)
    {
        out_layout->scene_frame.width = 1.0f;
    }
    if (out_layout->scene_frame.height < 1.0f)
    {
        out_layout->scene_frame.height = 1.0f;
    }

    out_layout->right_dock = (henka_ui_rect)
    {
        out_layout->scene_frame.x + out_layout->scene_frame.width + (right_width > 0.0f ? gap : 0.0f),
        margin,
        right_width,
        frame_height
    };
    out_layout->bottom_dock = (henka_ui_rect)
    {
        margin,
        margin + frame_height + (bottom_height > 0.0f ? gap : 0.0f),
        content_width,
        bottom_height
    };

    viewport_left = (int)ceilf(out_layout->scene_frame.x + padding);
    viewport_top = (int)ceilf(out_layout->scene_frame.y + scene_header_height + padding);
    viewport_right = (int)floorf(out_layout->scene_frame.x + out_layout->scene_frame.width - padding);
    viewport_bottom = (int)floorf(out_layout->scene_frame.y + out_layout->scene_frame.height - padding);

    viewport_left = henka_workspace_clamp_int(viewport_left, 0, desc->framebuffer_width - 1);
    viewport_top = henka_workspace_clamp_int(viewport_top, 0, desc->framebuffer_height - 1);
    viewport_right = henka_workspace_clamp_int(viewport_right, viewport_left + 1, desc->framebuffer_width);
    viewport_bottom = henka_workspace_clamp_int(viewport_bottom, viewport_top + 1, desc->framebuffer_height);

    out_layout->scene_viewport = (henka_viewport)
    {
        viewport_left,
        viewport_top,
        viewport_right - viewport_left,
        viewport_bottom - viewport_top
    };

    return HENKA_SUCCESS;
}
