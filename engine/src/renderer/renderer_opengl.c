#include "henka_internal.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

#include <henka/log.h>
#include <henka/memory.h>

struct henka_renderer
{
    struct henka_platform* platform;
    SDL_Window* window;
    SDL_GLContext gl_context;
    bool vsync_enabled;
};

henka_result henka_renderer_begin_frame(struct henka_renderer* renderer)
{
    if (renderer == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    if (!SDL_GL_MakeCurrent(renderer->window, renderer->gl_context))
    {
        HENKA_LOG_ERROR("SDL_GL_MakeCurrent failed during begin frame: %s", SDL_GetError());
        return HENKA_ERROR_RENDERER;
    }

    return HENKA_SUCCESS;
}

void henka_renderer_clear_frame(struct henka_renderer* renderer)
{
    if (renderer == NULL)
    {
        return;
    }

    glClearColor(0.08f, 0.09f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

henka_result henka_renderer_end_frame(struct henka_renderer* renderer)
{
    if (renderer == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    if (!SDL_GL_SwapWindow(renderer->window))
    {
        HENKA_LOG_ERROR("SDL_GL_SwapWindow failed: %s", SDL_GetError());
        return HENKA_ERROR_RENDERER;
    }

    return HENKA_SUCCESS;
}

void henka_renderer_resize_viewport(struct henka_renderer* renderer, int width, int height)
{
    if (renderer == NULL)
    {
        return;
    }

    glViewport(0, 0, width, height);
}

henka_result henka_renderer_set_vsync(struct henka_renderer* renderer, bool enabled)
{
    henka_result result;

    if (renderer == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    result = henka_platform_set_vsync(renderer->platform, enabled);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    renderer->vsync_enabled = enabled;
    return HENKA_SUCCESS;
}

void henka_renderer_destroy(struct henka_renderer* renderer)
{
    if (renderer == NULL)
    {
        return;
    }

    HENKA_LOG_INFO("destroying renderer");

    if (renderer->gl_context != NULL)
    {
        SDL_GL_DestroyContext(renderer->gl_context);
    }

    henka_free(renderer);
}
