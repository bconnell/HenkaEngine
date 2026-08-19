#ifndef HENKA_UI_INTERNAL_H
#define HENKA_UI_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

#include <henka/ui.h>

#define HENKA_UI_MAX_DISCLOSURE_ROWS 256U

typedef struct henka_ui_flow_state
{
    bool active;
    henka_ui_rect bounds;
    float scroll_offset;
    float row_spacing;
    float indent_width;
    float cursor_y;
    float content_height;
} henka_ui_flow_state;
typedef struct henka_ui_draw_rect
{
    henka_ui_rect bounds;
    henka_vec4 color;
} henka_ui_draw_rect;

typedef struct henka_ui_draw_line
{
    henka_vec2 start;
    henka_vec2 end;
    float thickness;
    henka_vec4 color;
} henka_ui_draw_line;

struct henka_ui_context
{
    henka_ui_theme theme;
    bool visible;
    bool frame_active;
    bool wants_mouse;
    int framebuffer_width;
    int framebuffer_height;
    henka_vec2 mouse_position;
    bool mouse_left_down;
    bool mouse_left_pressed;
    bool mouse_left_released;
    bool active_id_set;
    char active_id[256];
    henka_ui_flow_state flow;
    char disclosure_ids[HENKA_UI_MAX_DISCLOSURE_ROWS][256];
    size_t disclosure_id_count;
    bool focused_disclosure_id_set;
    char focused_disclosure_id[256];
    bool navigation_up_pressed;
    bool navigation_down_pressed;
    bool navigation_left_pressed;
    bool navigation_right_pressed;
    bool navigation_enter_pressed;
    unsigned int consumed_navigation_mask;
    henka_ui_draw_rect* draw_rects;
    size_t draw_rect_count;
    size_t draw_rect_capacity;
    henka_ui_draw_line* draw_lines;
    size_t draw_line_count;
    size_t draw_line_capacity;
};

#endif
