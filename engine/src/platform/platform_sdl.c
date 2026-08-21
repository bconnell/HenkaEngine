#include "henka_internal.h"

#include <SDL3/SDL.h>

#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <henka/log.h>
#include <henka/memory.h>

#if defined(_WIN32)
#include <share.h>
#endif

bool henka_input_automation_begin(
    henka_input_state* input,
    const char* event_path)
{
    size_t path_length;

    if (input == NULL || event_path == NULL)
    {
        return false;
    }
    path_length = strlen(event_path);
    if (path_length == 0U || path_length >= sizeof(input->automation_input_path))
    {
        return false;
    }

    {
        FILE* stream = NULL;
#if defined(_WIN32)
        stream = _fsopen(event_path, "rb", _SH_DENYNO);
        if (stream == NULL)
        {
            return false;
        }
#else
        stream = fopen(event_path, "rb");
        if (stream == NULL)
        {
            return false;
        }
#endif
        fclose(stream);
    }

    memcpy(input->automation_input_path, event_path, path_length + 1U);
    input->automation_input_offset = 0U;
    input->automation_input_owned = true;
    input->automation_input_faulted = false;
    input->automation_input_stream_failures = 0U;
    input->mouse_position = (henka_vec2){0.0f, 0.0f};
    input->mouse_delta = (henka_vec2){0.0f, 0.0f};
    input->mouse_wheel_delta = (henka_vec2){0.0f, 0.0f};
    return true;
}

void henka_input_automation_release(henka_input_state* input)
{
    if (input == NULL)
    {
        return;
    }

    input->automation_input_owned = false;
    input->automation_input_faulted = false;
    input->automation_input_path[0] = '\0';
    input->automation_input_offset = 0U;
    input->automation_input_stream_failures = 0U;
    henka_platform_release_input_on_focus_loss(input);
}

static bool henka_input_automation_parse_button(
    const char* text,
    henka_mouse_button* out_button)
{
    if (text == NULL || out_button == NULL)
    {
        return false;
    }
    if (strcmp(text, "left") == 0)
    {
        *out_button = HENKA_MOUSE_BUTTON_LEFT;
    }
    else if (strcmp(text, "right") == 0)
    {
        *out_button = HENKA_MOUSE_BUTTON_RIGHT;
    }
    else if (strcmp(text, "middle") == 0)
    {
        *out_button = HENKA_MOUSE_BUTTON_MIDDLE;
    }
    else
    {
        *out_button = HENKA_MOUSE_BUTTON_UNKNOWN;
    }
    return *out_button != HENKA_MOUSE_BUTTON_UNKNOWN;
}

static bool henka_input_automation_next_token(
    const char** cursor,
    char* out_token,
    size_t out_token_capacity)
{
    const char* start;
    size_t length;

    if (cursor == NULL || *cursor == NULL || out_token == NULL ||
        out_token_capacity < 2U)
    {
        return false;
    }

    while (isspace((unsigned char)**cursor))
    {
        ++*cursor;
    }
    if (**cursor == '\0')
    {
        out_token[0] = '\0';
        return false;
    }

    start = *cursor;
    while (**cursor != '\0' && !isspace((unsigned char)**cursor))
    {
        ++*cursor;
    }
    length = (size_t)(*cursor - start);
    if (length == 0U || length >= out_token_capacity)
    {
        out_token[0] = '\0';
        return false;
    }
    memcpy(out_token, start, length);
    out_token[length] = '\0';
    return true;
}

static bool henka_input_automation_no_more_tokens(const char* cursor)
{
    if (cursor == NULL)
    {
        return false;
    }
    while (isspace((unsigned char)*cursor))
    {
        ++cursor;
    }
    return *cursor == '\0';
}

static bool henka_input_automation_parse_float(
    const char* token,
    float minimum,
    float maximum,
    float* out_value)
{
    char* end;
    float value;

    if (token == NULL || out_value == NULL)
    {
        return false;
    }
    errno = 0;
    end = NULL;
    value = strtof(token, &end);
    if (errno == ERANGE || end == token || end == NULL || *end != '\0' ||
        !isfinite(value) || value < minimum || value > maximum)
    {
        return false;
    }
    *out_value = value;
    return true;
}

bool henka_input_automation_apply_event(
    henka_input_state* input,
    const char* event_line)
{
    char command[32];
    char first[32];
    char second[32];
    char third[32];
    char fourth[32];
    const char* cursor;
    float x;
    float y;

    if (input == NULL || event_line == NULL || !input->automation_input_owned)
    {
        return false;
    }

    cursor = event_line;
    if (!henka_input_automation_next_token(&cursor, command, sizeof(command)))
    {
        return false;
    }

    if (strcmp(command, "move") == 0)
    {
        if (!henka_input_automation_next_token(&cursor, first, sizeof(first)) ||
            !henka_input_automation_next_token(&cursor, second, sizeof(second)) ||
            !henka_input_automation_no_more_tokens(cursor) ||
            !henka_input_automation_parse_float(first, -65536.0f, 65536.0f, &x) ||
            !henka_input_automation_parse_float(second, -65536.0f, 65536.0f, &y))
        {
            return false;
        }
        input->mouse_delta.x += x - input->mouse_position.x;
        input->mouse_delta.y += y - input->mouse_position.y;
        input->mouse_position = (henka_vec2){x, y};
        return true;
    }

    if (strcmp(command, "wheel") == 0)
    {
        if (!henka_input_automation_next_token(&cursor, first, sizeof(first)) ||
            !henka_input_automation_next_token(&cursor, second, sizeof(second)) ||
            !henka_input_automation_no_more_tokens(cursor) ||
            !henka_input_automation_parse_float(first, -1024.0f, 1024.0f, &x) ||
            !henka_input_automation_parse_float(second, -1024.0f, 1024.0f, &y))
        {
            return false;
        }
        input->mouse_wheel_delta.x += x;
        input->mouse_wheel_delta.y += y;
        return true;
    }

    if (strcmp(command, "button") == 0)
    {
        henka_mouse_button button;
        if (!henka_input_automation_next_token(&cursor, first, sizeof(first)) ||
            !henka_input_automation_next_token(&cursor, second, sizeof(second)) ||
            !henka_input_automation_next_token(&cursor, third, sizeof(third)) ||
            !henka_input_automation_next_token(&cursor, fourth, sizeof(fourth)) ||
            !henka_input_automation_no_more_tokens(cursor) ||
            !henka_input_automation_parse_float(third, -65536.0f, 65536.0f, &x) ||
            !henka_input_automation_parse_float(fourth, -65536.0f, 65536.0f, &y) ||
            !henka_input_automation_parse_button(first, &button))
        {
            return false;
        }
        if (strcmp(second, "down") != 0 && strcmp(second, "up") != 0)
        {
            return false;
        }
        input->mouse_delta.x += x - input->mouse_position.x;
        input->mouse_delta.y += y - input->mouse_position.y;
        input->mouse_position = (henka_vec2){x, y};
        if (strcmp(second, "down") == 0)
        {
            input->mouse_buttons_down[button] = true;
            input->mouse_buttons_pressed[button] = true;
            return true;
        }
        input->mouse_buttons_down[button] = false;
        input->mouse_buttons_released[button] = true;
        return true;
    }

    if (strcmp(command, "key") == 0)
    {
        henka_key key;
        if (!henka_input_automation_next_token(&cursor, first, sizeof(first)) ||
            !henka_input_automation_next_token(&cursor, second, sizeof(second)) ||
            !henka_input_automation_no_more_tokens(cursor))
        {
            return false;
        }
        key = henka_input_key_find_by_name(first);
        if (key == HENKA_KEY_UNKNOWN)
        {
            return false;
        }
        if (strcmp(second, "down") == 0)
        {
            input->keys_down[key] = true;
            input->keys_pressed[key] = true;
            return true;
        }
        if (strcmp(second, "up") == 0)
        {
            input->keys_down[key] = false;
            input->keys_released[key] = true;
            return true;
        }
    }

    return false;
}

