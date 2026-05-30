#include "henka_internal.h"

#include <SDL3/SDL.h>

#include <stdio.h>
#include <string.h>

#include <henka/log.h>
#include <henka/memory.h>

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
    char last_event[48];
} henka_platform_tool_window;

struct henka_platform
{
    SDL_Window* window;
    SDL_WindowID main_window_id;
    bool main_window_focused;
    henka_window_id next_tool_window_id;
    henka_platform_tool_window tool_windows[HENKA_MAX_TOOL_WINDOWS];
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
    const char* event_name)
{
    if (platform == NULL || tool_window == NULL || event_name == NULL)
    {
        return;
    }

    platform->last_event_route = HENKA_WINDOW_EVENT_ROUTE_TOOL;
    platform->last_tool_window_id = tool_window->id;
    platform->last_tool_window_close_requested = false;
    platform->last_tool_window_resized = false;
    snprintf(tool_window->last_event, sizeof(tool_window->last_event), "%s", event_name);
}

static bool henka_platform_event_is_main_window(const struct henka_platform* platform, SDL_WindowID window_id)
{
    return platform != NULL && window_id != 0U && platform->main_window_id == window_id;
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

henka_result henka_platform_create(const henka_platform_desc* desc, struct henka_platform** out_platform)
{
    struct henka_platform* platform;
    Uint64 window_flags;

    if (desc == NULL || out_platform == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    *out_platform = NULL;

    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        HENKA_LOG_ERROR("SDL_Init failed: %s", SDL_GetError());
        return HENKA_ERROR_PLATFORM;
    }

    HENKA_LOG_INFO("platform video subsystem initialized");

    platform = henka_calloc(1U, sizeof(*platform));
    if (platform == NULL)
    {
        SDL_Quit();
        return HENKA_ERROR_OUT_OF_MEMORY;
    }

    window_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;
    platform->window = SDL_CreateWindow(desc->application_name, desc->window_width, desc->window_height, window_flags);
    if (platform->window == NULL)
    {
        HENKA_LOG_ERROR("SDL_CreateWindow failed: %s", SDL_GetError());
        henka_free(platform);
        SDL_Quit();
        return HENKA_ERROR_PLATFORM;
    }

    platform->main_window_id = SDL_GetWindowID(platform->window);
    platform->main_window_focused = true;
    platform->next_tool_window_id = 2U;
    platform->last_event_route = HENKA_WINDOW_EVENT_ROUTE_NONE;
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
        SDL_DestroyWindow(platform->window);
    }

    henka_free(platform);
    SDL_Quit();
}

henka_result henka_platform_create_tool_window(
    struct henka_platform* platform,
    const henka_tool_window_desc* desc,
    henka_window_id* out_window_id)
{
    henka_platform_tool_window* slot;
    size_t index;

    if (platform == NULL || desc == NULL || out_window_id == NULL ||
        desc->title == NULL || desc->title[0] == '\0' ||
        desc->width <= 0 || desc->height <= 0 ||
        desc->minimum_width <= 0 || desc->minimum_height <= 0)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    *out_window_id = HENKA_INVALID_WINDOW_ID;
    slot = NULL;
    for (index = 0U; index < HENKA_MAX_TOOL_WINDOWS; ++index)
    {
        if (!platform->tool_windows[index].open)
        {
            slot = &platform->tool_windows[index];
            break;
        }
    }
    if (slot == NULL)
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
        HENKA_LOG_ERROR("SDL_CreateWindow failed for tool window: %s", SDL_GetError());
        return HENKA_ERROR_PLATFORM;
    }
    if (!SDL_SetWindowMinimumSize(slot->window, desc->minimum_width, desc->minimum_height))
    {
        HENKA_LOG_WARN("SDL_SetWindowMinimumSize failed for tool window: %s", SDL_GetError());
    }

    slot->id = platform->next_tool_window_id++;
    slot->native_window_id = SDL_GetWindowID(slot->window);
    slot->open = true;
    slot->focused = false;
    if (!SDL_GetWindowSizeInPixels(slot->window, &slot->width, &slot->height))
    {
        slot->width = desc->width;
        slot->height = desc->height;
    }
    snprintf(slot->last_event, sizeof(slot->last_event), "opened");
    platform->last_event_route = HENKA_WINDOW_EVENT_ROUTE_TOOL;
    platform->last_tool_window_id = slot->id;
    platform->last_tool_window_close_requested = false;
    platform->last_tool_window_resized = false;
    *out_window_id = slot->id;
    HENKA_LOG_INFO("tool window created (engine id=%u, native id=%u)", (unsigned int)slot->id, (unsigned int)slot->native_window_id);
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

    if (platform == NULL || out_state == NULL)
    {
        return false;
    }

    slot = henka_platform_find_tool_window_const(platform, window_id);
    if (slot == NULL)
    {
        return false;
    }

    memset(out_state, 0, sizeof(*out_state));
    out_state->id = slot->id;
    out_state->native_window_id = (uint32_t)slot->native_window_id;
    out_state->open = slot->open;
    out_state->focused = slot->focused;
    out_state->width = slot->width;
    out_state->height = slot->height;
    out_state->close_requested = slot->close_requested;
    out_state->resized = slot->resized;
    snprintf(out_state->last_event, sizeof(out_state->last_event), "%s", slot->last_event);
    return true;
}

