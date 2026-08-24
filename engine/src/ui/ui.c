#include <henka/ui.h>

#include <ctype.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include <henka/core.h>
#include <henka/memory.h>

#include "../core/checked.h"
#include "ui_internal.h"

typedef struct henka_ui_glyph
{
    char character;
    unsigned char rows[7];
} henka_ui_glyph;

#define HENKA_UI_MAX_DRAW_ITEMS ((size_t)1048576U)
#define HENKA_UI_MAX_ID_BYTES ((size_t)255U)
#define HENKA_UI_MAX_TEXT_BYTES ((size_t)4096U)
#define HENKA_UI_MAX_SCALE 64.0f
#define HENKA_UI_OVERLAY_DISC_SAMPLES ((size_t)64U)

static bool henka_ui_float_is_finite(float value)
{
    return isfinite(value) != 0;
}

static bool henka_ui_vec2_is_finite(henka_vec2 value)
{
    return henka_ui_float_is_finite(value.x) && henka_ui_float_is_finite(value.y);
}

static bool henka_ui_vec4_is_finite(henka_vec4 value)
{
    return henka_ui_float_is_finite(value.x) &&
        henka_ui_float_is_finite(value.y) &&
        henka_ui_float_is_finite(value.z) &&
        henka_ui_float_is_finite(value.w);
}

static bool henka_ui_rect_is_finite(henka_ui_rect rect)
{
    return henka_ui_float_is_finite(rect.x) &&
        henka_ui_float_is_finite(rect.y) &&
        henka_ui_float_is_finite(rect.width) &&
        henka_ui_float_is_finite(rect.height) &&
        henka_ui_float_is_finite(rect.x + rect.width) &&
        henka_ui_float_is_finite(rect.y + rect.height);
}

static void henka_ui_clear_active_id(henka_ui_context* context)
{
    if (context == NULL)
    {
        return;
    }

    context->active_id_set = false;
    context->active_id[0] = '\0';
}

static bool henka_ui_set_active_id(henka_ui_context* context, const char* id)
{
    size_t length;

    if (context == NULL ||
        !henka_checked_c_string_length(id, HENKA_UI_MAX_ID_BYTES, &length))
    {
        return false;
    }

    memcpy(context->active_id, id, length + 1U);
    context->active_id_set = true;
    return true;
}

static bool henka_ui_active_id_equals(const henka_ui_context* context, const char* id)
{
    size_t length;

    if (context == NULL || !context->active_id_set ||
        !henka_checked_c_string_length(id, HENKA_UI_MAX_ID_BYTES, &length))
    {
        return false;
    }

    return strcmp(context->active_id, id) == 0;
}

static void henka_ui_clear_focused_text_id(henka_ui_context* context)
{
    if (context == NULL)
    {
        return;
    }
    context->focused_text_id_set = false;
    context->focused_text_id[0] = '\0';
}

static bool henka_ui_set_focused_text_id(henka_ui_context* context, const char* id)
{
    size_t length;

    if (context == NULL ||
        !henka_checked_c_string_length(id, HENKA_UI_MAX_ID_BYTES, &length))
    {
        return false;
    }
    memcpy(context->focused_text_id, id, length + 1U);
    context->focused_text_id_set = true;
    return true;
}

static bool henka_ui_focused_text_id_equals(const henka_ui_context* context, const char* id)
{
    size_t length;

    if (context == NULL || !context->focused_text_id_set ||
        !henka_checked_c_string_length(id, HENKA_UI_MAX_ID_BYTES, &length))
    {
        return false;
    }
    return strcmp(context->focused_text_id, id) == 0;
}

static bool henka_ui_register_disclosure_id(
    henka_ui_context* context,
    const char* id)
{
    size_t index;
    size_t length;

    if (context == NULL ||
        !henka_checked_c_string_length(
            id,
            HENKA_UI_MAX_ID_BYTES,
            &length) ||
        length == 0U ||
        context->disclosure_id_count >=
            HENKA_UI_MAX_DISCLOSURE_ROWS)
    {
        return false;
    }

    for (index = 0U;
         index < context->disclosure_id_count;
         ++index)
    {
        if (strcmp(context->disclosure_ids[index], id) == 0)
        {
            return false;
        }
    }

    memcpy(
        context->disclosure_ids[context->disclosure_id_count],
        id,
        length + 1U);
    context->disclosure_id_count += 1U;
    return true;
}

static void henka_ui_clear_focused_disclosure_id(
    henka_ui_context* context)
{
    if (context == NULL)
    {
        return;
    }

    context->focused_disclosure_id_set = false;
    context->focused_disclosure_id[0] = '\0';
}

static bool henka_ui_set_focused_disclosure_id(
    henka_ui_context* context,
    const char* id)
{
    size_t length;

    if (context == NULL ||
        !henka_checked_c_string_length(
            id,
            HENKA_UI_MAX_ID_BYTES,
            &length) ||
        length == 0U)
    {
        return false;
    }

    memcpy(
        context->focused_disclosure_id,
        id,
        length + 1U);
    context->focused_disclosure_id_set = true;
    return true;
}

static bool henka_ui_focused_disclosure_id_equals(
    const henka_ui_context* context,
    const char* id)
{
    size_t length;

    if (context == NULL ||
        !context->focused_disclosure_id_set ||
        !henka_checked_c_string_length(
            id,
            HENKA_UI_MAX_ID_BYTES,
            &length) ||
        length == 0U)
    {
        return false;
    }

    return strcmp(context->focused_disclosure_id, id) == 0;
}

static bool henka_ui_find_focused_disclosure_index(
    const henka_ui_context* context,
    size_t* out_index)
{
    size_t index;

    if (out_index != NULL)
    {
        *out_index = 0U;
    }

    if (context == NULL ||
        out_index == NULL ||
        !context->focused_disclosure_id_set)
    {
        return false;
    }

    for (index = 0U;
         index < context->disclosure_id_count;
         ++index)
    {
        if (strcmp(
                context->disclosure_ids[index],
                context->focused_disclosure_id) == 0)
        {
            *out_index = index;
            return true;
        }
    }

    return false;
}

static void henka_ui_finalize_disclosure_navigation(
    henka_ui_context* context)
{
    size_t focused_index;

    if (context == NULL ||
        !context->focused_disclosure_id_set)
    {
        return;
    }

    if (!henka_ui_find_focused_disclosure_index(
            context,
            &focused_index))
    {
        henka_ui_clear_focused_disclosure_id(context);
        return;
    }

    if (context->navigation_up_pressed)
    {
        context->consumed_navigation_mask |=
            HENKA_UI_NAVIGATION_UP;
        if (focused_index > 0U)
        {
            focused_index -= 1U;
            (void)henka_ui_set_focused_disclosure_id(
                context,
                context->disclosure_ids[focused_index]);
        }
    }

    if (context->navigation_down_pressed)
    {
        context->consumed_navigation_mask |=
            HENKA_UI_NAVIGATION_DOWN;
        if (focused_index + 1U <
            context->disclosure_id_count)
        {
            focused_index += 1U;
            (void)henka_ui_set_focused_disclosure_id(
                context,
                context->disclosure_ids[focused_index]);
        }
    }
}
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
    { 'a', {0, 0, 0x0E, 0x01, 0x0F, 0x11, 0x0F} },
    { 'b', {0x10, 0x10, 0x1E, 0x11, 0x11, 0x11, 0x1E} },
    { 'c', {0, 0, 0x0E, 0x10, 0x10, 0x11, 0x0E} },
    { 'd', {0x01, 0x01, 0x0F, 0x11, 0x11, 0x11, 0x0F} },
    { 'e', {0, 0, 0x0E, 0x11, 0x1F, 0x10, 0x0E} },
    { 'f', {0x06, 0x08, 0x1E, 0x08, 0x08, 0x08, 0x08} },
    { 'g', {0, 0, 0x0F, 0x11, 0x0F, 0x01, 0x0E} },
    { 'h', {0x10, 0x10, 0x1E, 0x11, 0x11, 0x11, 0x11} },
    { 'i', {0x04, 0, 0x0C, 0x04, 0x04, 0x04, 0x0E} },
    { 'j', {0x02, 0, 0x06, 0x02, 0x02, 0x12, 0x0C} },
    { 'k', {0x10, 0x10, 0x12, 0x14, 0x18, 0x14, 0x12} },
    { 'l', {0x0C, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E} },
    { 'm', {0, 0, 0x1A, 0x15, 0x15, 0x15, 0x15} },
    { 'n', {0, 0, 0x1E, 0x11, 0x11, 0x11, 0x11} },
    { 'o', {0, 0, 0x0E, 0x11, 0x11, 0x11, 0x0E} },
    { 'p', {0, 0, 0x1E, 0x11, 0x1E, 0x10, 0x10} },
    { 'q', {0, 0, 0x0F, 0x11, 0x0F, 0x01, 0x01} },
    { 'r', {0, 0, 0x16, 0x19, 0x10, 0x10, 0x10} },
    { 's', {0, 0, 0x0F, 0x10, 0x0E, 0x01, 0x1E} },
    { 't', {0x08, 0x08, 0x1E, 0x08, 0x08, 0x09, 0x06} },
    { 'u', {0, 0, 0x11, 0x11, 0x11, 0x13, 0x0D} },
    { 'v', {0, 0, 0x11, 0x11, 0x11, 0x0A, 0x04} },
    { 'w', {0, 0, 0x11, 0x11, 0x15, 0x15, 0x0A} },
    { 'x', {0, 0, 0x11, 0x0A, 0x04, 0x0A, 0x11} },
    { 'y', {0, 0, 0x11, 0x11, 0x0F, 0x01, 0x0E} },
    { 'z', {0, 0, 0x1F, 0x02, 0x04, 0x08, 0x1F} },
    { ',', {0, 0, 0, 0, 0, 0x04, 0x08} },
    { '+', {0, 0x04, 0x04, 0x1F, 0x04, 0x04, 0} },
    { '=', {0, 0, 0x1F, 0, 0x1F, 0, 0} },
    { '!', {0x04, 0x04, 0x04, 0x04, 0x04, 0, 0x04} },
    { '?', {0x0E, 0x11, 0x01, 0x02, 0x04, 0, 0x04} }
};

typedef struct henka_ui_palette
{
    henka_vec4 panel_fill;
    henka_vec4 panel_header_fill;
    henka_vec4 panel_border;
    henka_vec4 panel_separator;
    henka_vec4 text_color;
    henka_vec4 heading_color;
    henka_vec4 button_fill;
    henka_vec4 button_hover;
    henka_vec4 button_active;
    henka_vec4 primary_fill;
    henka_vec4 primary_hover;
    henka_vec4 primary_active;
    henka_vec4 tab_fill;
    henka_vec4 tab_hover;
    henka_vec4 tab_active;
    henka_vec4 selected_fill;
    henka_vec4 selected_hover;
    henka_vec4 toggle_on;
    henka_vec4 toggle_off;
    henka_vec4 value_fill;
    henka_vec4 row_fill;
    henka_vec4 status_fill;
    henka_vec4 status_warning_fill;
    henka_vec4 muted_text_color;
    henka_vec4 hint_fill;
    henka_vec4 hint_border;
} henka_ui_palette;

