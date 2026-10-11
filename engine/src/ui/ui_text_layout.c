#include "ui_font_internal.h"

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

static bool henka_ui_text_append_glyph(
    const henka_ui_font_resource* resource,
    henka_ui_text_run* run,
    uint32_t source_codepoint,
    uint32_t rendered_codepoint,
    uint32_t previous_glyph_index,
    float* in_out_pen_x)
{
    henka_ui_font_glyph_info glyph_info;
    henka_ui_text_glyph* placement;
    float kerning = 0.0f;

    if (run == NULL || run->glyphs == NULL || in_out_pen_x == NULL ||
        !henka_ui_font_resource_get_glyph(resource, rendered_codepoint, &glyph_info))
    {
        return false;
    }
    if (previous_glyph_index != 0U &&
        !henka_ui_font_resource_get_kerning(resource, previous_glyph_index, glyph_info.glyph_index, &kerning))
    {
        return false;
    }
    placement = &run->glyphs[run->glyph_count];
    placement->source_codepoint = source_codepoint;
    placement->rendered_codepoint = rendered_codepoint;
    placement->glyph_index = glyph_info.glyph_index;
    placement->pen_x = *in_out_pen_x + kerning;
    placement->x = placement->pen_x + glyph_info.bitmap_x;
    placement->y = glyph_info.bitmap_y;
    placement->width = (float)glyph_info.bitmap_width;
    placement->height = (float)glyph_info.bitmap_height;
    placement->advance_x = glyph_info.advance_x;
    placement->atlas_x = glyph_info.atlas_x;
    placement->atlas_y = glyph_info.atlas_y;
    henka_ui_text_run_include_ink(run, placement);
    *in_out_pen_x = placement->pen_x + placement->advance_x;
    run->advance_width = *in_out_pen_x;
    ++run->glyph_count;
    return true;
}

static uint32_t henka_ui_text_previous_glyph(const henka_ui_font_resource* resource, const henka_ui_text_run* run)
{
    if (resource == NULL || run == NULL || run->glyph_count == 0U)
    {
        return 0U;
    }
    return run->glyphs[run->glyph_count - 1U].glyph_index;
}

static bool henka_ui_text_append_ellipsis(
    const henka_ui_font_resource* resource,
    henka_ui_text_run* run,
    float max_width,
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
        const uint32_t previous = henka_ui_text_previous_glyph(resource, run);
        float kerning = 0.0f;
        if (previous != 0U &&
            !henka_ui_font_resource_get_kerning(resource, previous, ellipsis.glyph_index, &kerning))
        {
            return false;
        }
        if (*in_out_pen_x + kerning + ellipsis.advance_x <= max_width)
        {
            return henka_ui_text_append_glyph(
                resource, run, HENKA_UI_TEXT_ELLIPSIS_CODEPOINT, codepoint, previous, in_out_pen_x);
        }
        if (run->glyph_count == 0U)
        {
            return true;
        }
        --run->glyph_count;
        if (run->glyph_count == 0U)
        {
            *in_out_pen_x = 0.0f;
            run->advance_width = 0.0f;
            run->ink_left = 0.0f;
            run->ink_top = 0.0f;
            run->ink_right = 0.0f;
            run->ink_bottom = 0.0f;
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

henka_result henka_ui_text_layout_build(
    const henka_ui_font_resource* resource,
    const char* utf8,
    size_t utf8_size,
    float max_width,
    henka_ui_text_run* out_run,
    henka_ui_font_diagnostic* out_diagnostic)
{
    henka_ui_text_run candidate;
    henka_ui_font_info font_info;
    float pen_x = 0.0f;
    size_t offset = 0U;
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
    if (resource == NULL || (utf8 == NULL && utf8_size > 0U) ||
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
    capacity = utf8_size;
    if (capacity > 0U)
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
        const henka_ui_utf8_decoded decoded = henka_ui_text_decode_utf8(
            (const uint8_t*)utf8 + offset, utf8_size - offset);
        uint32_t rendered_codepoint = decoded.codepoint;
        henka_ui_font_glyph_info glyph_info;
        const uint32_t previous_glyph = henka_ui_text_previous_glyph(resource, &candidate);
        float kerning = 0.0f;
        bool replaced = !decoded.valid;

        offset += decoded.byte_count;
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
        if (previous_glyph != 0U &&
            !henka_ui_font_resource_get_kerning(resource, previous_glyph, glyph_info.glyph_index, &kerning))
        {
            henka_ui_text_run_dispose(&candidate);
            return HENKA_ERROR_ASSET_SOURCE;
        }
        if (max_width > 0.0f && pen_x + kerning + glyph_info.advance_x > max_width)
        {
            candidate.truncated = true;
            if (!henka_ui_text_append_ellipsis(resource, &candidate, max_width, &pen_x))
            {
                henka_ui_text_run_dispose(&candidate);
                return HENKA_ERROR_ASSET_SOURCE;
            }
            break;
        }
        if (!henka_ui_text_append_glyph(resource, &candidate,
                decoded.codepoint, rendered_codepoint, previous_glyph, &pen_x))
        {
            henka_ui_text_run_dispose(&candidate);
            return HENKA_ERROR_ASSET_SOURCE;
        }
        if (replaced)
        {
            ++candidate.replacement_count;
        }
    }
    *out_run = candidate;
    if (out_diagnostic != NULL)
    {
        out_diagnostic->stage = HENKA_UI_FONT_STAGE_NONE;
        (void)snprintf(out_diagnostic->identity, sizeof(out_diagnostic->identity), "%s", font_info.identity);
    }
    return HENKA_SUCCESS;
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
