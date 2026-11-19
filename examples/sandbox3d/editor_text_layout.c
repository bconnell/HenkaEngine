#include "editor_text_layout.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

#define SANDBOX3D_EDITOR_TEXT_MAX_WRAPPED_BYTES (SANDBOX3D_EDITOR_TEXT_MAX_BYTES * 2U + 1U)

static bool sandbox3d_editor_text_width_is_valid(float width)
{
    return isfinite((double)width) && width > 0.0f;
}

static henka_result sandbox3d_editor_text_measure_width(
    const henka_ui_context* ui,
    const char* text,
    float* out_width)
{
    int width = 0;
    int height = 0;

    if (out_width == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_width = 0.0f;
    if (henka_ui_measure_text_for_context(ui, text, 1.0f, &width, &height) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_width = (float)width;
    return HENKA_SUCCESS;
}

static size_t sandbox3d_editor_text_next_utf8_boundary(
    const char* text,
    size_t offset,
    size_t length)
{
    const unsigned char first = (unsigned char)text[offset];
    size_t expected = 1U;
    size_t index;

    if (first >= 0xc2U && first <= 0xdfU)
    {
        expected = 2U;
    }
    else if (first >= 0xe0U && first <= 0xefU)
    {
        expected = 3U;
    }
    else if (first >= 0xf0U && first <= 0xf4U)
    {
        expected = 4U;
    }
    if (expected == 1U || expected > length - offset)
    {
        return offset + 1U;
    }
    for (index = 1U; index < expected; ++index)
    {
        if (((unsigned char)text[offset + index] & 0xc0U) != 0x80U)
        {
            return offset + 1U;
        }
    }
    return offset + expected;
}

static henka_result sandbox3d_editor_text_measure_candidate(
    const henka_ui_context* ui,
    const char* line,
    size_t line_bytes,
    const char* addition,
    size_t addition_bytes,
    float* out_width,
    char* scratch,
    size_t scratch_capacity)
{
    if (line_bytes > scratch_capacity || addition_bytes > scratch_capacity - line_bytes ||
        line_bytes + addition_bytes >= scratch_capacity)
    {
        return HENKA_ERROR_LIMIT;
    }
    if (line_bytes > 0U)
    {
        memcpy(scratch, line, line_bytes);
    }
    if (addition_bytes > 0U)
    {
        memcpy(scratch + line_bytes, addition, addition_bytes);
    }
    scratch[line_bytes + addition_bytes] = '\0';
    return sandbox3d_editor_text_measure_width(ui, scratch, out_width);
}

henka_result sandbox3d_editor_text_byte_offset_x(
    const henka_ui_context* ui,
    const char* text,
    size_t text_byte_count,
    size_t byte_offset,
    float scale,
    float* out_x)
{
    if (out_x == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_x = 0.0f;
    if (ui == NULL || text == NULL || !isfinite((double)scale) || scale <= 0.0f ||
        byte_offset > text_byte_count)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (text_byte_count > SANDBOX3D_EDITOR_TEXT_MAX_BYTES)
    {
        return HENKA_ERROR_LIMIT;
    }
    return henka_ui_measure_text_byte_offset_x_for_context(
        ui, text, text_byte_count, byte_offset, scale, out_x);
}

henka_result sandbox3d_editor_text_wrap(
    const henka_ui_context* ui,
    const char* text,
    float maximum_width,
    char* out_text,
    size_t out_capacity,
    size_t* out_line_count)
{
    char candidate[SANDBOX3D_EDITOR_TEXT_MAX_BYTES + 1U];
    size_t text_length;
    size_t source_offset = 0U;
    size_t written = 0U;
    size_t line_start = 0U;
    size_t line_count = 1U;
    int validation_width = 0;
    int validation_height = 0;

    if (out_line_count == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_line_count = 0U;
    if (ui == NULL || text == NULL || out_text == NULL || out_capacity == 0U ||
        !sandbox3d_editor_text_width_is_valid(maximum_width) ||
        henka_ui_measure_text_for_context(
            ui, text, 1.0f, &validation_width, &validation_height) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    text_length = strlen(text);
    if (text_length > SANDBOX3D_EDITOR_TEXT_MAX_BYTES)
    {
        return HENKA_ERROR_LIMIT;
    }

    while (source_offset < text_length)
    {
        size_t token_start;
        size_t token_end;
        size_t token_offset;
        size_t current_line_bytes = written - line_start;
        float measured_width = 0.0f;
        henka_result result;

        if (text[source_offset] == '\r' || text[source_offset] == '\n')
        {
            if (written + 1U >= out_capacity || written >= SANDBOX3D_EDITOR_TEXT_MAX_BYTES)
            {
                return HENKA_ERROR_LIMIT;
            }
            out_text[written++] = '\n';
            if (text[source_offset] == '\r' && source_offset + 1U < text_length &&
                text[source_offset + 1U] == '\n')
            {
                source_offset += 2U;
            }
            else
            {
                ++source_offset;
            }
            line_start = written;
            if (line_count == SIZE_MAX)
            {
                return HENKA_ERROR_NUMERIC_RANGE;
            }
            ++line_count;
            continue;
        }
        if (text[source_offset] == ' ' || text[source_offset] == '\t')
        {
            ++source_offset;
            continue;
        }

        token_start = source_offset;
        while (source_offset < text_length && text[source_offset] != ' ' &&
            text[source_offset] != '\t' && text[source_offset] != '\r' &&
            text[source_offset] != '\n')
        {
            source_offset = sandbox3d_editor_text_next_utf8_boundary(
                text, source_offset, text_length);
        }
        token_end = source_offset;
        token_offset = token_start;

        if (current_line_bytes > 0U)
        {
            size_t addition_bytes = token_end - token_start + 1U;
            char addition[SANDBOX3D_EDITOR_TEXT_MAX_BYTES + 1U];
            addition[0] = ' ';
            memcpy(addition + 1U, text + token_start, token_end - token_start);
            result = sandbox3d_editor_text_measure_candidate(
                ui, out_text + line_start, current_line_bytes,
                addition, addition_bytes, &measured_width,
                candidate, sizeof(candidate));
            if (result != HENKA_SUCCESS)
            {
                return result;
            }
            if (measured_width <= maximum_width)
            {
                if (written + addition_bytes >= out_capacity ||
                    written + addition_bytes > SANDBOX3D_EDITOR_TEXT_MAX_BYTES)
                {
                    return HENKA_ERROR_LIMIT;
                }
                memcpy(out_text + written, addition, addition_bytes);
                written += addition_bytes;
                out_text[written] = '\0';
                continue;
            }

            if (written + 1U >= out_capacity || written >= SANDBOX3D_EDITOR_TEXT_MAX_BYTES)
            {
                return HENKA_ERROR_LIMIT;
            }
            out_text[written++] = '\n';
            line_start = written;
            if (line_count == SIZE_MAX)
            {
                return HENKA_ERROR_NUMERIC_RANGE;
            }
            ++line_count;
        }

        while (token_offset < token_end)
        {
            size_t next = sandbox3d_editor_text_next_utf8_boundary(
                text, token_offset, token_end);
            const size_t unit_bytes = next - token_offset;
            const size_t current_bytes = written - line_start;

            result = sandbox3d_editor_text_measure_candidate(
                ui, out_text + line_start, current_bytes,
                text + token_offset, unit_bytes, &measured_width,
                candidate, sizeof(candidate));
            if (result != HENKA_SUCCESS)
            {
                return result;
            }
            if (measured_width > maximum_width)
            {
                if (current_bytes == 0U)
                {
                    return HENKA_ERROR_LIMIT;
                }
                if (written + 1U >= out_capacity || written >= SANDBOX3D_EDITOR_TEXT_MAX_BYTES)
                {
                    return HENKA_ERROR_LIMIT;
                }
                out_text[written++] = '\n';
                line_start = written;
                if (line_count == SIZE_MAX)
                {
                    return HENKA_ERROR_NUMERIC_RANGE;
                }
                ++line_count;
                continue;
            }

            if (written + unit_bytes >= out_capacity ||
                written + unit_bytes > SANDBOX3D_EDITOR_TEXT_MAX_BYTES)
            {
                return HENKA_ERROR_LIMIT;
            }
            memcpy(out_text + written, text + token_offset, unit_bytes);
            written += unit_bytes;
            token_offset = next;
        }
    }

    if (out_capacity <= written)
    {
        return HENKA_ERROR_LIMIT;
    }
    out_text[written] = '\0';
    *out_line_count = line_count;
    return HENKA_SUCCESS;
}

henka_result sandbox3d_editor_text_measure_wrapped_lines(
    const henka_ui_context* ui,
    const char* text,
    float maximum_width,
    size_t* out_line_count)
{
    char wrapped[SANDBOX3D_EDITOR_TEXT_MAX_WRAPPED_BYTES + 1U];
    return sandbox3d_editor_text_wrap(
        ui, text, maximum_width, wrapped, sizeof(wrapped), out_line_count);
}

static henka_result sandbox3d_editor_text_fit_with_suffix(
    const henka_ui_context* ui,
    const char* text,
    const char* suffix,
    float maximum_width,
    bool force_suffix,
    char* out_text,
    size_t out_capacity,
    bool* out_truncated)
{
    char candidate[SANDBOX3D_EDITOR_TEXT_MAX_BYTES + 1U];
    size_t text_length;
    size_t suffix_length;
    size_t offset = 0U;
    size_t best_prefix = 0U;
    float measured_width;
    henka_result result;

    if (out_truncated != NULL)
    {
        *out_truncated = false;
    }
    if (ui == NULL || text == NULL || suffix == NULL || out_text == NULL ||
        out_capacity == 0U || !sandbox3d_editor_text_width_is_valid(maximum_width))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    text_length = strlen(text);
    suffix_length = strlen(suffix);
    if (text_length > SANDBOX3D_EDITOR_TEXT_MAX_BYTES ||
        suffix_length > SANDBOX3D_EDITOR_TEXT_MAX_BYTES)
    {
        return HENKA_ERROR_LIMIT;
    }
    result = sandbox3d_editor_text_measure_width(ui, text, &measured_width);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    if (!force_suffix && measured_width <= maximum_width)
    {
        if (text_length >= out_capacity)
        {
            return HENKA_ERROR_LIMIT;
        }
        memcpy(out_text, text, text_length + 1U);
        return HENKA_SUCCESS;
    }

    result = sandbox3d_editor_text_measure_width(ui, suffix, &measured_width);
    if (result != HENKA_SUCCESS || measured_width > maximum_width)
    {
        return result == HENKA_SUCCESS ? HENKA_ERROR_LIMIT : result;
    }

    while (offset < text_length)
    {
        const size_t next = sandbox3d_editor_text_next_utf8_boundary(text, offset, text_length);
        const size_t candidate_length = next + suffix_length;
        if (candidate_length >= sizeof(candidate))
        {
            return HENKA_ERROR_LIMIT;
        }
        memcpy(candidate, text, next);
        memcpy(candidate + next, suffix, suffix_length + 1U);
        result = sandbox3d_editor_text_measure_width(ui, candidate, &measured_width);
        if (result != HENKA_SUCCESS)
        {
            return result;
        }
        if (measured_width <= maximum_width)
        {
            best_prefix = next;
        }
        offset = next;
    }
    if (best_prefix + suffix_length >= out_capacity)
    {
        return HENKA_ERROR_LIMIT;
    }
    memcpy(out_text, text, best_prefix);
    memcpy(out_text + best_prefix, suffix, suffix_length + 1U);
    if (out_truncated != NULL)
    {
        *out_truncated = true;
    }
    return HENKA_SUCCESS;
}

henka_result sandbox3d_editor_text_fit(
    const henka_ui_context* ui,
    const char* text,
    float maximum_width,
    char* out_text,
    size_t out_capacity,
    bool* out_truncated)
{
    return sandbox3d_editor_text_fit_with_suffix(
        ui, text, "...", maximum_width, false,
        out_text, out_capacity, out_truncated);
}

henka_result sandbox3d_editor_text_limit_wrapped_lines(
    const henka_ui_context* ui,
    const char* wrapped_text,
    size_t maximum_line_count,
    float maximum_width,
    char* out_text,
    size_t out_capacity,
    size_t* out_line_count,
    bool* out_truncated)
{
    char final_line[SANDBOX3D_EDITOR_TEXT_MAX_BYTES + 1U];
    char fitted_line[SANDBOX3D_EDITOR_TEXT_MAX_BYTES + 1U];
    const size_t text_length = wrapped_text != NULL ? strlen(wrapped_text) : 0U;
    size_t line_start = 0U;
    size_t line_number = 0U;
    size_t line_count = 1U;
    size_t output_bytes = 0U;
    size_t output_lines;
    int validation_width = 0;
    int validation_height = 0;

    if (out_line_count == NULL || out_truncated == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_line_count = 0U;
    *out_truncated = false;
    if (ui == NULL || wrapped_text == NULL || maximum_line_count == 0U ||
        !sandbox3d_editor_text_width_is_valid(maximum_width) ||
        out_text == NULL || out_capacity == 0U ||
        text_length > SANDBOX3D_EDITOR_TEXT_MAX_BYTES ||
        henka_ui_measure_text_for_context(
            ui, wrapped_text, 1.0f, &validation_width, &validation_height) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    for (size_t index = 0U; index <= text_length; ++index)
    {
        if (index == text_length || wrapped_text[index] == '\n')
        {
            char line[SANDBOX3D_EDITOR_TEXT_MAX_BYTES + 1U];
            const size_t line_bytes = index - line_start;
            float line_width;
            henka_result measure_result;

            if (line_bytes >= sizeof(line))
            {
                return HENKA_ERROR_LIMIT;
            }
            memcpy(line, wrapped_text + line_start, line_bytes);
            line[line_bytes] = '\0';
            measure_result = sandbox3d_editor_text_measure_width(ui, line, &line_width);
            if (measure_result != HENKA_SUCCESS)
            {
                return measure_result;
            }
            if (line_width > maximum_width)
            {
                return HENKA_ERROR_NUMERIC_RANGE;
            }
            if (line_number < maximum_line_count - 1U)
            {
                if (output_bytes + line_bytes + 1U > out_capacity)
                {
                    return HENKA_ERROR_LIMIT;
                }
                memcpy(out_text + output_bytes, line, line_bytes);
                output_bytes += line_bytes;
                if (index < text_length)
                {
                    out_text[output_bytes++] = '\n';
                }
            }
            else if (line_number == maximum_line_count - 1U)
            {
                memcpy(final_line, line, line_bytes + 1U);
            }
            ++line_number;
            if (index < text_length)
            {
                line_start = index + 1U;
                ++line_count;
            }
        }
    }

    if (line_count <= maximum_line_count)
    {
        if (text_length >= out_capacity)
        {
            return HENKA_ERROR_LIMIT;
        }
        memmove(out_text, wrapped_text, text_length + 1U);
        *out_line_count = line_count;
        return HENKA_SUCCESS;
    }

    if (sandbox3d_editor_text_fit_with_suffix(
            ui, final_line, "...", maximum_width, true,
            fitted_line, sizeof(fitted_line), NULL) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_LIMIT;
    }
    {
        const size_t fitted_bytes = strlen(fitted_line);
        if (output_bytes + fitted_bytes >= out_capacity)
        {
            return HENKA_ERROR_LIMIT;
        }
        memcpy(out_text + output_bytes, fitted_line, fitted_bytes + 1U);
    }
    output_lines = maximum_line_count;
    *out_line_count = output_lines;
    *out_truncated = true;
    return HENKA_SUCCESS;
}

henka_result sandbox3d_editor_text_byte_offset_at_x(
    const henka_ui_context* ui,
    const char* text,
    float scale,
    float x,
    size_t* out_byte_offset)
{
    const size_t text_length = text != NULL ? strlen(text) : 0U;
    size_t previous_boundary = 0U;
    float previous_width = 0.0f;
    size_t next_boundary;

    if (out_byte_offset == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_byte_offset = 0U;
    if (ui == NULL || text == NULL || !isfinite((double)scale) || scale <= 0.0f ||
        !isfinite((double)x) || x < 0.0f)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (text_length > SANDBOX3D_EDITOR_TEXT_MAX_BYTES)
    {
        return HENKA_ERROR_LIMIT;
    }

    while (previous_boundary < text_length)
    {
        float next_width = 0.0f;
        float midpoint;
        henka_result result;

        next_boundary = sandbox3d_editor_text_next_utf8_boundary(
            text, previous_boundary, text_length);
        result = sandbox3d_editor_text_byte_offset_x(
            ui, text, text_length, next_boundary, scale, &next_width);
        if (result != HENKA_SUCCESS)
        {
            return result;
        }
        if (!isfinite((double)next_width) || next_width < previous_width)
        {
            return HENKA_ERROR_NUMERIC_RANGE;
        }
        midpoint = ((float)previous_width + (float)next_width) * 0.5f;
        if (x < midpoint)
        {
            *out_byte_offset = previous_boundary;
            return HENKA_SUCCESS;
        }
        previous_boundary = next_boundary;
        previous_width = next_width;
    }

    *out_byte_offset = text_length;
    return HENKA_SUCCESS;
}

henka_result sandbox3d_editor_text_format_hidden_row(
    const henka_ui_context* ui,
    const char* object_name,
    float maximum_width,
    char* out_text,
    size_t out_capacity,
    bool* out_name_truncated)
{
    static const char hidden_suffix[] = " [Hidden]";
    static const char unnamed[] = "(unnamed)";
    char candidate[SANDBOX3D_EDITOR_TEXT_MAX_BYTES + 1U];
    char normalized_name[SANDBOX3D_EDITOR_TEXT_MAX_BYTES + 1U];
    const char* name;
    size_t name_length;
    size_t normalized_length = 0U;
    size_t name_offset;
    size_t prefix_end;
    size_t suffix_start;
    float measured_width;
    henka_result result;
    bool empty_name;

    if (out_name_truncated == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_name_truncated = false;
    if (ui == NULL || object_name == NULL || out_text == NULL || out_capacity == 0U ||
        !sandbox3d_editor_text_width_is_valid(maximum_width))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    empty_name = object_name[0] == '\0';
    name = empty_name ? unnamed : object_name;
    name_length = strlen(name);
    if (name_length > SANDBOX3D_EDITOR_TEXT_MAX_BYTES)
    {
        return HENKA_ERROR_LIMIT;
    }
    if (!empty_name)
    {
        for (name_offset = 0U; name_offset < name_length; ++name_offset)
        {
            char value = name[name_offset];
            if (value == '\r')
            {
                if (name_offset + 1U < name_length && name[name_offset + 1U] == '\n')
                {
                    ++name_offset;
                }
                value = ' ';
            }
            else if (value == '\n' || value == '\t')
            {
                value = ' ';
            }
            normalized_name[normalized_length++] = value;
        }
        normalized_name[normalized_length] = '\0';
        name = normalized_name;
        name_length = normalized_length;
    }
    if (name_length + sizeof(hidden_suffix) >= sizeof(candidate))
    {
        return HENKA_ERROR_LIMIT;
    }
    memcpy(candidate, name, name_length);
    memcpy(candidate + name_length, hidden_suffix, sizeof(hidden_suffix));
    result = sandbox3d_editor_text_measure_width(ui, candidate, &measured_width);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    if (measured_width <= maximum_width &&
        name_length + sizeof(hidden_suffix) <= out_capacity)
    {
        memcpy(out_text, candidate, name_length + sizeof(hidden_suffix));
        return HENKA_SUCCESS;
    }

    if (empty_name)
    {
        static const char compact[] = "? [Hidden]";
        result = sandbox3d_editor_text_measure_width(ui, compact, &measured_width);
        if (result != HENKA_SUCCESS || measured_width > maximum_width ||
            sizeof(compact) > out_capacity)
        {
            return HENKA_ERROR_LIMIT;
        }
        memcpy(out_text, compact, sizeof(compact));
        *out_name_truncated = true;
        return HENKA_SUCCESS;
    }

    prefix_end = sandbox3d_editor_text_next_utf8_boundary(name, 0U, name_length);
    suffix_start = name_length;
    {
        bool prefix_blocked = false;
        bool suffix_blocked = false;
        while (!prefix_blocked || !suffix_blocked)
        {
            size_t prefix_bytes = prefix_end;
            size_t suffix_bytes = name_length - suffix_start;
            size_t cursor = 0U;
            size_t required = prefix_bytes + 3U + suffix_bytes + sizeof(hidden_suffix);
            bool fit_found = false;

            if (required >= sizeof(candidate) || required >= out_capacity)
            {
                return HENKA_ERROR_LIMIT;
            }
            memcpy(candidate, name, prefix_bytes);
            cursor += prefix_bytes;
            memcpy(candidate + cursor, "...", 3U);
            cursor += 3U;
            if (suffix_bytes > 0U)
            {
                memcpy(candidate + cursor, name + suffix_start, suffix_bytes);
                cursor += suffix_bytes;
            }
            memcpy(candidate + cursor, hidden_suffix, sizeof(hidden_suffix));
            result = sandbox3d_editor_text_measure_width(ui, candidate, &measured_width);
            if (result != HENKA_SUCCESS)
            {
                return result;
            }
            if (measured_width > maximum_width)
            {
                return HENKA_ERROR_LIMIT;
            }

            if (!prefix_blocked && prefix_end < suffix_start)
            {
                const size_t next_prefix = sandbox3d_editor_text_next_utf8_boundary(
                    name, prefix_end, name_length);
                const size_t candidate_bytes = next_prefix + 3U + suffix_bytes +
                    sizeof(hidden_suffix);
                if (next_prefix < suffix_start && candidate_bytes < sizeof(candidate) &&
                    candidate_bytes < out_capacity)
                {
                    memcpy(candidate, name, next_prefix);
                    cursor = next_prefix;
                    memcpy(candidate + cursor, "...", 3U);
                    cursor += 3U;
                    if (suffix_bytes > 0U)
                    {
                        memcpy(candidate + cursor, name + suffix_start, suffix_bytes);
                        cursor += suffix_bytes;
                    }
                    memcpy(candidate + cursor, hidden_suffix, sizeof(hidden_suffix));
                    result = sandbox3d_editor_text_measure_width(ui, candidate, &measured_width);
                    if (result != HENKA_SUCCESS)
                    {
                        return result;
                    }
                    if (measured_width <= maximum_width)
                    {
                        prefix_end = next_prefix;
                        fit_found = true;
                    }
                    else
                    {
                        prefix_blocked = true;
                    }
                }
                else
                {
                    prefix_blocked = true;
                }
            }
            else
            {
                prefix_blocked = true;
            }

            if (!suffix_blocked && suffix_start > prefix_end)
            {
                size_t previous_start = suffix_start - 1U;
                size_t candidate_bytes;
                while (previous_start > prefix_end &&
                    (((unsigned char)name[previous_start] & 0xc0U) == 0x80U))
                {
                    --previous_start;
                }
                candidate_bytes = prefix_end + 3U + (name_length - previous_start) +
                    sizeof(hidden_suffix);
                if (previous_start > prefix_end && candidate_bytes < sizeof(candidate) &&
                    candidate_bytes < out_capacity)
                {
                    cursor = prefix_end;
                    memcpy(candidate + cursor, "...", 3U);
                    cursor += 3U;
                    memcpy(candidate + cursor, name + previous_start, name_length - previous_start);
                    cursor += name_length - previous_start;
                    memcpy(candidate + cursor, hidden_suffix, sizeof(hidden_suffix));
                    result = sandbox3d_editor_text_measure_width(ui, candidate, &measured_width);
                    if (result != HENKA_SUCCESS)
                    {
                        return result;
                    }
                    if (measured_width <= maximum_width)
                    {
                        suffix_start = previous_start;
                        fit_found = true;
                    }
                    else
                    {
                        suffix_blocked = true;
                    }
                }
                else
                {
                    suffix_blocked = true;
                }
            }
            else
            {
                suffix_blocked = true;
            }

            if (!fit_found && prefix_blocked && suffix_blocked)
            {
                break;
            }
        }
    }

    {
        const size_t prefix_bytes = prefix_end;
        const size_t suffix_bytes = name_length - suffix_start;
        size_t cursor = 0U;
        const size_t required = prefix_bytes + 3U + suffix_bytes + sizeof(hidden_suffix);
        memcpy(candidate, name, prefix_bytes);
        cursor += prefix_bytes;
        memcpy(candidate + cursor, "...", 3U);
        cursor += 3U;
        if (suffix_bytes > 0U)
        {
            memcpy(candidate + cursor, name + suffix_start, suffix_bytes);
            cursor += suffix_bytes;
        }
        memcpy(candidate + cursor, hidden_suffix, sizeof(hidden_suffix));
        memcpy(out_text, candidate, required);
    }
    *out_name_truncated = true;
    return HENKA_SUCCESS;
}
