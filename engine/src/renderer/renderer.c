#include "henka_internal.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

#include <henka/log.h>
#include "../ui/ui_internal.h"
#include <henka/memory.h>

static henka_viewport henka_renderer_full_viewport(
    const struct henka_renderer* renderer)
{
    if (renderer == NULL ||
        renderer->framebuffer_width <= 0 ||
        renderer->framebuffer_height <= 0)
    {
        return (henka_viewport){0, 0, 1, 1};
    }

    return (henka_viewport){
        0,
        0,
        renderer->framebuffer_width,
        renderer->framebuffer_height};
}

static henka_viewport henka_renderer_clip_viewport(
    const struct henka_renderer* renderer,
    henka_viewport viewport)
{
    int64_t bottom;
    int64_t right;

    if (renderer == NULL ||
        renderer->framebuffer_width <= 0 ||
        renderer->framebuffer_height <= 0 ||
        !henka_viewport_is_valid(viewport) ||
        viewport.x >= renderer->framebuffer_width ||
        viewport.y >= renderer->framebuffer_height)
    {
        return henka_renderer_full_viewport(renderer);
    }

    right = (int64_t)viewport.x + (int64_t)viewport.width;
    bottom = (int64_t)viewport.y + (int64_t)viewport.height;
    if (right > (int64_t)renderer->framebuffer_width)
    {
        right = (int64_t)renderer->framebuffer_width;
    }
    if (bottom > (int64_t)renderer->framebuffer_height)
    {
        bottom = (int64_t)renderer->framebuffer_height;
    }

    viewport.width = (int)(right - (int64_t)viewport.x);
    viewport.height = (int)(bottom - (int64_t)viewport.y);
    if (!henka_viewport_is_valid(viewport))
    {
        return henka_renderer_full_viewport(renderer);
    }

    return viewport;
}

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
    renderer->scene_view.viewport = renderer->scene_viewport;
    renderer->scene_view.shading_mode = HENKA_VIEWPORT_SHADING_SOLID;
    renderer->scene_view.overlays_visible = true;
    renderer->scene_view.xray_enabled = false;
    renderer->last_non_wireframe_mode = HENKA_VIEWPORT_SHADING_SOLID;
    renderer->ibl_diagnostic_mode = HENKA_IBL_DIAGNOSTIC_NONE;
    renderer->ibl_diagnostic_prefilter_lod = -1.0f;
    renderer->exposure = 0.0f;

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
    if (renderer->frame_active)
    {
        henka_renderer_abort_frame(renderer);
    }
    henka_opengl_renderer_destroy(renderer);
    henka_free(renderer);
}

henka_result henka_renderer_begin_frame(struct henka_renderer* renderer)
{
    henka_result result;

    if (renderer == NULL || renderer->frame_active)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    result = henka_opengl_renderer_begin_frame(renderer);
    if (result == HENKA_SUCCESS)
    {
        renderer->frame_active = true;
    }
    return result;
}

henka_result henka_renderer_abort_frame(struct henka_renderer* renderer)
{
    henka_result result;

    if (renderer == NULL || !renderer->frame_active)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    result = henka_opengl_renderer_abort_frame(renderer);
    if (result == HENKA_SUCCESS)
    {
        renderer->frame_active = false;
    }
    return result;
}

void henka_renderer_clear_frame(struct henka_renderer* renderer)
{
    if (renderer == NULL || !renderer->frame_active)
    {
        return;
    }
    henka_opengl_renderer_clear_frame(renderer);
}

henka_result henka_renderer_draw_scene(
    struct henka_renderer* renderer,
    const struct henka_scene* scene)
{
    if (renderer == NULL || scene == NULL || !renderer->frame_active)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    return henka_opengl_renderer_draw_scene(renderer, scene);
}

henka_result henka_renderer_draw_ui(
    struct henka_renderer* renderer,
    const struct henka_ui_context* ui_context)
{
    if (renderer == NULL || ui_context == NULL ||
        !renderer->frame_active || ui_context->frame_active)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    return henka_opengl_renderer_draw_ui(renderer, ui_context);
}

henka_result henka_renderer_end_frame(struct henka_renderer* renderer)
{
    henka_result result;

    if (renderer == NULL || !renderer->frame_active)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    result = henka_opengl_renderer_end_frame(renderer);
    if (result == HENKA_SUCCESS)
    {
        renderer->frame_active = false;
    }
    return result;
}

