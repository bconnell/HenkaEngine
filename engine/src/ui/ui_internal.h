#ifndef HENKA_UI_INTERNAL_H
#define HENKA_UI_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

#include <henka/ui.h>

typedef struct henka_ui_draw_rect
{
    henka_ui_rect bounds;
    henka_vec4 color;
} henka_ui_draw_rect;

struct henka_ui_context
{
    bool visible;
    bool frame_active;
    bool wants_mouse;
    int framebuffer_width;
    int framebuffer_height;
    henka_vec2 mouse_position;
    bool mouse_left_down;
    bool mouse_left_pressed;
    bool mouse_left_released;
    const char* active_id;
    henka_ui_draw_rect* draw_rects;
    size_t draw_rect_count;
    size_t draw_rect_capacity;
};

#endif