static void henka_platform_poll_automation_event(henka_input_state* input)
{
    FILE* stream;
    char line[256];
    long next_offset;

    if (input == NULL || !input->automation_input_owned ||
        input->automation_input_faulted ||
        input->automation_input_path[0] == '\0')
    {
        return;
    }

    stream = NULL;
#if defined(_WIN32)
    stream = _fsopen(input->automation_input_path, "rb", _SH_DENYNO);
#else
    stream = fopen(input->automation_input_path, "rb");
#endif
    if (stream == NULL || input->automation_input_offset > (uint64_t)LONG_MAX ||
        fseek(stream, (long)input->automation_input_offset, SEEK_SET) != 0)
    {
        if (stream != NULL)
        {
            fclose(stream);
        }
        if (input->automation_input_stream_failures < UINT32_MAX)
        {
            ++input->automation_input_stream_failures;
        }
        if (input->automation_input_stream_failures < 4U)
        {
            return;
        }
        input->automation_input_faulted = true;
        input->close_requested = true;
        HENKA_LOG_ERROR("automation input stream remained unavailable; input ownership remains fail-closed");
        return;
    }
    input->automation_input_stream_failures = 0U;
    if (fgets(line, sizeof(line), stream) == NULL)
    {
        const int read_error = ferror(stream);
        fclose(stream);
        if (read_error != 0)
        {
            input->automation_input_faulted = true;
            input->close_requested = true;
            HENKA_LOG_ERROR("automation input stream read failed; input ownership remains fail-closed");
        }
        return;
    }
    if (strchr(line, '\n') == NULL)
    {
        const int at_end = feof(stream);
        fclose(stream);
        if (!at_end)
        {
            input->automation_input_faulted = true;
            input->close_requested = true;
            HENKA_LOG_ERROR("automation input event exceeded the bounded line length");
        }
        return;
    }
    next_offset = ftell(stream);
    fclose(stream);
    if (next_offset < 0L)
    {
        input->automation_input_faulted = true;
        input->close_requested = true;
        HENKA_LOG_ERROR("automation input stream position could not be read");
        return;
    }
    line[strcspn(line, "\r\n")] = '\0';
    if (henka_input_automation_apply_event(input, line))
    {
        input->automation_input_offset = (uint64_t)next_offset;
    }
    else
    {
        input->automation_input_offset = (uint64_t)next_offset;
        input->automation_input_faulted = true;
        input->close_requested = true;
        HENKA_LOG_ERROR("malformed automation input event; input ownership remains fail-closed");
    }
}

void henka_platform_release_input_on_focus_loss(henka_input_state* input)
{
    size_t index;

    if (input == NULL)
    {
        return;
    }

    for (index = 0U; index < HENKA_KEY_COUNT; ++index)
    {
        if (input->keys_down[index])
        {
            input->keys_released[index] = true;
        }
        input->keys_down[index] = false;
        input->keys_pressed[index] = false;
    }

    for (index = 0U; index < HENKA_MOUSE_BUTTON_COUNT; ++index)
    {
        if (input->mouse_buttons_down[index])
        {
            input->mouse_buttons_released[index] = true;
        }
        input->mouse_buttons_down[index] = false;
        input->mouse_buttons_pressed[index] = false;
    }

    input->mouse_delta = (henka_vec2){0.0f, 0.0f};
    input->mouse_wheel_delta = (henka_vec2){0.0f, 0.0f};
    input->text_input_size = 0U;
    input->text_input[0] = '\0';
    input->text_input_overflowed = false;
}

static bool henka_platform_append_text_input(
    henka_input_state* input,
    const char* text)
{
    size_t text_size;
    if (input == NULL || text == NULL)
    {
        return false;
    }
    text_size = strlen(text);
    if (text_size > (size_t)HENKA_INPUT_MAX_TEXT_INPUT_BYTES - 1U -
        input->text_input_size)
    {
        input->text_input_overflowed = true;
        return false;
    }
    memcpy(input->text_input + input->text_input_size, text, text_size);
    input->text_input_size += text_size;
    input->text_input[input->text_input_size] = '\0';
    return true;
}

bool henka_platform_choose_tool_window_id(
    henka_window_id next_candidate,
    const henka_window_id* occupied_ids,
    size_t occupied_count,
    henka_window_id* out_window_id,
    henka_window_id* out_next_candidate)
{
    size_t attempt;
    henka_window_id candidate;

    if (out_window_id != NULL)
    {
        *out_window_id = HENKA_INVALID_WINDOW_ID;
    }
    if (out_next_candidate != NULL)
    {
        *out_next_candidate = HENKA_INVALID_WINDOW_ID;
    }

    if (out_window_id == NULL ||
        out_next_candidate == NULL ||
        occupied_count > HENKA_MAX_TOOL_WINDOWS ||
        (occupied_count > 0U && occupied_ids == NULL))
    {
        return false;
    }

    candidate = next_candidate < 2U ? 2U : next_candidate;
    for (attempt = 0U; attempt <= occupied_count; ++attempt)
    {
        size_t index;
        bool occupied;

        occupied = false;
        for (index = 0U; index < occupied_count; ++index)
        {
            if (occupied_ids[index] == candidate)
            {
                occupied = true;
                break;
            }
        }

        if (!occupied)
        {
            *out_window_id = candidate;
            *out_next_candidate =
                candidate == UINT32_MAX ? 2U : candidate + 1U;
            return true;
        }

        candidate = candidate == UINT32_MAX ? 2U : candidate + 1U;
        if (candidate < 2U)
        {
            candidate = 2U;
        }
    }

    return false;
}