static const henka_ui_palette g_ui_light_palette =
{
    {0.882f, 0.894f, 0.906f, 0.985f},
    {0.800f, 0.824f, 0.843f, 0.995f},
    {0.663f, 0.694f, 0.722f, 1.000f},
    {0.753f, 0.776f, 0.796f, 1.000f},
    {0.145f, 0.169f, 0.188f, 1.000f},
    {0.200f, 0.294f, 0.357f, 1.000f},
    {0.933f, 0.941f, 0.949f, 0.995f},
    {0.886f, 0.910f, 0.925f, 1.000f},
    {0.835f, 0.894f, 0.929f, 1.000f},
    {0.784f, 0.863f, 0.914f, 1.000f},
    {0.718f, 0.824f, 0.890f, 1.000f},
    {0.663f, 0.784f, 0.863f, 1.000f},
    {0.847f, 0.867f, 0.882f, 1.000f},
    {0.804f, 0.851f, 0.882f, 1.000f},
    {0.784f, 0.863f, 0.914f, 1.000f},
    {0.784f, 0.863f, 0.914f, 1.000f},
    {0.718f, 0.824f, 0.890f, 1.000f},
    {0.184f, 0.443f, 0.592f, 1.000f},
    {0.714f, 0.741f, 0.765f, 1.000f},
    {0.953f, 0.957f, 0.961f, 1.000f},
    {0.910f, 0.922f, 0.929f, 0.995f},
    {0.847f, 0.925f, 0.867f, 1.000f},
    {0.949f, 0.890f, 0.722f, 1.000f},
    {0.373f, 0.412f, 0.447f, 1.000f},
    {0.961f, 0.969f, 0.976f, 0.980f},
    {0.663f, 0.694f, 0.722f, 1.000f}
};

static const henka_ui_palette g_ui_dark_palette =
{
    {0.075f, 0.078f, 0.083f, 1.000f},
    {0.095f, 0.100f, 0.108f, 1.000f},
    {0.180f, 0.188f, 0.205f, 1.000f},
    {0.125f, 0.132f, 0.145f, 1.000f},
    {0.900f, 0.915f, 0.930f, 1.000f},
    {0.825f, 0.860f, 0.895f, 1.000f},
    {0.112f, 0.118f, 0.128f, 1.000f},
    {0.158f, 0.168f, 0.183f, 1.000f},
    {0.175f, 0.205f, 0.235f, 1.000f},
    {0.105f, 0.285f, 0.390f, 1.000f},
    {0.135f, 0.350f, 0.470f, 1.000f},
    {0.085f, 0.235f, 0.325f, 1.000f},
    {0.092f, 0.098f, 0.108f, 1.000f},
    {0.140f, 0.153f, 0.170f, 1.000f},
    {0.120f, 0.300f, 0.405f, 1.000f},
    {0.105f, 0.255f, 0.345f, 1.000f},
    {0.135f, 0.315f, 0.420f, 1.000f},
    {0.170f, 0.520f, 0.355f, 1.000f},
    {0.215f, 0.225f, 0.240f, 1.000f},
    {0.088f, 0.093f, 0.102f, 1.000f},
    {0.083f, 0.088f, 0.097f, 1.000f},
    {0.110f, 0.330f, 0.235f, 1.000f},
    {0.480f, 0.305f, 0.115f, 1.000f},
    {0.555f, 0.585f, 0.620f, 1.000f},
    {0.060f, 0.063f, 0.070f, 0.980f},
    {0.190f, 0.205f, 0.225f, 0.980f}
};

static const henka_ui_palette* henka_ui_palette_for(
    const henka_ui_context* context)
{
    if (context != NULL &&
        context->theme == HENKA_UI_THEME_DARK)
    {
        return &g_ui_dark_palette;
    }

    return &g_ui_light_palette;
}

#define g_ui_panel_fill (henka_ui_palette_for(context)->panel_fill)
#define g_ui_panel_header_fill (henka_ui_palette_for(context)->panel_header_fill)
#define g_ui_panel_border (henka_ui_palette_for(context)->panel_border)
#define g_ui_panel_separator (henka_ui_palette_for(context)->panel_separator)
#define g_ui_text_color (henka_ui_palette_for(context)->text_color)
#define g_ui_heading_color (henka_ui_palette_for(context)->heading_color)
#define g_ui_button_fill (henka_ui_palette_for(context)->button_fill)
#define g_ui_button_hover (henka_ui_palette_for(context)->button_hover)
#define g_ui_button_active (henka_ui_palette_for(context)->button_active)
#define g_ui_primary_fill (henka_ui_palette_for(context)->primary_fill)
#define g_ui_primary_hover (henka_ui_palette_for(context)->primary_hover)
#define g_ui_primary_active (henka_ui_palette_for(context)->primary_active)
#define g_ui_tab_fill (henka_ui_palette_for(context)->tab_fill)
#define g_ui_tab_hover (henka_ui_palette_for(context)->tab_hover)
#define g_ui_tab_active (henka_ui_palette_for(context)->tab_active)
#define g_ui_selected_fill (henka_ui_palette_for(context)->selected_fill)
#define g_ui_selected_hover (henka_ui_palette_for(context)->selected_hover)
#define g_ui_toggle_on (henka_ui_palette_for(context)->toggle_on)
#define g_ui_toggle_off (henka_ui_palette_for(context)->toggle_off)
#define g_ui_value_fill (henka_ui_palette_for(context)->value_fill)
#define g_ui_row_fill (henka_ui_palette_for(context)->row_fill)
#define g_ui_status_fill (henka_ui_palette_for(context)->status_fill)
#define g_ui_status_warning_fill (henka_ui_palette_for(context)->status_warning_fill)
#define g_ui_muted_text_color (henka_ui_palette_for(context)->muted_text_color)
#define g_ui_hint_fill (henka_ui_palette_for(context)->hint_fill)
#define g_ui_hint_border (henka_ui_palette_for(context)->hint_border)
static const henka_vec4 g_ui_semantic_info = {0.48f, 0.69f, 0.87f, 1.0f};
static const henka_vec4 g_ui_semantic_accent = {0.30f, 0.66f, 0.84f, 1.0f};
static const henka_vec4 g_ui_semantic_success = {0.37f, 0.78f, 0.51f, 1.0f};
static const henka_vec4 g_ui_semantic_warning = {0.92f, 0.75f, 0.28f, 1.0f};
static const henka_vec4 g_ui_semantic_orange = {0.92f, 0.49f, 0.22f, 1.0f};
static const henka_vec4 g_ui_semantic_danger = {0.88f, 0.34f, 0.38f, 1.0f};
static const henka_vec4 g_ui_semantic_disabled = {0.40f, 0.45f, 0.51f, 1.0f};

