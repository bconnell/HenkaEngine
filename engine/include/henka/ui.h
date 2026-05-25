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

typedef struct henka_ui_frame_desc
{
    int framebuffer_width;
    int framebuffer_height;
    henka_vec2 mouse_position;
    bool mouse_left_down;
    bool mouse_left_pressed;
    bool mouse_left_released;
} henka_ui_frame_desc;

henka_result henka_ui_create(henka_ui_context** out_context);
void henka_ui_destroy(henka_ui_context* context);
henka_result henka_ui_begin_frame(henka_ui_context* context, const henka_ui_frame_desc* frame_desc);
henka_result henka_ui_end_frame(henka_ui_context* context);
void henka_ui_set_visible(henka_ui_context* context, bool visible);
bool henka_ui_is_visible(const henka_ui_context* context);
bool henka_ui_get_wants_mouse(const henka_ui_context* context);
size_t henka_ui_get_draw_rect_count(const henka_ui_context* context);
bool henka_ui_rect_contains(henka_ui_rect rect, henka_vec2 point);
henka_result henka_ui_measure_text(const char* text, float scale, int* out_width, int* out_height);
henka_result henka_ui_panel(henka_ui_context* context, henka_ui_rect bounds, const char* title);
henka_result henka_ui_viewport_frame(henka_ui_context* context, henka_ui_rect bounds, const char* title);
henka_result henka_ui_heading(henka_ui_context* context, float x, float y, float scale, const char* text);
henka_result henka_ui_label(henka_ui_context* context, float x, float y, float scale, const char* text);
henka_result henka_ui_value_row(henka_ui_context* context, henka_ui_rect bounds, const char* label, const char* value);
henka_result henka_ui_overlay_hint(henka_ui_context* context, henka_ui_rect bounds, const char* primary_text, const char* secondary_text);
bool henka_ui_button(henka_ui_context* context, const char* id, henka_ui_rect bounds, const char* label);
bool henka_ui_primary_button(henka_ui_context* context, const char* id, henka_ui_rect bounds, const char* label);
bool henka_ui_selectable(henka_ui_context* context, const char* id, henka_ui_rect bounds, const char* label, bool selected);
bool henka_ui_tab(henka_ui_context* context, const char* id, henka_ui_rect bounds, const char* label, bool selected);
bool henka_ui_toggle(henka_ui_context* context, const char* id, henka_ui_rect bounds, const char* label, bool* value);
henka_result henka_ui_status_chip(henka_ui_context* context, henka_ui_rect bounds, const char* label, bool warning);

#endif
