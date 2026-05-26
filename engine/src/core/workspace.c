#include <henka/workspace.h>

static float henka_max_float(float left, float right)
{
    return left > right ? left : right;
}

static int henka_max_int(int left, int right)
{
    return left > right ? left : right;
}

static float henka_min_float(float left, float right)
{
    return left < right ? left : right;
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
    if (!henka_viewport_is_valid(viewport))
    {
        return false;
    }

    return window_point.x >= (float)viewport.x &&
        window_point.y >= (float)viewport.y &&
        window_point.x < (float)(viewport.x + viewport.width) &&
        window_point.y < (float)(viewport.y + viewport.height);
}

henka_result henka_window_point_to_framebuffer_point(
    int window_width,
    int window_height,
    int framebuffer_width,
    int framebuffer_height,
    henka_vec2 window_point,
    henka_vec2* out_framebuffer_point)
{
    if (out_framebuffer_point == NULL ||
        window_width <= 0 ||
        window_height <= 0 ||
        framebuffer_width <= 0 ||
        framebuffer_height <= 0)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    out_framebuffer_point->x = (window_point.x / (float)window_width) * (float)framebuffer_width;
    out_framebuffer_point->y = (window_point.y / (float)window_height) * (float)framebuffer_height;
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
    float left_width;
    float margin;
    float min_scene_height;
    float min_scene_width;
    float padding;
    float right_width;
    float scene_header_height;
    float top_y;
    float total_gap_width;
    int viewport_height;
    int viewport_width;
    int viewport_x;
    int viewport_y;

    if (desc == NULL || out_layout == NULL || desc->framebuffer_width <= 0 || desc->framebuffer_height <= 0)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    margin = desc->margin > 0.0f ? desc->margin : 0.0f;
    padding = desc->scene_padding > 0.0f ? desc->scene_padding : 0.0f;
    scene_header_height = desc->scene_header_height > 0.0f ? desc->scene_header_height : 0.0f;
    min_scene_width = (float)henka_max_int(desc->min_scene_width, 1);
    min_scene_height = (float)henka_max_int(desc->min_scene_height, 1);

    content_width = (float)desc->framebuffer_width - margin * 2.0f;
    content_height = (float)desc->framebuffer_height - margin * 2.0f;
    if (content_width < min_scene_width)
    {
        content_width = min_scene_width;
    }
    if (content_height < min_scene_height)
    {
        content_height = min_scene_height;
    }

    left_width = desc->left_dock_visible ? henka_max_float(desc->left_dock_width, 0.0f) : 0.0f;
    right_width = desc->right_dock_visible ? henka_max_float(desc->right_dock_width, 0.0f) : 0.0f;
    bottom_height = desc->bottom_dock_visible ? henka_max_float(desc->bottom_dock_height, 0.0f) : 0.0f;

    total_gap_width = 0.0f;
    if (left_width > 0.0f)
    {
        total_gap_width += desc->gap;
    }
    if (right_width > 0.0f)
    {
        total_gap_width += desc->gap;
    }

    if (left_width + right_width + total_gap_width + min_scene_width > content_width)
    {
        float available_dock_width;
        float total_dock_width;

        available_dock_width = content_width - min_scene_width - total_gap_width;
        if (available_dock_width < 0.0f)
        {
            available_dock_width = 0.0f;
        }

        total_dock_width = left_width + right_width;
        if (total_dock_width > 0.0f && available_dock_width < total_dock_width)
        {
            float scale;

            scale = available_dock_width / total_dock_width;
            left_width *= scale;
            right_width *= scale;
        }
    }

    top_y = margin;
    if (bottom_height > 0.0f && bottom_height + desc->gap + min_scene_height > content_height)
    {
        bottom_height = henka_max_float(0.0f, content_height - desc->gap - min_scene_height);
    }

    frame_width = content_width - left_width - right_width;
    if (left_width > 0.0f)
    {
        frame_width -= desc->gap;
    }
    if (right_width > 0.0f)
    {
        frame_width -= desc->gap;
    }
    if (frame_width < min_scene_width)
    {
        frame_width = min_scene_width;
    }

    frame_height = content_height - bottom_height;
    if (bottom_height > 0.0f)
    {
        frame_height -= desc->gap;
    }
    if (frame_height < min_scene_height + scene_header_height + padding * 2.0f)
    {
        frame_height = min_scene_height + scene_header_height + padding * 2.0f;
    }

    out_layout->left_dock = (henka_ui_rect){margin, margin, left_width, frame_height};
    out_layout->scene_frame = (henka_ui_rect)
    {
        margin + left_width + (left_width > 0.0f ? desc->gap : 0.0f),
        top_y,
        frame_width,
        frame_height
    };
    out_layout->right_dock = (henka_ui_rect)
    {
        out_layout->scene_frame.x + out_layout->scene_frame.width + (right_width > 0.0f ? desc->gap : 0.0f),
        top_y,
        right_width,
        frame_height
    };
    out_layout->bottom_dock = (henka_ui_rect)
    {
        margin,
        top_y + frame_height + (bottom_height > 0.0f ? desc->gap : 0.0f),
        content_width,
        bottom_height
    };

    viewport_x = (int)(out_layout->scene_frame.x + padding);
    viewport_y = (int)(out_layout->scene_frame.y + scene_header_height + padding);
    viewport_width = (int)(out_layout->scene_frame.width - padding * 2.0f);
    viewport_height = (int)(out_layout->scene_frame.height - scene_header_height - padding * 2.0f);
    if (viewport_width < 1)
    {
        viewport_width = 1;
    }
    if (viewport_height < 1)
    {
        viewport_height = 1;
    }

    out_layout->scene_viewport = (henka_viewport){viewport_x, viewport_y, viewport_width, viewport_height};
    out_layout->scene_frame.width = henka_min_float(out_layout->scene_frame.width, (float)desc->framebuffer_width - out_layout->scene_frame.x - margin);
    out_layout->scene_frame.height = henka_min_float(out_layout->scene_frame.height, (float)desc->framebuffer_height - out_layout->scene_frame.y - margin);
    return HENKA_SUCCESS;
}
