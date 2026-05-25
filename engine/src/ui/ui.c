#include <henka/ui.h>

#include <ctype.h>
#include <string.h>

#include <henka/memory.h>

#include "ui_internal.h"

typedef struct henka_ui_glyph
{
    char character;
    unsigned char rows[7];
} henka_ui_glyph;

static const henka_ui_glyph g_ui_glyphs[] =
{
    { ' ', {0, 0, 0, 0, 0, 0, 0} },
    { '.', {0, 0, 0, 0, 0, 0x0C, 0x0C} },
    { ':', {0, 0x0C, 0x0C, 0, 0x0C, 0x0C, 0} },
    { '-', {0, 0, 0, 0x1E, 0, 0, 0} },
    { '_', {0, 0, 0, 0, 0, 0, 0x1F} },
    { '/', {0x01, 0x02, 0x04, 0x08, 0x10, 0, 0} },
    { '\\', {0x10, 0x08, 0x04, 0x02, 0x01, 0, 0} },
    { '(', {0x06, 0x08, 0x10, 0x10, 0x10, 0x08, 0x06} },
    { ')', {0x0C, 0x02, 0x01, 0x01, 0x01, 0x02, 0x0C} },
    { '0', {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E} },
    { '1', {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E} },
    { '2', {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F} },
    { '3', {0x1E, 0x01, 0x01, 0x06, 0x01, 0x01, 0x1E} },
    { '4', {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02} },
    { '5', {0x1F, 0x10, 0x10, 0x1E, 0x01, 0x01, 0x1E} },
    { '6', {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E} },
    { '7', {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08} },
    { '8', {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E} },
    { '9', {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C} },
    { 'A', {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11} },
    { 'B', {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E} },
    { 'C', {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E} },
    { 'D', {0x1E, 0x12, 0x11, 0x11, 0x11, 0x12, 0x1E} },
    { 'E', {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F} },
    { 'F', {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10} },
    { 'G', {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F} },
    { 'H', {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11} },
    { 'I', {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E} },
    { 'J', {0x1F, 0x02, 0x02, 0x02, 0x12, 0x12, 0x0C} },
    { 'K', {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11} },
    { 'L', {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F} },
    { 'M', {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11} },
    { 'N', {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11} },
    { 'O', {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E} },
    { 'P', {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10} },
    { 'Q', {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D} },
    { 'R', {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11} },
    { 'S', {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E} },
    { 'T', {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04} },
    { 'U', {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E} },
    { 'V', {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04} },
    { 'W', {0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A} },
    { 'X', {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11} },
    { 'Y', {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04} },
    { 'Z', {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F} },
    { '?', {0x0E, 0x11, 0x01, 0x02, 0x04, 0, 0x04} }
};

static const henka_vec4 g_ui_panel_fill = {0.08f, 0.10f, 0.14f, 0.92f};
static const henka_vec4 g_ui_panel_border = {0.22f, 0.36f, 0.56f, 1.0f};
static const henka_vec4 g_ui_text_color = {0.94f, 0.96f, 0.99f, 1.0f};
static const henka_vec4 g_ui_heading_color = {0.72f, 0.84f, 0.98f, 1.0f};
static const henka_vec4 g_ui_button_fill = {0.17f, 0.21f, 0.28f, 0.98f};
static const henka_vec4 g_ui_button_hover = {0.25f, 0.31f, 0.41f, 1.0f};
static const henka_vec4 g_ui_button_active = {0.33f, 0.40f, 0.52f, 1.0f};
static const henka_vec4 g_ui_toggle_on = {0.18f, 0.58f, 0.40f, 1.0f};
static const henka_vec4 g_ui_toggle_off = {0.44f, 0.20f, 0.20f, 1.0f};
static const henka_vec4 g_ui_value_fill = {0.11f, 0.15f, 0.21f, 1.0f};

static char henka_ui_normalize_character(char character)
{
    if (character >= 'a' && character <= 'z')
    {
        return (char)(character - 'a' + 'A');
    }

    return character;
}

