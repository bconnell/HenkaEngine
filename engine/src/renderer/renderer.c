#include "henka_internal.h"

#include <SDL3/SDL.h>

#include <henka/log.h>
#include <henka/memory.h>

struct henka_renderer
{
    struct henka_platform* platform;
    SDL_Window* window;
    SDL_GLContext gl_context;
    bool vsync_enabled;
};

SDL_Window* henka_platform_get_sdl_window(struct henka_platform* platform);

static henka_result henka_renderer_configure_gl_attributes(void)
{
    if (!SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3) ||
        !SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3) ||
        !SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE) ||
        !SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1) ||
        !SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24))
    {
        HENKA_LOG_ERROR("SDL_GL_SetAttribute failed: %s", SDL_GetError());
        return HENKA_ERROR_RENDERER;
    }

    return HENKA_SUCCESS;
}

henka_result henka_renderer_create(struct henka_platform* platform, bool enable_vsync, struct henka_renderer** out_renderer)
{
    henka_renderer* renderer;
    int framebuffer_height;
    int framebuffer_width;
    henka_result result;

    if (platform == NULL || out_renderer == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    *out_renderer = NULL;

    result = henka_renderer_configure_gl_attributes();
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    renderer = henka_calloc(1U, sizeof(*renderer));
    if (renderer == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }

    renderer->platform = platform;
    renderer->window = henka_platform_get_sdl_window(platform);
    renderer->gl_context = SDL_GL_CreateContext(renderer->window);
    if (renderer->gl_context == NULL)
    {
        HENKA_LOG_ERROR("SDL_GL_CreateContext failed: %s", SDL_GetError());
        henka_free(renderer);
        return HENKA_ERROR_RENDERER;
    }

    if (!SDL_GL_MakeCurrent(renderer->window, renderer->gl_context))
    {
        HENKA_LOG_ERROR("SDL_GL_MakeCurrent failed: %s", SDL_GetError());
        SDL_GL_DestroyContext(renderer->gl_context);
        henka_free(renderer);
        return HENKA_ERROR_RENDERER;
    }

    result = henka_renderer_set_vsync(renderer, enable_vsync);
    if (result != HENKA_SUCCESS)
    {
        SDL_GL_DestroyContext(renderer->gl_context);
        henka_free(renderer);
        return result;
    }

    HENKA_LOG_INFO("renderer initialized with OpenGL backend");

    if (SDL_GetWindowSizeInPixels(renderer->window, &framebuffer_width, &framebuffer_height))
    {
        henka_renderer_resize_viewport(renderer, framebuffer_width, framebuffer_height);
    }

    *out_renderer = renderer;
    return HENKA_SUCCESS;
}