typedef struct henka_platform_tool_window
{
    henka_window_id id;
    SDL_Window* window;
    SDL_WindowID native_window_id;
    bool open;
    bool focused;
    bool close_requested;
    bool resized;
    int width;
    int height;
    int position_x;
    int position_y;
    henka_vec2 mouse_position;
    bool mouse_left_down;
    bool mouse_left_pressed;
    bool mouse_left_released;
    henka_vec2 mouse_wheel_delta;
    char last_event[48];
} henka_platform_tool_window;

struct henka_platform
{
    SDL_Window* window;
    SDL_WindowID main_window_id;
    bool multi_window_available;
    bool main_window_focused;
    henka_window_id next_tool_window_id;
    henka_platform_tool_window tool_windows[HENKA_MAX_TOOL_WINDOWS];
    SDL_Cursor* horizontal_resize_cursor;
    SDL_Cursor* vertical_resize_cursor;
    henka_cursor_shape cursor_shape;
    henka_window_event_route last_event_route;
    henka_window_id last_tool_window_id;
    bool last_tool_window_close_requested;
    bool last_tool_window_resized;
};

static henka_platform_tool_window* henka_platform_find_tool_window(
    struct henka_platform* platform,
    henka_window_id window_id)
{
    size_t index;

    if (platform == NULL || window_id == HENKA_INVALID_WINDOW_ID)
    {
        return NULL;
    }

    for (index = 0U; index < HENKA_MAX_TOOL_WINDOWS; ++index)
    {
        if (platform->tool_windows[index].open && platform->tool_windows[index].id == window_id)
        {
            return &platform->tool_windows[index];
        }
    }

    return NULL;
}

static const henka_platform_tool_window* henka_platform_find_tool_window_const(
    const struct henka_platform* platform,
    henka_window_id window_id)
{
    size_t index;

    if (platform == NULL || window_id == HENKA_INVALID_WINDOW_ID)
    {
        return NULL;
    }

    for (index = 0U; index < HENKA_MAX_TOOL_WINDOWS; ++index)
    {
        if (platform->tool_windows[index].open && platform->tool_windows[index].id == window_id)
        {
            return &platform->tool_windows[index];
        }
    }

    return NULL;
}

static henka_platform_tool_window* henka_platform_find_tool_window_by_native_id(
    struct henka_platform* platform,
    SDL_WindowID native_window_id)
{
    size_t index;

    if (platform == NULL || native_window_id == 0U)
    {
        return NULL;
    }

    for (index = 0U; index < HENKA_MAX_TOOL_WINDOWS; ++index)
    {
        if (platform->tool_windows[index].open &&
            platform->tool_windows[index].native_window_id == native_window_id)
        {
            return &platform->tool_windows[index];
        }
    }

    return NULL;
}

static void henka_platform_record_tool_event(
    struct henka_platform* platform,
    henka_platform_tool_window* tool_window,
    const char* event_name,
    bool close_requested,
    bool resized)
{
    if (platform == NULL || tool_window == NULL || event_name == NULL)
    {
        return;
    }

    platform->last_event_route = HENKA_WINDOW_EVENT_ROUTE_TOOL;
    if (close_requested ||
        resized ||
        (!platform->last_tool_window_close_requested &&
            !platform->last_tool_window_resized))
    {
        platform->last_tool_window_id = tool_window->id;
    }
    platform->last_tool_window_close_requested =
        platform->last_tool_window_close_requested || close_requested;
    platform->last_tool_window_resized =
        platform->last_tool_window_resized || resized;
    snprintf(
        tool_window->last_event,
        sizeof(tool_window->last_event),
        "%s",
        event_name);
}

static bool henka_platform_event_is_main_window(const struct henka_platform* platform, SDL_WindowID window_id)
{
    return platform != NULL && window_id != 0U && platform->main_window_id == window_id;
}

static void henka_platform_reset_tool_window_frame_input(
    struct henka_platform* platform)
{
    size_t index;

    if (platform == NULL)
    {
        return;
    }

    for (index = 0U; index < HENKA_MAX_TOOL_WINDOWS; ++index)
    {
        if (!platform->tool_windows[index].open)
        {
            continue;
        }

        platform->tool_windows[index].mouse_left_pressed = false;
        platform->tool_windows[index].mouse_left_released = false;
        platform->tool_windows[index].mouse_wheel_delta = (henka_vec2){0.0f, 0.0f};
        platform->tool_windows[index].resized = false;
    }
}

static void henka_platform_clear_tool_window_input_for_automation(
    struct henka_platform* platform)
{
    size_t index;

    if (platform == NULL)
    {
        return;
    }
    for (index = 0U; index < HENKA_MAX_TOOL_WINDOWS; ++index)
    {
        henka_platform_tool_window* tool_window = &platform->tool_windows[index];
        if (!tool_window->open)
        {
            continue;
        }
        tool_window->mouse_left_down = false;
        tool_window->mouse_left_pressed = false;
        tool_window->mouse_left_released = false;
        tool_window->mouse_wheel_delta = (henka_vec2){0.0f, 0.0f};
    }
}

static void henka_platform_apply_window_icon(SDL_Window* window)
{
    const char* base_path;
    SDL_Surface* icon;
    char icon_path[4096];
    size_t base_length;
    int written;

    if (window == NULL)
    {
        return;
    }

    base_path = SDL_GetBasePath();
    if (base_path == NULL)
    {
        HENKA_LOG_WARN("official window icon could not resolve the executable base path");
        return;
    }

    base_length = SDL_strlen(base_path);
    written = SDL_snprintf(
        icon_path,
        sizeof(icon_path),
        "%s%sassets/branding/henka_engine_emblem.png",
        base_path,
        base_length > 0U &&
                (base_path[base_length - 1U] == '/' || base_path[base_length - 1U] == '\\')
            ? ""
            : "/");
    if (written <= 0 || (size_t)written >= sizeof(icon_path))
    {
        HENKA_LOG_WARN("official window icon path exceeded the supported limit");
        return;
    }

    icon = SDL_LoadPNG(icon_path);
    if (icon == NULL)
    {
        HENKA_LOG_WARN("official window icon resource is unavailable; using the platform default");
        return;
    }

    if (!SDL_SetWindowIcon(window, icon))
    {
        HENKA_LOG_WARN("official window icon could not be applied; using the platform default");
    }
    SDL_DestroySurface(icon);
}