static const henka_ui_glyph* henka_ui_find_glyph(char character)
{
    size_t index;
    char normalized_character;

    normalized_character = henka_ui_normalize_character(character);
    for (index = 0U; index < sizeof(g_ui_glyphs) / sizeof(g_ui_glyphs[0]); ++index)
    {
        if (g_ui_glyphs[index].character == normalized_character)
        {
            return &g_ui_glyphs[index];
        }
    }

    for (index = 0U; index < sizeof(g_ui_glyphs) / sizeof(g_ui_glyphs[0]); ++index)
    {
        if (g_ui_glyphs[index].character == '?')
        {
            return &g_ui_glyphs[index];
        }
    }

    return NULL;
}

static henka_result henka_ui_ensure_rect_capacity(henka_ui_context* context, size_t additional_rects)
{
    henka_ui_draw_rect* rects;
    size_t minimum_capacity;
    size_t new_capacity;

    if (context == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    minimum_capacity = context->draw_rect_count + additional_rects;
    if (minimum_capacity <= context->draw_rect_capacity)
    {
        return HENKA_SUCCESS;
    }

    new_capacity = context->draw_rect_capacity == 0U ? 128U : context->draw_rect_capacity;
    while (new_capacity < minimum_capacity)
    {
        new_capacity *= 2U;
    }

    rects = henka_realloc(context->draw_rects, new_capacity * sizeof(*rects));
    if (rects == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }

    context->draw_rects = rects;
    context->draw_rect_capacity = new_capacity;
    return HENKA_SUCCESS;
}

static henka_result henka_ui_push_rect(henka_ui_context* context, henka_ui_rect bounds, henka_vec4 color)
{
    henka_result result;

    if (context == NULL || bounds.width <= 0.0f || bounds.height <= 0.0f)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    result = henka_ui_ensure_rect_capacity(context, 1U);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    context->draw_rects[context->draw_rect_count].bounds = bounds;
    context->draw_rects[context->draw_rect_count].color = color;
    context->draw_rect_count += 1U;
    return HENKA_SUCCESS;
}

static henka_result henka_ui_push_border(henka_ui_context* context, henka_ui_rect bounds, float thickness, henka_vec4 color)
{
    henka_result result;

    result = henka_ui_push_rect(context, (henka_ui_rect){bounds.x, bounds.y, bounds.width, thickness}, color);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    result = henka_ui_push_rect(context, (henka_ui_rect){bounds.x, bounds.y + bounds.height - thickness, bounds.width, thickness}, color);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    result = henka_ui_push_rect(context, (henka_ui_rect){bounds.x, bounds.y, thickness, bounds.height}, color);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    return henka_ui_push_rect(context, (henka_ui_rect){bounds.x + bounds.width - thickness, bounds.y, thickness, bounds.height}, color);
}

static henka_result henka_ui_draw_text(henka_ui_context* context, float x, float y, float scale, const char* text, henka_vec4 color)
{
    float cursor_x;
    float cursor_y;
    size_t index;

    if (context == NULL || text == NULL || scale <= 0.0f)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    cursor_x = x;
    cursor_y = y;

    for (index = 0U; text[index] != '\0'; ++index)
    {
        const henka_ui_glyph* glyph;
        int column;
        int row;

        if (text[index] == '\n')
        {
            cursor_x = x;
            cursor_y += 8.0f * scale;
            continue;
        }

        glyph = henka_ui_find_glyph(text[index]);
        if (glyph == NULL)
        {
            continue;
        }

        for (row = 0; row < 7; ++row)
        {
            for (column = 0; column < 5; ++column)
            {
                unsigned char mask;

                mask = (unsigned char)(1U << (4 - column));
                if ((glyph->rows[row] & mask) != 0U)
                {
                    henka_result result;

                    result = henka_ui_push_rect(
                        context,
                        (henka_ui_rect){cursor_x + (float)column * scale, cursor_y + (float)row * scale, scale, scale},
                        color);
                    if (result != HENKA_SUCCESS)
                    {
                        return result;
                    }
                }
            }
        }

        cursor_x += 6.0f * scale;
    }

    return HENKA_SUCCESS;
}

static bool henka_ui_control_is_hot(henka_ui_context* context, henka_ui_rect bounds)
{
    if (context == NULL || !context->visible)
    {
        return false;
    }

    if (henka_ui_rect_contains(bounds, context->mouse_position))
    {
        context->wants_mouse = true;
        return true;
    }

    return false;
}

static bool henka_ui_id_equals(const char* left, const char* right)
{
    if (left == NULL || right == NULL)
    {
        return false;
    }

    return strcmp(left, right) == 0;
}

henka_result henka_ui_create(henka_ui_context** out_context)
{
    henka_ui_context* context;

    if (out_context == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    *out_context = NULL;
    context = henka_calloc(1U, sizeof(*context));
    if (context == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }

    *out_context = context;
    return HENKA_SUCCESS;
}

void henka_ui_destroy(henka_ui_context* context)
{
    if (context == NULL)
    {
        return;
    }

    henka_free(context->draw_rects);
    henka_free(context);
}

henka_result henka_ui_begin_frame(henka_ui_context* context, const henka_ui_frame_desc* frame_desc)
{
    if (context == NULL || frame_desc == NULL || frame_desc->framebuffer_width <= 0 || frame_desc->framebuffer_height <= 0)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    context->frame_active = true;
    context->wants_mouse = false;
    context->framebuffer_width = frame_desc->framebuffer_width;
    context->framebuffer_height = frame_desc->framebuffer_height;
    context->mouse_position = frame_desc->mouse_position;
    context->mouse_left_down = frame_desc->mouse_left_down;
    context->mouse_left_pressed = frame_desc->mouse_left_pressed;
    context->mouse_left_released = frame_desc->mouse_left_released;
    context->draw_rect_count = 0U;
    if (!context->mouse_left_down && !context->mouse_left_released)
    {
        context->active_id = NULL;
    }
    return HENKA_SUCCESS;
}

henka_result henka_ui_end_frame(henka_ui_context* context)
{
    if (context == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    context->frame_active = false;
    return HENKA_SUCCESS;
}

void henka_ui_set_visible(henka_ui_context* context, bool visible)
{
    if (context != NULL)
    {
        context->visible = visible;
        if (!visible)
        {
            context->active_id = NULL;
            context->wants_mouse = false;
        }
    }
}

bool henka_ui_is_visible(const henka_ui_context* context)
{
    return context != NULL && context->visible;
}

bool henka_ui_get_wants_mouse(const henka_ui_context* context)
{
    return context != NULL && context->wants_mouse;
}

bool henka_ui_rect_contains(henka_ui_rect rect, henka_vec2 point)
{
    return point.x >= rect.x &&
        point.y >= rect.y &&
        point.x < rect.x + rect.width &&
        point.y < rect.y + rect.height;
}

henka_result henka_ui_measure_text(const char* text, float scale, int* out_width, int* out_height)
{
    int current_width;
    int line_width;
    int lines;
    size_t index;

    if (text == NULL || out_width == NULL || out_height == NULL || scale <= 0.0f)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    line_width = 0;
    current_width = 0;
    lines = 1;

    for (index = 0U; text[index] != '\0'; ++index)
    {
        if (text[index] == '\n')
        {
            if (line_width > current_width)
            {
                current_width = line_width;
            }
            line_width = 0;
            lines += 1;
            continue;
        }

        line_width += (int)(6.0f * scale);
    }

    if (line_width > current_width)
    {
        current_width = line_width;
    }

    *out_width = current_width > 0 ? current_width - (int)scale : 0;
    *out_height = (int)(lines * 7.0f * scale + (lines - 1) * scale);
    return HENKA_SUCCESS;
}

henka_result henka_ui_panel(henka_ui_context* context, henka_ui_rect bounds, const char* title)
{
    henka_result result;

    if (context == NULL || title == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    if (!context->visible)
    {
        return HENKA_SUCCESS;
    }

    if (henka_ui_rect_contains(bounds, context->mouse_position))
    {
        context->wants_mouse = true;
    }

    result = henka_ui_push_rect(context, bounds, g_ui_panel_fill);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    result = henka_ui_push_border(context, bounds, 2.0f, g_ui_panel_border);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    return henka_ui_draw_text(context, bounds.x + 12.0f, bounds.y + 10.0f, 2.0f, title, g_ui_text_color);
}

henka_result henka_ui_label(henka_ui_context* context, float x, float y, float scale, const char* text)
{
    if (context == NULL || text == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    if (!context->visible)
    {
        return HENKA_SUCCESS;
    }

    return henka_ui_draw_text(context, x, y, scale, text, g_ui_text_color);
}

bool henka_ui_button(henka_ui_context* context, const char* id, henka_ui_rect bounds, const char* label)
{
    bool clicked;
    bool hot;
    bool active;
    henka_result result;

    if (context == NULL || id == NULL || label == NULL || !context->visible)
    {
        return false;
    }

    (void)id;

    hot = henka_ui_control_is_hot(context, bounds);
    if (hot && context->mouse_left_pressed)
    {
        context->active_id = id;
    }

    active = context->mouse_left_down && henka_ui_id_equals(context->active_id, id);
    clicked = hot && context->mouse_left_pressed;
    if (context->mouse_left_released && henka_ui_id_equals(context->active_id, id))
    {
        context->active_id = NULL;
    }

    result = henka_ui_push_rect(context, bounds, active ? g_ui_button_active : (hot ? g_ui_button_hover : g_ui_button_fill));
    if (result != HENKA_SUCCESS)
    {
        return false;
    }

    result = henka_ui_push_border(context, bounds, 1.0f, g_ui_panel_border);
    if (result != HENKA_SUCCESS)
    {
        return false;
    }

    if (henka_ui_draw_text(context, bounds.x + 10.0f, bounds.y + 9.0f, 1.0f, label, g_ui_text_color) != HENKA_SUCCESS)
    {
        return false;
    }

    return clicked;
}

bool henka_ui_toggle(henka_ui_context* context, const char* id, henka_ui_rect bounds, const char* label, bool* value)
{
    bool clicked;
    bool hot;
    henka_result result;
    henka_ui_rect checkbox_bounds;
    henka_ui_rect value_bounds;

    if (context == NULL || id == NULL || label == NULL || value == NULL || !context->visible)
    {
        return false;
    }

    (void)id;

    hot = henka_ui_control_is_hot(context, bounds);
    if (hot && context->mouse_left_pressed)
    {
        context->active_id = id;
    }

    clicked = hot && context->mouse_left_pressed;
    if (context->mouse_left_released && henka_ui_id_equals(context->active_id, id))
    {
        context->active_id = NULL;
    }
    if (clicked)
    {
        *value = !*value;
    }

    result = henka_ui_push_rect(context, bounds, hot ? g_ui_button_hover : g_ui_button_fill);
    if (result != HENKA_SUCCESS)
    {
        return false;
    }

    result = henka_ui_push_border(context, bounds, 1.0f, g_ui_panel_border);
    if (result != HENKA_SUCCESS)
    {
        return false;
    }

    checkbox_bounds = (henka_ui_rect){bounds.x + 8.0f, bounds.y + 6.0f, 20.0f, 18.0f};
    result = henka_ui_push_rect(context, checkbox_bounds, *value ? g_ui_toggle_on : g_ui_toggle_off);
    if (result != HENKA_SUCCESS)
    {
        return false;
    }

    result = henka_ui_push_border(context, checkbox_bounds, 1.0f, g_ui_text_color);
    if (result != HENKA_SUCCESS)
    {
        return false;
    }

    value_bounds = (henka_ui_rect){bounds.x + bounds.width - 48.0f, bounds.y + 6.0f, 38.0f, 18.0f};
    result = henka_ui_push_rect(context, value_bounds, g_ui_value_fill);
    if (result != HENKA_SUCCESS)
    {
        return false;
    }

    result = henka_ui_push_border(context, value_bounds, 1.0f, g_ui_panel_border);
    if (result != HENKA_SUCCESS)
    {
        return false;
    }

    if (henka_ui_draw_text(context, bounds.x + 36.0f, bounds.y + 9.0f, 1.0f, label, g_ui_text_color) != HENKA_SUCCESS)
    {
        return false;
    }

    if (henka_ui_draw_text(context, value_bounds.x + 7.0f, value_bounds.y + 5.0f, 1.0f, *value ? "ON" : "OFF", g_ui_heading_color) != HENKA_SUCCESS)
    {
        return false;
    }

    return clicked;
}
