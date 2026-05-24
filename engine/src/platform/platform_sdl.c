#include "henka_internal.h"

#include <SDL3/SDL.h>

#include <henka/log.h>
#include <henka/memory.h>

struct henka_platform
{
    SDL_Window* window;
};

static henka_key henka_translate_key(SDL_Keycode keycode)
{
    switch (keycode)
    {
        case SDLK_ESCAPE:
            return HENKA_KEY_ESCAPE;
        default:
            return HENKA_KEY_UNKNOWN;
    }
}

henka_result henka_platform_create(const henka_platform_desc* desc, henka_platform** out_platform)
{
    henka_platform* platform;
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

void henka_platform_destroy(henka_platform* platform)
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

henka_result henka_platform_poll_events(henka_platform* platform, henka_input_state* input, henka_platform_frame_state* out_state)
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

            default:
                break;
        }
    }

    (void)platform;
    return HENKA_SUCCESS;
}

henka_result henka_platform_set_vsync(henka_platform* platform, bool enabled)
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
        return HENKA_SUCCESS;
    }

    return HENKA_SUCCESS;
}

SDL_Window* henka_platform_get_sdl_window(henka_platform* platform);

SDL_Window* henka_platform_get_sdl_window(henka_platform* platform)
{
    if (platform == NULL)
    {
        return NULL;
    }

    return platform->window;
}
