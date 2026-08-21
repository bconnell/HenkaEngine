#include "script_editor.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <henka/henkascript.h>
#include <henka/memory.h>
#include <henka/script_asset.h>

#define SANDBOX3D_SCRIPT_EDITOR_MAX_LINES 8U
#define SANDBOX3D_SCRIPT_EDITOR_MAX_LINE_BYTES 192U
#define SANDBOX3D_SCRIPT_EDITOR_HEADER_HEIGHT 22.0f
#define SANDBOX3D_SCRIPT_EDITOR_LINE_HEIGHT 15.0f
#define SANDBOX3D_SCRIPT_EDITOR_TEXT_SCALE 0.72f

static bool sandbox3d_script_editor_rect_is_valid(henka_ui_rect bounds)
{
    return isfinite(bounds.x) && isfinite(bounds.y) &&
        isfinite(bounds.width) && isfinite(bounds.height) &&
        bounds.width >= 80.0f && bounds.height >= 40.0f &&
        bounds.x >= 0.0f && bounds.y >= 0.0f;
}

static const char* sandbox3d_script_editor_language_label(
    henka_script_language language)
{
    return language == HENKA_SCRIPT_LANGUAGE_LUA ? "Lua" :
        language == HENKA_SCRIPT_LANGUAGE_HENKASCRIPT ? "HenkaScript" :
        "Unknown";
}

static henka_ui_semantic_color sandbox3d_script_editor_token_color(
    henka_hks_token_class token_class)
{
    switch (token_class)
    {
        case HENKA_HKS_TOKEN_CLASS_LITERAL:
            return HENKA_UI_COLOR_ORANGE;
        case HENKA_HKS_TOKEN_CLASS_TYPE:
            return HENKA_UI_COLOR_INFO;
        case HENKA_HKS_TOKEN_CLASS_KEYWORD:
            return HENKA_UI_COLOR_ACCENT;
        case HENKA_HKS_TOKEN_CLASS_BUILTIN:
            return HENKA_UI_COLOR_WARNING;
        case HENKA_HKS_TOKEN_CLASS_IDENTIFIER:
        case HENKA_HKS_TOKEN_CLASS_PUNCTUATION:
        default:
            return HENKA_UI_COLOR_NORMAL;
    }
}

static bool sandbox3d_script_editor_get_line(
    const char* source,
    size_t source_size,
    size_t line_index,
    size_t* out_offset,
    size_t* out_length)
{
    size_t current_line = 0U;
    size_t offset = 0U;
    size_t index;
    if (out_offset != NULL)
    {
        *out_offset = 0U;
    }
    if (out_length != NULL)
    {
        *out_length = 0U;
    }
    if (source == NULL || out_offset == NULL || out_length == NULL)
    {
        return false;
    }
    for (index = 0U; index <= source_size; ++index)
    {
        if (index == source_size || source[index] == '\n')
        {
            if (current_line == line_index)
            {
                *out_offset = offset;
                *out_length = index - offset;
                return true;
            }
            ++current_line;
            offset = index + 1U;
        }
    }
    return false;
}

static void sandbox3d_script_editor_copy_line(
    const char* source,
    size_t source_size,
    size_t offset,
    size_t length,
    char* out_line,
    size_t out_capacity)
{
    size_t copy_length;
    if (out_line == NULL || out_capacity == 0U)
    {
        return;
    }
    copy_length = length < out_capacity - 1U ? length : out_capacity - 1U;
    if (source == NULL || offset > source_size || copy_length > source_size - offset)
    {
        copy_length = 0U;
    }
    if (copy_length > 0U)
    {
        memcpy(out_line, source + offset, copy_length);
    }
    out_line[copy_length] = '\0';
    if (length >= out_capacity && out_capacity > 4U)
    {
        out_line[out_capacity - 4U] = '.';
        out_line[out_capacity - 3U] = '.';
        out_line[out_capacity - 2U] = '.';
        out_line[out_capacity - 1U] = '\0';
    }
}