char* henka_platform_get_base_path_copy(void)
{
    char* copy;
    const char* sdl_base_path;
    size_t length;

    sdl_base_path = SDL_GetBasePath();
    if (sdl_base_path == NULL)
    {
        HENKA_LOG_ERROR("SDL_GetBasePath failed: %s", SDL_GetError());
        return NULL;
    }

    length = SDL_strlen(sdl_base_path);
    copy = henka_malloc(length + 1U);
    if (copy == NULL)
    {
        return NULL;
    }

    SDL_memcpy(copy, sdl_base_path, length + 1U);
    return copy;
}

henka_result henka_platform_create_directory_tree(const char* path)
{
    if (path == NULL || path[0] == '\0')
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    if (!SDL_CreateDirectory(path))
    {
        HENKA_LOG_ERROR("SDL_CreateDirectory failed for '%s': %s", path, SDL_GetError());
        return HENKA_ERROR_PLATFORM;
    }

    return HENKA_SUCCESS;
}

static henka_key henka_translate_key(SDL_Keycode keycode)
{
    switch (keycode)
    {
        case SDLK_ESCAPE:
            return HENKA_KEY_ESCAPE;
        case SDLK_F:
            return HENKA_KEY_F;
        case SDLK_W:
            return HENKA_KEY_W;
        case SDLK_A:
            return HENKA_KEY_A;
        case SDLK_S:
            return HENKA_KEY_S;
        case SDLK_D:
            return HENKA_KEY_D;
        case SDLK_Q:
            return HENKA_KEY_Q;
        case SDLK_E:
            return HENKA_KEY_E;
        case SDLK_G:
            return HENKA_KEY_G;
        case SDLK_R:
            return HENKA_KEY_R;
        case SDLK_M:
            return HENKA_KEY_M;
        case SDLK_X:
            return HENKA_KEY_X;
        case SDLK_Y:
            return HENKA_KEY_Y;
        case SDLK_Z:
            return HENKA_KEY_Z;
        case SDLK_RETURN:
            return HENKA_KEY_ENTER;
        case SDLK_LCTRL:
            return HENKA_KEY_LEFT_CTRL;
        case SDLK_LALT:
            return HENKA_KEY_LEFT_ALT;
        case SDLK_LSHIFT:
            return HENKA_KEY_LEFT_SHIFT;
        case SDLK_HOME:
            return HENKA_KEY_HOME;
        case SDLK_UP:
            return HENKA_KEY_UP;
        case SDLK_DOWN:
            return HENKA_KEY_DOWN;
        case SDLK_LEFT:
            return HENKA_KEY_LEFT;
        case SDLK_RIGHT:
            return HENKA_KEY_RIGHT;
        case SDLK_TAB:
            return HENKA_KEY_TAB;
        case SDLK_F1:
            return HENKA_KEY_F1;
        case SDLK_F2:
            return HENKA_KEY_F2;
        case SDLK_F3:
            return HENKA_KEY_F3;
        case SDLK_F4:
            return HENKA_KEY_F4;
        case SDLK_F5:
            return HENKA_KEY_F5;
        case SDLK_H:
            return HENKA_KEY_H;
        case SDLK_BACKSPACE:
            return HENKA_KEY_BACKSPACE;
        default:
            return HENKA_KEY_UNKNOWN;
    }
}

static henka_mouse_button henka_translate_mouse_button(Uint8 button)
{
    switch (button)
    {
        case SDL_BUTTON_LEFT:
            return HENKA_MOUSE_BUTTON_LEFT;
        case SDL_BUTTON_RIGHT:
            return HENKA_MOUSE_BUTTON_RIGHT;
        case SDL_BUTTON_MIDDLE:
            return HENKA_MOUSE_BUTTON_MIDDLE;
        default:
            return HENKA_MOUSE_BUTTON_UNKNOWN;
    }
}

henka_result henka_platform_create(
    const henka_platform_desc* desc,
    struct henka_platform** out_platform)
{
    struct henka_platform* platform;
    Uint64 window_flags;

    if (out_platform != NULL)
    {
        *out_platform = NULL;
    }

    if (desc == NULL || out_platform == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    if (!SDL_InitSubSystem(SDL_INIT_VIDEO))
    {
        HENKA_LOG_ERROR(
            "SDL video initialization failed: %s",
            SDL_GetError());
        return HENKA_ERROR_PLATFORM;
    }

    HENKA_LOG_INFO("platform video subsystem reference acquired");

    platform = henka_calloc(1U, sizeof(*platform));
    if (platform == NULL)
    {
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return HENKA_ERROR_OUT_OF_MEMORY;
    }

    window_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;
    platform->window = SDL_CreateWindow(
        desc->application_name,
        desc->window_width,
        desc->window_height,
        window_flags);
    if (platform->window == NULL)
    {
        HENKA_LOG_ERROR(
            "SDL_CreateWindow failed: %s",
            SDL_GetError());
        henka_free(platform);
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return HENKA_ERROR_PLATFORM;
    }

    if (!SDL_StartTextInput(platform->window))
    {
        HENKA_LOG_WARN("SDL text input could not be started; editor text events may be unavailable");
    }

    henka_platform_apply_window_icon(platform->window);

    platform->main_window_id = SDL_GetWindowID(platform->window);
    if (platform->main_window_id == 0U)
    {
        HENKA_LOG_ERROR(
            "SDL_GetWindowID failed for the main window: %s",
            SDL_GetError());
        SDL_DestroyWindow(platform->window);
        henka_free(platform);
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return HENKA_ERROR_PLATFORM;
    }

    platform->multi_window_available = true;
    platform->horizontal_resize_cursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_EW_RESIZE);
    platform->vertical_resize_cursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NS_RESIZE);
    platform->cursor_shape = HENKA_CURSOR_DEFAULT;
    if (platform->horizontal_resize_cursor == NULL || platform->vertical_resize_cursor == NULL)
    {
        HENKA_LOG_WARN("system resize cursor creation failed; resize cursor feedback may use the default cursor");
    }
    platform->main_window_focused =
        (SDL_GetWindowFlags(platform->window) &
            SDL_WINDOW_INPUT_FOCUS) != 0U;
    platform->next_tool_window_id = 2U;
    platform->last_event_route = HENKA_WINDOW_EVENT_ROUTE_NONE;
    platform->last_tool_window_id = HENKA_INVALID_WINDOW_ID;
    HENKA_LOG_INFO("platform window created");

    *out_platform = platform;
    return HENKA_SUCCESS;
}