henka_result henka_renderer_request_frame_capture(
    struct henka_renderer* renderer,
    const char* path)
{
    size_t path_length;

    if (renderer == NULL || path == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    path_length = 0U;
    while (path_length < sizeof(renderer->frame_capture_path) &&
        path[path_length] != '\0')
    {
        ++path_length;
    }
    if (path_length == 0U || path_length >= sizeof(renderer->frame_capture_path))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (renderer->frame_capture_requested)
    {
        return HENKA_ERROR_LIMIT;
    }
    memcpy(renderer->frame_capture_path, path, path_length + 1U);
    renderer->frame_capture_requested = true;
    return HENKA_SUCCESS;
}

henka_result henka_renderer_create_tool_window_target(struct henka_renderer* renderer, henka_window_id window_id)
{
    return henka_opengl_renderer_create_tool_window_target(renderer, window_id);
}

void henka_renderer_destroy_tool_window_target(struct henka_renderer* renderer, henka_window_id window_id)
{
    henka_opengl_renderer_destroy_tool_window_target(renderer, window_id);
}

henka_result henka_renderer_draw_tool_window_ui(
    struct henka_renderer* renderer,
    henka_window_id window_id,
    const struct henka_ui_context* ui_context)
{
    if (renderer == NULL ||
        ui_context == NULL ||
        !renderer->frame_active ||
        ui_context->frame_active)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    return henka_opengl_renderer_draw_tool_window_ui(
        renderer,
        window_id,
        ui_context);
}

void henka_renderer_resize_viewport(
    struct henka_renderer* renderer,
    int width,
    int height)
{
    henka_viewport previous_viewport;

    if (renderer == NULL || width <= 0 || height <= 0)
    {
        return;
    }

    previous_viewport = renderer->scene_viewport;
    renderer->framebuffer_width = width;
    renderer->framebuffer_height = height;
    renderer->scene_viewport = renderer->scene_viewport_custom ?
        henka_renderer_clip_viewport(renderer, previous_viewport) :
        henka_renderer_full_viewport(renderer);
    renderer->scene_view.viewport = renderer->scene_viewport;
    henka_opengl_renderer_resize_viewport(renderer, width, height);
}

void henka_renderer_set_scene_viewport(
    struct henka_renderer* renderer,
    henka_viewport viewport)
{
    if (renderer == NULL)
    {
        return;
    }

    if (!henka_viewport_is_valid(viewport))
    {
        renderer->scene_viewport_custom = false;
        renderer->scene_viewport =
            henka_renderer_full_viewport(renderer);
        renderer->scene_view.viewport =
            renderer->scene_viewport;
        henka_opengl_renderer_sync_scene_target(renderer);
        return;
    }

    renderer->scene_viewport_custom = true;
    renderer->scene_viewport =
        henka_renderer_clip_viewport(renderer, viewport);
    renderer->scene_view.viewport =
        renderer->scene_viewport;
    henka_opengl_renderer_sync_scene_target(renderer);
}
henka_viewport henka_renderer_get_scene_viewport(
    const struct henka_renderer* renderer)
{
    if (renderer == NULL)
    {
        return (henka_viewport){0, 0, 1, 1};
    }

    return henka_renderer_clip_viewport(renderer, renderer->scene_viewport);
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

henka_result henka_viewport_render_policy_resolve(
    henka_viewport_shading_mode mode,
    henka_viewport_render_policy* out_policy)
{
    henka_viewport_render_policy policy;

    if (out_policy == NULL ||
        !henka_viewport_shading_mode_is_valid(mode))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    policy = (henka_viewport_render_policy){0};
    switch (mode)
    {
        case HENKA_VIEWPORT_SHADING_WIREFRAME:
            policy.polygon_wireframe = true;
            policy.force_unlit = true;
            break;
        case HENKA_VIEWPORT_SHADING_SOLID:
            policy.use_preview_lighting = true;
            break;
        case HENKA_VIEWPORT_SHADING_MATERIAL_PREVIEW:
            policy.use_material_base_color = true;
            policy.sample_material_texture = true;
            policy.use_preview_lighting = true;
            policy.use_hdr_presentation = true;
            break;
        case HENKA_VIEWPORT_SHADING_RENDERED:
            policy.use_material_base_color = true;
            policy.sample_material_texture = true;
            policy.use_scene_lighting = true;
            policy.use_hdr_presentation = true;
            policy.use_rendered_post_processing = true;
            break;
        case HENKA_VIEWPORT_SHADING_COUNT:
        default:
            return HENKA_ERROR_INVALID_ARGUMENT;
    }

    *out_policy = policy;
    return HENKA_SUCCESS;
}

henka_result henka_renderer_set_viewport_shading_mode(
    struct henka_renderer* renderer,
    henka_viewport_shading_mode mode)
{
    if (renderer == NULL ||
        !henka_viewport_shading_mode_is_valid(mode))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    renderer->scene_view.shading_mode = mode;
    if (mode != HENKA_VIEWPORT_SHADING_WIREFRAME)
    {
        renderer->last_non_wireframe_mode = mode;
    }
    return HENKA_SUCCESS;
}

henka_viewport_shading_mode henka_renderer_get_viewport_shading_mode(
    const struct henka_renderer* renderer)
{
    if (renderer == NULL ||
        !henka_viewport_shading_mode_is_valid(
            renderer->scene_view.shading_mode))
    {
        return HENKA_VIEWPORT_SHADING_SOLID;
    }

    return renderer->scene_view.shading_mode;
}

henka_result henka_renderer_set_ibl_diagnostic_mode(
    struct henka_renderer* renderer,
    henka_ibl_diagnostic_mode mode)
{
    if (renderer == NULL || mode < HENKA_IBL_DIAGNOSTIC_NONE ||
        mode >= HENKA_IBL_DIAGNOSTIC_COUNT)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    renderer->ibl_diagnostic_mode = mode;
    return HENKA_SUCCESS;
}

henka_ibl_diagnostic_mode henka_renderer_get_ibl_diagnostic_mode(
    const struct henka_renderer* renderer)
{
    if (renderer == NULL || renderer->ibl_diagnostic_mode < HENKA_IBL_DIAGNOSTIC_NONE ||
        renderer->ibl_diagnostic_mode >= HENKA_IBL_DIAGNOSTIC_COUNT)
    {
        return HENKA_IBL_DIAGNOSTIC_NONE;
    }

    return renderer->ibl_diagnostic_mode;
}

henka_result henka_renderer_set_ibl_diagnostic_prefilter_lod(
    struct henka_renderer* renderer,
    float lod)
{
    if (renderer == NULL || !isfinite(lod) || lod < -1.0f || lod > 1024.0f)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    renderer->ibl_diagnostic_prefilter_lod = lod;
    return HENKA_SUCCESS;
}

float henka_renderer_get_ibl_diagnostic_prefilter_lod(
    const struct henka_renderer* renderer)
{
    return renderer != NULL && isfinite(renderer->ibl_diagnostic_prefilter_lod)
        ? renderer->ibl_diagnostic_prefilter_lod
        : -1.0f;
}

henka_result henka_renderer_set_viewport_exposure(
    struct henka_renderer* renderer,
    float exposure_stops)
{
    if (renderer == NULL || !isfinite(exposure_stops) || exposure_stops < -16.0f || exposure_stops > 16.0f)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    renderer->exposure = exposure_stops;
    return HENKA_SUCCESS;
}

float henka_renderer_get_viewport_exposure(const struct henka_renderer* renderer)
{
    return renderer != NULL && isfinite(renderer->exposure) ? renderer->exposure : 0.0f;
}

bool henka_renderer_is_hdr_ready(const struct henka_renderer* renderer)
{
    return henka_opengl_renderer_is_hdr_ready(renderer);
}

bool henka_renderer_is_shadow_ready(const struct henka_renderer* renderer)
{
    return henka_opengl_renderer_is_shadow_ready(renderer);
}

henka_result henka_renderer_set_wireframe(
    struct henka_renderer* renderer,
    bool enabled)
{
    henka_viewport_shading_mode restore_mode;

    if (renderer == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    if (enabled)
    {
        return henka_renderer_set_viewport_shading_mode(
            renderer,
            HENKA_VIEWPORT_SHADING_WIREFRAME);
    }

    restore_mode = renderer->last_non_wireframe_mode;
    if (!henka_viewport_shading_mode_is_valid(restore_mode) ||
        restore_mode == HENKA_VIEWPORT_SHADING_WIREFRAME)
    {
        restore_mode = HENKA_VIEWPORT_SHADING_SOLID;
    }

    return henka_renderer_set_viewport_shading_mode(
        renderer,
        restore_mode);
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

henka_result henka_renderer_set_terrain_weights(
    struct henka_mesh* mesh,
    const uint8_t* weights,
    int vertex_count)
{
    return henka_opengl_renderer_set_terrain_weights(mesh, weights, vertex_count);
}

henka_result henka_renderer_create_shader_from_files(
    struct henka_renderer* renderer,
    const char* vertex_path,
    const char* fragment_path,
    struct henka_shader** out_shader)
{
    henka_shader_contract_desc contract =
        henka_shader_contract_desc_default(HENKA_SHADER_CONTRACT_MINIMAL_GEOMETRY);

    return henka_renderer_create_shader_from_files_with_contract(
        renderer,
        vertex_path,
        fragment_path,
        &contract,
        out_shader);
}

henka_result henka_renderer_create_shader_from_files_with_contract(
    struct henka_renderer* renderer,
    const char* vertex_path,
    const char* fragment_path,
    const henka_shader_contract_desc* contract,
    struct henka_shader** out_shader)
{
    return henka_opengl_renderer_create_shader_from_files_with_contract(
        renderer,
        vertex_path,
        fragment_path,
        contract,
        out_shader);
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
    henka_texture_descriptor descriptor = henka_texture_descriptor_default_color();

    return henka_renderer_create_texture_from_rgba8_with_descriptor(
        renderer,
        width,
        height,
        pixels,
        &descriptor,
        out_texture);
}

henka_result henka_renderer_create_texture_from_rgba8_with_descriptor(
    struct henka_renderer* renderer,
    int width,
    int height,
    const unsigned char* pixels,
    const henka_texture_descriptor* descriptor,
    struct henka_texture** out_texture)
{
    if (out_texture != NULL)
    {
        *out_texture = NULL;
    }

    if (renderer == NULL ||
        renderer->backend_state == NULL ||
        pixels == NULL || out_texture == NULL || descriptor == NULL ||
        henka_texture_descriptor_validate(descriptor) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    return henka_opengl_renderer_create_texture_from_rgba8_with_descriptor(
        renderer,
        width,
        height,
        pixels,
        descriptor,
        out_texture);
}

henka_result henka_renderer_create_texture_from_rgba32f_with_descriptor(
    struct henka_renderer* renderer,
    int width,
    int height,
    const float* pixels,
    const henka_texture_descriptor* descriptor,
    struct henka_texture** out_texture)
{
    if (out_texture != NULL)
    {
        *out_texture = NULL;
    }
    if (renderer == NULL || renderer->backend_state == NULL ||
        pixels == NULL || out_texture == NULL || descriptor == NULL ||
        henka_texture_descriptor_validate(descriptor) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return henka_opengl_renderer_create_texture_from_rgba32f_with_descriptor(
        renderer,
        width,
        height,
        pixels,
        descriptor,
        out_texture);
}

henka_result henka_renderer_create_texture_from_ktx2_memory_with_mip_limit(
    struct henka_renderer* renderer,
    const unsigned char* data,
    size_t data_size,
    const henka_texture_descriptor* descriptor,
    uint32_t max_resident_mips,
    struct henka_texture** out_texture)
{
    if (out_texture != NULL)
    {
        *out_texture = NULL;
    }
    if (renderer == NULL || renderer->backend_state == NULL || data == NULL ||
        data_size == 0U || descriptor == NULL || out_texture == NULL ||
        henka_texture_descriptor_validate(descriptor) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
#if defined(HENKA_WITH_KTX2_TRANSCODER)
    return henka_opengl_renderer_create_texture_from_ktx2_memory_with_mip_limit(
        renderer, data, data_size, descriptor, max_resident_mips, out_texture);
#else
    (void)data;
    (void)data_size;
    return HENKA_ERROR_ASSET_SOURCE;
#endif
}

henka_result henka_renderer_create_texture_from_ktx2_memory(
    struct henka_renderer* renderer,
    const unsigned char* data,
    size_t data_size,
    const henka_texture_descriptor* descriptor,
    struct henka_texture** out_texture)
{
    return henka_renderer_create_texture_from_ktx2_memory_with_mip_limit(
        renderer,
        data,
        data_size,
        descriptor,
        0U,
        out_texture);
}

void henka_renderer_destroy_texture(struct henka_texture* texture)
{
    henka_opengl_renderer_destroy_texture(texture);
}
