#include "script_editor.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <henka/henkascript.h>
#include <henka/input.h>
#include <henka/memory.h>
#include <henka/script_backends.h>

#include "game_authoring.h"

#define SANDBOX3D_SCRIPT_EDITOR_MAX_LINES 8U
#define SANDBOX3D_SCRIPT_EDITOR_MAX_LINE_BYTES 192U
#define SANDBOX3D_SCRIPT_EDITOR_HEADER_HEIGHT 22.0f
#define SANDBOX3D_SCRIPT_EDITOR_LINE_HEIGHT 15.0f
#define SANDBOX3D_SCRIPT_EDITOR_TEXT_SCALE 0.72f
#define SANDBOX3D_SCRIPT_EDITOR_INDENT_SPACES 4U
#define SANDBOX3D_SCRIPT_EDITOR_MAX_INDENT_SPACES \
    (HENKA_HKS_MAX_TOKENS * SANDBOX3D_SCRIPT_EDITOR_INDENT_SPACES)

typedef struct sandbox3d_script_editor_token_span
{
    size_t offset;
    size_t length;
    uint32_t line;
    henka_ui_semantic_color color;
} sandbox3d_script_editor_token_span;

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

static henka_ui_semantic_color sandbox3d_script_editor_lua_token_color(
    henka_lua_token_class token_class)
{
    switch (token_class)
    {
        case HENKA_LUA_TOKEN_CLASS_LITERAL:
            return HENKA_UI_COLOR_ORANGE;
        case HENKA_LUA_TOKEN_CLASS_KEYWORD:
            return HENKA_UI_COLOR_ACCENT;
        case HENKA_LUA_TOKEN_CLASS_BUILTIN:
            return HENKA_UI_COLOR_WARNING;
        case HENKA_LUA_TOKEN_CLASS_COMMENT:
            return HENKA_UI_COLOR_MUTED;
        case HENKA_LUA_TOKEN_CLASS_IDENTIFIER:
        case HENKA_LUA_TOKEN_CLASS_PUNCTUATION:
        case HENKA_LUA_TOKEN_CLASS_OPERATOR:
        default:
            return HENKA_UI_COLOR_NORMAL;
    }
}

