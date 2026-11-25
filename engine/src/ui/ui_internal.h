#ifndef HENKA_UI_INTERNAL_H
#define HENKA_UI_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <henka/ui.h>

#include "ui_font_internal.h"

#define HENKA_UI_MAX_NAVIGATION_ITEMS 256U
#define HENKA_UI_MAX_SCALE 64.0f
#define HENKA_UI_MAX_DRAW_ITEMS ((size_t)1048576U)

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

typedef enum henka_ui_draw_quad_kind
{
    HENKA_UI_DRAW_QUAD_SOLID = 0,
    HENKA_UI_DRAW_QUAD_GLYPH
} henka_ui_draw_quad_kind;

typedef struct henka_ui_draw_quad
{
    henka_ui_draw_quad_kind kind;
    henka_ui_rect bounds;
    henka_vec4 color;
    float u0;
    float v0;
    float u1;
    float v1;
} henka_ui_draw_quad;

typedef struct henka_ui_draw_vertex
{
    float x;
    float y;
    float r;
    float g;
    float b;
    float a;
    float u;
    float v;
    float glyph_coverage;
} henka_ui_draw_vertex;

typedef struct henka_ui_draw_line
{
    henka_vec2 start;
    henka_vec2 end;
    float thickness;
    henka_vec4 color;
} henka_ui_draw_line;

typedef struct henka_ui_draw_triangle
{
    henka_vec2 points[3];
    henka_vec4 color;
} henka_ui_draw_triangle;

struct henka_ui_context
{
    henka_ui_theme theme;
    const henka_ui_font_raster_profile* font_profile;
    uint16_t font_app_scale_percent;
    float output_scale;
    uint64_t font_context_identity;
    bool visible;
    bool frame_active;
    bool wants_mouse;
    int framebuffer_width;
    int framebuffer_height;
    henka_vec2 mouse_position;
    bool mouse_left_down;
    bool mouse_left_pressed;
    bool mouse_left_released;
    bool text_backspace_pressed;
    const char* text_input;
    size_t text_input_size;
    bool active_id_set;
    char active_id[256];
    bool focused_text_id_set;
    char focused_text_id[256];
    henka_ui_flow_state flow;
    char navigation_ids[HENKA_UI_MAX_NAVIGATION_ITEMS][256];
    size_t navigation_id_count;
    bool focused_navigation_id_set;
    char focused_navigation_id[256];
    bool navigation_up_pressed;
    bool navigation_down_pressed;
    bool navigation_left_pressed;
    bool navigation_right_pressed;
    bool navigation_enter_pressed;
    unsigned int consumed_navigation_mask;
    henka_ui_draw_rect* draw_rects;
    size_t draw_rect_count;
    size_t draw_rect_capacity;
    henka_ui_draw_quad* draw_quads;
    size_t draw_quad_count;
    size_t draw_quad_capacity;
    henka_ui_draw_line* draw_lines;
    size_t draw_line_count;
    size_t draw_line_capacity;
    henka_ui_draw_triangle* draw_triangles;
    size_t draw_triangle_count;
    size_t draw_triangle_capacity;
};

/* Profile and app scale are published together between frames only. */
henka_result henka_ui_context_set_font_profile(
    henka_ui_context* context,
    const henka_ui_font_raster_profile* profile,
    uint16_t app_scale_percent);

henka_result henka_ui_context_reserve_draw_quads(
    henka_ui_context* context,
    size_t additional_quads);

henka_result henka_ui_context_build_quad_vertices(
    const henka_ui_context* context,
    henka_ui_draw_vertex* out_vertices,
    size_t vertex_capacity,
    size_t* out_vertex_count);

/* Applies the active output transform once to a complete UI vertex batch. */
henka_result henka_ui_context_apply_output_transform(
    const henka_ui_context* context,
    henka_ui_draw_vertex* vertices,
    size_t vertex_count);

/* Converts physical input back to the logical coordinate space used by UI layout. */
henka_result henka_ui_context_framebuffer_to_logical(
    const henka_ui_context* context,
    henka_vec2 framebuffer_position,
    henka_vec2* out_logical_position);

/* Internal proof seam used by the public label path when a font is attached. */
henka_result henka_ui_draw_font_text(
    henka_ui_context* context,
    float x,
    float y,
    float scale,
    const char* text,
    henka_vec4 color);

henka_result henka_ui_draw_font_text_limited(
    henka_ui_context* context,
    float x,
    float y,
    float scale,
    const char* text,
    henka_vec4 color,
    float maximum_width,
    size_t maximum_line_count);

henka_result henka_ui_draw_font_text_centered_limited(
    henka_ui_context* context,
    float x,
    float center_y,
    float scale,
    const char* text,
    henka_vec4 color,
    float maximum_width,
    size_t maximum_line_count);

/* Shared one-line fit and visible-ink centering for semantic-color controls. */
henka_result henka_ui_draw_fit_semantic_text_centered(
    henka_ui_context* context,
    henka_ui_rect bounds,
    float padding_x,
    float scale,
    const char* text,
    henka_ui_semantic_color color);

/* Context metrics share the font/profile authority used by font drawing. */
henka_result henka_ui_measure_font_text(
    const henka_ui_context* context,
    const char* text,
    float scale,
    int* out_width,
    int* out_height);
henka_result henka_ui_measure_font_text_byte_offset_x(
    const henka_ui_context* context,
    const char* text,
    size_t text_byte_count,
    size_t byte_offset,
    float scale,
    float* out_x);
henka_result henka_ui_measure_font_text_byte_offsets_x(
    const henka_ui_context* context,
    const char* text,
    size_t text_byte_count,
    const size_t* byte_offsets,
    size_t byte_offset_count,
    float scale,
    float* out_x_positions);

henka_result henka_ui_text_layout_build_limited(
    const henka_ui_font_resource* resource,
    const char* utf8,
    size_t utf8_size,
    float max_width,
    size_t max_line_count,
    henka_ui_text_run* out_run,
    henka_ui_font_diagnostic* out_diagnostic);

#endif