static void sandbox3d_script_editor_draw_token_overlays(
    henka_ui_context* ui,
    henka_ui_rect bounds,
    const char* source,
    const henka_hks_token* tokens,
    size_t token_count,
    uint32_t line_number,
    size_t line_offset,
    size_t line_length,
    float character_width)
{
    size_t token_index;
    const float code_x = bounds.x + 42.0f;
    const float line_y = bounds.y + SANDBOX3D_SCRIPT_EDITOR_HEADER_HEIGHT +
        ((float)line_number - 1.0f) * SANDBOX3D_SCRIPT_EDITOR_LINE_HEIGHT;
    for (token_index = 0U; token_index < token_count; ++token_index)
    {
        const henka_hks_token* token = &tokens[token_index];
        char token_text[SANDBOX3D_SCRIPT_EDITOR_MAX_LINE_BYTES];
        size_t token_offset;
        size_t token_length;
        if (token->kind == HENKA_HKS_TOKEN_EOF ||
            token->line != line_number || token->offset < line_offset ||
            token->offset >= line_offset + line_length)
        {
            continue;
        }
        token_offset = token->offset - line_offset;
        token_length = token->length;
        if (token_offset + token_length > line_length)
        {
            token_length = line_length - token_offset;
        }
        if (token_offset >= sizeof(token_text))
        {
            continue;
        }
        if (token_length >= sizeof(token_text) - token_offset)
        {
            token_length = sizeof(token_text) - token_offset - 1U;
        }
        memcpy(token_text, source + token->offset, token_length);
        token_text[token_length] = '\0';
        (void)henka_ui_label_colored(
            ui,
            code_x + (float)token_offset * character_width,
            line_y,
            SANDBOX3D_SCRIPT_EDITOR_TEXT_SCALE,
            token_text,
            sandbox3d_script_editor_token_color(
                henka_hks_token_kind_get_class(token->kind)));
    }
}