static bool sandbox3d_script_editor_tokenize(
    henka_script_language language,
    const char* source,
    size_t source_size,
    henka_hks_token* hks_tokens,
    henka_lua_token* lua_tokens,
    sandbox3d_script_editor_token_span* spans,
    size_t* out_token_count,
    henka_hks_diagnostic* hks_diagnostic,
    henka_lua_diagnostic* lua_diagnostic)
{
    size_t backend_token_count = 0U;
    size_t index;
    if (out_token_count != NULL)
    {
        *out_token_count = 0U;
    }
    if (source == NULL || out_token_count == NULL || spans == NULL)
    {
        return false;
    }
    if (language == HENKA_SCRIPT_LANGUAGE_HENKASCRIPT)
    {
        if (hks_tokens == NULL ||
            henka_hks_lex(
                source,
                source_size,
                hks_tokens,
                HENKA_HKS_MAX_TOKENS,
                &backend_token_count,
                hks_diagnostic) != HENKA_SUCCESS)
        {
            return false;
        }
        for (index = 0U; index < backend_token_count; ++index)
        {
            spans[*out_token_count] = (sandbox3d_script_editor_token_span){
                hks_tokens[index].offset,
                hks_tokens[index].length,
                hks_tokens[index].line,
                hks_tokens[index].kind == HENKA_HKS_TOKEN_EOF
                    ? HENKA_UI_COLOR_NORMAL
                    : sandbox3d_script_editor_token_color(
                        henka_hks_token_kind_get_class(hks_tokens[index].kind))};
            ++(*out_token_count);
        }
        return true;
    }
    if (language == HENKA_SCRIPT_LANGUAGE_LUA)
    {
        if (lua_tokens == NULL ||
            henka_lua_lex(
                source,
                source_size,
                lua_tokens,
                HENKA_LUA_MAX_TOKENS,
                &backend_token_count,
                lua_diagnostic) != HENKA_SUCCESS)
        {
            return false;
        }
        for (index = 0U; index < backend_token_count; ++index)
        {
            spans[*out_token_count] = (sandbox3d_script_editor_token_span){
                lua_tokens[index].offset,
                lua_tokens[index].length,
                lua_tokens[index].line,
                lua_tokens[index].token_class == HENKA_LUA_TOKEN_CLASS_NONE
                    ? HENKA_UI_COLOR_NORMAL
                    : sandbox3d_script_editor_lua_token_color(
                        lua_tokens[index].token_class)};
            ++(*out_token_count);
        }
        return true;
    }
    return false;
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

static void sandbox3d_script_editor_draw_source_segment(
    henka_ui_context* ui,
    const char* source,
    size_t source_size,
    size_t offset,
    size_t length,
    float x,
    float y,
    henka_ui_semantic_color color)
{
    char segment[SANDBOX3D_SCRIPT_EDITOR_MAX_LINE_BYTES];
    if (ui == NULL || source == NULL || offset > source_size ||
        length > source_size - offset || length == 0U)
    {
        return;
    }
    sandbox3d_script_editor_copy_line(
        source,
        source_size,
        offset,
        length,
        segment,
        sizeof(segment));
    (void)henka_ui_label_colored(
        ui,
        x,
        y,
        SANDBOX3D_SCRIPT_EDITOR_TEXT_SCALE,
        segment,
        color);
}

static void sandbox3d_script_editor_draw_tokenized_line(
    henka_ui_context* ui,
    henka_ui_rect bounds,
    const char* source,
    size_t source_size,
    const sandbox3d_script_editor_token_span* tokens,
    size_t token_count,
    uint32_t line_number,
    size_t line_offset,
    size_t line_length,
    float character_width)
{
    const float code_x = bounds.x + 42.0f;
    const float line_y = bounds.y + SANDBOX3D_SCRIPT_EDITOR_HEADER_HEIGHT +
        ((float)line_number - 1.0f) * SANDBOX3D_SCRIPT_EDITOR_LINE_HEIGHT;
    size_t cursor = 0U;
    size_t token_index;

    for (token_index = 0U; token_index < token_count; ++token_index)
    {
        const sandbox3d_script_editor_token_span* token = &tokens[token_index];
        size_t token_offset;
        size_t token_length;
        if (token->length == 0U || token->line != line_number ||
            token->offset < line_offset ||
            token->offset - line_offset >= line_length)
        {
            continue;
        }
        token_offset = token->offset - line_offset;
        if (token_offset < cursor)
        {
            continue;
        }
        if (token_offset > cursor)
        {
            sandbox3d_script_editor_draw_source_segment(
                ui,
                source,
                source_size,
                line_offset + cursor,
                token_offset - cursor,
                code_x + (float)cursor * character_width,
                line_y,
                HENKA_UI_COLOR_NORMAL);
        }
        token_length = token->length;
        if (token_length > line_length - token_offset)
        {
            token_length = line_length - token_offset;
        }
        sandbox3d_script_editor_draw_source_segment(
            ui,
            source,
            source_size,
            line_offset + token_offset,
            token_length,
            code_x + (float)token_offset * character_width,
            line_y,
            token->color);
        cursor = token_offset + token_length;
    }

    if (cursor < line_length)
    {
        sandbox3d_script_editor_draw_source_segment(
            ui,
            source,
            source_size,
            line_offset + cursor,
            line_length - cursor,
            code_x + (float)cursor * character_width,
            line_y,
            HENKA_UI_COLOR_NORMAL);
    }
}

static size_t sandbox3d_script_editor_previous_boundary(
    const char* source,
    size_t offset)
{
    if (source == NULL || offset == 0U)
    {
        return offset;
    }
    --offset;
    while (offset > 0U &&
        ((unsigned char)source[offset] & 0xC0U) == 0x80U)
    {
        --offset;
    }
    return offset;
}

static size_t sandbox3d_script_editor_indent_replacement(
    const henka_hks_token* tokens,
    size_t token_count,
    size_t source_offset,
    uint32_t source_line,
    char* out_replacement,
    size_t replacement_capacity)
{
    uint32_t indent_level = 0U;
    size_t indent_spaces;
    size_t index;

    if (out_replacement == NULL || replacement_capacity == 0U)
    {
        return 0U;
    }
    out_replacement[0] = '\0';
    if (henka_hks_token_stream_get_indent_level(
            tokens,
            token_count,
            source_offset,
            source_line,
            &indent_level) != HENKA_SUCCESS ||
        indent_level > SIZE_MAX / SANDBOX3D_SCRIPT_EDITOR_INDENT_SPACES)
    {
        return 0U;
    }
    indent_spaces = (size_t)indent_level * SANDBOX3D_SCRIPT_EDITOR_INDENT_SPACES;
    if (indent_spaces > SANDBOX3D_SCRIPT_EDITOR_MAX_INDENT_SPACES ||
        indent_spaces + 1U > replacement_capacity)
    {
        return 0U;
    }
    out_replacement[0] = '\n';
    for (index = 0U; index < indent_spaces; ++index)
    {
        out_replacement[index + 1U] = ' ';
    }
    out_replacement[indent_spaces + 1U] = '\0';
    return indent_spaces + 1U;
}

static size_t sandbox3d_script_editor_lua_indent_replacement(
    const char* source,
    size_t source_size,
    const henka_lua_token* tokens,
    size_t token_count,
    size_t source_offset,
    uint32_t source_line,
    char* out_replacement,
    size_t replacement_capacity)
{
    uint32_t indent_level = 0U;
    size_t indent_spaces;
    size_t index;
    if (out_replacement == NULL || replacement_capacity == 0U ||
        henka_lua_token_stream_get_indent_level(
            source,
            source_size,
            tokens,
            token_count,
            source_offset,
            source_line,
            &indent_level) != HENKA_SUCCESS ||
        indent_level > SIZE_MAX / SANDBOX3D_SCRIPT_EDITOR_INDENT_SPACES)
    {
        return 0U;
    }
    indent_spaces = (size_t)indent_level * SANDBOX3D_SCRIPT_EDITOR_INDENT_SPACES;
    if (indent_spaces > SANDBOX3D_SCRIPT_EDITOR_MAX_INDENT_SPACES ||
        indent_spaces + 1U > replacement_capacity)
    {
        return 0U;
    }
    out_replacement[0] = '\n';
    for (index = 0U; index < indent_spaces; ++index)
    {
        out_replacement[index + 1U] = ' ';
    }
    out_replacement[indent_spaces + 1U] = '\0';
    return indent_spaces + 1U;
}

static void sandbox3d_script_editor_draw_caret(
    henka_ui_context* ui,
    henka_ui_rect bounds,
    sandbox3d_script_editor_model* model,
    float character_width)
{
    const size_t line = sandbox3d_script_editor_model_get_caret_line(model);
    const size_t column = sandbox3d_script_editor_model_get_caret_column(model);
    if (ui == NULL || model == NULL || line == 0U || column == 0U)
    {
        return;
    }
    (void)henka_ui_overlay_rect(
        ui,
        (henka_ui_rect){
            bounds.x + 42.0f + (float)(column - 1U) * character_width,
            bounds.y + SANDBOX3D_SCRIPT_EDITOR_HEADER_HEIGHT +
                (float)(line - 1U) * SANDBOX3D_SCRIPT_EDITOR_LINE_HEIGHT,
            1.0f,
            SANDBOX3D_SCRIPT_EDITOR_LINE_HEIGHT},
        (henka_vec4){0.35f, 0.75f, 1.0f, 0.9f});
}

henka_result sandbox3d_script_editor_draw_preview(
    struct henka_engine* engine,
    henka_ui_context* ui,
    henka_ui_rect bounds,
    const char* project_root,
    const henka_scene_document_behavior* behavior,
    sandbox3d_game_authoring* authoring,
    henka_entity entity,
    sandbox3d_script_editor_model** io_model,
    bool play_active)
{
    const char* source = NULL;
    size_t source_size = 0U;
    henka_hks_token* hks_tokens = NULL;
    henka_lua_token* lua_tokens = NULL;
    sandbox3d_script_editor_token_span* presentation_tokens = NULL;
    size_t token_count = 0U;
    henka_hks_diagnostic diagnostic;
    henka_lua_diagnostic lua_diagnostic;
    henka_script_source_diagnostic source_diagnostic;
    henka_ui_interaction_state code_interaction;
    henka_vec2 mouse_position;
    char status_text[128];
    char diagnostic_text[192];
    size_t text_input_size = 0U;
    const char* text_input;
    henka_result result = HENKA_SUCCESS;
    henka_result validation_result;
    int character_width = 6;
    size_t line_index;
    size_t visible_lines;
    bool tokenized = false;
    int character_height = 0;
    bool save_clicked;
    bool revert_clicked;
    bool reload_clicked;
    henka_result reload_result = HENKA_SUCCESS;
    henka_script_source_diagnostic reload_diagnostic;
    char indent_replacement[SANDBOX3D_SCRIPT_EDITOR_MAX_INDENT_SPACES + 2U];
    sandbox3d_script_editor_model* model;
    if (ui == NULL || !sandbox3d_script_editor_rect_is_valid(bounds) ||
        engine == NULL || project_root == NULL || behavior == NULL ||
        io_model == NULL ||
        behavior->asset_path[0] == '\0' ||
        (behavior->language != HENKA_SCRIPT_LANGUAGE_LUA &&
         behavior->language != HENKA_SCRIPT_LANGUAGE_HENKASCRIPT))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    model = *io_model;
    if (model == NULL || sandbox3d_script_editor_model_get_language(model) !=
            behavior->language ||
        strcmp(
            sandbox3d_script_editor_model_get_asset_path(model),
            behavior->asset_path) != 0)
    {
        sandbox3d_script_editor_model_destroy(model);
        model = NULL;
        result = sandbox3d_script_editor_model_create(
            behavior->language, &model);
        if (result == HENKA_SUCCESS)
        {
            result = sandbox3d_script_editor_model_load_asset(
                model, project_root, behavior->asset_path);
        }
        if (result != HENKA_SUCCESS)
        {
            sandbox3d_script_editor_model_destroy(model);
            *io_model = NULL;
            model = NULL;
        }
        else
        {
            *io_model = model;
        }
    }
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
    if (model == NULL)
    {
        (void)henka_ui_label_colored(
            ui,
            bounds.x + 8.0f,
            bounds.y + 5.0f,
            SANDBOX3D_SCRIPT_EDITOR_TEXT_SCALE,
            "Source unavailable",
            HENKA_UI_COLOR_WARNING);
        return result == HENKA_SUCCESS ? HENKA_ERROR_ASSET_SOURCE : result;
    }
    result = sandbox3d_script_editor_model_set_play_active(model, play_active);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    result = sandbox3d_script_editor_model_get_source(
        model, &source, &source_size);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    presentation_tokens = (sandbox3d_script_editor_token_span*)henka_calloc(
        HENKA_HKS_MAX_TOKENS,
        sizeof(*presentation_tokens));
    if (behavior->language == HENKA_SCRIPT_LANGUAGE_HENKASCRIPT)
    {
        hks_tokens = (henka_hks_token*)henka_calloc(
            HENKA_HKS_MAX_TOKENS,
            sizeof(*hks_tokens));
    }
    else if (behavior->language == HENKA_SCRIPT_LANGUAGE_LUA)
    {
        lua_tokens = (henka_lua_token*)henka_calloc(
            HENKA_LUA_MAX_TOKENS,
            sizeof(*lua_tokens));
    }
    tokenized = sandbox3d_script_editor_tokenize(
        behavior->language,
        source,
        source_size,
        hks_tokens,
        lua_tokens,
        presentation_tokens,
        &token_count,
        &diagnostic,
        &lua_diagnostic);
    mouse_position = henka_ui_get_mouse_position(ui);
    result = henka_ui_custom_interaction(
        ui,
        "script-editor-source",
        henka_ui_rect_contains(
            (henka_ui_rect){
                bounds.x,
                bounds.y + SANDBOX3D_SCRIPT_EDITOR_HEADER_HEIGHT,
                bounds.width,
                bounds.height - SANDBOX3D_SCRIPT_EDITOR_HEADER_HEIGHT},
            mouse_position),
        true,
        &code_interaction);
    if (result != HENKA_SUCCESS)
    {
        henka_free(hks_tokens);
        henka_free(lua_tokens);
        henka_free(presentation_tokens);
        return result;
    }
    if (code_interaction.pressed)
    {
        size_t clicked_line = (size_t)((mouse_position.y - bounds.y -
            SANDBOX3D_SCRIPT_EDITOR_HEADER_HEIGHT) /
            SANDBOX3D_SCRIPT_EDITOR_LINE_HEIGHT);
        size_t clicked_column = mouse_position.x > bounds.x + 42.0f
            ? (size_t)((mouse_position.x - bounds.x - 42.0f) /
                (float)character_width)
            : 0U;
        (void)sandbox3d_script_editor_model_set_focused(model, true);
        (void)sandbox3d_script_editor_model_set_caret_position(
            model, clicked_line, clicked_column);
    }
    text_input = henka_ui_get_text_input(ui, &text_input_size);
    if (sandbox3d_script_editor_model_is_focused(model) && !play_active)
    {
        if (text_input != NULL && text_input_size > 0U)
        {
            (void)sandbox3d_script_editor_model_replace_selection(
                model, text_input, text_input_size);
        }
        if (henka_input_was_key_pressed(engine, HENKA_KEY_ENTER))
        {
            size_t replacement_size = 0U;
            if (tokenized)
            {
                if (behavior->language == HENKA_SCRIPT_LANGUAGE_HENKASCRIPT)
                {
                    replacement_size = sandbox3d_script_editor_indent_replacement(
                        hks_tokens,
                        token_count,
                        sandbox3d_script_editor_model_get_caret_offset(model),
                        (uint32_t)sandbox3d_script_editor_model_get_caret_line(model) + 1U,
                        indent_replacement,
                        sizeof(indent_replacement));
                }
                else
                {
                    replacement_size = sandbox3d_script_editor_lua_indent_replacement(
                        source,
                        source_size,
                        lua_tokens,
                        token_count,
                        sandbox3d_script_editor_model_get_caret_offset(model),
                        (uint32_t)sandbox3d_script_editor_model_get_caret_line(model) + 1U,
                        indent_replacement,
                        sizeof(indent_replacement));
                }
            }
            if (replacement_size == 0U)
            {
                indent_replacement[0] = '\n';
                indent_replacement[1] = '\0';
                replacement_size = 1U;
            }
            (void)sandbox3d_script_editor_model_replace_selection(
                model,
                indent_replacement,
                replacement_size);
            henka_input_consume_key_press(engine, HENKA_KEY_ENTER);
        }
        else if (henka_input_was_key_pressed(engine, HENKA_KEY_TAB))
        {
            (void)sandbox3d_script_editor_model_replace_selection(
                model, "    ", 4U);
            henka_input_consume_key_press(engine, HENKA_KEY_TAB);
        }
        else if (henka_input_was_key_pressed(engine, HENKA_KEY_BACKSPACE))
        {
            const size_t caret =
                sandbox3d_script_editor_model_get_caret_offset(model);
            if (caret > 0U)
            {
                const size_t previous =
                    sandbox3d_script_editor_previous_boundary(source, caret);
                (void)sandbox3d_script_editor_model_set_selection(
                    model, previous, caret);
                (void)sandbox3d_script_editor_model_replace_selection(
                    model, NULL, 0U);
            }
            henka_input_consume_key_press(engine, HENKA_KEY_BACKSPACE);
        }
        else if (henka_input_was_key_pressed(engine, HENKA_KEY_UP))
        {
            (void)sandbox3d_script_editor_model_move_vertical(model, -1);
            henka_input_consume_key_press(engine, HENKA_KEY_UP);
        }
        else if (henka_input_was_key_pressed(engine, HENKA_KEY_DOWN))
        {
            (void)sandbox3d_script_editor_model_move_vertical(model, 1);
            henka_input_consume_key_press(engine, HENKA_KEY_DOWN);
        }
        (void)sandbox3d_script_editor_model_get_source(
            model, &source, &source_size);
    }
    save_clicked = henka_ui_button(
        ui,
        "script-editor-save",
        (henka_ui_rect){bounds.x + bounds.width - 112.0f, bounds.y + 2.0f, 50.0f, 18.0f},
        "Save");
    revert_clicked = henka_ui_button(
        ui,
        "script-editor-revert",
        (henka_ui_rect){bounds.x + bounds.width - 58.0f, bounds.y + 2.0f, 50.0f, 18.0f},
        "Revert");
    reload_clicked = henka_ui_button(
        ui,
        "script-editor-reload",
        (henka_ui_rect){bounds.x + bounds.width - 166.0f, bounds.y + 2.0f, 50.0f, 18.0f},
        "Reload");
    if (save_clicked && !play_active)
    {
        result = sandbox3d_script_editor_model_save(
            model, project_root, behavior->asset_path);
        if (result != HENKA_SUCCESS)
        {
            (void)snprintf(status_text, sizeof(status_text), "Save failed");
        }
    }
    if (revert_clicked && !play_active)
    {
        (void)sandbox3d_script_editor_model_revert(model);
        (void)sandbox3d_script_editor_model_get_source(
            model, &source, &source_size);
    }
    if (reload_clicked)
    {
        if (play_active)
        {
            reload_result = sandbox3d_game_authoring_reload_behavior_for_entity(
                authoring,
                entity,
                behavior->id,
                &reload_diagnostic);
        }
        else
        {
            reload_result = sandbox3d_script_editor_model_load_asset(
                model,
                project_root,
                behavior->asset_path);
            if (reload_result != HENKA_SUCCESS)
            {
                memset(&reload_diagnostic, 0, sizeof(reload_diagnostic));
                reload_diagnostic.result = reload_result;
                (void)snprintf(
                    reload_diagnostic.message,
                    sizeof(reload_diagnostic.message),
                    "Source reload rejected");
            }
        }
        (void)sandbox3d_script_editor_model_get_source(
            model, &source, &source_size);
    }
    validation_result = sandbox3d_script_editor_model_validate(
        model, &source_diagnostic);
    if (reload_result != HENKA_SUCCESS)
    {
        (void)snprintf(
            status_text,
            sizeof(status_text),
            "Reload failed%s",
            play_active ? " | Play locked" : "");
        (void)snprintf(
            diagnostic_text,
            sizeof(diagnostic_text),
            "%s",
            reload_diagnostic.message[0] != '\0'
                ? reload_diagnostic.message
                : "Behavior reload rejected");
    }
    else if (reload_clicked && play_active)
    {
        (void)snprintf(
            status_text,
            sizeof(status_text),
            "Reloaded%s",
            play_active ? " | Play locked" : "");
        diagnostic_text[0] = '\0';
    }
    else if (validation_result == HENKA_SUCCESS)
    {
        (void)snprintf(
            status_text,
            sizeof(status_text),
            "%s%s",
            sandbox3d_script_editor_model_is_dirty(model) ? "Modified" : "Saved",
            play_active ? " | Play locked" : "");
        diagnostic_text[0] = '\0';
    }
    else
    {
        (void)snprintf(
            status_text,
            sizeof(status_text),
            "%s%s",
            sandbox3d_script_editor_model_is_dirty(model) ? "Modified" : "Saved",
            play_active ? " | Play locked" : "");
        (void)snprintf(
            diagnostic_text,
            sizeof(diagnostic_text),
            "Line %u, column %u: %s",
            source_diagnostic.line,
            source_diagnostic.column,
            source_diagnostic.message);
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
        bounds.x + bounds.width - 230.0f,
        bounds.y + 5.0f,
        SANDBOX3D_SCRIPT_EDITOR_TEXT_SCALE,
            status_text,
            validation_result == HENKA_SUCCESS && reload_result == HENKA_SUCCESS
                ? HENKA_UI_COLOR_SUCCESS
                : HENKA_UI_COLOR_WARNING);
    tokenized = sandbox3d_script_editor_tokenize(
        behavior->language,
        source,
        source_size,
        hks_tokens,
        lua_tokens,
        presentation_tokens,
        &token_count,
        &diagnostic,
        &lua_diagnostic);
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
        if (tokenized)
        {
            sandbox3d_script_editor_draw_tokenized_line(
                ui,
                bounds,
                source,
                source_size,
                presentation_tokens,
                token_count,
                (uint32_t)line_index + 1U,
                line_offset,
                line_length,
                (float)character_width);
        }
        else
        {
            (void)henka_ui_label_colored(
                ui,
                bounds.x + 42.0f,
                line_y,
                SANDBOX3D_SCRIPT_EDITOR_TEXT_SCALE,
                line_text,
                HENKA_UI_COLOR_NORMAL);
        }
    }
    if (sandbox3d_script_editor_model_is_focused(model))
    {
        sandbox3d_script_editor_draw_caret(
            ui, bounds, model, (float)character_width);
    }
    if (diagnostic_text[0] != '\0')
    {
        (void)henka_ui_label_colored(
            ui,
            bounds.x + 42.0f,
            bounds.y + bounds.height - 15.0f,
            SANDBOX3D_SCRIPT_EDITOR_TEXT_SCALE,
            diagnostic_text,
            HENKA_UI_COLOR_WARNING);
    }
    henka_free(hks_tokens);
    henka_free(lua_tokens);
    henka_free(presentation_tokens);
    return HENKA_SUCCESS;
}
