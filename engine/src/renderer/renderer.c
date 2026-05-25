#include "henka_internal.h"

#include <henka/log.h>
#include <henka/memory.h>

henka_result henka_renderer_create(struct henka_platform* platform, bool enable_vsync, struct henka_renderer** out_renderer)
{
    struct henka_renderer* renderer;
    henka_result result;

    if (platform == NULL || out_renderer == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    *out_renderer = NULL;

    renderer = henka_calloc(1U, sizeof(*renderer));
    if (renderer == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }

    renderer->platform = platform;
    renderer->scene_viewport = (henka_viewport){0, 0, 1, 1};

    result = henka_opengl_renderer_create(renderer, platform, enable_vsync);
    if (result != HENKA_SUCCESS)
    {
        henka_free(renderer);
        return result;
    }

    *out_renderer = renderer;
    return HENKA_SUCCESS;
}

void henka_renderer_destroy(struct henka_renderer* renderer)
{
    if (renderer == NULL)
    {
        return;
    }

    HENKA_LOG_INFO("destroying renderer");
    henka_opengl_renderer_destroy(renderer);
    henka_free(renderer);
}

henka_result henka_renderer_begin_frame(struct henka_renderer* renderer)
{
    return henka_opengl_renderer_begin_frame(renderer);
}

void henka_renderer_clear_frame(struct henka_renderer* renderer)
{
    henka_opengl_renderer_clear_frame(renderer);
}

henka_result henka_renderer_draw_scene(struct henka_renderer* renderer, const struct henka_scene* scene)
{
    if (renderer == NULL || scene == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    return henka_opengl_renderer_draw_scene(renderer, scene);
}

henka_result henka_renderer_draw_ui(struct henka_renderer* renderer, const struct henka_ui_context* ui_context)
{
    if (renderer == NULL || ui_context == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    return henka_opengl_renderer_draw_ui(renderer, ui_context);
}

henka_result henka_renderer_end_frame(struct henka_renderer* renderer)
{
    return henka_opengl_renderer_end_frame(renderer);
}

void henka_renderer_resize_viewport(struct henka_renderer* renderer, int width, int height)
{
    if (renderer == NULL)
    {
        return;
    }

    renderer->framebuffer_width = width;
    renderer->framebuffer_height = height;
    renderer->scene_viewport = (henka_viewport){0, 0, width > 0 ? width : 1, height > 0 ? height : 1};
    henka_opengl_renderer_resize_viewport(renderer, width, height);
}

void henka_renderer_set_scene_viewport(struct henka_renderer* renderer, henka_viewport viewport)
{
    if (renderer == NULL)
    {
        return;
    }

    if (viewport.width <= 0 || viewport.height <= 0)
    {
        renderer->scene_viewport = (henka_viewport)
        {
            0,
            0,
            renderer->framebuffer_width > 0 ? renderer->framebuffer_width : 1,
            renderer->framebuffer_height > 0 ? renderer->framebuffer_height : 1
        };
        return;
    }

    renderer->scene_viewport = viewport;
}

henka_viewport henka_renderer_get_scene_viewport(const struct henka_renderer* renderer)
{
    if (renderer == NULL || renderer->scene_viewport.width <= 0 || renderer->scene_viewport.height <= 0)
    {
        return (henka_viewport){0, 0, 1, 1};
    }

    return renderer->scene_viewport;
}

henka_result henka_renderer_set_vsync(struct henka_renderer* renderer, bool enabled)
{
    henka_result result;

    if (renderer == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    result = henka_opengl_renderer_set_vsync(renderer, enabled);
    if (result == HENKA_SUCCESS)
    {
        renderer->vsync_enabled = enabled;
    }

    return result;
}

henka_result henka_renderer_set_wireframe(struct henka_renderer* renderer, bool enabled)
{
    henka_result result;

    if (renderer == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    result = henka_opengl_renderer_set_wireframe(renderer, enabled);
    if (result == HENKA_SUCCESS)
    {
        renderer->wireframe_enabled = enabled;
    }

    return result;
}

henka_result henka_renderer_create_mesh_from_data(
    struct henka_renderer* renderer,
    const henka_vertex* vertices,
    int vertex_count,
    const unsigned int* indices,
    int index_count,
    henka_mesh_primitive primitive,
    struct henka_mesh** out_mesh)
{
    return henka_opengl_renderer_create_mesh_from_data(renderer, vertices, vertex_count, indices, index_count, primitive, out_mesh);
}

void henka_renderer_destroy_mesh(struct henka_mesh* mesh)
{
    henka_opengl_renderer_destroy_mesh(mesh);
}

henka_result henka_renderer_create_shader_from_files(
    struct henka_renderer* renderer,
    const char* vertex_path,
    const char* fragment_path,
    struct henka_shader** out_shader)
{
    return henka_opengl_renderer_create_shader_from_files(renderer, vertex_path, fragment_path, out_shader);
}

void henka_renderer_destroy_shader(struct henka_shader* shader)
{
    henka_opengl_renderer_destroy_shader(shader);
}

henka_result henka_renderer_create_texture_from_rgba8(
    struct henka_renderer* renderer,
    int width,
    int height,
    const unsigned char* pixels,
    struct henka_texture** out_texture)
{
    return henka_opengl_renderer_create_texture_from_rgba8(renderer, width, height, pixels, out_texture);
}

void henka_renderer_destroy_texture(struct henka_texture* texture)
{
    henka_opengl_renderer_destroy_texture(texture);
}