static henka_vec4 henka_ui_semantic_color_to_vec4(const henka_ui_context* context, henka_ui_semantic_color color)
{
    switch (color)
    {
        case HENKA_UI_COLOR_MUTED:
            return g_ui_muted_text_color;
        case HENKA_UI_COLOR_INFO:
            return g_ui_semantic_info;
        case HENKA_UI_COLOR_ACCENT:
            return g_ui_semantic_accent;
        case HENKA_UI_COLOR_SUCCESS:
            return g_ui_semantic_success;
        case HENKA_UI_COLOR_WARNING:
            return g_ui_semantic_warning;
        case HENKA_UI_COLOR_ORANGE:
            return g_ui_semantic_orange;
        case HENKA_UI_COLOR_DANGER:
            return g_ui_semantic_danger;
        case HENKA_UI_COLOR_DISABLED:
            return g_ui_semantic_disabled;
        case HENKA_UI_COLOR_NORMAL:
        default:
            return g_ui_text_color;
    }
}
static char henka_ui_normalize_character(char character)
{
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

typedef struct henka_ui_draw_checkpoint
{
    size_t draw_rect_count;
    size_t draw_line_count;
    size_t draw_triangle_count;
    size_t disclosure_id_count;
    bool wants_mouse;
    bool active_id_set;
    char active_id[HENKA_UI_MAX_ID_BYTES + 1U];
} henka_ui_draw_checkpoint;

static void henka_ui_capture_checkpoint(
    const henka_ui_context* context,
    henka_ui_draw_checkpoint* checkpoint)
{
    if (context == NULL || checkpoint == NULL)
    {
        return;
    }

    checkpoint->draw_rect_count = context->draw_rect_count;
    checkpoint->draw_line_count = context->draw_line_count;
    checkpoint->draw_triangle_count = context->draw_triangle_count;
    checkpoint->disclosure_id_count = context->disclosure_id_count;
    checkpoint->wants_mouse = context->wants_mouse;
    checkpoint->active_id_set = context->active_id_set;
    memcpy(checkpoint->active_id, context->active_id, sizeof(checkpoint->active_id));
}

static void henka_ui_restore_checkpoint(
    henka_ui_context* context,
    const henka_ui_draw_checkpoint* checkpoint)
{
    if (context == NULL || checkpoint == NULL)
    {
        return;
    }

    context->draw_rect_count = checkpoint->draw_rect_count;
    context->draw_line_count = checkpoint->draw_line_count;
    context->draw_triangle_count = checkpoint->draw_triangle_count;
    context->disclosure_id_count = checkpoint->disclosure_id_count;
    context->wants_mouse = checkpoint->wants_mouse;
    context->active_id_set = checkpoint->active_id_set;
    memcpy(context->active_id, checkpoint->active_id, sizeof(context->active_id));
}

static bool henka_ui_id_is_valid(const char* id)
{
    size_t length;
    return henka_checked_c_string_length(id, HENKA_UI_MAX_ID_BYTES, &length);
}

static size_t henka_ui_clamped_character_count(
    float width,
    float advance,
    size_t minimum,
    size_t maximum)
{
    double count;

    if (!henka_ui_float_is_finite(width) ||
        !henka_ui_float_is_finite(advance) ||
        width <= 0.0f ||
        advance <= 0.0f ||
        minimum > maximum)
    {
        return minimum <= maximum ? minimum : maximum;
    }

    count = (double)width / (double)advance;
    if (!isfinite(count) || count >= (double)maximum)
    {
        return maximum;
    }
    if (count <= (double)minimum)
    {
        return minimum;
    }

    return (size_t)count;
}

static henka_result henka_ui_ensure_rect_capacity(henka_ui_context* context, size_t additional_rects)
{
    size_t allocation_size;
    size_t minimum_capacity;
    size_t new_capacity;
    henka_ui_draw_rect* rects;

    if (context == NULL ||
        !henka_checked_size_add(context->draw_rect_count, additional_rects, &minimum_capacity) ||
        !henka_checked_capacity(
            context->draw_rect_capacity,
            minimum_capacity,
            128U,
            HENKA_UI_MAX_DRAW_ITEMS,
            &new_capacity) ||
        !henka_checked_size_multiply(new_capacity, sizeof(*rects), &allocation_size))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    if (new_capacity == context->draw_rect_capacity)
    {
        return HENKA_SUCCESS;
    }

    rects = henka_realloc(context->draw_rects, allocation_size);
    if (rects == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }

    context->draw_rects = rects;
    context->draw_rect_capacity = new_capacity;
    return HENKA_SUCCESS;
}

static henka_result henka_ui_ensure_line_capacity(henka_ui_context* context, size_t additional_lines)
{
    size_t allocation_size;
    size_t minimum_capacity;
    size_t new_capacity;
    henka_ui_draw_line* lines;

    if (context == NULL ||
        !henka_checked_size_add(context->draw_line_count, additional_lines, &minimum_capacity) ||
        !henka_checked_capacity(
            context->draw_line_capacity,
            minimum_capacity,
            128U,
            HENKA_UI_MAX_DRAW_ITEMS,
            &new_capacity) ||
        !henka_checked_size_multiply(new_capacity, sizeof(*lines), &allocation_size))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    if (new_capacity == context->draw_line_capacity)
    {
        return HENKA_SUCCESS;
    }

    lines = henka_realloc(context->draw_lines, allocation_size);
    if (lines == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }

    context->draw_lines = lines;
    context->draw_line_capacity = new_capacity;
    return HENKA_SUCCESS;
}

static henka_result henka_ui_ensure_triangle_capacity(
    henka_ui_context* context,
    size_t additional_triangles)
{
    size_t allocation_size;
    size_t minimum_capacity;
    size_t new_capacity;
    henka_ui_draw_triangle* triangles;

    if (context == NULL ||
        !henka_checked_size_add(
            context->draw_triangle_count,
            additional_triangles,
            &minimum_capacity) ||
        !henka_checked_capacity(
            context->draw_triangle_capacity,
            minimum_capacity,
            128U,
            HENKA_UI_MAX_DRAW_ITEMS,
            &new_capacity) ||
        !henka_checked_size_multiply(
            new_capacity,
            sizeof(*triangles),
            &allocation_size))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    if (new_capacity == context->draw_triangle_capacity)
    {
        return HENKA_SUCCESS;
    }

    triangles = henka_realloc(context->draw_triangles, allocation_size);
    if (triangles == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }

    context->draw_triangles = triangles;
    context->draw_triangle_capacity = new_capacity;
    return HENKA_SUCCESS;
}

static henka_result henka_ui_push_rect(henka_ui_context* context, henka_ui_rect bounds, henka_vec4 color)
{
    henka_result result;

    if (context == NULL || !henka_ui_rect_is_finite(bounds) || !henka_ui_vec4_is_finite(color) ||
        bounds.width <= 0.0f || bounds.height <= 0.0f)
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

static henka_result henka_ui_push_line(
    henka_ui_context* context,
    henka_vec2 start,
    henka_vec2 end,
    float thickness,
    henka_vec4 color)
{
    double delta_x;
    double delta_y;
    double length;
    henka_result result;

    if (context == NULL ||
        !henka_ui_vec2_is_finite(start) ||
        !henka_ui_vec2_is_finite(end) ||
        !henka_ui_float_is_finite(thickness) ||
        !henka_ui_vec4_is_finite(color) ||
        thickness <= 0.0f)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    delta_x = (double)end.x - (double)start.x;
    delta_y = (double)end.y - (double)start.y;
    length = hypot(delta_x, delta_y);
    if (!isfinite(length))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    result = henka_ui_ensure_line_capacity(context, 1U);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    context->draw_lines[context->draw_line_count].start = start;
    context->draw_lines[context->draw_line_count].end = end;
    context->draw_lines[context->draw_line_count].thickness = thickness;
    context->draw_lines[context->draw_line_count].color = color;
    context->draw_line_count += 1U;
    return HENKA_SUCCESS;
}

static henka_result henka_ui_push_triangle(
    henka_ui_context* context,
    henka_vec2 first,
    henka_vec2 second,
    henka_vec2 third,
    henka_vec4 color)
{
    const float area =
        (second.x - first.x) * (third.y - first.y) -
        (second.y - first.y) * (third.x - first.x);
    henka_result result;

    if (context == NULL ||
        !henka_ui_vec2_is_finite(first) ||
        !henka_ui_vec2_is_finite(second) ||
        !henka_ui_vec2_is_finite(third) ||
        !henka_ui_vec4_is_finite(color) ||
        !henka_ui_float_is_finite(area))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    /* Degenerate projected faces are valid topology but have no visible
     * surface in this 2D overlay. Treat them as a no-op rather than allowing
     * malformed geometry to enter the renderer draw list. */
    if (fabsf(area) <= 0.0001f)
    {
        return HENKA_SUCCESS;
    }

    result = henka_ui_ensure_triangle_capacity(context, 1U);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    context->draw_triangles[context->draw_triangle_count].points[0] = first;
    context->draw_triangles[context->draw_triangle_count].points[1] = second;
    context->draw_triangles[context->draw_triangle_count].points[2] = third;
    context->draw_triangles[context->draw_triangle_count].color = color;
    context->draw_triangle_count += 1U;
    return HENKA_SUCCESS;
}

static henka_result henka_ui_push_masked_border(
    henka_ui_context* context,
    henka_ui_rect bounds,
    float thickness,
    henka_vec4 color,
    unsigned int border_mask)
{
    henka_ui_draw_checkpoint checkpoint;
    henka_result result;

    if ((border_mask & ~(unsigned int)HENKA_UI_BORDER_ALL) != 0U ||
        context == NULL ||
        !henka_ui_rect_is_finite(bounds) ||
        !henka_ui_float_is_finite(thickness) ||
        !henka_ui_vec4_is_finite(color) ||
        bounds.width <= 0.0f ||
        bounds.height <= 0.0f ||
        thickness <= 0.0f ||
        thickness > bounds.width ||
        thickness > bounds.height)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    henka_ui_capture_checkpoint(context, &checkpoint);

    result = HENKA_SUCCESS;
    if ((border_mask & HENKA_UI_BORDER_TOP) != 0U)
    {
        result = henka_ui_push_rect(
            context,
            (henka_ui_rect){bounds.x, bounds.y, bounds.width, thickness},
            color);
    }
    if (result == HENKA_SUCCESS && (border_mask & HENKA_UI_BORDER_BOTTOM) != 0U)
    {
        result = henka_ui_push_rect(
            context,
            (henka_ui_rect){bounds.x, bounds.y + bounds.height - thickness, bounds.width, thickness},
            color);
    }
    if (result == HENKA_SUCCESS && (border_mask & HENKA_UI_BORDER_LEFT) != 0U)
    {
        result = henka_ui_push_rect(
            context,
            (henka_ui_rect){bounds.x, bounds.y, thickness, bounds.height},
            color);
    }
    if (result == HENKA_SUCCESS && (border_mask & HENKA_UI_BORDER_RIGHT) != 0U)
    {
        result = henka_ui_push_rect(
            context,
            (henka_ui_rect){bounds.x + bounds.width - thickness, bounds.y, thickness, bounds.height},
            color);
    }
    if (result != HENKA_SUCCESS)
    {
        henka_ui_restore_checkpoint(context, &checkpoint);
    }
    return result;
}

static henka_result henka_ui_push_border(
    henka_ui_context* context,
    henka_ui_rect bounds,
    float thickness,
    henka_vec4 color)
{
    return henka_ui_push_masked_border(
        context,
        bounds,
        thickness,
        color,
        HENKA_UI_BORDER_ALL);
}

#define HENKA_UI_READABILITY_TEXT_SCALE 1.20f
#define HENKA_UI_MAX_READABILITY_DISPLAY_SCALE 1.125f

static float henka_ui_effective_text_scale(
    const henka_ui_context* context,
    float requested_scale)
{
    float display_scale = 1.0f;

    if (context != NULL &&
        context->framebuffer_width > 0 &&
        context->framebuffer_height > 0)
    {
        const float width_scale = (float)context->framebuffer_width / 1600.0f;
        const float height_scale = (float)context->framebuffer_height / 900.0f;
        display_scale = fminf(width_scale, height_scale);
        display_scale = fmaxf(1.0f, display_scale);
        display_scale = fminf(
            HENKA_UI_MAX_READABILITY_DISPLAY_SCALE,
            display_scale);
    }

    return fminf(
        HENKA_UI_MAX_SCALE,
        requested_scale * HENKA_UI_READABILITY_TEXT_SCALE * display_scale);
}

static henka_result henka_ui_draw_text_raw(
    henka_ui_context* context,
    float x,
    float y,
    float scale,
    const char* text,
    henka_vec4 color)
{
    float cursor_x;
    float cursor_y;
    henka_ui_draw_checkpoint checkpoint;
    size_t index;
    size_t text_length;

    if (context == NULL || text == NULL ||
        !henka_ui_float_is_finite(x) || !henka_ui_float_is_finite(y) ||
        !henka_ui_float_is_finite(scale) || scale <= 0.0f || scale > HENKA_UI_MAX_SCALE ||
        !henka_ui_vec4_is_finite(color) ||
        !henka_checked_c_string_length(text, HENKA_UI_MAX_TEXT_BYTES, &text_length))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    henka_ui_capture_checkpoint(context, &checkpoint);
    cursor_x = x;
    cursor_y = y;

    for (index = 0U; index < text_length; ++index)
    {
        const henka_ui_glyph* glyph;
        int column;
        int row;

        if (text[index] == '\n')
        {
            const double next_y = (double)cursor_y + 8.0 * (double)scale;
            if (!isfinite(next_y) || next_y < -(double)FLT_MAX || next_y > (double)FLT_MAX)
            {
                henka_ui_restore_checkpoint(context, &checkpoint);
                return HENKA_ERROR_INVALID_ARGUMENT;
            }
            cursor_x = x;
            cursor_y = (float)next_y;
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
                const unsigned char mask = (unsigned char)(1U << (4 - column));
                if ((glyph->rows[row] & mask) != 0U)
                {
                    henka_result result;
                    result = henka_ui_push_rect(
                        context,
                        (henka_ui_rect){
                            cursor_x + (float)column * scale,
                            cursor_y + (float)row * scale,
                            scale,
                            scale},
                        color);
                    if (result != HENKA_SUCCESS)
                    {
                        henka_ui_restore_checkpoint(context, &checkpoint);
                        return result;
                    }
                }
            }
        }

        {
            const double next_x = (double)cursor_x + 6.0 * (double)scale;
            if (!isfinite(next_x) || next_x < -(double)FLT_MAX || next_x > (double)FLT_MAX)
            {
                henka_ui_restore_checkpoint(context, &checkpoint);
                return HENKA_ERROR_INVALID_ARGUMENT;
            }
            cursor_x = (float)next_x;
        }
    }

    return HENKA_SUCCESS;
}

static henka_result henka_ui_draw_text(
    henka_ui_context* context,
    float x,
    float y,
    float scale,
    const char* text,
    henka_vec4 color)
{
    if (context == NULL ||
        !henka_ui_float_is_finite(scale) ||
        scale <= 0.0f ||
        scale > HENKA_UI_MAX_SCALE)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    return henka_ui_draw_text_raw(
        context,
        x,
        y,
        henka_ui_effective_text_scale(context, scale),
        text,
        color);
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

static void henka_ui_copy_fit_text(const char* source, char* buffer, size_t buffer_size, size_t max_characters)
{
    size_t length;

    if (buffer == NULL || buffer_size == 0U)
    {
        return;
    }

    if (source == NULL)
    {
        buffer[0] = '\0';
        return;
    }

    if (!henka_checked_c_string_length(source, HENKA_UI_MAX_TEXT_BYTES, &length))
    {
        buffer[0] = '\0';
        return;
    }

    if (length <= max_characters || max_characters < 4U)
    {
        snprintf(buffer, buffer_size, "%s", source);
        return;
    }

    snprintf(buffer, buffer_size, "%.*s...", (int)(max_characters - 3U), source);
}

static henka_result henka_ui_draw_fit_text(
    henka_ui_context* context,
    henka_ui_rect bounds,
    float padding_x,
    float y,
    float scale,
    const char* text,
    henka_vec4 color)
{
    double available_width;
    char buffer[96];
    float effective_scale;
    size_t max_characters;

    if (context == NULL || text == NULL || !henka_ui_rect_is_finite(bounds) ||
        !henka_ui_float_is_finite(padding_x) || !henka_ui_float_is_finite(y) ||
        !henka_ui_float_is_finite(scale) || scale <= 0.0f || scale > HENKA_UI_MAX_SCALE ||
        bounds.width <= 0.0f || bounds.height <= 0.0f || !henka_ui_vec4_is_finite(color))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    effective_scale = henka_ui_effective_text_scale(context, scale);

    available_width = (double)bounds.width - (double)padding_x * 2.0;
    if (!isfinite(available_width) ||
        available_width <= 0.0 ||
        available_width > (double)FLT_MAX)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    max_characters = henka_ui_clamped_character_count(
        (float)available_width,
        6.0f * effective_scale,
        4U,
        sizeof(buffer) - 1U);
    henka_ui_copy_fit_text(text, buffer, sizeof(buffer), max_characters);
    return henka_ui_draw_text_raw(
        context,
        bounds.x + padding_x,
        y,
        effective_scale,
        buffer,
        color);
}

static bool henka_ui_button_internal(
    henka_ui_context* context,
    const char* id,
    henka_ui_rect bounds,
    const char* label,
    henka_vec4 idle_fill,
    henka_vec4 hover_fill,
    henka_vec4 active_fill,
    henka_vec4 accent_color,
    bool emphasized)
{
    bool active;
    bool clicked;
    bool had_active;
    bool hot;
    bool owns_active;
    bool pressed;
    henka_ui_draw_checkpoint checkpoint;
    henka_result result;
    henka_vec4 border_color;

    /* HENKA UI A2 CONTROL CHROME */
    if (context == NULL || id == NULL || label == NULL ||
        !context->frame_active || !henka_ui_id_is_valid(id))
    {
        return false;
    }
    if (!context->visible)
    {
        return false;
    }

    henka_ui_capture_checkpoint(context, &checkpoint);
    hot = henka_ui_control_is_hot(context, bounds);
    pressed = hot && context->mouse_left_pressed;
    had_active = henka_ui_active_id_equals(context, id);
    owns_active = had_active || pressed;
    active = context->mouse_left_down && owns_active;
    clicked = hot && context->mouse_left_released && owns_active;

    result = henka_ui_push_rect(
        context,
        bounds,
        active ? active_fill : (hot ? hover_fill : idle_fill));

    border_color =
        emphasized || hot || active
            ? accent_color
            : g_ui_panel_border;

    if (result == HENKA_SUCCESS)
    {
        result = henka_ui_push_border(
            context,
            bounds,
            1.0f,
            border_color);
    }

    if (result != HENKA_SUCCESS ||
        henka_ui_draw_fit_text(
            context,
            bounds,
            emphasized ? 11.0f : 10.0f,
            bounds.y + 9.0f,
            1.0f,
            label,
            g_ui_text_color) != HENKA_SUCCESS)
    {
        henka_ui_restore_checkpoint(context, &checkpoint);
        return false;
    }

    if (pressed && !henka_ui_set_active_id(context, id))
    {
        henka_ui_restore_checkpoint(context, &checkpoint);
        return false;
    }
    if (context->mouse_left_released && owns_active)
    {
        henka_ui_clear_active_id(context);
    }
    return clicked;
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

    context->theme = HENKA_UI_THEME_LIGHT;
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
    henka_free(context->draw_lines);
    henka_free(context->draw_triangles);
    henka_free(context);
}

henka_result henka_ui_set_theme(
    henka_ui_context* context,
    henka_ui_theme theme)
{
    if (context == NULL ||
        (theme != HENKA_UI_THEME_LIGHT &&
         theme != HENKA_UI_THEME_DARK))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    context->theme = theme;
    return HENKA_SUCCESS;
}

henka_ui_theme henka_ui_get_theme(
    const henka_ui_context* context)
{
    if (context == NULL)
    {
        return HENKA_UI_THEME_LIGHT;
    }

    return context->theme;
}

henka_result henka_ui_begin_frame(
    henka_ui_context* context,
    const henka_ui_frame_desc* frame_desc)
{
    if (context == NULL ||
        frame_desc == NULL ||
        context->frame_active ||
        frame_desc->framebuffer_width <= 0 ||
        frame_desc->framebuffer_height <= 0 ||
        !henka_ui_vec2_is_finite(frame_desc->mouse_position) ||
        (frame_desc->text_input == NULL && frame_desc->text_input_size != 0U) ||
        frame_desc->text_input_size > HENKA_UI_MAX_TEXT_BYTES)
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
    context->text_backspace_pressed = frame_desc->text_backspace_pressed;
    context->text_input = frame_desc->text_input;
    context->text_input_size = frame_desc->text_input_size;
    context->draw_rect_count = 0U;
    context->draw_line_count = 0U;
    context->draw_triangle_count = 0U;
    context->disclosure_id_count = 0U;
    context->navigation_up_pressed =
        frame_desc->navigation_up_pressed;
    context->navigation_down_pressed =
        frame_desc->navigation_down_pressed;
    context->navigation_left_pressed =
        frame_desc->navigation_left_pressed;
    context->navigation_right_pressed =
        frame_desc->navigation_right_pressed;
    context->navigation_enter_pressed =
        frame_desc->navigation_enter_pressed;
    context->consumed_navigation_mask = 0U;
    if (!context->mouse_left_down && !context->mouse_left_released)
    {
        henka_ui_clear_active_id(context);
    }
    return HENKA_SUCCESS;
}

henka_result henka_ui_end_frame(henka_ui_context* context)
{
    if (context == NULL ||
        !context->frame_active ||
        context->flow.active)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    henka_ui_finalize_disclosure_navigation(context);

    context->frame_active = false;
    context->mouse_left_pressed = false;
    context->mouse_left_released = false;
    return HENKA_SUCCESS;
}

void henka_ui_set_visible(henka_ui_context* context, bool visible)
{
    if (context != NULL)
    {
        context->visible = visible;
        if (!visible)
        {
            henka_ui_clear_active_id(context);
            henka_ui_clear_focused_text_id(context);
            henka_ui_clear_focused_disclosure_id(context);
            context->consumed_navigation_mask = 0U;
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

henka_vec2 henka_ui_get_mouse_position(const henka_ui_context* context)
{
    if (context == NULL)
    {
        return (henka_vec2){0.0f, 0.0f};
    }
    return context->mouse_position;
}

const char* henka_ui_get_text_input(
    const henka_ui_context* context,
    size_t* out_text_size)
{
    if (out_text_size != NULL)
    {
        *out_text_size = context == NULL ? 0U : context->text_input_size;
    }
    return context == NULL ? NULL : context->text_input;
}

henka_result henka_ui_text_field(
    henka_ui_context* context,
    const char* id,
    henka_ui_rect bounds,
    char* value,
    size_t value_capacity,
    bool* out_changed)
{
    char candidate[HENKA_UI_MAX_TEXT_BYTES + 1U];
    henka_ui_draw_checkpoint checkpoint;
    size_t candidate_length;
    size_t value_length;
    size_t input_index;
    bool focused;
    bool hot;
    bool requested_focus;
    bool changed = false;
    henka_result result;

    if (out_changed != NULL)
    {
        *out_changed = false;
    }
    if (context == NULL || id == NULL || value == NULL || out_changed == NULL ||
        !context->frame_active || !context->visible || !henka_ui_id_is_valid(id) ||
        !henka_ui_rect_is_finite(bounds) || bounds.width <= 0.0f || bounds.height <= 0.0f ||
        value_capacity == 0U || value_capacity > sizeof(candidate) ||
        !henka_checked_c_string_length(value, value_capacity - 1U, &value_length))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    hot = henka_ui_control_is_hot(context, bounds);
    focused = henka_ui_focused_text_id_equals(context, id);
    requested_focus = focused;
    if (hot)
    {
        context->wants_mouse = true;
    }
    if (context->mouse_left_pressed)
    {
        requested_focus = hot;
    }

    memcpy(candidate, value, value_length + 1U);
    candidate_length = value_length;
    if (requested_focus && context->text_backspace_pressed && candidate_length > 0U)
    {
        do
        {
            --candidate_length;
        } while (candidate_length > 0U &&
                 (((unsigned char)candidate[candidate_length] & 0xC0U) == 0x80U));
        candidate[candidate_length] = '\0';
        changed = true;
    }
    if (requested_focus && context->text_input_size > 0U)
    {
        if (context->text_input == NULL ||
            context->text_input_size > value_capacity - 1U - candidate_length)
        {
            return HENKA_ERROR_LIMIT;
        }
        for (input_index = 0U; input_index < context->text_input_size; ++input_index)
        {
            const unsigned char character = (unsigned char)context->text_input[input_index];
            if (character < 0x20U || character == 0x7FU)
            {
                return HENKA_ERROR_INVALID_ARGUMENT;
            }
        }
        memcpy(candidate + candidate_length, context->text_input, context->text_input_size);
        candidate_length += context->text_input_size;
        candidate[candidate_length] = '\0';
        changed = true;
    }

    henka_ui_capture_checkpoint(context, &checkpoint);
    result = henka_ui_push_rect(
        context,
        bounds,
        requested_focus ? g_ui_selected_fill : (hot ? g_ui_button_hover : g_ui_value_fill));
    if (result == HENKA_SUCCESS)
    {
        result = henka_ui_push_border(
            context,
            bounds,
            1.0f,
            requested_focus ? g_ui_heading_color : g_ui_panel_border);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_ui_draw_fit_text(
            context,
            bounds,
            8.0f,
            bounds.y + (bounds.height - 8.0f) * 0.5f,
            1.0f,
            candidate,
            g_ui_text_color);
    }
    if (result != HENKA_SUCCESS)
    {
        henka_ui_restore_checkpoint(context, &checkpoint);
        return result;
    }

    if (requested_focus)
    {
        if (!henka_ui_set_focused_text_id(context, id))
        {
            henka_ui_restore_checkpoint(context, &checkpoint);
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
    }
    else if (focused)
    {
        henka_ui_clear_focused_text_id(context);
    }
    if (changed)
    {
        memcpy(value, candidate, candidate_length + 1U);
        *out_changed = true;
    }
    return HENKA_SUCCESS;
}

henka_result henka_ui_custom_interaction(
    henka_ui_context* context,
    const char* id,
    bool pointer_inside,
    bool enabled,
    henka_ui_interaction_state* out_state)
{
    bool had_active;
    bool competing_active;
    bool pressed;
    bool owns_active;

    if (out_state != NULL)
    {
        *out_state = (henka_ui_interaction_state){0};
    }
    if (context == NULL || id == NULL || out_state == NULL ||
        !context->frame_active || !henka_ui_id_is_valid(id))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (!enabled || !context->visible)
    {
        return HENKA_SUCCESS;
    }

    had_active = henka_ui_active_id_equals(context, id);
    competing_active = context->active_id_set && !had_active;
    pressed = pointer_inside && context->mouse_left_pressed && !competing_active;
    owns_active = had_active || pressed;

    out_state->hovered = pointer_inside && !competing_active;
    out_state->pressed = pressed;
    out_state->held = context->mouse_left_down && owns_active;
    out_state->active = owns_active && (context->mouse_left_down || context->mouse_left_released);
    out_state->released = context->mouse_left_released && owns_active;

    if (out_state->hovered || out_state->active)
    {
        context->wants_mouse = true;
    }
    if (pressed && !henka_ui_set_active_id(context, id))
    {
        *out_state = (henka_ui_interaction_state){0};
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (out_state->released)
    {
        henka_ui_clear_active_id(context);
    }
    return HENKA_SUCCESS;
}
unsigned int henka_ui_get_consumed_navigation_mask(
    const henka_ui_context* context)
{
    if (context == NULL)
    {
        return 0U;
    }

    return context->consumed_navigation_mask;
}

size_t henka_ui_get_draw_rect_count(const henka_ui_context* context)
{
    if (context == NULL)
    {
        return 0U;
    }

    return context->draw_rect_count;
}

size_t henka_ui_get_draw_line_count(const henka_ui_context* context)
{
    if (context == NULL)
    {
        return 0U;
    }

    return context->draw_line_count;
}

size_t henka_ui_get_draw_triangle_count(const henka_ui_context* context)
{
    if (context == NULL)
    {
        return 0U;
    }

    return context->draw_triangle_count;
}

henka_result henka_ui_overlay_rect(henka_ui_context* context, henka_ui_rect bounds, henka_vec4 color)
{
    if (context == NULL || !context->frame_active)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    return henka_ui_push_rect(context, bounds, color);
}

henka_result henka_ui_overlay_line(henka_ui_context* context, henka_vec2 start, henka_vec2 end, float thickness, henka_vec4 color)
{
    if (context == NULL || !context->frame_active)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    return henka_ui_push_line(context, start, end, thickness, color);
}

henka_result henka_ui_overlay_triangle(
    henka_ui_context* context,
    henka_vec2 first,
    henka_vec2 second,
    henka_vec2 third,
    henka_vec4 color)
{
    if (context == NULL || !context->frame_active)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    return henka_ui_push_triangle(context, first, second, third, color);
}

henka_result henka_ui_overlay_disc(
    henka_ui_context* context,
    henka_vec2 center,
    float radius,
    henka_vec4 color)
{
    henka_ui_draw_checkpoint checkpoint;
    const float step = radius * 2.0f / (float)HENKA_UI_OVERLAY_DISC_SAMPLES;
    size_t index;

    if (context == NULL || !context->frame_active ||
        !henka_ui_vec2_is_finite(center) || !henka_ui_float_is_finite(radius) ||
        radius <= 0.0f || !henka_ui_vec4_is_finite(color))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    henka_ui_capture_checkpoint(context, &checkpoint);
    for (index = 0U; index < HENKA_UI_OVERLAY_DISC_SAMPLES; ++index)
    {
        const float normalized =
            (((float)index + 0.5f) / (float)HENKA_UI_OVERLAY_DISC_SAMPLES) * 2.0f - 1.0f;
        const float half_width = radius * sqrtf(fmaxf(0.0f, 1.0f - normalized * normalized));
        const float y = center.y - radius + ((float)index + 0.5f) * step;
        const henka_result result = henka_ui_push_line(
            context,
            (henka_vec2){center.x - half_width, y},
            (henka_vec2){center.x + half_width, y},
            step + 0.75f,
            color);

        if (result != HENKA_SUCCESS)
        {
            henka_ui_restore_checkpoint(context, &checkpoint);
            return result;
        }
    }
    return HENKA_SUCCESS;
}

henka_result henka_ui_overlay_circle(
    henka_ui_context* context,
    henka_vec2 center,
    float radius,
    float thickness,
    henka_vec4 color)
{
    henka_vec2 points[HENKA_UI_OVERLAY_DISC_SAMPLES + 1U];
    size_t index;

    if (context == NULL || !context->frame_active ||
        !henka_ui_vec2_is_finite(center) || !henka_ui_float_is_finite(radius) ||
        radius <= 0.0f || !henka_ui_float_is_finite(thickness) || thickness <= 0.0f ||
        !henka_ui_vec4_is_finite(color))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    for (index = 0U; index <= HENKA_UI_OVERLAY_DISC_SAMPLES; ++index)
    {
        const float angle = HENKA_PI * 2.0f * (float)index /
            (float)HENKA_UI_OVERLAY_DISC_SAMPLES;
        points[index] = (henka_vec2){
            center.x + cosf(angle) * radius,
            center.y + sinf(angle) * radius};
    }
    return henka_ui_overlay_polyline(
        context,
        points,
        HENKA_UI_OVERLAY_DISC_SAMPLES + 1U,
        thickness,
        color);
}

henka_result henka_ui_overlay_polyline(
    henka_ui_context* context,
    const henka_vec2* points,
    size_t point_count,
    float thickness,
    henka_vec4 color)
{
    henka_ui_draw_checkpoint checkpoint;
    size_t index;

    if (context == NULL || !context->frame_active || points == NULL ||
        point_count < 2U || !henka_ui_float_is_finite(thickness) ||
        thickness <= 0.0f || !henka_ui_vec4_is_finite(color))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    for (index = 0U; index < point_count; ++index)
    {
        if (!henka_ui_vec2_is_finite(points[index]))
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
    }

    henka_ui_capture_checkpoint(context, &checkpoint);
    for (index = 0U; index + 1U < point_count; ++index)
    {
        const henka_result result =
            henka_ui_push_line(context, points[index], points[index + 1U], thickness, color);
        if (result != HENKA_SUCCESS)
        {
            henka_ui_restore_checkpoint(context, &checkpoint);
            return result;
        }
    }

    return HENKA_SUCCESS;
}
bool henka_ui_rect_contains(henka_ui_rect rect, henka_vec2 point)
{
    if (!henka_ui_rect_is_finite(rect) || !henka_ui_vec2_is_finite(point) ||
        rect.width <= 0.0f || rect.height <= 0.0f)
    {
        return false;
    }

    return point.x >= rect.x &&
        point.y >= rect.y &&
        point.x < rect.x + rect.width &&
        point.y < rect.y + rect.height;
}

static float henka_ui_scroll_clamp_offset(
    float offset,
    float content_height,
    float viewport_height)
{
    const float maximum_offset = content_height - viewport_height;

    if (!henka_ui_float_is_finite(offset) || offset < 0.0f ||
        !henka_ui_float_is_finite(content_height) || content_height < 0.0f ||
        !henka_ui_float_is_finite(viewport_height) || viewport_height <= 0.0f ||
        content_height <= viewport_height)
    {
        return 0.0f;
    }
    return offset > maximum_offset ? maximum_offset : offset;
}

henka_result henka_ui_scroll_state_set_content(
    henka_ui_scroll_state* state,
    float content_height,
    float viewport_height)
{
    if (state == NULL || !henka_ui_float_is_finite(content_height) ||
        content_height < 0.0f || !henka_ui_float_is_finite(viewport_height) ||
        viewport_height <= 0.0f)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    state->content_height = content_height;
    state->viewport_height = viewport_height;
    state->offset = henka_ui_scroll_clamp_offset(
        state->offset,
        content_height,
        viewport_height);
    return HENKA_SUCCESS;
}

henka_result henka_ui_scroll_state_apply_delta(
    henka_ui_scroll_state* state,
    float delta_pixels)
{
    double requested_offset;

    if (state == NULL || !henka_ui_float_is_finite(delta_pixels) ||
        !henka_ui_float_is_finite(state->content_height) ||
        !henka_ui_float_is_finite(state->viewport_height) ||
        state->content_height < 0.0f || state->viewport_height <= 0.0f)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    state->offset = henka_ui_scroll_clamp_offset(
        state->offset,
        state->content_height,
        state->viewport_height);
    if (state->content_height <= state->viewport_height || delta_pixels == 0.0f)
    {
        return HENKA_SUCCESS;
    }
    requested_offset = (double)state->offset + (double)delta_pixels;
    if (!isfinite(requested_offset))
    {
        requested_offset = delta_pixels < 0.0f ? 0.0 : (double)FLT_MAX;
    }
    if (requested_offset < 0.0)
    {
        requested_offset = 0.0;
    }
    if (requested_offset > (double)FLT_MAX)
    {
        requested_offset = (double)FLT_MAX;
    }
    state->offset = henka_ui_scroll_clamp_offset(
        (float)requested_offset,
        state->content_height,
        state->viewport_height);
    return HENKA_SUCCESS;
}

henka_result henka_ui_scrollbar_thumb_height(
    float content_height,
    float viewport_height,
    float track_height,
    float* out_thumb_height)
{
    float thumb_height;

    if (out_thumb_height != NULL) *out_thumb_height = 0.0f;
    if (out_thumb_height == NULL || !henka_ui_float_is_finite(content_height) ||
        !henka_ui_float_is_finite(viewport_height) ||
        !henka_ui_float_is_finite(track_height) || content_height <= viewport_height ||
        viewport_height <= 0.0f || track_height <= 0.0f)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    thumb_height = track_height * viewport_height / content_height;
    if (!henka_ui_float_is_finite(thumb_height))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (thumb_height < 24.0f) thumb_height = 24.0f;
    if (thumb_height > track_height) thumb_height = track_height;
    *out_thumb_height = thumb_height;
    return HENKA_SUCCESS;
}

henka_result henka_ui_scrollbar_thumb_offset(
    float scroll_offset,
    float content_height,
    float viewport_height,
    float track_height,
    float thumb_height,
    float* out_thumb_offset)
{
    const float maximum_offset = content_height - viewport_height;
    const float travel = track_height - thumb_height;

    if (out_thumb_offset != NULL) *out_thumb_offset = 0.0f;
    if (out_thumb_offset == NULL || !henka_ui_float_is_finite(scroll_offset) ||
        !henka_ui_float_is_finite(content_height) ||
        !henka_ui_float_is_finite(viewport_height) ||
        !henka_ui_float_is_finite(track_height) ||
        !henka_ui_float_is_finite(thumb_height) || content_height <= viewport_height ||
        viewport_height <= 0.0f || track_height <= 0.0f || thumb_height <= 0.0f ||
        thumb_height > track_height || maximum_offset <= 0.0f || travel <= 0.0f)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_thumb_offset = travel * henka_ui_scroll_clamp_offset(
        scroll_offset,
        content_height,
        viewport_height) / maximum_offset;
    return HENKA_SUCCESS;
}

henka_result henka_ui_scroll_state_set_from_scrollbar(
    henka_ui_scroll_state* state,
    float pointer_y,
    float track_y,
    float track_height,
    float thumb_height,
    float grab_offset)
{
    float travel;
    float thumb_y;
    float normalized;

    if (state == NULL || !henka_ui_float_is_finite(pointer_y) ||
        !henka_ui_float_is_finite(track_y) || !henka_ui_float_is_finite(track_height) ||
        !henka_ui_float_is_finite(thumb_height) || !henka_ui_float_is_finite(grab_offset) ||
        track_height <= 0.0f || thumb_height <= 0.0f || thumb_height > track_height ||
        state->content_height <= state->viewport_height)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    travel = track_height - thumb_height;
    if (travel <= 0.0f)
    {
        state->offset = 0.0f;
        return HENKA_SUCCESS;
    }
    thumb_y = pointer_y - grab_offset - track_y;
    if (thumb_y < 0.0f) thumb_y = 0.0f;
    if (thumb_y > travel) thumb_y = travel;
    normalized = thumb_y / travel;
    state->offset = henka_ui_scroll_clamp_offset(
        normalized * (state->content_height - state->viewport_height),
        state->content_height,
        state->viewport_height);
    return HENKA_SUCCESS;
}

henka_result henka_ui_flow_begin(
    henka_ui_context* context,
    const henka_ui_flow_desc* desc)
{
    double cursor_y;

    if (context == NULL ||
        desc == NULL ||
        !context->frame_active ||
        context->flow.active ||
        !henka_ui_rect_is_finite(desc->bounds) ||
        !henka_ui_float_is_finite(desc->scroll_offset) ||
        !henka_ui_float_is_finite(desc->row_spacing) ||
        !henka_ui_float_is_finite(desc->indent_width) ||
        desc->bounds.width <= 0.0f ||
        desc->bounds.height <= 0.0f ||
        desc->scroll_offset < 0.0f ||
        desc->row_spacing < 0.0f ||
        desc->indent_width < 0.0f)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    cursor_y = (double)desc->bounds.y -
        (double)desc->scroll_offset;
    if (!isfinite(cursor_y) ||
        cursor_y < -(double)FLT_MAX ||
        cursor_y > (double)FLT_MAX)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    context->flow.active = true;
    context->flow.bounds = desc->bounds;
    context->flow.scroll_offset = desc->scroll_offset;
    context->flow.row_spacing = desc->row_spacing;
    context->flow.indent_width = desc->indent_width;
    context->flow.cursor_y = (float)cursor_y;
    context->flow.content_height = 0.0f;
    return HENKA_SUCCESS;
}

henka_result henka_ui_flow_next_row(
    henka_ui_context* context,
    float row_height,
    size_t indent_level,
    henka_ui_rect* out_bounds,
    bool* out_visible)
{
    double content_offset;
    double indent;
    double row_x;
    double row_y;
    double row_width;
    double row_bottom;
    double clip_bottom;
    double next_content_height;

    if (out_bounds != NULL)
    {
        *out_bounds = (henka_ui_rect){0.0f, 0.0f, 0.0f, 0.0f};
    }
    if (out_visible != NULL)
    {
        *out_visible = false;
    }

    if (context == NULL ||
        !context->frame_active ||
        !context->flow.active ||
        out_bounds == NULL ||
        out_visible == NULL ||
        !henka_ui_float_is_finite(row_height) ||
        row_height <= 0.0f)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    indent = (double)context->flow.indent_width *
        (double)indent_level;
    content_offset = (double)context->flow.content_height;
    if (context->flow.content_height > 0.0f)
    {
        content_offset += (double)context->flow.row_spacing;
    }

    row_x = (double)context->flow.bounds.x + indent;
    row_y = (double)context->flow.bounds.y -
        (double)context->flow.scroll_offset +
        content_offset;
    row_width = (double)context->flow.bounds.width - indent;
    row_bottom = row_y + (double)row_height;
    clip_bottom = (double)context->flow.bounds.y +
        (double)context->flow.bounds.height;
    next_content_height =
        content_offset + (double)row_height;

    if (!isfinite(indent) ||
        !isfinite(content_offset) ||
        !isfinite(row_x) ||
        !isfinite(row_y) ||
        !isfinite(row_width) ||
        !isfinite(row_bottom) ||
        !isfinite(clip_bottom) ||
        !isfinite(next_content_height) ||
        row_width <= 0.0 ||
        row_x < -(double)FLT_MAX ||
        row_x > (double)FLT_MAX ||
        row_y < -(double)FLT_MAX ||
        row_y > (double)FLT_MAX ||
        row_bottom < -(double)FLT_MAX ||
        row_bottom > (double)FLT_MAX ||
        row_width > (double)FLT_MAX ||
        next_content_height < 0.0 ||
        next_content_height > (double)FLT_MAX)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    *out_bounds = (henka_ui_rect){
        (float)row_x,
        (float)row_y,
        (float)row_width,
        row_height};

    *out_visible =
        row_bottom > (double)context->flow.bounds.y &&
        row_y < clip_bottom;

    context->flow.content_height =
        (float)next_content_height;
    context->flow.cursor_y =
        (float)row_bottom;
    return HENKA_SUCCESS;
}

henka_result henka_ui_flow_end(
    henka_ui_context* context,
    float* out_content_height)
{
    float content_height;

    if (out_content_height != NULL)
    {
        *out_content_height = 0.0f;
    }

    if (context == NULL ||
        !context->frame_active ||
        !context->flow.active ||
        out_content_height == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    content_height = context->flow.content_height;
    context->flow.active = false;
    context->flow.bounds =
        (henka_ui_rect){0.0f, 0.0f, 0.0f, 0.0f};
    context->flow.scroll_offset = 0.0f;
    context->flow.row_spacing = 0.0f;
    context->flow.indent_width = 0.0f;
    context->flow.cursor_y = 0.0f;
    context->flow.content_height = 0.0f;
    *out_content_height = content_height;
    return HENKA_SUCCESS;
}

henka_result henka_ui_disclosure_row(
    henka_ui_context* context,
    const char* id,
    henka_ui_rect bounds,
    const char* label,
    bool* expanded,
    bool* out_changed)
{
    bool active;
    bool clicked;
    bool focused;
    bool had_active;
    bool hot;
    bool owns_active;
    bool pressed;
    bool requested_expanded;
    bool original_expanded;
    unsigned int keyboard_consumed_mask;
    henka_ui_draw_checkpoint checkpoint;
    henka_result result;
    henka_vec4 fill;
    float center_y;
    float arrow_x;

    if (out_changed != NULL)
    {
        *out_changed = false;
    }

    if (context == NULL ||
        id == NULL ||
        label == NULL ||
        expanded == NULL ||
        out_changed == NULL ||
        !context->frame_active ||
        !henka_ui_id_is_valid(id) ||
        !henka_ui_rect_is_finite(bounds) ||
        bounds.width <= 0.0f ||
        bounds.height <= 0.0f)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    if (!context->visible)
    {
        return HENKA_SUCCESS;
    }

    henka_ui_capture_checkpoint(context, &checkpoint);
    if (!henka_ui_register_disclosure_id(context, id))
    {
        henka_ui_restore_checkpoint(context, &checkpoint);
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    original_expanded = *expanded;
    requested_expanded = original_expanded;
    keyboard_consumed_mask = 0U;
    focused =
        henka_ui_focused_disclosure_id_equals(context, id);

    if (focused && context->navigation_enter_pressed)
    {
        requested_expanded = !requested_expanded;
        keyboard_consumed_mask |= HENKA_UI_NAVIGATION_ENTER;
    }
    if (focused && context->navigation_left_pressed)
    {
        requested_expanded = false;
        keyboard_consumed_mask |= HENKA_UI_NAVIGATION_LEFT;
    }
    if (focused && context->navigation_right_pressed)
    {
        requested_expanded = true;
        keyboard_consumed_mask |= HENKA_UI_NAVIGATION_RIGHT;
    }

    hot = henka_ui_control_is_hot(context, bounds);
    pressed = hot && context->mouse_left_pressed;
    had_active = henka_ui_active_id_equals(context, id);
    owns_active = had_active || pressed;
    active = context->mouse_left_down && owns_active;
    clicked = hot && context->mouse_left_released && owns_active;

    if (clicked)
    {
        requested_expanded = !requested_expanded;
    }

    fill = active
        ? g_ui_button_active
        : (hot ? g_ui_button_hover : g_ui_row_fill);

    result = henka_ui_push_rect(context, bounds, fill);
    if (result == HENKA_SUCCESS)
    {
        result = henka_ui_push_rect(
            context,
            (henka_ui_rect){
                bounds.x,
                bounds.y + bounds.height - 1.0f,
                bounds.width,
                1.0f},
            g_ui_panel_separator);
    }

    if (result == HENKA_SUCCESS && focused)
    {
        result = henka_ui_push_rect(
            context,
            (henka_ui_rect){
                bounds.x,
                bounds.y + 2.0f,
                2.0f,
                bounds.height - 4.0f},
            g_ui_heading_color);
    }

    center_y = bounds.y + bounds.height * 0.5f;
    arrow_x = bounds.x + 10.0f;
    if (result == HENKA_SUCCESS)
    {
        if (requested_expanded)
        {
            result = henka_ui_push_line(
                context,
                (henka_vec2){arrow_x - 3.0f, center_y - 2.0f},
                (henka_vec2){arrow_x, center_y + 2.0f},
                1.0f,
                g_ui_heading_color);
            if (result == HENKA_SUCCESS)
            {
                result = henka_ui_push_line(
                    context,
                    (henka_vec2){arrow_x, center_y + 2.0f},
                    (henka_vec2){arrow_x + 3.0f, center_y - 2.0f},
                    1.0f,
                    g_ui_heading_color);
            }
        }
        else
        {
            result = henka_ui_push_line(
                context,
                (henka_vec2){arrow_x - 2.0f, center_y - 3.0f},
                (henka_vec2){arrow_x + 2.0f, center_y},
                1.0f,
                g_ui_heading_color);
            if (result == HENKA_SUCCESS)
            {
                result = henka_ui_push_line(
                    context,
                    (henka_vec2){arrow_x + 2.0f, center_y},
                    (henka_vec2){arrow_x - 2.0f, center_y + 3.0f},
                    1.0f,
                    g_ui_heading_color);
            }
        }
    }

    if (result == HENKA_SUCCESS)
    {
        result = henka_ui_draw_fit_text(
            context,
            bounds,
            24.0f,
            bounds.y + (bounds.height - 7.0f) * 0.5f,
            1.0f,
            label,
            g_ui_text_color);
    }

    if (result != HENKA_SUCCESS)
    {
        henka_ui_restore_checkpoint(context, &checkpoint);
        return result;
    }

    if (pressed)
    {
        if (!henka_ui_set_active_id(context, id) ||
            !henka_ui_set_focused_disclosure_id(context, id))
        {
            henka_ui_restore_checkpoint(context, &checkpoint);
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
    }

    if (context->mouse_left_released && owns_active)
    {
        henka_ui_clear_active_id(context);
    }

    *expanded = requested_expanded;
    *out_changed =
        requested_expanded != original_expanded;
    context->consumed_navigation_mask |=
        keyboard_consumed_mask;
    return HENKA_SUCCESS;
}
static henka_result henka_ui_measure_text_raw(
    const char* text,
    float scale,
    int* out_width,
    int* out_height)
{
    int current_width;
    int line_width;
    int lines;
    size_t index;
    size_t text_length;

    if (out_width != NULL)
    {
        *out_width = 0;
    }
    if (out_height != NULL)
    {
        *out_height = 0;
    }

    if (text == NULL || out_width == NULL || out_height == NULL ||
        !henka_ui_float_is_finite(scale) || scale <= 0.0f || scale > HENKA_UI_MAX_SCALE ||
        !henka_checked_c_string_length(text, HENKA_UI_MAX_TEXT_BYTES, &text_length))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    line_width = 0;
    current_width = 0;
    lines = 1;

    for (index = 0U; index < text_length; ++index)
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

henka_result henka_ui_measure_text(
    const char* text,
    float scale,
    int* out_width,
    int* out_height)
{
    return henka_ui_measure_text_raw(text, scale, out_width, out_height);
}

henka_result henka_ui_measure_text_for_context(
    const henka_ui_context* context,
    const char* text,
    float scale,
    int* out_width,
    int* out_height)
{
    if (context == NULL)
    {
        if (out_width != NULL)
        {
            *out_width = 0;
        }
        if (out_height != NULL)
        {
            *out_height = 0;
        }
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    return henka_ui_measure_text_raw(
        text,
        henka_ui_effective_text_scale(context, scale),
        out_width,
        out_height);
}

henka_result henka_ui_panel_with_border_mask(
    henka_ui_context* context,
    henka_ui_rect bounds,
    const char* title,
    unsigned int border_mask)
{
    henka_ui_draw_checkpoint checkpoint;
    henka_ui_rect header_bounds = {0};
    henka_result result;

    if ((border_mask & ~(unsigned int)HENKA_UI_BORDER_ALL) != 0U ||
        context == NULL || title == NULL || !context->frame_active)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (!context->visible)
    {
        return HENKA_SUCCESS;
    }

    henka_ui_capture_checkpoint(context, &checkpoint);
    if (henka_ui_rect_contains(bounds, context->mouse_position))
    {
        context->wants_mouse = true;
    }

    result = henka_ui_push_rect(context, bounds, g_ui_panel_fill);
    if (result == HENKA_SUCCESS)
    {
        header_bounds = (henka_ui_rect){bounds.x, bounds.y, bounds.width, 30.0f};
        result = henka_ui_push_rect(context, header_bounds, g_ui_panel_header_fill);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_ui_push_rect(
            context,
            (henka_ui_rect){bounds.x, bounds.y + header_bounds.height, bounds.width, 1.0f},
            g_ui_panel_separator);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_ui_push_masked_border(
            context,
            bounds,
            1.0f,
            g_ui_panel_border,
            border_mask);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_ui_draw_text(
            context,
            bounds.x + 12.0f,
            bounds.y + 9.0f,
            1.25f,
            title,
            g_ui_heading_color);
    }
    if (result != HENKA_SUCCESS)
    {
        henka_ui_restore_checkpoint(context, &checkpoint);
    }
    return result;
}

henka_result henka_ui_panel(
    henka_ui_context* context,
    henka_ui_rect bounds,
    const char* title)
{
    return henka_ui_panel_with_border_mask(
        context,
        bounds,
        title,
        HENKA_UI_BORDER_ALL);
}

henka_result henka_ui_viewport_frame_with_border_mask(
    henka_ui_context* context,
    henka_ui_rect bounds,
    const char* title,
    unsigned int border_mask)
{
    henka_ui_draw_checkpoint checkpoint;
    henka_ui_rect header_bounds = {0};
    henka_result result;

    if ((border_mask & ~(unsigned int)HENKA_UI_BORDER_ALL) != 0U ||
        context == NULL || title == NULL || !context->frame_active)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (!context->visible)
    {
        return HENKA_SUCCESS;
    }

    henka_ui_capture_checkpoint(context, &checkpoint);
    header_bounds = (henka_ui_rect){bounds.x, bounds.y, bounds.width, 30.0f};
    result = henka_ui_push_rect(context, header_bounds, g_ui_panel_header_fill);
    if (result == HENKA_SUCCESS)
    {
        result = henka_ui_push_rect(
            context,
            (henka_ui_rect){bounds.x, bounds.y + header_bounds.height, bounds.width, 1.0f},
            g_ui_panel_separator);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_ui_push_masked_border(
            context,
            bounds,
            1.0f,
            g_ui_panel_border,
            border_mask);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_ui_draw_text(
            context,
            bounds.x + 12.0f,
            bounds.y + 9.0f,
            1.25f,
            title,
            g_ui_heading_color);
    }
    if (result != HENKA_SUCCESS)
    {
        henka_ui_restore_checkpoint(context, &checkpoint);
    }
    return result;
}

henka_result henka_ui_viewport_frame(
    henka_ui_context* context,
    henka_ui_rect bounds,
    const char* title)
{
    return henka_ui_viewport_frame_with_border_mask(
        context,
        bounds,
        title,
        HENKA_UI_BORDER_ALL);
}

henka_result henka_ui_heading(
    henka_ui_context* context,
    float x,
    float y,
    float scale,
    const char* text)
{
    if (context == NULL || text == NULL || !context->frame_active)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (!context->visible)
    {
        return HENKA_SUCCESS;
    }
    return henka_ui_draw_text(context, x, y, scale, text, g_ui_heading_color);
}

henka_result henka_ui_label_colored(
    henka_ui_context* context,
    float x,
    float y,
    float scale,
    const char* text,
    henka_ui_semantic_color color)
{
    if (context == NULL || text == NULL || !context->frame_active)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (!context->visible)
    {
        return HENKA_SUCCESS;
    }
    return henka_ui_draw_text(
        context,
        x,
        y,
        scale,
        text,
        henka_ui_semantic_color_to_vec4(context, color));
}

henka_result henka_ui_label(henka_ui_context* context, float x, float y, float scale, const char* text)
{
    return henka_ui_label_colored(context, x, y, scale, text, HENKA_UI_COLOR_NORMAL);
}
henka_result henka_ui_value_row_colored(
    henka_ui_context* context,
    henka_ui_rect bounds,
    const char* label,
    const char* value,
    henka_ui_semantic_color label_color,
    henka_ui_semantic_color value_color)
{
    char label_buffer[48];
    char value_buffer[96];
    henka_ui_draw_checkpoint checkpoint;
    float label_x;
    float value_x;
    henka_result result;
    henka_ui_rect value_bounds;
    size_t label_characters;
    size_t value_characters;

    if (context == NULL || label == NULL || value == NULL ||
        !context->frame_active)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (!context->visible)
    {
        return HENKA_SUCCESS;
    }

    henka_ui_capture_checkpoint(context, &checkpoint);

    value_bounds = (henka_ui_rect){
        bounds.x + bounds.width * 0.38f,
        bounds.y + 2.0f,
        bounds.width - bounds.width * 0.38f - 4.0f,
        bounds.height - 4.0f};

    result = henka_ui_push_rect(context, bounds, g_ui_row_fill);
    if (result == HENKA_SUCCESS)
    {
        result = henka_ui_push_rect(
            context,
            (henka_ui_rect){
                bounds.x,
                bounds.y + bounds.height - 1.0f,
                bounds.width,
                1.0f},
            g_ui_panel_separator);
    }
    if (result == HENKA_SUCCESS &&
        value_bounds.width > 0.0f &&
        value_bounds.height > 0.0f)
    {
        result = henka_ui_push_rect(
            context,
            value_bounds,
            g_ui_value_fill);
    }
    if (result == HENKA_SUCCESS &&
        value_bounds.width > 0.0f &&
        value_bounds.height > 0.0f)
    {
        result = henka_ui_push_border(
            context,
            value_bounds,
            1.0f,
            g_ui_panel_border);
    }
    if (result != HENKA_SUCCESS)
    {
        henka_ui_restore_checkpoint(context, &checkpoint);
        return result;
    }

    label_characters = henka_ui_clamped_character_count(
        bounds.width * 0.34f,
        6.0f,
        6U,
        sizeof(label_buffer) - 1U);

    value_characters = henka_ui_clamped_character_count(
        value_bounds.width > 12.0f
            ? value_bounds.width - 12.0f
            : 12.0f,
        6.0f,
        8U,
        sizeof(value_buffer) - 1U);

    henka_ui_copy_fit_text(
        label,
        label_buffer,
        sizeof(label_buffer),
        label_characters);

    henka_ui_copy_fit_text(
        value,
        value_buffer,
        sizeof(value_buffer),
        value_characters);

    label_x = bounds.x + 8.0f;
    value_x = value_bounds.x + 6.0f;

    result = henka_ui_draw_text(
        context,
        label_x,
        bounds.y + 6.0f,
        1.0f,
        label_buffer,
        henka_ui_semantic_color_to_vec4(context, label_color));

    if (result == HENKA_SUCCESS)
    {
        result = henka_ui_draw_text(
            context,
            value_x,
            bounds.y + 6.0f,
            1.0f,
            value_buffer,
            henka_ui_semantic_color_to_vec4(context, value_color));
    }

    if (result != HENKA_SUCCESS)
    {
        henka_ui_restore_checkpoint(context, &checkpoint);
    }
    return result;
}

henka_result henka_ui_value_row(henka_ui_context* context, henka_ui_rect bounds, const char* label, const char* value)
{
    return henka_ui_value_row_colored(
        context,
        bounds,
        label,
        value,
        HENKA_UI_COLOR_INFO,
        HENKA_UI_COLOR_NORMAL);
}
henka_result henka_ui_overlay_hint(
    henka_ui_context* context,
    henka_ui_rect bounds,
    const char* primary_text,
    const char* secondary_text)
{
    henka_ui_draw_checkpoint checkpoint;
    henka_result result;

    if (context == NULL || primary_text == NULL || secondary_text == NULL ||
        !context->frame_active)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    henka_ui_capture_checkpoint(context, &checkpoint);
    result = henka_ui_push_rect(context, bounds, g_ui_hint_fill);
    if (result == HENKA_SUCCESS)
    {
        result = henka_ui_push_border(context, bounds, 1.0f, g_ui_hint_border);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_ui_draw_fit_text(
            context,
            bounds,
            10.0f,
            bounds.y + 8.0f,
            1.0f,
            primary_text,
            g_ui_text_color);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_ui_draw_fit_text(
            context,
            bounds,
            10.0f,
            bounds.y + 24.0f,
            1.0f,
            secondary_text,
            g_ui_muted_text_color);
    }
    if (result != HENKA_SUCCESS)
    {
        henka_ui_restore_checkpoint(context, &checkpoint);
    }
    return result;
}

bool henka_ui_button(henka_ui_context* context, const char* id, henka_ui_rect bounds, const char* label)
{
    return henka_ui_button_internal(
        context,
        id,
        bounds,
        label,
        g_ui_button_fill,
        g_ui_button_hover,
        g_ui_button_active,
        g_ui_heading_color,
        false);
}

bool henka_ui_primary_button(henka_ui_context* context, const char* id, henka_ui_rect bounds, const char* label)
{
    return henka_ui_button_internal(
        context,
        id,
        bounds,
        label,
        g_ui_primary_fill,
        g_ui_primary_hover,
        g_ui_primary_active,
        g_ui_heading_color,
        true);
}

bool henka_ui_selectable(
    henka_ui_context* context,
    const char* id,
    henka_ui_rect bounds,
    const char* label,
    bool selected)
{
    bool active;
    bool clicked;
    bool had_active;
    bool hot;
    bool owns_active;
    bool pressed;
    henka_ui_draw_checkpoint checkpoint;
    henka_result result;
    henka_vec4 fill_color;

    if (context == NULL || id == NULL || label == NULL ||
        !context->frame_active || !henka_ui_id_is_valid(id))
    {
        return false;
    }
    if (!context->visible)
    {
        return false;
    }

    henka_ui_capture_checkpoint(context, &checkpoint);
    hot = henka_ui_control_is_hot(context, bounds);
    pressed = hot && context->mouse_left_pressed;
    had_active = henka_ui_active_id_equals(context, id);
    owns_active = had_active || pressed;
    active = context->mouse_left_down && owns_active;
    clicked = hot && context->mouse_left_released && owns_active;

    fill_color = active ? g_ui_button_active :
        (selected ? (hot ? g_ui_selected_hover : g_ui_selected_fill) :
            (hot ? g_ui_button_hover : g_ui_row_fill));
    result = henka_ui_push_rect(context, bounds, fill_color);
    if (result == HENKA_SUCCESS)
    {
        result = henka_ui_push_rect(
            context,
            (henka_ui_rect){
                bounds.x,
                bounds.y + bounds.height - 1.0f,
                bounds.width,
                1.0f},
            g_ui_panel_separator);
    }
    if (result == HENKA_SUCCESS && selected)
    {
        result = henka_ui_push_rect(
            context,
            (henka_ui_rect){
                bounds.x,
                bounds.y + 2.0f,
                3.0f,
                bounds.height - 4.0f},
            g_ui_heading_color);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_ui_draw_fit_text(
            context,
            bounds,
            selected ? 12.0f : 10.0f,
            bounds.y + 9.0f,
            1.0f,
            label,
            selected ? g_ui_text_color : g_ui_muted_text_color);
    }
    if (result != HENKA_SUCCESS)
    {
        henka_ui_restore_checkpoint(context, &checkpoint);
        return false;
    }

    if (pressed && !henka_ui_set_active_id(context, id))
    {
        henka_ui_restore_checkpoint(context, &checkpoint);
        return false;
    }
    if (context->mouse_left_released && owns_active)
    {
        henka_ui_clear_active_id(context);
    }
    return clicked;
}

bool henka_ui_tab(
    henka_ui_context* context,
    const char* id,
    henka_ui_rect bounds,
    const char* label,
    bool selected)
{
    bool active;
    bool clicked;
    bool had_active;
    bool hot;
    bool owns_active;
    bool pressed;
    henka_ui_draw_checkpoint checkpoint;
    henka_result result;
    henka_vec4 border_color;
    henka_vec4 fill_color;

    if (context == NULL || id == NULL || label == NULL ||
        !context->frame_active || !henka_ui_id_is_valid(id))
    {
        return false;
    }
    if (!context->visible)
    {
        return false;
    }

    henka_ui_capture_checkpoint(context, &checkpoint);
    hot = henka_ui_control_is_hot(context, bounds);
    pressed = hot && context->mouse_left_pressed;
    had_active = henka_ui_active_id_equals(context, id);
    owns_active = had_active || pressed;
    active = context->mouse_left_down && owns_active;
    clicked = hot && context->mouse_left_released && owns_active;

    fill_color = active
        ? g_ui_primary_active
        : (selected
            ? g_ui_tab_active
            : (hot ? g_ui_tab_hover : g_ui_tab_fill));

    border_color =
        selected || hot || active
            ? g_ui_heading_color
            : g_ui_panel_border;

    result = henka_ui_push_rect(context, bounds, fill_color);
    if (result == HENKA_SUCCESS)
    {
        result = henka_ui_push_border(
            context,
            bounds,
            1.0f,
            border_color);
    }
    if (result == HENKA_SUCCESS &&
        selected &&
        bounds.width > 12.0f &&
        bounds.height >= 3.0f)
    {
        result = henka_ui_push_rect(
            context,
            (henka_ui_rect){
                bounds.x + 6.0f,
                bounds.y + bounds.height - 3.0f,
                bounds.width - 12.0f,
                2.0f},
            g_ui_heading_color);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_ui_draw_fit_text(
            context,
            bounds,
            10.0f,
            bounds.y + 8.0f,
            1.0f,
            label,
            selected ? g_ui_text_color : g_ui_muted_text_color);
    }
    if (result != HENKA_SUCCESS)
    {
        henka_ui_restore_checkpoint(context, &checkpoint);
        return false;
    }

    if (pressed && !henka_ui_set_active_id(context, id))
    {
        henka_ui_restore_checkpoint(context, &checkpoint);
        return false;
    }
    if (context->mouse_left_released && owns_active)
    {
        henka_ui_clear_active_id(context);
    }
    return clicked;
}

#define HENKA_UI_SEGMENTED_MAX_OPTIONS 16U

static henka_result henka_ui_segment_internal(
    henka_ui_context* context,
    const char* id,
    henka_ui_rect bounds,
    const char* label,
    bool selected,
    bool* out_clicked)
{
    bool active;
    bool clicked;
    bool had_active;
    bool hot;
    bool owns_active;
    bool pressed;
    henka_ui_draw_checkpoint checkpoint;
    henka_result result;
    henka_vec4 fill_color;

    if (out_clicked != NULL)
    {
        *out_clicked = false;
    }

    if (context == NULL ||
        id == NULL ||
        label == NULL ||
        out_clicked == NULL ||
        !context->frame_active ||
        !henka_ui_id_is_valid(id) ||
        id[0] == '\0' ||
        !henka_ui_rect_is_finite(bounds) ||
        bounds.width <= 0.0f ||
        bounds.height <= 0.0f)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    if (!context->visible)
    {
        return HENKA_SUCCESS;
    }

    henka_ui_capture_checkpoint(context, &checkpoint);

    hot = henka_ui_control_is_hot(context, bounds);
    pressed = hot && context->mouse_left_pressed;
    had_active = henka_ui_active_id_equals(context, id);
    owns_active = had_active || pressed;
    active = context->mouse_left_down && owns_active;
    clicked = hot && context->mouse_left_released && owns_active;

    fill_color = active
        ? g_ui_primary_active
        : (hot
            ? g_ui_tab_hover
            : (selected ? g_ui_button_fill : g_ui_value_fill));

    result = henka_ui_push_rect(context, bounds, fill_color);

    if (result == HENKA_SUCCESS && selected)
    {
        result = henka_ui_push_rect(
            context,
            (henka_ui_rect){
                bounds.x + 4.0f,
                bounds.y + bounds.height - 3.0f,
                bounds.width - 8.0f,
                2.0f},
            g_ui_heading_color);
    }

    if (result == HENKA_SUCCESS)
    {
        result = henka_ui_draw_fit_text(
            context,
            bounds,
            8.0f,
            bounds.y + (bounds.height - 7.0f) * 0.5f,
            1.0f,
            label,
            selected ? g_ui_text_color : g_ui_muted_text_color);
    }

    if (result != HENKA_SUCCESS)
    {
        henka_ui_restore_checkpoint(context, &checkpoint);
        return result;
    }

    if (pressed && !henka_ui_set_active_id(context, id))
    {
        henka_ui_restore_checkpoint(context, &checkpoint);
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    if (context->mouse_left_released && owns_active)
    {
        henka_ui_clear_active_id(context);
    }

    *out_clicked = clicked;
    return HENKA_SUCCESS;
}

henka_result henka_ui_segmented_select(
    henka_ui_context* context,
    const char* id,
    henka_ui_rect bounds,
    const char* const* labels,
    size_t option_count,
    size_t* selected_index,
    bool* out_changed)
{
    henka_ui_draw_checkpoint checkpoint;
    henka_result result;
    size_t id_length;
    size_t index;
    size_t requested_index;
    float segment_width;

    if (out_changed != NULL)
    {
        *out_changed = false;
    }

    if (context == NULL ||
        id == NULL ||
        labels == NULL ||
        selected_index == NULL ||
        out_changed == NULL ||
        !context->frame_active ||
        !henka_ui_rect_is_finite(bounds) ||
        bounds.width <= 0.0f ||
        bounds.height <= 0.0f ||
        option_count < 2U ||
        option_count > HENKA_UI_SEGMENTED_MAX_OPTIONS ||
        *selected_index > option_count ||
        !henka_checked_c_string_length(
            id,
            HENKA_UI_MAX_ID_BYTES - 4U,
            &id_length) ||
        id_length == 0U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    for (index = 0U; index < option_count; ++index)
    {
        size_t label_length;

        if (labels[index] == NULL ||
            !henka_checked_c_string_length(
                labels[index],
                HENKA_UI_MAX_TEXT_BYTES,
                &label_length) ||
            label_length == 0U)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
    }

    if (!context->visible)
    {
        return HENKA_SUCCESS;
    }

    segment_width = bounds.width / (float)option_count;
    if (!henka_ui_float_is_finite(segment_width) ||
        segment_width <= 0.0f)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    henka_ui_capture_checkpoint(context, &checkpoint);
    requested_index = *selected_index;

    for (index = 0U; index < option_count; ++index)
    {
        bool clicked;
        char segment_id[HENKA_UI_MAX_ID_BYTES + 1U];
        henka_ui_rect segment_bounds;
        const int id_result = snprintf(
            segment_id,
            sizeof(segment_id),
            "%s.%u",
            id,
            (unsigned int)index);

        if (id_result < 0 ||
            (size_t)id_result >= sizeof(segment_id))
        {
            henka_ui_restore_checkpoint(context, &checkpoint);
            return HENKA_ERROR_INVALID_ARGUMENT;
        }

        segment_bounds.x =
            bounds.x + segment_width * (float)index;
        segment_bounds.y = bounds.y;
        segment_bounds.width =
            index + 1U == option_count
                ? bounds.x + bounds.width - segment_bounds.x
                : segment_width;
        segment_bounds.height = bounds.height;

        result = henka_ui_segment_internal(
            context,
            segment_id,
            segment_bounds,
            labels[index],
            index == *selected_index,
            &clicked);

        if (result != HENKA_SUCCESS)
        {
            henka_ui_restore_checkpoint(context, &checkpoint);
            return result;
        }

        if (clicked)
        {
            requested_index = index;
        }

        if (index > 0U)
        {
            result = henka_ui_push_rect(
                context,
                (henka_ui_rect){
                    segment_bounds.x,
                    bounds.y + 4.0f,
                    1.0f,
                    bounds.height - 8.0f},
                g_ui_panel_separator);

            if (result != HENKA_SUCCESS)
            {
                henka_ui_restore_checkpoint(context, &checkpoint);
                return result;
            }
        }
    }

    result = henka_ui_push_border(
        context,
        bounds,
        1.0f,
        g_ui_panel_border);

    if (result != HENKA_SUCCESS)
    {
        henka_ui_restore_checkpoint(context, &checkpoint);
        return result;
    }

    if (requested_index != *selected_index)
    {
        *selected_index = requested_index;
        *out_changed = true;
    }

    return HENKA_SUCCESS;
}
bool henka_ui_toggle(
    henka_ui_context* context,
    const char* id,
    henka_ui_rect bounds,
    const char* label,
    bool* value)
{
    bool active;
    bool clicked;
    bool displayed_value;
    bool had_active;
    bool hot;
    bool owns_active;
    bool pressed;
    char label_buffer[48];
    henka_ui_draw_checkpoint checkpoint;
    henka_result result;
    henka_ui_rect knob_bounds;
    henka_ui_rect track_bounds;
    size_t label_characters;

    if (context == NULL || id == NULL || label == NULL || value == NULL ||
        !context->frame_active || !henka_ui_id_is_valid(id))
    {
        return false;
    }
    if (!context->visible)
    {
        return false;
    }

    henka_ui_capture_checkpoint(context, &checkpoint);
    hot = henka_ui_control_is_hot(context, bounds);
    pressed = hot && context->mouse_left_pressed;
    had_active = henka_ui_active_id_equals(context, id);
    owns_active = had_active || pressed;
    active = context->mouse_left_down && owns_active;
    clicked = hot && context->mouse_left_released && owns_active;
    displayed_value = clicked ? !*value : *value;

    result = henka_ui_push_rect(
        context,
        bounds,
        active
            ? g_ui_button_active
            : (hot ? g_ui_button_hover : g_ui_row_fill));

    if (result == HENKA_SUCCESS)
    {
        result = henka_ui_push_rect(
            context,
            (henka_ui_rect){
                bounds.x,
                bounds.y + bounds.height - 1.0f,
                bounds.width,
                1.0f},
            g_ui_panel_separator);
    }

    track_bounds = (henka_ui_rect){
        bounds.x + bounds.width - 42.0f,
        bounds.y + (bounds.height - 16.0f) * 0.5f,
        32.0f,
        16.0f};

    knob_bounds = (henka_ui_rect){
        displayed_value
            ? track_bounds.x + track_bounds.width - 14.0f
            : track_bounds.x + 2.0f,
        track_bounds.y + 2.0f,
        12.0f,
        12.0f};

    if (result == HENKA_SUCCESS)
    {
        result = henka_ui_push_rect(
            context,
            track_bounds,
            displayed_value ? g_ui_toggle_on : g_ui_button_fill);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_ui_push_border(
            context,
            track_bounds,
            1.0f,
            displayed_value || hot
                ? g_ui_heading_color
                : g_ui_panel_border);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_ui_push_rect(
            context,
            knob_bounds,
            displayed_value ? g_ui_text_color : g_ui_muted_text_color);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_ui_push_border(
            context,
            knob_bounds,
            1.0f,
            g_ui_panel_border);
    }

    label_characters = henka_ui_clamped_character_count(
        bounds.width > 64.0f ? bounds.width - 64.0f : 24.0f,
        6.0f,
        4U,
        sizeof(label_buffer) - 1U);

    henka_ui_copy_fit_text(
        label,
        label_buffer,
        sizeof(label_buffer),
        label_characters);

    if (result == HENKA_SUCCESS)
    {
        result = henka_ui_draw_text(
            context,
            bounds.x + 10.0f,
            bounds.y + 9.0f,
            1.0f,
            label_buffer,
            displayed_value ? g_ui_text_color : g_ui_muted_text_color);
    }
    if (result != HENKA_SUCCESS)
    {
        henka_ui_restore_checkpoint(context, &checkpoint);
        return false;
    }

    if (pressed && !henka_ui_set_active_id(context, id))
    {
        henka_ui_restore_checkpoint(context, &checkpoint);
        return false;
    }
    if (context->mouse_left_released && owns_active)
    {
        henka_ui_clear_active_id(context);
    }
    if (clicked)
    {
        *value = displayed_value;
    }
    return clicked;
}

henka_result henka_ui_status_chip(
    henka_ui_context* context,
    henka_ui_rect bounds,
    const char* label,
    bool warning)
{
    henka_ui_draw_checkpoint checkpoint;
    henka_result result;

    if (context == NULL || label == NULL || !context->frame_active)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (!context->visible)
    {
        return HENKA_SUCCESS;
    }

    henka_ui_capture_checkpoint(context, &checkpoint);
    result = henka_ui_push_rect(
        context,
        bounds,
        warning ? g_ui_status_warning_fill : g_ui_status_fill);
    if (result == HENKA_SUCCESS)
    {
        result = henka_ui_push_rect(
            context,
            (henka_ui_rect){
                bounds.x,
                bounds.y,
                3.0f,
                bounds.height},
            warning ? g_ui_semantic_warning : g_ui_semantic_success);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_ui_draw_fit_text(
            context,
            bounds,
            9.0f,
            bounds.y + 5.0f,
            1.0f,
            label,
            g_ui_text_color);
    }
    if (result != HENKA_SUCCESS)
    {
        henka_ui_restore_checkpoint(context, &checkpoint);
    }
    return result;
}