henka_result sandbox3d_script_editor_draw_preview(
    henka_ui_context* ui,
    henka_ui_rect bounds,
    const char* project_root,
    const henka_scene_document_behavior* behavior)
{
    char* source = NULL;
    size_t source_size = 0U;
    henka_hks_token* tokens = NULL;
    size_t token_count = 0U;
    henka_hks_diagnostic diagnostic;
    henka_result result;
    int character_width = 6;
    size_t line_index;
    size_t visible_lines;
    bool tokenized = false;
    int character_height = 0;
    if (ui == NULL || !sandbox3d_script_editor_rect_is_valid(bounds) ||
        project_root == NULL || behavior == NULL ||
        behavior->asset_path[0] == '\0' ||
        (behavior->language != HENKA_SCRIPT_LANGUAGE_LUA &&
         behavior->language != HENKA_SCRIPT_LANGUAGE_HENKASCRIPT))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_script_asset_read_source(
        project_root,
        behavior->asset_path,
        &source,
        &source_size);
    (void)henka_ui_overlay_rect(
        ui,
        bounds,
        (henka_vec4){0.035f, 0.045f, 0.060f, 0.995f});
    (void)henka_ui_overlay_line(
        ui,
        (henka_vec2){bounds.x, bounds.y + SANDBOX3D_SCRIPT_EDITOR_HEADER_HEIGHT},
        (henka_vec2){bounds.x + bounds.width, bounds.y + SANDBOX3D_SCRIPT_EDITOR_HEADER_HEIGHT},
        1.0f,
        (henka_vec4){0.16f, 0.22f, 0.29f, 1.0f});
    if (result != HENKA_SUCCESS)
    {
        (void)henka_ui_label_colored(
            ui,
            bounds.x + 8.0f,
            bounds.y + 5.0f,
            SANDBOX3D_SCRIPT_EDITOR_TEXT_SCALE,
            "Source unavailable",
            HENKA_UI_COLOR_WARNING);
        henka_free(source);
        return result;
    }
    (void)henka_ui_label_colored(
        ui,
        bounds.x + 8.0f,
        bounds.y + 5.0f,
        SANDBOX3D_SCRIPT_EDITOR_TEXT_SCALE,
        sandbox3d_script_editor_language_label(behavior->language),
        HENKA_UI_COLOR_INFO);
    (void)henka_ui_label_colored(
        ui,
        bounds.x + bounds.width - 90.0f,
        bounds.y + 5.0f,
        SANDBOX3D_SCRIPT_EDITOR_TEXT_SCALE,
        "Read-only",
        HENKA_UI_COLOR_MUTED);
    if (behavior->language == HENKA_SCRIPT_LANGUAGE_HENKASCRIPT)
    {
        tokens = (henka_hks_token*)henka_calloc(
            HENKA_HKS_MAX_TOKENS,
            sizeof(*tokens));
        if (tokens != NULL &&
            henka_hks_lex(
                source,
                source_size,
                tokens,
                HENKA_HKS_MAX_TOKENS,
                &token_count,
                &diagnostic) == HENKA_SUCCESS)
        {
            tokenized = true;
        }
    }
    (void)henka_ui_measure_text(
        "M",
        SANDBOX3D_SCRIPT_EDITOR_TEXT_SCALE,
        &character_width,
        &character_height);
    (void)character_height;
    visible_lines = (size_t)((bounds.height - SANDBOX3D_SCRIPT_EDITOR_HEADER_HEIGHT) /
        SANDBOX3D_SCRIPT_EDITOR_LINE_HEIGHT);
    if (visible_lines > SANDBOX3D_SCRIPT_EDITOR_MAX_LINES)
    {
        visible_lines = SANDBOX3D_SCRIPT_EDITOR_MAX_LINES;
    }
    for (line_index = 0U; line_index < visible_lines; ++line_index)
    {
        char line_number[16];
        char line_text[SANDBOX3D_SCRIPT_EDITOR_MAX_LINE_BYTES];
        size_t line_offset;
        size_t line_length;
        const float line_y = bounds.y + SANDBOX3D_SCRIPT_EDITOR_HEADER_HEIGHT +
            (float)line_index * SANDBOX3D_SCRIPT_EDITOR_LINE_HEIGHT;
        if (!sandbox3d_script_editor_get_line(
                source,
                source_size,
                line_index,
                &line_offset,
                &line_length))
        {
            break;
        }
        (void)snprintf(
            line_number,
            sizeof(line_number),
            "%zu",
            line_index + 1U);
        sandbox3d_script_editor_copy_line(
            source,
            source_size,
            line_offset,
            line_length,
            line_text,
            sizeof(line_text));
        (void)henka_ui_label_colored(
            ui,
            bounds.x + 8.0f,
            line_y,
            SANDBOX3D_SCRIPT_EDITOR_TEXT_SCALE,
            line_number,
            HENKA_UI_COLOR_MUTED);
        (void)henka_ui_label_colored(
            ui,
            bounds.x + 42.0f,
            line_y,
            SANDBOX3D_SCRIPT_EDITOR_TEXT_SCALE,
            line_text,
            HENKA_UI_COLOR_NORMAL);
        if (tokenized)
        {
            sandbox3d_script_editor_draw_token_overlays(
                ui,
                bounds,
                source,
                tokens,
                token_count,
                (uint32_t)line_index + 1U,
                line_offset,
                line_length,
                (float)character_width);
        }
    }
    if (!tokenized && behavior->language == HENKA_SCRIPT_LANGUAGE_HENKASCRIPT)
    {
        (void)henka_ui_label_colored(
            ui,
            bounds.x + 42.0f,
            bounds.y + bounds.height - 15.0f,
            SANDBOX3D_SCRIPT_EDITOR_TEXT_SCALE,
            "Compiler tokenization unavailable; source shown plainly.",
            HENKA_UI_COLOR_WARNING);
    }
    henka_free(tokens);
    henka_free(source);
    return HENKA_SUCCESS;
}
