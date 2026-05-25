#include "henka_internal.h"

#include <SDL3/SDL.h>

#include <henka/log.h>
#include <henka/memory.h>

struct henka_platform
{
    SDL_Window* window;
};

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
        case SDLK_LSHIFT:
            return HENKA_KEY_LEFT_SHIFT;
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

    HENKA_LOG_INFO("platform window created");

    *out_platform = platform;
    return HENKA_SUCCESS;
}

void henka_platform_destroy(struct henka_platform* platform)
{
    if (platform == NULL)
    {
        return;
    }

    HENKA_LOG_INFO("destroying platform");

    if (platform->window != NULL)
    {
        SDL_DestroyWindow(platform->window);
    }

    henka_free(platform);
    SDL_Quit();
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
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                input->close_requested = true;
                out_state->close_requested = true;
                break;

            case SDL_EVENT_WINDOW_RESIZED:
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                out_state->resized = true;
                out_state->framebuffer_width = event.window.data1;
                out_state->framebuffer_height = event.window.data2;
                break;

            case SDL_EVENT_KEY_DOWN:
            {
                henka_key key;

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

                key = henka_translate_key(event.key.key);
                if (key != HENKA_KEY_UNKNOWN)
                {
                    input->keys_down[key] = false;
                }
                break;
            }

            case SDL_EVENT_MOUSE_MOTION:
                input->mouse_position.x = event.motion.x;
                input->mouse_position.y = event.motion.y;
                input->mouse_delta.x += event.motion.xrel;
                input->mouse_delta.y += event.motion.yrel;
                break;

            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            {
                henka_mouse_button button;

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
