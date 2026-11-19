#include "ui_internal.h"

#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include <henka/memory.h>

#include "../core/checked.h"

#define HENKA_UI_TEXT_REPLACEMENT_CODEPOINT 0xfffdU
#define HENKA_UI_TEXT_ELLIPSIS_CODEPOINT 0x2026U

typedef struct henka_ui_utf8_decoded
{
    uint32_t codepoint;
    size_t byte_count;
    bool valid;
} henka_ui_utf8_decoded;

struct henka_ui_font_context_scale_update
{
    size_t binding_count;
    uint16_t app_scale_percent;
    henka_ui_font_context_scale_binding bindings[HENKA_UI_FONT_MAX_CONTEXTS];
    float output_scales[HENKA_UI_FONT_MAX_CONTEXTS];
};

static double henka_ui_context_editor_scale_factor(
    const henka_ui_context* context)
{
    if (context == NULL ||
        !henka_ui_font_app_scale_is_supported(context->font_app_scale_percent))
    {
        return 0.0;
    }
    return (double)context->font_app_scale_percent / 100.0;
}

static henka_ui_utf8_decoded henka_ui_text_decode_utf8(const uint8_t* bytes, size_t remaining)
{
    henka_ui_utf8_decoded decoded = {0U, 1U, false};
    const uint8_t first = bytes[0];
    uint32_t codepoint;
    size_t expected;
    uint32_t minimum;
    size_t index;

    if (first <= 0x7fU)
    {
        decoded.codepoint = first;
        decoded.valid = true;
        return decoded;
    }
    if (first >= 0xc2U && first <= 0xdfU)
    {
        expected = 2U;
        minimum = 0x80U;
        codepoint = (uint32_t)(first & 0x1fU);
    }
    else if (first >= 0xe0U && first <= 0xefU)
    {
        expected = 3U;
        minimum = 0x800U;
        codepoint = (uint32_t)(first & 0x0fU);
    }
    else if (first >= 0xf0U && first <= 0xf4U)
    {
        expected = 4U;
        minimum = 0x10000U;
        codepoint = (uint32_t)(first & 0x07U);
    }
    else
    {
        decoded.codepoint = HENKA_UI_TEXT_REPLACEMENT_CODEPOINT;
        return decoded;
    }
    if (remaining < expected)
    {
        decoded.codepoint = HENKA_UI_TEXT_REPLACEMENT_CODEPOINT;
        return decoded;
    }
    for (index = 1U; index < expected; ++index)
    {
        if ((bytes[index] & 0xc0U) != 0x80U)
        {
            decoded.codepoint = HENKA_UI_TEXT_REPLACEMENT_CODEPOINT;
            return decoded;
        }
        codepoint = (codepoint << 6U) | (uint32_t)(bytes[index] & 0x3fU);
    }
    if (codepoint < minimum || codepoint > 0x10ffffU ||
        (codepoint >= 0xd800U && codepoint <= 0xdfffU))
    {
        decoded.codepoint = HENKA_UI_TEXT_REPLACEMENT_CODEPOINT;
        return decoded;
    }
    decoded.codepoint = codepoint;
    decoded.byte_count = expected;
    decoded.valid = true;
    return decoded;
}

static henka_result henka_ui_text_layout_build_internal(
    const henka_ui_font_resource* resource,
    const char* utf8,
    size_t utf8_size,
    float max_width,
    size_t max_line_count,
    bool include_glyph_placements,
    henka_ui_text_run* out_run,
    henka_ui_font_diagnostic* out_diagnostic);

henka_result henka_ui_measure_font_text(
    const henka_ui_context* context,
    const char* text,
    float scale,
    int* out_width,
    int* out_height)
{
    const henka_ui_font_resource* font;
    henka_ui_text_run run;
    size_t text_length;
    double measured_width;
    double measured_height;
    double logical_scale;
    henka_result result;

    if (out_width != NULL)
    {
        *out_width = 0;
    }
    if (out_height != NULL)
    {
        *out_height = 0;
    }
    if (context == NULL || context->font_profile == NULL || text == NULL ||
        !henka_ui_font_app_scale_is_supported(context->font_app_scale_percent) ||
        out_width == NULL || out_height == NULL || !isfinite((double)scale) ||
        scale <= 0.0f || scale > HENKA_UI_MAX_SCALE ||
        !isfinite((double)context->output_scale) ||
        context->output_scale < 0.25f || context->output_scale > 4.0f ||
        !henka_checked_c_string_length(text, HENKA_UI_TEXT_MAX_BYTES, &text_length))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    font = henka_ui_font_raster_profile_get_font(
        context->font_profile, context->font_app_scale_percent);
    if (font == NULL)
    {
        return HENKA_ERROR_ASSET_SOURCE;
    }
    logical_scale = (double)scale /
        ((double)context->output_scale *
         henka_ui_context_editor_scale_factor(context));

    memset(&run, 0, sizeof(run));
    result = henka_ui_text_layout_build_internal(
        font,
        text,
        text_length,
        0.0f,
        SIZE_MAX,
        false,
        &run,
        NULL);
    if (result != HENKA_SUCCESS)
    {
        henka_ui_text_run_dispose(&run);
        return result;
    }
    measured_width = ceil((double)run.advance_width * logical_scale);
    measured_height = ceil(
        (double)run.line_height * (double)run.line_count * logical_scale);
    if (!isfinite(measured_width) || !isfinite(measured_height) ||
        measured_width < 0.0 || measured_height <= 0.0 ||
        measured_width > (double)INT_MAX || measured_height > (double)INT_MAX)
    {
        henka_ui_text_run_dispose(&run);
        return HENKA_ERROR_NUMERIC_RANGE;
    }
    *out_width = (int)measured_width;
    *out_height = (int)measured_height;
    henka_ui_text_run_dispose(&run);
    return HENKA_SUCCESS;
}