void henka_platform_destroy(struct henka_platform* platform)
{
    size_t index;

    if (platform == NULL)
    {
        return;
    }

    HENKA_LOG_INFO("destroying platform");

    for (index = 0U; index < HENKA_MAX_TOOL_WINDOWS; ++index)
    {
        if (platform->tool_windows[index].window != NULL)
        {
            SDL_DestroyWindow(platform->tool_windows[index].window);
            platform->tool_windows[index].window = NULL;
            platform->tool_windows[index].open = false;
        }
    }

    if (platform->window != NULL)
    {
        (void)SDL_StopTextInput(platform->window);
        SDL_DestroyWindow(platform->window);
    }

    if (platform->horizontal_resize_cursor != NULL)
    {
        SDL_DestroyCursor(platform->horizontal_resize_cursor);
    }
    if (platform->vertical_resize_cursor != NULL)
    {
        SDL_DestroyCursor(platform->vertical_resize_cursor);
    }

    henka_free(platform);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

void henka_platform_set_multi_window_available(
    struct henka_platform* platform,
    bool available)
{
    if (platform != NULL)
    {
        platform->multi_window_available = available;
    }
}

henka_result henka_platform_create_tool_window(
    struct henka_platform* platform,
    const henka_tool_window_desc* desc,
    henka_window_id* out_window_id)
{
    henka_window_id chosen_id;
    henka_window_id next_candidate;
    henka_window_id occupied_ids[HENKA_MAX_TOOL_WINDOWS];
    size_t occupied_count;
    henka_platform_tool_window* slot;
    size_t index;

    if (out_window_id != NULL)
    {
        *out_window_id = HENKA_INVALID_WINDOW_ID;
    }

    if (platform == NULL || desc == NULL || out_window_id == NULL ||
        desc->title == NULL || desc->title[0] == '\0' ||
        desc->width <= 0 || desc->height <= 0 ||
        desc->minimum_width <= 0 || desc->minimum_height <= 0)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    slot = NULL;
    occupied_count = 0U;
    for (index = 0U; index < HENKA_MAX_TOOL_WINDOWS; ++index)
    {
        if (platform->tool_windows[index].open)
        {
            occupied_ids[occupied_count++] =
                platform->tool_windows[index].id;
            continue;
        }

        if (slot == NULL)
        {
            slot = &platform->tool_windows[index];
        }
    }

    if (slot == NULL)
    {
        return HENKA_ERROR_PLATFORM;
    }

    if (!henka_platform_choose_tool_window_id(
            platform->next_tool_window_id,
            occupied_ids,
            occupied_count,
            &chosen_id,
            &next_candidate))
    {
        return HENKA_ERROR_PLATFORM;
    }

    memset(slot, 0, sizeof(*slot));
    slot->window = SDL_CreateWindow(
        desc->title,
        desc->width,
        desc->height,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (slot->window == NULL)
    {
        platform->multi_window_available = false;
        HENKA_LOG_ERROR(
            "SDL_CreateWindow failed for tool window: %s",
            SDL_GetError());
        return HENKA_ERROR_PLATFORM;
    }

    henka_platform_apply_window_icon(slot->window);

    slot->native_window_id = SDL_GetWindowID(slot->window);
    if (slot->native_window_id == 0U)
    {
        platform->multi_window_available = false;
        HENKA_LOG_ERROR(
            "SDL_GetWindowID failed for tool window: %s",
            SDL_GetError());
        SDL_DestroyWindow(slot->window);
        memset(slot, 0, sizeof(*slot));
        return HENKA_ERROR_PLATFORM;
    }

    if (!SDL_SetWindowMinimumSize(
            slot->window,
            desc->minimum_width,
            desc->minimum_height))
    {
        HENKA_LOG_WARN(
            "SDL_SetWindowMinimumSize failed for tool window: %s",
            SDL_GetError());
    }

    slot->id = chosen_id;
    slot->open = true;
    slot->focused =
        (SDL_GetWindowFlags(slot->window) &
            SDL_WINDOW_INPUT_FOCUS) != 0U;
    if (!SDL_GetWindowSizeInPixels(
            slot->window,
            &slot->width,
            &slot->height))
    {
        slot->width = desc->width;
        slot->height = desc->height;
    }
    if (!SDL_GetWindowPosition(slot->window, &slot->position_x, &slot->position_y))
    {
        slot->position_x = 0;
        slot->position_y = 0;
    }

    platform->next_tool_window_id = next_candidate;
    platform->multi_window_available = true;
    snprintf(slot->last_event, sizeof(slot->last_event), "opened");
    platform->last_event_route = HENKA_WINDOW_EVENT_ROUTE_TOOL;
    platform->last_tool_window_id = slot->id;
    platform->last_tool_window_close_requested = false;
    platform->last_tool_window_resized = false;
    *out_window_id = slot->id;
    HENKA_LOG_INFO(
        "tool window created (engine id=%u, native id=%u)",
        (unsigned int)slot->id,
        (unsigned int)slot->native_window_id);
    return HENKA_SUCCESS;
}

void henka_platform_destroy_tool_window(struct henka_platform* platform, henka_window_id window_id)
{
    henka_platform_tool_window* slot;

    slot = henka_platform_find_tool_window(platform, window_id);
    if (slot == NULL)
    {
        return;
    }

    if (slot->window != NULL)
    {
        SDL_DestroyWindow(slot->window);
    }
    slot->window = NULL;
    slot->open = false;
    slot->focused = false;
    slot->close_requested = false;
    slot->resized = false;
    snprintf(slot->last_event, sizeof(slot->last_event), "closed");
    HENKA_LOG_INFO("tool window closed (engine id=%u)", (unsigned int)window_id);
}

bool henka_platform_get_tool_window_state(
    const struct henka_platform* platform,
    henka_window_id window_id,
    henka_tool_window_state* out_state)
{
    const henka_platform_tool_window* slot;

    if (out_state != NULL)
    {
        memset(out_state, 0, sizeof(*out_state));
    }

    if (platform == NULL || out_state == NULL)
    {
        return false;
    }

    slot = henka_platform_find_tool_window_const(platform, window_id);
    if (slot == NULL)
    {
        return false;
    }

    out_state->id = slot->id;
    out_state->native_window_id = (uint32_t)slot->native_window_id;
    out_state->open = slot->open;
    out_state->focused = slot->focused;
    out_state->width = slot->width;
    out_state->height = slot->height;
    out_state->position_x = slot->position_x;
    out_state->position_y = slot->position_y;
    out_state->mouse_position = slot->mouse_position;
    out_state->mouse_left_down = slot->mouse_left_down;
    out_state->mouse_left_pressed = slot->mouse_left_pressed;
    out_state->mouse_left_released = slot->mouse_left_released;
    out_state->mouse_wheel_delta = slot->mouse_wheel_delta;
    out_state->close_requested = slot->close_requested;
    out_state->resized = slot->resized;
    snprintf(
        out_state->last_event,
        sizeof(out_state->last_event),
        "%s",
        slot->last_event);
    return true;
}

bool henka_platform_set_tool_window_position(
    struct henka_platform* platform,
    henka_window_id window_id,
    int position_x,
    int position_y)
{
    henka_platform_tool_window* slot = henka_platform_find_tool_window(platform, window_id);
    if (slot == NULL || !SDL_SetWindowPosition(slot->window, position_x, position_y))
    {
        return false;
    }
    slot->position_x = position_x;
    slot->position_y = position_y;
    return true;
}

henka_result henka_platform_set_cursor(struct henka_platform* platform, henka_cursor_shape shape)
{
    SDL_Cursor* cursor;

    if (platform == NULL || shape < HENKA_CURSOR_DEFAULT || shape > HENKA_CURSOR_RESIZE_VERTICAL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (platform->cursor_shape == shape)
    {
        return HENKA_SUCCESS;
    }

    cursor = shape == HENKA_CURSOR_RESIZE_HORIZONTAL
        ? platform->horizontal_resize_cursor
        : shape == HENKA_CURSOR_RESIZE_VERTICAL
            ? platform->vertical_resize_cursor
            : SDL_GetDefaultCursor();
    if (cursor == NULL || !SDL_SetCursor(cursor))
    {
        return HENKA_ERROR_PLATFORM;
    }
    platform->cursor_shape = shape;
    return HENKA_SUCCESS;
}

void henka_platform_get_diagnostics(
    const struct henka_platform* platform,
    henka_platform_diagnostics* out_diagnostics)
{
    size_t index;

    if (out_diagnostics == NULL)
    {
        return;
    }

    memset(out_diagnostics, 0, sizeof(*out_diagnostics));
    if (platform == NULL)
    {
        return;
    }

    out_diagnostics->multi_window_available =
        platform->multi_window_available;
    out_diagnostics->main_window_focused =
        platform->main_window_focused;
    out_diagnostics->last_event_route =
        platform->last_event_route;
    out_diagnostics->last_tool_window_id =
        platform->last_tool_window_id;
    out_diagnostics->last_tool_window_close_requested =
        platform->last_tool_window_close_requested;
    out_diagnostics->last_tool_window_resized =
        platform->last_tool_window_resized;
    for (index = 0U; index < HENKA_MAX_TOOL_WINDOWS; ++index)
    {
        if (platform->tool_windows[index].open)
        {
            out_diagnostics->open_tool_window_count += 1U;
        }
    }
}

void* henka_platform_get_native_tool_window(struct henka_platform* platform, henka_window_id window_id)
{
    henka_platform_tool_window* slot = henka_platform_find_tool_window(platform, window_id);
    return slot != NULL ? slot->window : NULL;
}

henka_result henka_platform_poll_events(struct henka_platform* platform, henka_input_state* input, henka_platform_frame_state* out_state)
{
    SDL_Event event;

    if (out_state != NULL)
    {
        memset(out_state, 0, sizeof(*out_state));
    }

    if (platform == NULL || input == NULL || out_state == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    platform->last_event_route = HENKA_WINDOW_EVENT_ROUTE_NONE;
    platform->last_tool_window_id = HENKA_INVALID_WINDOW_ID;
    platform->last_tool_window_close_requested = false;
    platform->last_tool_window_resized = false;
    henka_platform_reset_tool_window_frame_input(platform);
    if (input->automation_input_owned)
    {
        henka_platform_clear_tool_window_input_for_automation(platform);
    }

    while (SDL_PollEvent(&event))
    {
        switch (event.type)
        {
            case SDL_EVENT_QUIT:
                input->close_requested = true;
                out_state->close_requested = true;
                platform->last_event_route = HENKA_WINDOW_EVENT_ROUTE_MAIN;
                break;

            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            {
                henka_platform_tool_window* tool_window;

                if (henka_platform_event_is_main_window(platform, event.window.windowID))
                {
                    input->close_requested = true;
                    out_state->close_requested = true;
                    platform->last_event_route = HENKA_WINDOW_EVENT_ROUTE_MAIN;
                }
                else if ((tool_window = henka_platform_find_tool_window_by_native_id(platform, event.window.windowID)) != NULL)
                {
                    henka_platform_record_tool_event(
                        platform,
                        tool_window,
                        "close requested",
                        true,
                        false);
                    tool_window->close_requested = true;
                }
                else
                {
                    platform->last_event_route = HENKA_WINDOW_EVENT_ROUTE_UNKNOWN;
                }
                break;
            }

            case SDL_EVENT_WINDOW_RESIZED:
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            {
                henka_platform_tool_window* tool_window;

                if (henka_platform_event_is_main_window(platform, event.window.windowID))
                {
                    out_state->resized = true;
                    out_state->framebuffer_width = event.window.data1;
                    out_state->framebuffer_height = event.window.data2;
                    platform->last_event_route = HENKA_WINDOW_EVENT_ROUTE_MAIN;
                }
                else if ((tool_window = henka_platform_find_tool_window_by_native_id(platform, event.window.windowID)) != NULL)
                {
                    henka_platform_record_tool_event(
                        platform,
                        tool_window,
                        "resized",
                        false,
                        true);
                    tool_window->width = event.window.data1;
                    tool_window->height = event.window.data2;
                    tool_window->resized = true;
                }
                else
                {
                    platform->last_event_route = HENKA_WINDOW_EVENT_ROUTE_UNKNOWN;
                }
                break;
            }

            case SDL_EVENT_WINDOW_FOCUS_GAINED:
            case SDL_EVENT_WINDOW_FOCUS_LOST:
            {
                henka_platform_tool_window* tool_window;
                const bool focused = event.type == SDL_EVENT_WINDOW_FOCUS_GAINED;

                if (henka_platform_event_is_main_window(platform, event.window.windowID))
                {
                    platform->main_window_focused = focused;
                    platform->last_event_route =
                        HENKA_WINDOW_EVENT_ROUTE_MAIN;
                    if (!focused)
                    {
                        henka_platform_release_input_on_focus_loss(input);
                    }
                }
                else if ((tool_window = henka_platform_find_tool_window_by_native_id(platform, event.window.windowID)) != NULL)
                {
                    henka_platform_record_tool_event(
                        platform,
                        tool_window,
                        focused ? "focused" : "focus lost",
                        false,
                        false);
                    tool_window->focused = focused;
                    if (!focused && tool_window->mouse_left_down)
                    {
                        tool_window->mouse_left_down = false;
                        tool_window->mouse_left_pressed = false;
                        tool_window->mouse_left_released = true;
                    }
                }
                else
                {
                    platform->last_event_route = HENKA_WINDOW_EVENT_ROUTE_UNKNOWN;
                }
                break;
            }

            case SDL_EVENT_WINDOW_MOVED:
            {
                henka_platform_tool_window* tool_window;
                if (henka_platform_event_is_main_window(platform, event.window.windowID))
                {
                    platform->last_event_route = HENKA_WINDOW_EVENT_ROUTE_MAIN;
                }
                else if ((tool_window = henka_platform_find_tool_window_by_native_id(platform, event.window.windowID)) != NULL)
                {
                    henka_platform_record_tool_event(platform, tool_window, "moved", false, false);
                    tool_window->position_x = event.window.data1;
                    tool_window->position_y = event.window.data2;
                }
                else
                {
                    platform->last_event_route = HENKA_WINDOW_EVENT_ROUTE_UNKNOWN;
                }
                break;
            }

            case SDL_EVENT_KEY_DOWN:
            {
                henka_key key;
                henka_platform_tool_window* tool_window;

                if (!henka_platform_event_is_main_window(platform, event.key.windowID))
                {
                    tool_window = henka_platform_find_tool_window_by_native_id(platform, event.key.windowID);
                    if (tool_window != NULL)
                    {
                        if (input->automation_input_owned)
                        {
                            break;
                        }
                        henka_platform_record_tool_event(platform, tool_window, "key pressed", false, false);
                    }
                    else
                    {
                        platform->last_event_route = HENKA_WINDOW_EVENT_ROUTE_UNKNOWN;
                    }
                    break;
                }
                platform->last_event_route = HENKA_WINDOW_EVENT_ROUTE_MAIN;
                key = henka_translate_key(event.key.key);
                if (input->automation_input_owned)
                {
                    if (key == HENKA_KEY_ESCAPE)
                    {
                        input->close_requested = true;
                    }
                    break;
                }
                if (key != HENKA_KEY_UNKNOWN && !event.key.repeat)
                {
                    input->keys_down[key] = true;
                    input->keys_pressed[key] = true;
                }
                break;
            }

            case SDL_EVENT_TEXT_INPUT:
                if (!henka_platform_event_is_main_window(
                        platform, event.text.windowID))
                {
                    platform->last_event_route =
                        HENKA_WINDOW_EVENT_ROUTE_UNKNOWN;
                    break;
                }
                platform->last_event_route = HENKA_WINDOW_EVENT_ROUTE_MAIN;
                if (!input->automation_input_owned &&
                    !henka_platform_append_text_input(input, event.text.text))
                {
                    if (input->text_input_overflowed)
                    {
                        HENKA_LOG_WARN("frame text input exceeded the bounded editor input buffer");
                    }
                }
                break;

            case SDL_EVENT_KEY_UP:
            {
                henka_key key;
                henka_platform_tool_window* tool_window;

                if (!henka_platform_event_is_main_window(platform, event.key.windowID))
                {
                    tool_window = henka_platform_find_tool_window_by_native_id(platform, event.key.windowID);
                    if (tool_window != NULL)
                    {
                        if (input->automation_input_owned)
                        {
                            break;
                        }
                        henka_platform_record_tool_event(platform, tool_window, "key released", false, false);
                    }
                    else
                    {
                        platform->last_event_route = HENKA_WINDOW_EVENT_ROUTE_UNKNOWN;
                    }
                    break;
                }
                platform->last_event_route = HENKA_WINDOW_EVENT_ROUTE_MAIN;
                key = henka_translate_key(event.key.key);
                if (input->automation_input_owned)
                {
                    break;
                }
                if (key != HENKA_KEY_UNKNOWN)
                {
                    input->keys_down[key] = false;
                    input->keys_released[key] = true;
                }
                break;
            }

            case SDL_EVENT_MOUSE_MOTION:
                if (!henka_platform_event_is_main_window(platform, event.motion.windowID))
                {
                    henka_platform_tool_window* tool_window = henka_platform_find_tool_window_by_native_id(platform, event.motion.windowID);
                    if (tool_window != NULL)
                    {
                        if (input->automation_input_owned)
                        {
                            break;
                        }
                        henka_platform_record_tool_event(platform, tool_window, "pointer moved", false, false);
                        tool_window->mouse_position.x = event.motion.x;
                        tool_window->mouse_position.y = event.motion.y;
                    }
                    else
                    {
                        platform->last_event_route = HENKA_WINDOW_EVENT_ROUTE_UNKNOWN;
                    }
                    break;
                }
                platform->last_event_route = HENKA_WINDOW_EVENT_ROUTE_MAIN;
                if (input->automation_input_owned)
                {
                    break;
                }
                input->mouse_position.x = event.motion.x;
                input->mouse_position.y = event.motion.y;
                input->mouse_delta.x += event.motion.xrel;
                input->mouse_delta.y += event.motion.yrel;
                break;

            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            {
                henka_mouse_button button;
                henka_platform_tool_window* tool_window;

                if (!henka_platform_event_is_main_window(platform, event.button.windowID))
                {
                    tool_window = henka_platform_find_tool_window_by_native_id(platform, event.button.windowID);
                    if (tool_window != NULL)
                    {
                        if (input->automation_input_owned)
                        {
                            break;
                        }
                        button = henka_translate_mouse_button(event.button.button);
                        henka_platform_record_tool_event(platform, tool_window, "button pressed", false, false);
                        tool_window->mouse_position.x = event.button.x;
                        tool_window->mouse_position.y = event.button.y;
                        if (button == HENKA_MOUSE_BUTTON_LEFT)
                        {
                            tool_window->mouse_left_down = true;
                            tool_window->mouse_left_pressed = true;
                        }
                    }
                    else
                    {
                        platform->last_event_route = HENKA_WINDOW_EVENT_ROUTE_UNKNOWN;
                    }
                    break;
                }
                platform->last_event_route = HENKA_WINDOW_EVENT_ROUTE_MAIN;
                if (input->automation_input_owned)
                {
                    break;
                }
                button = henka_translate_mouse_button(event.button.button);
                input->mouse_position.x = event.button.x;
                input->mouse_position.y = event.button.y;
                if (button != HENKA_MOUSE_BUTTON_UNKNOWN)
                {
                    input->mouse_buttons_down[button] = true;
                    input->mouse_buttons_pressed[button] = true;
                }
                break;
            }

            case SDL_EVENT_MOUSE_BUTTON_UP:
            {
                henka_mouse_button button;
                henka_platform_tool_window* tool_window;

                if (!henka_platform_event_is_main_window(platform, event.button.windowID))
                {
                    tool_window = henka_platform_find_tool_window_by_native_id(platform, event.button.windowID);
                    if (tool_window != NULL)
                    {
                        if (input->automation_input_owned)
                        {
                            break;
                        }
                        button = henka_translate_mouse_button(event.button.button);
                        henka_platform_record_tool_event(platform, tool_window, "button released", false, false);
                        tool_window->mouse_position.x = event.button.x;
                        tool_window->mouse_position.y = event.button.y;
                        if (button == HENKA_MOUSE_BUTTON_LEFT)
                        {
                            tool_window->mouse_left_down = false;
                            tool_window->mouse_left_released = true;
                        }
                    }
                    else
                    {
                        platform->last_event_route = HENKA_WINDOW_EVENT_ROUTE_UNKNOWN;
                    }
                    break;
                }
                platform->last_event_route = HENKA_WINDOW_EVENT_ROUTE_MAIN;
                if (input->automation_input_owned)
                {
                    break;
                }
                button = henka_translate_mouse_button(event.button.button);
                input->mouse_position.x = event.button.x;
                input->mouse_position.y = event.button.y;
                if (button != HENKA_MOUSE_BUTTON_UNKNOWN)
                {
                    input->mouse_buttons_down[button] = false;
                    input->mouse_buttons_released[button] = true;
                }
                break;
            }

            case SDL_EVENT_MOUSE_WHEEL:
                if (!henka_platform_event_is_main_window(platform, event.wheel.windowID))
                {
                    henka_platform_tool_window* tool_window = henka_platform_find_tool_window_by_native_id(platform, event.wheel.windowID);
                    if (tool_window != NULL)
                    {
                        if (input->automation_input_owned)
                        {
                            break;
                        }
                        henka_platform_record_tool_event(platform, tool_window, "wheel", false, false);
                        tool_window->mouse_wheel_delta.x += event.wheel.x;
                        tool_window->mouse_wheel_delta.y += event.wheel.y;
                    }
                    else
                    {
                        platform->last_event_route = HENKA_WINDOW_EVENT_ROUTE_UNKNOWN;
                    }
                    break;
                }
                platform->last_event_route = HENKA_WINDOW_EVENT_ROUTE_MAIN;
                if (input->automation_input_owned)
                {
                    break;
                }
                input->mouse_wheel_delta.x += event.wheel.x;
                input->mouse_wheel_delta.y += event.wheel.y;
                break;

            default:
                break;
        }
    }

    henka_platform_poll_automation_event(input);
    out_state->close_requested = out_state->close_requested || input->close_requested;

    return HENKA_SUCCESS;
}

henka_result henka_platform_set_vsync(
    struct henka_platform* platform,
    bool enabled)
{
    int interval;

    if (platform == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    interval = enabled ? 1 : 0;
    if (!SDL_GL_SetSwapInterval(interval))
    {
        HENKA_LOG_WARN(
            "SDL_GL_SetSwapInterval failed: %s",
            SDL_GetError());
        return HENKA_ERROR_PLATFORM;
    }

    return HENKA_SUCCESS;
}

bool henka_platform_get_framebuffer_size(
    struct henka_platform* platform,
    int* out_width,
    int* out_height)
{
    int height;
    int width;

    if (out_width != NULL)
    {
        *out_width = 0;
    }
    if (out_height != NULL)
    {
        *out_height = 0;
    }

    if (platform == NULL || out_width == NULL || out_height == NULL)
    {
        return false;
    }

    if (!SDL_GetWindowSizeInPixels(
            platform->window,
            &width,
            &height))
    {
        return false;
    }

    *out_width = width;
    *out_height = height;
    return true;
}

bool henka_platform_get_window_size(
    struct henka_platform* platform,
    int* out_width,
    int* out_height)
{
    int height;
    int width;

    if (out_width != NULL)
    {
        *out_width = 0;
    }
    if (out_height != NULL)
    {
        *out_height = 0;
    }

    if (platform == NULL || out_width == NULL || out_height == NULL)
    {
        return false;
    }

    if (!SDL_GetWindowSize(platform->window, &width, &height))
    {
        return false;
    }

    *out_width = width;
    *out_height = height;
    return true;
}

bool henka_platform_get_window_position(
    struct henka_platform* platform,
    int* out_x,
    int* out_y)
{
    if (out_x != NULL)
    {
        *out_x = 0;
    }
    if (out_y != NULL)
    {
        *out_y = 0;
    }
    if (platform == NULL || platform->window == NULL || out_x == NULL || out_y == NULL)
    {
        return false;
    }
    SDL_GetWindowPosition(platform->window, out_x, out_y);
    return true;
}

henka_result henka_platform_set_mouse_capture(
    struct henka_platform* platform,
    bool enabled)
{
    bool previous_mouse_grab;
    bool previous_relative_mode;

    if (platform == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    previous_relative_mode =
        SDL_GetWindowRelativeMouseMode(platform->window);
    previous_mouse_grab =
        SDL_GetWindowMouseGrab(platform->window);
    if (previous_relative_mode == enabled &&
        previous_mouse_grab == enabled)
    {
        return HENKA_SUCCESS;
    }

    if (!SDL_SetWindowRelativeMouseMode(platform->window, enabled))
    {
        HENKA_LOG_ERROR(
            "SDL_SetWindowRelativeMouseMode failed: %s",
            SDL_GetError());
        return HENKA_ERROR_PLATFORM;
    }

    if (!SDL_SetWindowMouseGrab(platform->window, enabled))
    {
        HENKA_LOG_ERROR(
            "SDL_SetWindowMouseGrab failed: %s",
            SDL_GetError());
        if (!SDL_SetWindowMouseGrab(
                platform->window,
                previous_mouse_grab))
        {
            HENKA_LOG_ERROR(
                "SDL_SetWindowMouseGrab rollback failed: %s",
                SDL_GetError());
        }
        if (!SDL_SetWindowRelativeMouseMode(
                platform->window,
                previous_relative_mode))
        {
            HENKA_LOG_ERROR(
                "SDL_SetWindowRelativeMouseMode rollback failed: %s",
                SDL_GetError());
        }
        return HENKA_ERROR_PLATFORM;
    }

    return HENKA_SUCCESS;
}

SDL_Window* henka_platform_get_sdl_window(struct henka_platform* platform);

SDL_Window* henka_platform_get_sdl_window(struct henka_platform* platform)
{
    if (platform == NULL)
    {
        return NULL;
    }

    return platform->window;
}
