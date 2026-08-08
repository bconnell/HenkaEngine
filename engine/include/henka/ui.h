#ifndef HENKA_UI_H
#define HENKA_UI_H

#include <stdbool.h>
#include <stddef.h>

#include <henka/math.h>
#include <henka/result.h>

typedef struct henka_ui_context henka_ui_context;

typedef struct henka_ui_rect
{
    float x;
    float y;
    float width;
    float height;
} henka_ui_rect;

typedef struct henka_ui_flow_desc
{
    henka_ui_rect bounds;
    float scroll_offset;
    float row_spacing;
    float indent_width;
} henka_ui_flow_desc;
typedef enum henka_ui_semantic_color
{
    HENKA_UI_COLOR_NORMAL = 0,
    HENKA_UI_COLOR_MUTED,
    HENKA_UI_COLOR_INFO,
    HENKA_UI_COLOR_ACCENT,
    HENKA_UI_COLOR_SUCCESS,
    HENKA_UI_COLOR_WARNING,
    HENKA_UI_COLOR_ORANGE,
    HENKA_UI_COLOR_DANGER,
    HENKA_UI_COLOR_DISABLED
} henka_ui_semantic_color;

typedef enum henka_ui_navigation_mask
{
    HENKA_UI_NAVIGATION_NONE = 0,
    HENKA_UI_NAVIGATION_UP = 1U << 0,
    HENKA_UI_NAVIGATION_DOWN = 1U << 1,
    HENKA_UI_NAVIGATION_LEFT = 1U << 2,
    HENKA_UI_NAVIGATION_RIGHT = 1U << 3,
    HENKA_UI_NAVIGATION_ENTER = 1U << 4
} henka_ui_navigation_mask;
typedef struct henka_ui_frame_desc
{
    int framebuffer_width;
    int framebuffer_height;
    henka_vec2 mouse_position;
    bool mouse_left_down;
    bool mouse_left_pressed;
    bool mouse_left_released;
    bool navigation_up_pressed;
    bool navigation_down_pressed;
    bool navigation_left_pressed;
    bool navigation_right_pressed;
    bool navigation_enter_pressed;
} henka_ui_frame_desc;

henka_result henka_ui_flow_begin(
    henka_ui_context* context,
    const henka_ui_flow_desc* desc);
henka_result henka_ui_flow_next_row(
    henka_ui_context* context,
    float row_height,
    size_t indent_level,
    henka_ui_rect* out_bounds,
    bool* out_visible);
henka_result henka_ui_flow_end(
    henka_ui_context* context,
    float* out_content_height);
henka_result henka_ui_disclosure_row(
    henka_ui_context* context,
    const char* id,
    henka_ui_rect bounds,
    const char* label,
    bool* expanded,
    bool* out_changed);
henka_result henka_ui_create(henka_ui_context** out_context);
void henka_ui_destroy(henka_ui_context* context);
henka_result henka_ui_begin_frame(henka_ui_context* context, const henka_ui_frame_desc* frame_desc);
henka_result henka_ui_end_frame(henka_ui_context* context);
void henka_ui_set_visible(henka_ui_context* context, bool visible);
bool henka_ui_is_visible(const henka_ui_context* context);
bool henka_ui_get_wants_mouse(const henka_ui_context* context);
unsigned int henka_ui_get_consumed_navigation_mask(
    const henka_ui_context* context);
size_t henka_ui_get_draw_rect_count(const henka_ui_context* context);
size_t henka_ui_get_draw_line_count(const henka_ui_context* context);
bool henka_ui_rect_contains(henka_ui_rect rect, henka_vec2 point);
henka_result henka_ui_measure_text(const char* text, float scale, int* out_width, int* out_height);
henka_result henka_ui_overlay_rect(henka_ui_context* context, henka_ui_rect bounds, henka_vec4 color);
henka_result henka_ui_overlay_line(henka_ui_context* context, henka_vec2 start, henka_vec2 end, float thickness, henka_vec4 color);
henka_result henka_ui_overlay_polyline(
    henka_ui_context* context,
    const henka_vec2* points,
    size_t point_count,
    float thickness,
    henka_vec4 color);
henka_result henka_ui_panel(henka_ui_context* context, henka_ui_rect bounds, const char* title);
henka_result henka_ui_viewport_frame(henka_ui_context* context, henka_ui_rect bounds, const char* title);
henka_result henka_ui_heading(henka_ui_context* context, float x, float y, float scale, const char* text);
henka_result henka_ui_label(henka_ui_context* context, float x, float y, float scale, const char* text);
henka_result henka_ui_label_colored(
    henka_ui_context* context,
    float x,
    float y,
    float scale,
    const char* text,
    henka_ui_semantic_color color);
henka_result henka_ui_value_row(henka_ui_context* context, henka_ui_rect bounds, const char* label, const char* value);
henka_result henka_ui_value_row_colored(
    henka_ui_context* context,
    henka_ui_rect bounds,
    const char* label,
    const char* value,
    henka_ui_semantic_color label_color,
    henka_ui_semantic_color value_color);
henka_result henka_ui_overlay_hint(henka_ui_context* context, henka_ui_rect bounds, const char* primary_text, const char* secondary_text);
bool henka_ui_button(henka_ui_context* context, const char* id, henka_ui_rect bounds, const char* label);
bool henka_ui_primary_button(henka_ui_context* context, const char* id, henka_ui_rect bounds, const char* label);
bool henka_ui_selectable(henka_ui_context* context, const char* id, henka_ui_rect bounds, const char* label, bool selected);
bool henka_ui_tab(henka_ui_context* context, const char* id, henka_ui_rect bounds, const char* label, bool selected);
bool henka_ui_toggle(henka_ui_context* context, const char* id, henka_ui_rect bounds, const char* label, bool* value);
henka_result henka_ui_status_chip(henka_ui_context* context, henka_ui_rect bounds, const char* label, bool warning);

#endif