henka_result henka_ui_measure_font_text_byte_offsets_x(
    const henka_ui_context* context,
    const char* text,
    size_t text_byte_count,
    const size_t* byte_offsets,
    size_t byte_offset_count,
    float scale,
    float* out_x_positions)
{
    const henka_ui_font_resource* font;
    henka_ui_text_run run;
    henka_ui_font_diagnostic diagnostic;
    size_t glyph_index = 0U;
    size_t index;
    double logical_scale;
    double maximum_x;
    henka_result result;

    if ((byte_offset_count > 0U &&
         (byte_offsets == NULL || out_x_positions == NULL)) ||
        byte_offset_count > HENKA_UI_TEXT_MAX_BYTES + 1U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    for (index = 0U; index < byte_offset_count; ++index)
    {
        out_x_positions[index] = 0.0f;
    }
    if (context == NULL || context->font_profile == NULL || text == NULL ||
        text_byte_count > HENKA_UI_TEXT_MAX_BYTES ||
        memchr(text, '\0', text_byte_count) != NULL ||
        memchr(text, '\n', text_byte_count) != NULL ||
        memchr(text, '\r', text_byte_count) != NULL ||
        !henka_ui_font_app_scale_is_supported(context->font_app_scale_percent) ||
        !isfinite((double)scale) || scale <= 0.0f || scale > HENKA_UI_MAX_SCALE ||
        !isfinite((double)context->output_scale) ||
        context->output_scale < 0.25f || context->output_scale > 4.0f)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (byte_offset_count == 0U)
    {
        return HENKA_SUCCESS;
    }
    for (index = 0U; index < byte_offset_count; ++index)
    {
        if (byte_offsets[index] > text_byte_count ||
            (index > 0U && byte_offsets[index] < byte_offsets[index - 1U]))
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
    }
    font = henka_ui_font_raster_profile_get_font(
        context->font_profile, context->font_app_scale_percent);
    if (font == NULL)
    {
        return HENKA_ERROR_ASSET_SOURCE;
    }
    memset(&run, 0, sizeof(run));
    result = henka_ui_text_layout_build_limited(
        font, text, text_byte_count, 0.0f, SIZE_MAX, &run, &diagnostic);
    if (result != HENKA_SUCCESS)
    {
        henka_ui_text_run_dispose(&run);
        return result;
    }
    logical_scale = (double)scale /
        ((double)context->output_scale *
         henka_ui_context_editor_scale_factor(context));
    if (!isfinite(logical_scale) || logical_scale <= 0.0 ||
        logical_scale > (double)FLT_MAX)
    {
        henka_ui_text_run_dispose(&run);
        return HENKA_ERROR_NUMERIC_RANGE;
    }
    maximum_x = (double)run.advance_width * logical_scale;
    if (!isfinite(maximum_x) || maximum_x < 0.0 || maximum_x > (double)FLT_MAX)
    {
        henka_ui_text_run_dispose(&run);
        return HENKA_ERROR_NUMERIC_RANGE;
    }
    for (index = 0U; index < byte_offset_count; ++index)
    {
        if (byte_offsets[index] < text_byte_count)
        {
            while (glyph_index < run.glyph_count &&
                run.glyphs[glyph_index].source_byte_offset < byte_offsets[index])
            {
                ++glyph_index;
            }
            if (glyph_index >= run.glyph_count ||
                run.glyphs[glyph_index].source_byte_offset != byte_offsets[index])
            {
                henka_ui_text_run_dispose(&run);
                return HENKA_ERROR_INVALID_ARGUMENT;
            }
        }
    }
    glyph_index = 0U;
    for (index = 0U; index < byte_offset_count; ++index)
    {
        double x;
        if (byte_offsets[index] == text_byte_count)
        {
            x = (double)run.advance_width;
        }
        else
        {
            while (glyph_index < run.glyph_count &&
                run.glyphs[glyph_index].source_byte_offset < byte_offsets[index])
            {
                ++glyph_index;
            }
            x = (double)run.glyphs[glyph_index].pen_x;
        }
        x *= logical_scale;
        out_x_positions[index] = (float)x;
    }
    henka_ui_text_run_dispose(&run);
    return HENKA_SUCCESS;
}

static void henka_ui_text_run_include_ink(henka_ui_text_run* run, const henka_ui_text_glyph* glyph)
{
    const float right = glyph->x + glyph->width;
    const float bottom = glyph->y + glyph->height;
    if (glyph->width <= 0.0f || glyph->height <= 0.0f)
    {
        return;
    }
    if (run->glyph_count == 0U || (run->ink_left == 0.0f && run->ink_right == 0.0f &&
            run->ink_top == 0.0f && run->ink_bottom == 0.0f))
    {
        run->ink_left = glyph->x;
        run->ink_top = glyph->y;
        run->ink_right = right;
        run->ink_bottom = bottom;
        return;
    }
    if (glyph->x < run->ink_left)
    {
        run->ink_left = glyph->x;
    }
    if (glyph->y < run->ink_top)
    {
        run->ink_top = glyph->y;
    }
    if (right > run->ink_right)
    {
        run->ink_right = right;
    }
    if (bottom > run->ink_bottom)
    {
        run->ink_bottom = bottom;
    }
}

static bool henka_ui_text_append_glyph_info(
    henka_ui_text_run* run,
    size_t source_byte_offset,
    uint32_t source_codepoint,
    uint32_t rendered_codepoint,
    const henka_ui_font_glyph_info* glyph_info,
    float kerning,
    float line_offset_y,
    float* in_out_pen_x,
    bool include_glyph_placement)
{
    float glyph_pen_x;
    float next_pen_x;

    if (run == NULL || glyph_info == NULL || in_out_pen_x == NULL ||
        !isfinite((double)kerning))
    {
        return false;
    }
    glyph_pen_x = *in_out_pen_x + kerning;
    next_pen_x = glyph_pen_x + glyph_info->advance_x;
    if (!isfinite((double)glyph_pen_x) || !isfinite((double)next_pen_x) ||
        glyph_pen_x < 0.0f || next_pen_x < 0.0f)
    {
        return false;
    }
    if (include_glyph_placement)
    {
        henka_ui_text_glyph* placement;
        if (run->glyphs == NULL)
        {
            return false;
        }
        placement = &run->glyphs[run->glyph_count];
        placement->source_byte_offset = source_byte_offset;
        placement->source_codepoint = source_codepoint;
        placement->rendered_codepoint = rendered_codepoint;
        placement->glyph_index = glyph_info->glyph_index;
        placement->pen_x = glyph_pen_x;
        placement->x = glyph_pen_x + glyph_info->bitmap_x;
        placement->y = glyph_info->bitmap_y + line_offset_y;
        placement->width = (float)glyph_info->bitmap_width;
        placement->height = (float)glyph_info->bitmap_height;
        placement->advance_x = glyph_info->advance_x;
        placement->atlas_x = glyph_info->atlas_x;
        placement->atlas_y = glyph_info->atlas_y;
        henka_ui_text_run_include_ink(run, placement);
        ++run->glyph_count;
    }
    *in_out_pen_x = next_pen_x;
    run->advance_width = next_pen_x;
    return true;
}

static uint32_t henka_ui_text_previous_glyph(
    const henka_ui_font_resource* resource,
    const henka_ui_text_run* run,
    size_t line_start)
{
    if (resource == NULL || run == NULL || run->glyph_count <= line_start)
    {
        return 0U;
    }
    return run->glyphs[run->glyph_count - 1U].glyph_index;
}

static bool henka_ui_text_append_ellipsis(
    const henka_ui_font_resource* resource,
    henka_ui_text_run* run,
    float max_width,
    size_t line_start,
    float line_offset_y,
    float* in_out_pen_x)
{
    henka_ui_font_glyph_info ellipsis;
    uint32_t codepoint = HENKA_UI_TEXT_ELLIPSIS_CODEPOINT;

    if (!henka_ui_font_resource_get_glyph(resource, codepoint, &ellipsis))
    {
        codepoint = (uint32_t)'?';
        if (!henka_ui_font_resource_get_glyph(resource, codepoint, &ellipsis))
        {
            return false;
        }
    }
    for (;;)
    {
        const uint32_t previous = henka_ui_text_previous_glyph(resource, run, line_start);
        float kerning = 0.0f;
        if (previous != 0U &&
            !henka_ui_font_resource_get_kerning(resource, previous, ellipsis.glyph_index, &kerning))
        {
            return false;
        }
        if (*in_out_pen_x + kerning + ellipsis.advance_x <= max_width)
        {
            return henka_ui_text_append_glyph_info(
                run,
                SIZE_MAX,
                HENKA_UI_TEXT_ELLIPSIS_CODEPOINT,
                codepoint,
                &ellipsis,
                kerning,
                line_offset_y,
                in_out_pen_x,
                true);
        }
        if (run->glyph_count <= line_start)
        {
            return true;
        }
        --run->glyph_count;
        if (run->glyph_count <= line_start)
        {
            *in_out_pen_x = 0.0f;
        }
        else
        {
            const henka_ui_text_glyph* last = &run->glyphs[run->glyph_count - 1U];
            *in_out_pen_x = last->pen_x + last->advance_x;
            run->advance_width = *in_out_pen_x;
            /* The ink union is recomputed below because removed glyphs may have
             * owned one of its extremal bounds. */
            run->ink_left = 0.0f;
            run->ink_top = 0.0f;
            run->ink_right = 0.0f;
            run->ink_bottom = 0.0f;
            {
                size_t index;
                for (index = 0U; index < run->glyph_count; ++index)
                {
                    henka_ui_text_run_include_ink(run, &run->glyphs[index]);
                }
            }
        }
    }
}

static henka_result henka_ui_text_layout_build_internal(
    const henka_ui_font_resource* resource,
    const char* utf8,
    size_t utf8_size,
    float max_width,
    size_t max_line_count,
    bool include_glyph_placements,
    henka_ui_text_run* out_run,
    henka_ui_font_diagnostic* out_diagnostic)
{
    henka_ui_text_run candidate;
    henka_ui_font_info font_info;
    float pen_x = 0.0f;
    float line_offset_y = 0.0f;
    float maximum_line_advance = 0.0f;
    size_t offset = 0U;
    size_t line_start = 0U;
    uint32_t previous_glyph_index = 0U;
    size_t capacity;
    size_t allocation_size;

    if (out_diagnostic != NULL)
    {
        memset(out_diagnostic, 0, sizeof(*out_diagnostic));
    }
    if (out_run == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    memset(out_run, 0, sizeof(*out_run));
    memset(&candidate, 0, sizeof(candidate));
    if (resource == NULL || (utf8 == NULL && utf8_size > 0U) || max_line_count == 0U ||
        (!include_glyph_placements && (max_width != 0.0f || max_line_count != SIZE_MAX)) ||
        !isfinite(max_width) || max_width < 0.0f ||
        !henka_ui_font_resource_get_info(resource, &font_info))
    {
        if (out_diagnostic != NULL)
        {
            out_diagnostic->stage = HENKA_UI_FONT_STAGE_TEXT;
            (void)snprintf(out_diagnostic->message, sizeof(out_diagnostic->message),
                "text layout arguments are invalid");
        }
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (utf8_size > HENKA_UI_TEXT_MAX_BYTES)
    {
        if (out_diagnostic != NULL)
        {
            out_diagnostic->stage = HENKA_UI_FONT_STAGE_TEXT;
            (void)snprintf(out_diagnostic->identity, sizeof(out_diagnostic->identity), "%s", font_info.identity);
            (void)snprintf(out_diagnostic->message, sizeof(out_diagnostic->message),
                "text exceeds the 4096-byte layout limit");
        }
        return HENKA_ERROR_LIMIT;
    }
    candidate.ascent = font_info.ascent;
    candidate.descent = font_info.descent;
    candidate.line_height = font_info.line_height;
    candidate.line_count = 1U;
    capacity = utf8_size;
    if (include_glyph_placements && capacity > 0U)
    {
        if (!henka_checked_size_multiply(capacity, sizeof(*candidate.glyphs), &allocation_size))
        {
            return HENKA_ERROR_LIMIT;
        }
        candidate.glyphs = (henka_ui_text_glyph*)henka_calloc(1U, allocation_size);
        if (candidate.glyphs == NULL)
        {
            if (out_diagnostic != NULL)
            {
                out_diagnostic->stage = HENKA_UI_FONT_STAGE_ALLOCATION;
                (void)snprintf(out_diagnostic->identity, sizeof(out_diagnostic->identity), "%s", font_info.identity);
                (void)snprintf(out_diagnostic->message, sizeof(out_diagnostic->message),
                    "text layout placement allocation failed");
            }
            return HENKA_ERROR_OUT_OF_MEMORY;
        }
    }
    while (offset < utf8_size)
    {
        const size_t source_byte_offset = offset;
        const henka_ui_utf8_decoded decoded = henka_ui_text_decode_utf8(
            (const uint8_t*)utf8 + offset, utf8_size - offset);
        uint32_t rendered_codepoint = decoded.codepoint;
        henka_ui_font_glyph_info glyph_info;
        uint32_t previous_glyph;
        float kerning = 0.0f;
        bool replaced = !decoded.valid;

        offset += decoded.byte_count;
        if (decoded.valid && rendered_codepoint == (uint32_t)'\r')
        {
            continue;
        }
        if (decoded.valid && rendered_codepoint == (uint32_t)'\n')
        {
            if (pen_x > maximum_line_advance)
            {
                maximum_line_advance = pen_x;
            }
            if (candidate.line_count >= max_line_count)
            {
                candidate.truncated = true;
                if (!henka_ui_text_append_ellipsis(
                        resource, &candidate, max_width, line_start,
                        line_offset_y, &pen_x))
                {
                    henka_ui_text_run_dispose(&candidate);
                    return HENKA_ERROR_ASSET_SOURCE;
                }
                break;
            }
            if (candidate.line_count == SIZE_MAX ||
                !isfinite((double)line_offset_y + (double)font_info.line_height) ||
                (double)line_offset_y + (double)font_info.line_height > (double)FLT_MAX)
            {
                henka_ui_text_run_dispose(&candidate);
                return HENKA_ERROR_LIMIT;
            }
            pen_x = 0.0f;
            line_offset_y += font_info.line_height;
            line_start = candidate.glyph_count;
            previous_glyph_index = 0U;
            ++candidate.line_count;
            continue;
        }
        if (rendered_codepoint < 0x20U || rendered_codepoint == 0x7fU)
        {
            rendered_codepoint = HENKA_UI_TEXT_REPLACEMENT_CODEPOINT;
            replaced = true;
        }
        if (!henka_ui_font_resource_get_glyph(resource, rendered_codepoint, &glyph_info))
        {
            rendered_codepoint = (uint32_t)'?';
            replaced = true;
            if (!henka_ui_font_resource_get_glyph(resource, rendered_codepoint, &glyph_info))
            {
                henka_ui_text_run_dispose(&candidate);
                return HENKA_ERROR_ASSET_SOURCE;
            }
        }
        previous_glyph = previous_glyph_index;
        if (previous_glyph != 0U &&
            !henka_ui_font_resource_get_kerning(resource, previous_glyph, glyph_info.glyph_index, &kerning))
        {
            henka_ui_text_run_dispose(&candidate);
            return HENKA_ERROR_ASSET_SOURCE;
        }
        if (max_width > 0.0f && pen_x + kerning + glyph_info.advance_x > max_width)
        {
            candidate.truncated = true;
            if (!henka_ui_text_append_ellipsis(
                    resource, &candidate, max_width, line_start,
                    line_offset_y, &pen_x))
            {
                henka_ui_text_run_dispose(&candidate);
                return HENKA_ERROR_ASSET_SOURCE;
            }
            break;
        }
        if (!henka_ui_text_append_glyph_info(
                &candidate,
                source_byte_offset,
                decoded.codepoint,
                rendered_codepoint,
                &glyph_info,
                kerning,
                line_offset_y,
                &pen_x,
                include_glyph_placements))
        {
            henka_ui_text_run_dispose(&candidate);
            return HENKA_ERROR_ASSET_SOURCE;
        }
        previous_glyph_index = glyph_info.glyph_index;
        if (replaced)
        {
            ++candidate.replacement_count;
        }
    }
    if (pen_x > maximum_line_advance)
    {
        maximum_line_advance = pen_x;
    }
    candidate.advance_width = maximum_line_advance;
    *out_run = candidate;
    if (out_diagnostic != NULL)
    {
        out_diagnostic->stage = HENKA_UI_FONT_STAGE_NONE;
        (void)snprintf(out_diagnostic->identity, sizeof(out_diagnostic->identity), "%s", font_info.identity);
    }
    return HENKA_SUCCESS;
}

henka_result henka_ui_text_layout_build_limited(
    const henka_ui_font_resource* resource,
    const char* utf8,
    size_t utf8_size,
    float max_width,
    size_t max_line_count,
    henka_ui_text_run* out_run,
    henka_ui_font_diagnostic* out_diagnostic)
{
    return henka_ui_text_layout_build_internal(
        resource,
        utf8,
        utf8_size,
        max_width,
        max_line_count,
        true,
        out_run,
        out_diagnostic);
}

henka_result henka_ui_text_layout_build(
    const henka_ui_font_resource* resource,
    const char* utf8,
    size_t utf8_size,
    float max_width,
    henka_ui_text_run* out_run,
    henka_ui_font_diagnostic* out_diagnostic)
{
    return henka_ui_text_layout_build_limited(
        resource,
        utf8,
        utf8_size,
        max_width,
        SIZE_MAX,
        out_run,
        out_diagnostic);
}

void henka_ui_text_run_dispose(henka_ui_text_run* run)
{
    if (run == NULL)
    {
        return;
    }
    henka_free(run->glyphs);
    memset(run, 0, sizeof(*run));
}

static void henka_ui_draw_vertex_set(
    henka_ui_draw_vertex* vertex,
    float x,
    float y,
    float u,
    float v,
    henka_vec4 color,
    bool glyph)
{
    vertex->x = x;
    vertex->y = y;
    vertex->r = color.x;
    vertex->g = color.y;
    vertex->b = color.z;
    vertex->a = color.w;
    vertex->u = u;
    vertex->v = v;
    vertex->glyph_coverage = glyph ? 1.0f : 0.0f;
}

henka_result henka_ui_context_build_quad_vertices(
    const henka_ui_context* context,
    henka_ui_draw_vertex* out_vertices,
    size_t vertex_capacity,
    size_t* out_vertex_count)
{
    size_t required_vertices;
    size_t index;

    if (out_vertex_count != NULL)
    {
        *out_vertex_count = 0U;
    }
    if (context == NULL || out_vertex_count == NULL ||
        (context->draw_quad_count > 0U && out_vertices == NULL) ||
        context->draw_quad_count > HENKA_UI_MAX_DRAW_ITEMS ||
        !henka_checked_size_multiply(context->draw_quad_count, 6U, &required_vertices))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (required_vertices > vertex_capacity)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    /* Validate the entire candidate before writing any output vertex. */
    for (index = 0U; index < context->draw_quad_count; ++index)
    {
        const henka_ui_draw_quad* quad = &context->draw_quads[index];
        const double right = (double)quad->bounds.x + (double)quad->bounds.width;
        const double bottom = (double)quad->bounds.y + (double)quad->bounds.height;
        const bool glyph = quad->kind == HENKA_UI_DRAW_QUAD_GLYPH;

        if ((quad->kind != HENKA_UI_DRAW_QUAD_SOLID && !glyph) ||
            !isfinite(quad->bounds.x) || !isfinite(quad->bounds.y) ||
            !isfinite(quad->bounds.width) || !isfinite(quad->bounds.height) ||
            !isfinite(right) || !isfinite(bottom) ||
            right < -(double)FLT_MAX || right > (double)FLT_MAX ||
            bottom < -(double)FLT_MAX || bottom > (double)FLT_MAX ||
            quad->bounds.width < 0.0f || quad->bounds.height < 0.0f ||
            !isfinite(quad->color.x) || !isfinite(quad->color.y) ||
            !isfinite(quad->color.z) || !isfinite(quad->color.w) ||
            (glyph && (!isfinite(quad->u0) || !isfinite(quad->v0) ||
                !isfinite(quad->u1) || !isfinite(quad->v1) ||
                quad->u0 < 0.0f || quad->v0 < 0.0f ||
                quad->u1 > 1.0f || quad->v1 > 1.0f ||
                quad->u0 > quad->u1 || quad->v0 > quad->v1)))
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
    }

    for (index = 0U; index < context->draw_quad_count; ++index)
    {
        const henka_ui_draw_quad* quad = &context->draw_quads[index];
        const float x0 = quad->bounds.x;
        const float y0 = quad->bounds.y;
        const float x1 = (float)((double)quad->bounds.x + (double)quad->bounds.width);
        const float y1 = (float)((double)quad->bounds.y + (double)quad->bounds.height);
        const float u0 = quad->kind == HENKA_UI_DRAW_QUAD_GLYPH ? quad->u0 : 0.0f;
        const float v0 = quad->kind == HENKA_UI_DRAW_QUAD_GLYPH ? quad->v0 : 0.0f;
        const float u1 = quad->kind == HENKA_UI_DRAW_QUAD_GLYPH ? quad->u1 : 0.0f;
        const float v1 = quad->kind == HENKA_UI_DRAW_QUAD_GLYPH ? quad->v1 : 0.0f;
        const bool glyph = quad->kind == HENKA_UI_DRAW_QUAD_GLYPH;
        henka_ui_draw_vertex* vertices = &out_vertices[index * 6U];

        henka_ui_draw_vertex_set(&vertices[0U], x0, y0, u0, v0, quad->color, glyph);
        henka_ui_draw_vertex_set(&vertices[1U], x1, y0, u1, v0, quad->color, glyph);
        henka_ui_draw_vertex_set(&vertices[2U], x1, y1, u1, v1, quad->color, glyph);
        henka_ui_draw_vertex_set(&vertices[3U], x0, y0, u0, v0, quad->color, glyph);
        henka_ui_draw_vertex_set(&vertices[4U], x1, y1, u1, v1, quad->color, glyph);
        henka_ui_draw_vertex_set(&vertices[5U], x0, y1, u0, v1, quad->color, glyph);
    }
    *out_vertex_count = required_vertices;
    return HENKA_SUCCESS;
}

static henka_result henka_ui_context_transform_position(
    const henka_ui_context* context,
    henka_vec2 position,
    bool to_framebuffer,
    henka_vec2* out_position)
{
    double scale;
    double x;
    double y;

    if (out_position != NULL)
    {
        *out_position = (henka_vec2){0.0f, 0.0f};
    }
    if (context == NULL || out_position == NULL ||
        !isfinite(position.x) || !isfinite(position.y) ||
        !isfinite(context->output_scale) || context->output_scale < 0.25f ||
        context->output_scale > 4.0f ||
        !henka_ui_font_app_scale_is_supported(context->font_app_scale_percent))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    scale = (double)context->output_scale *
        henka_ui_context_editor_scale_factor(context);
    if (!isfinite(scale) || scale < 0.25 || scale > 8.0)
    {
        return HENKA_ERROR_NUMERIC_RANGE;
    }
    if (!to_framebuffer)
    {
        scale = 1.0 / scale;
    }
    x = (double)position.x * scale;
    y = (double)position.y * scale;
    if (!isfinite(x) || !isfinite(y) ||
        x < -(double)FLT_MAX || x > (double)FLT_MAX ||
        y < -(double)FLT_MAX || y > (double)FLT_MAX)
    {
        return HENKA_ERROR_NUMERIC_RANGE;
    }
    out_position->x = (float)x;
    out_position->y = (float)y;
    return HENKA_SUCCESS;
}

henka_result henka_ui_context_framebuffer_to_logical(
    const henka_ui_context* context,
    henka_vec2 framebuffer_position,
    henka_vec2* out_logical_position)
{
    return henka_ui_context_transform_position(
        context, framebuffer_position, false, out_logical_position);
}

henka_result henka_ui_framebuffer_to_logical(
    const henka_ui_context* context,
    henka_vec2 framebuffer_position,
    henka_vec2* out_logical_position)
{
    return henka_ui_context_framebuffer_to_logical(
        context, framebuffer_position, out_logical_position);
}

henka_result henka_ui_context_apply_output_transform(
    const henka_ui_context* context,
    henka_ui_draw_vertex* vertices,
    size_t vertex_count)
{
    size_t index;

    if (context == NULL || context->frame_active ||
        (vertex_count > 0U && vertices == NULL) ||
        vertex_count > HENKA_UI_MAX_DRAW_ITEMS * 15U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    /* Validate the full candidate first so a range error cannot scale a prefix. */
    for (index = 0U; index < vertex_count; ++index)
    {
        henka_vec2 transformed;
        const henka_result result = henka_ui_context_transform_position(
            context, (henka_vec2){vertices[index].x, vertices[index].y}, true,
            &transformed);
        if (result != HENKA_SUCCESS)
        {
            return result;
        }
    }

    for (index = 0U; index < vertex_count; ++index)
    {
        henka_vec2 transformed;
        (void)henka_ui_context_transform_position(
            context, (henka_vec2){vertices[index].x, vertices[index].y}, true,
            &transformed);
        vertices[index].x = transformed.x;
        vertices[index].y = transformed.y;
    }
    return HENKA_SUCCESS;
}

henka_result henka_ui_context_set_font_profile(
    henka_ui_context* context,
    const henka_ui_font_raster_profile* profile,
    uint16_t app_scale_percent)
{
    henka_ui_font_context_scale_binding binding;
    henka_ui_font_context_scale_update* update = NULL;
    henka_result result;

    binding.context = context;
    binding.profile = profile;
    result = henka_ui_font_context_scale_update_prepare(
        &binding, 1U, app_scale_percent, &update);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    henka_ui_font_context_scale_update_publish(update);
    return HENKA_SUCCESS;
}

henka_result henka_ui_font_context_scale_update_prepare(
    const henka_ui_font_context_scale_binding* bindings,
    size_t binding_count,
    uint16_t app_scale_percent,
    henka_ui_font_context_scale_update** out_update)
{
    henka_ui_font_context_scale_update* candidate;
    float output_scales[HENKA_UI_FONT_MAX_CONTEXTS] = {0.0f};
    size_t index;

    if (out_update != NULL)
    {
        *out_update = NULL;
    }
    if (bindings == NULL || binding_count == 0U ||
        binding_count > HENKA_UI_FONT_MAX_CONTEXTS || out_update == NULL ||
        !henka_ui_font_app_scale_is_supported(app_scale_percent))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    for (index = 0U; index < binding_count; ++index)
    {
        const henka_ui_font_context_scale_binding* binding = &bindings[index];
        const uint64_t identity =
            henka_ui_font_raster_profile_get_context_identity(binding->profile);
        size_t previous_index;

        if (binding->context == NULL || binding->context->frame_active ||
            (binding->profile == NULL &&
                (binding->context->font_profile != NULL ||
                    binding->context->font_context_identity != 0U)) ||
            (binding->profile != NULL &&
                (identity == 0U ||
                    henka_ui_font_raster_profile_get_font(
                        binding->profile, app_scale_percent) == NULL ||
                    (binding->context->font_context_identity != 0U &&
                        binding->context->font_context_identity != identity))))
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }

        if (binding->profile != NULL)
        {
            henka_ui_font_raster_profile_info profile_info;
            double output_scale;

            if (!henka_ui_font_raster_profile_get_info(
                    binding->profile, app_scale_percent, &profile_info) ||
                profile_info.key.output_scale_numerator == 0U ||
                profile_info.key.output_scale_denominator == 0U)
            {
                return HENKA_ERROR_ASSET_SOURCE;
            }
            output_scale = (double)profile_info.key.output_scale_numerator /
                (double)profile_info.key.output_scale_denominator;
            if (!isfinite(output_scale) || output_scale < 0.25 || output_scale > 4.0)
            {
                return HENKA_ERROR_NUMERIC_RANGE;
            }
            output_scales[index] = (float)output_scale;
        }
        else
        {
            output_scales[index] = 1.0f;
        }

        for (previous_index = 0U; previous_index < index; ++previous_index)
        {
            const uint64_t previous_identity =
                henka_ui_font_raster_profile_get_context_identity(
                    bindings[previous_index].profile);
            if (bindings[previous_index].context == binding->context ||
                (binding->profile != NULL &&
                    bindings[previous_index].profile != NULL &&
                    previous_identity == identity))
            {
                return HENKA_ERROR_INVALID_ARGUMENT;
            }
        }
    }

    candidate = (henka_ui_font_context_scale_update*)henka_calloc(
        1U, sizeof(*candidate));
    if (candidate == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    candidate->binding_count = binding_count;
    candidate->app_scale_percent = app_scale_percent;
    memcpy(candidate->bindings, bindings,
        binding_count * sizeof(candidate->bindings[0]));
    memcpy(candidate->output_scales, output_scales,
        binding_count * sizeof(candidate->output_scales[0]));
    *out_update = candidate;
    return HENKA_SUCCESS;
}

void henka_ui_font_context_scale_update_publish(
    henka_ui_font_context_scale_update* update)
{
    size_t index;

    if (update == NULL)
    {
        return;
    }
    for (index = 0U; index < update->binding_count; ++index)
    {
        henka_ui_context* context = update->bindings[index].context;
        if (update->bindings[index].profile != NULL)
        {
            context->font_context_identity =
                henka_ui_font_raster_profile_get_context_identity(
                    update->bindings[index].profile);
            context->font_profile = update->bindings[index].profile;
        }
        context->output_scale = update->output_scales[index];
        context->font_app_scale_percent = update->app_scale_percent;
    }
    henka_free(update);
}

void henka_ui_font_context_scale_update_dispose(
    henka_ui_font_context_scale_update* update)
{
    henka_free(update);
}

static henka_result henka_ui_draw_font_text_with_limits(
    henka_ui_context* context,
    float x,
    float y,
    float scale,
    const char* text,
    henka_vec4 color,
    float maximum_width,
    size_t maximum_line_count,
    bool center_visible_ink)
{
    const henka_ui_font_resource* font;
    henka_ui_font_info font_info;
    henka_ui_text_run run;
    henka_ui_font_diagnostic diagnostic;
    size_t text_length;
    size_t index;
    henka_result result;
    double logical_scale;
    double raster_maximum_width;
    double baseline_y;

    if (context == NULL || !context->frame_active || context->font_profile == NULL ||
        text == NULL || !isfinite(x) || !isfinite(y) || !isfinite(scale) ||
        scale <= 0.0f || scale > HENKA_UI_MAX_SCALE ||
        !isfinite(color.x) || !isfinite(color.y) ||
        !isfinite(color.z) || !isfinite(color.w) ||
        !isfinite(maximum_width) || maximum_width < 0.0f || maximum_line_count == 0U ||
        !isfinite((double)context->output_scale) ||
        context->output_scale < 0.25f || context->output_scale > 4.0f ||
        !henka_ui_font_app_scale_is_supported(context->font_app_scale_percent) ||
        !henka_checked_c_string_length(text, HENKA_UI_TEXT_MAX_BYTES, &text_length))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    font = henka_ui_font_raster_profile_get_font(
        context->font_profile, context->font_app_scale_percent);
    if (font == NULL || !henka_ui_font_resource_get_info(font, &font_info))
    {
        return HENKA_ERROR_ASSET_SOURCE;
    }
    logical_scale = (double)scale /
        ((double)context->output_scale *
         henka_ui_context_editor_scale_factor(context));
    raster_maximum_width = maximum_width > 0.0f ?
        (double)maximum_width / logical_scale : 0.0;
    if (!isfinite(logical_scale) || logical_scale <= 0.0 ||
        !isfinite(raster_maximum_width) || raster_maximum_width > (double)FLT_MAX)
    {
        return HENKA_ERROR_NUMERIC_RANGE;
    }
    memset(&run, 0, sizeof(run));
    result = henka_ui_text_layout_build_limited(
        font,
        text,
        text_length,
        (float)raster_maximum_width,
        maximum_line_count,
        &run,
        &diagnostic);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    if (center_visible_ink && run.ink_bottom > run.ink_top)
    {
        const double ink_center =
            ((double)run.ink_top + (double)run.ink_bottom) * 0.5;
        baseline_y = (double)y - ink_center * logical_scale;
    }
    else if (center_visible_ink)
    {
        /* Empty/whitespace-only runs still occupy a typographic line box. */
        baseline_y = (double)y +
            ((double)font_info.ascent - (double)font_info.line_height * 0.5) *
                logical_scale;
    }
    else
    {
        baseline_y = (double)y + (double)font_info.ascent * logical_scale;
    }
    if (!isfinite(baseline_y) || baseline_y < -(double)FLT_MAX || baseline_y > (double)FLT_MAX)
    {
        henka_ui_text_run_dispose(&run);
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    /* Validate every generated rectangle before publishing any glyph command. */
    for (index = 0U; index < run.glyph_count; ++index)
    {
        const henka_ui_text_glyph* glyph = &run.glyphs[index];
        const double gx = (double)x + (double)glyph->x * logical_scale;
        const double gy = baseline_y + (double)glyph->y * logical_scale;
        const double width = (double)glyph->width * logical_scale;
        const double height = (double)glyph->height * logical_scale;
        const double right = gx + width;
        const double bottom = gy + height;
        henka_ui_font_glyph_info glyph_info;

        if (!henka_ui_font_resource_get_glyph(font, glyph->rendered_codepoint, &glyph_info) ||
            !isfinite(gx) || !isfinite(gy) || !isfinite(width) || !isfinite(height) ||
            !isfinite(right) || !isfinite(bottom) ||
            gx < -(double)FLT_MAX || gx > (double)FLT_MAX ||
            gy < -(double)FLT_MAX || gy > (double)FLT_MAX ||
            right < -(double)FLT_MAX || right > (double)FLT_MAX ||
            bottom < -(double)FLT_MAX || bottom > (double)FLT_MAX ||
            width < 0.0 || height < 0.0 ||
            (uint64_t)glyph_info.atlas_x + glyph_info.bitmap_width > font_info.atlas_width ||
            (uint64_t)glyph_info.atlas_y + glyph_info.bitmap_height > font_info.atlas_height)
        {
            henka_ui_text_run_dispose(&run);
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
    }
    result = henka_ui_context_reserve_draw_quads(context, run.glyph_count);
    if (result != HENKA_SUCCESS)
    {
        henka_ui_text_run_dispose(&run);
        return result;
    }

    for (index = 0U; index < run.glyph_count; ++index)
    {
        const henka_ui_text_glyph* glyph = &run.glyphs[index];
        henka_ui_font_glyph_info glyph_info;
        henka_ui_draw_quad* quad;
        const double gx = (double)x + (double)glyph->x * logical_scale;
        const double gy = baseline_y + (double)glyph->y * logical_scale;
        const double width = (double)glyph->width * logical_scale;
        const double height = (double)glyph->height * logical_scale;

        if (glyph->width <= 0.0f || glyph->height <= 0.0f)
        {
            continue;
        }
        (void)henka_ui_font_resource_get_glyph(font, glyph->rendered_codepoint, &glyph_info);
        quad = &context->draw_quads[context->draw_quad_count];
        quad->kind = HENKA_UI_DRAW_QUAD_GLYPH;
        quad->bounds = (henka_ui_rect){
            (float)gx, (float)gy, (float)width, (float)height};
        quad->color = color;
        quad->u0 = (float)glyph_info.atlas_x / (float)font_info.atlas_width;
        quad->u1 = (float)((double)glyph_info.atlas_x + glyph_info.bitmap_width) /
            (float)font_info.atlas_width;
        quad->v0 = (float)glyph_info.atlas_y / (float)font_info.atlas_height;
        quad->v1 = (float)((double)glyph_info.atlas_y + glyph_info.bitmap_height) /
            (float)font_info.atlas_height;
        context->draw_quad_count += 1U;
    }
    henka_ui_text_run_dispose(&run);
    return HENKA_SUCCESS;
}

henka_result henka_ui_draw_font_text(
    henka_ui_context* context,
    float x,
    float y,
    float scale,
    const char* text,
    henka_vec4 color)
{
    return henka_ui_draw_font_text_with_limits(
        context, x, y, scale, text, color, 0.0f, SIZE_MAX, false);
}

henka_result henka_ui_draw_font_text_limited(
    henka_ui_context* context,
    float x,
    float y,
    float scale,
    const char* text,
    henka_vec4 color,
    float maximum_width,
    size_t maximum_line_count)
{
    if (!isfinite((double)maximum_width) || maximum_width <= 0.0f)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return henka_ui_draw_font_text_with_limits(
        context,
        x,
        y,
        scale,
        text,
        color,
        maximum_width,
        maximum_line_count,
        false);
}

henka_result henka_ui_draw_font_text_centered_limited(
    henka_ui_context* context,
    float x,
    float center_y,
    float scale,
    const char* text,
    henka_vec4 color,
    float maximum_width,
    size_t maximum_line_count)
{
    if (!isfinite((double)maximum_width) || maximum_width <= 0.0f)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return henka_ui_draw_font_text_with_limits(
        context,
        x,
        center_y,
        scale,
        text,
        color,
        maximum_width,
        maximum_line_count,
        true);
}
