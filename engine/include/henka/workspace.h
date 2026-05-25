#ifndef HENKA_WORKSPACE_H
#define HENKA_WORKSPACE_H

#include <stdbool.h>

#include <henka/core.h>
#include <henka/math.h>
#include <henka/result.h>
#include <henka/ui.h>

typedef struct henka_workspace_desc
{
    int framebuffer_width;
    int framebuffer_height;
    float margin;
    float gap;
    float left_dock_width;
    float right_dock_width;
    float bottom_dock_height;
    float scene_header_height;
    float scene_padding;
    int min_scene_width;
    int min_scene_height;
    bool left_dock_visible;
    bool right_dock_visible;
    bool bottom_dock_visible;
} henka_workspace_desc;

typedef struct henka_workspace_layout
{
    henka_ui_rect left_dock;
    henka_ui_rect scene_frame;
    henka_ui_rect right_dock;
    henka_ui_rect bottom_dock;
    henka_viewport scene_viewport;
} henka_workspace_layout;

bool henka_viewport_is_valid(henka_viewport viewport);
float henka_viewport_get_aspect_ratio(henka_viewport viewport);
bool henka_viewport_contains_point(henka_viewport viewport, henka_vec2 window_point);
henka_result henka_viewport_window_to_local(henka_viewport viewport, henka_vec2 window_point, henka_vec2* out_local_point);
henka_result henka_workspace_layout_docked(const henka_workspace_desc* desc, henka_workspace_layout* out_layout);

#endif