void henka_platform_get_diagnostics(const struct henka_platform* platform, henka_platform_diagnostics* out_diagnostics)
{
    size_t index;

    if (platform == NULL || out_diagnostics == NULL)
    {
        return;
    }

    memset(out_diagnostics, 0, sizeof(*out_diagnostics));
    out_diagnostics->main_window_focused = platform->main_window_focused;
    out_diagnostics->last_event_route = platform->last_event_route;
    out_diagnostics->last_tool_window_id = platform->last_tool_window_id;
    out_diagnostics->last_tool_window_close_requested = platform->last_tool_window_close_requested;
    out_diagnostics->last_tool_window_resized = platform->last_tool_window_resized;
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

    if (platform == NULL || input == NULL || out_state == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    out_state->close_requested = false;
    out_state->resized = false;
    out_state->framebuffer_width = 0;
    out_state->framebuffer_height = 0;

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
                    henka_platform_record_tool_event(platform, tool_window, "close requested");
                    tool_window->close_requested = true;
                    platform->last_tool_window_close_requested = true;
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
                    henka_platform_record_tool_event(platform, tool_window, "resized");
                    tool_window->width = event.window.data1;
                    tool_window->height = event.window.data2;
                    tool_window->resized = true;
                    platform->last_tool_window_resized = true;
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
                    platform->last_event_route = HENKA_WINDOW_EVENT_ROUTE_MAIN;
                }
                else if ((tool_window = henka_platform_find_tool_window_by_native_id(platform, event.window.windowID)) != NULL)
                {
                    henka_platform_record_tool_event(platform, tool_window, focused ? "focused" : "focus lost");
                    tool_window->focused = focused;
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
                    henka_platform_record_tool_event(platform, tool_window, "moved");
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
                        henka_platform_record_tool_event(platform, tool_window, "key pressed");
                    }
                    else
                    {
                        platform->last_event_route = HENKA_WINDOW_EVENT_ROUTE_UNKNOWN;
                    }
                    break;
                }
                platform->last_event_route = HENKA_WINDOW_EVENT_ROUTE_MAIN;
                key = henka_translate_key(event.key.key);
                if (key != HENKA_KEY_UNKNOWN && !event.key.repeat)
                {
                    input->keys_down[key] = true;
                    input->keys_pressed[key] = true;
                }
                break;
            }

            case SDL_EVENT_KEY_UP:
            {
                henka_key key;
                henka_platform_tool_window* tool_window;

                if (!henka_platform_event_is_main_window(platform, event.key.windowID))
                {
                    tool_window = henka_platform_find_tool_window_by_native_id(platform, event.key.windowID);
                    if (tool_window != NULL)
                    {
                        henka_platform_record_tool_event(platform, tool_window, "key released");
                    }
                    else
                    {
                        platform->last_event_route = HENKA_WINDOW_EVENT_ROUTE_UNKNOWN;
                    }
                    break;
                }
                platform->last_event_route = HENKA_WINDOW_EVENT_ROUTE_MAIN;
                key = henka_translate_key(event.key.key);
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
                        henka_platform_record_tool_event(platform, tool_window, "pointer moved");
                    }
                    else
                    {
                        platform->last_event_route = HENKA_WINDOW_EVENT_ROUTE_UNKNOWN;
                    }
                    break;
                }
                platform->last_event_route = HENKA_WINDOW_EVENT_ROUTE_MAIN;
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
                        henka_platform_record_tool_event(platform, tool_window, "button pressed");
                    }
                    else
                    {
                        platform->last_event_route = HENKA_WINDOW_EVENT_ROUTE_UNKNOWN;
                    }
                    break;
                }
                platform->last_event_route = HENKA_WINDOW_EVENT_ROUTE_MAIN;
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
                        henka_platform_record_tool_event(platform, tool_window, "button released");
                    }
                    else
                    {
                        platform->last_event_route = HENKA_WINDOW_EVENT_ROUTE_UNKNOWN;
                    }
                    break;
                }
                platform->last_event_route = HENKA_WINDOW_EVENT_ROUTE_MAIN;
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
                        henka_platform_record_tool_event(platform, tool_window, "wheel");
                    }
                    else
                    {
                        platform->last_event_route = HENKA_WINDOW_EVENT_ROUTE_UNKNOWN;
                    }
                    break;
                }
                platform->last_event_route = HENKA_WINDOW_EVENT_ROUTE_MAIN;
                input->mouse_wheel_delta.x += event.wheel.x;
                input->mouse_wheel_delta.y += event.wheel.y;
                break;

            default:
                break;
        }
    }

    return HENKA_SUCCESS;
}

henka_result henka_platform_set_vsync(struct henka_platform* platform, bool enabled)
{
    int interval;

    if (platform == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    interval = enabled ? 1 : 0;
    if (!SDL_GL_SetSwapInterval(interval))
    {
        HENKA_LOG_WARN("SDL_GL_SetSwapInterval failed: %s", SDL_GetError());
    }

    return HENKA_SUCCESS;
}

bool henka_platform_get_framebuffer_size(struct henka_platform* platform, int* out_width, int* out_height)
{
    if (platform == NULL || out_width == NULL || out_height == NULL)
    {
        return false;
    }

    return SDL_GetWindowSizeInPixels(platform->window, out_width, out_height);
}

bool henka_platform_get_window_size(struct henka_platform* platform, int* out_width, int* out_height)
{
    if (platform == NULL || out_width == NULL || out_height == NULL)
    {
        return false;
    }

    return SDL_GetWindowSize(platform->window, out_width, out_height);
}

henka_result henka_platform_set_mouse_capture(struct henka_platform* platform, bool enabled)
{
    if (platform == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    if (!SDL_SetWindowRelativeMouseMode(platform->window, enabled))
    {
        HENKA_LOG_ERROR("SDL_SetWindowRelativeMouseMode failed: %s", SDL_GetError());
        return HENKA_ERROR_PLATFORM;
    }

    if (!SDL_SetWindowMouseGrab(platform->window, enabled))
    {
        HENKA_LOG_WARN("SDL_SetWindowMouseGrab failed: %s", SDL_GetError());
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
