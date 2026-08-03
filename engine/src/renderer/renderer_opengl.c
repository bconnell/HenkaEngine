#include "henka_internal.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <SDL3/SDL_opengl_glext.h>

#include <float.h>
#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <henka/log.h>
#include <henka/memory.h>

#include "../core/checked.h"
#include "../ui/ui_internal.h"

#ifndef GL_TEXTURE_MAX_ANISOTROPY_EXT
#define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF
#endif

#ifndef GL_COMPRESSED_RGB_S3TC_DXT1_EXT
#define GL_COMPRESSED_RGB_S3TC_DXT1_EXT 0x83F0
#endif
#ifndef GL_COMPRESSED_RGBA_S3TC_DXT5_EXT
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT 0x83F3
#endif
#ifndef GL_COMPRESSED_RED_RGTC1
#define GL_COMPRESSED_RED_RGTC1 0x8DBB
#endif
#ifndef GL_COMPRESSED_RG_RGTC2
#define GL_COMPRESSED_RG_RGTC2 0x8DBD
#endif
#ifndef GL_COMPRESSED_RGBA_BPTC_UNORM
#define GL_COMPRESSED_RGBA_BPTC_UNORM 0x8E8C
#endif
#ifndef GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM
#define GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM 0x8E8D
#endif
#ifndef GL_COMPRESSED_RGB8_ETC2
#define GL_COMPRESSED_RGB8_ETC2 0x9274
#endif
#ifndef GL_COMPRESSED_SRGB8_ETC2
#define GL_COMPRESSED_SRGB8_ETC2 0x9275
#endif
#ifndef GL_COMPRESSED_RGBA8_ETC2_EAC
#define GL_COMPRESSED_RGBA8_ETC2_EAC 0x9278
#endif
#ifndef GL_COMPRESSED_RG11_EAC
#define GL_COMPRESSED_RG11_EAC 0x9272
#endif
#ifndef GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC
#define GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC 0x9279
#endif
#ifndef GL_COMPRESSED_RGBA_ASTC_4x4_KHR
#define GL_COMPRESSED_RGBA_ASTC_4x4_KHR 0x93B0
#endif
#ifndef GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR 0x93D0
#endif
#ifndef GL_ANY_SAMPLES_PASSED
#define GL_ANY_SAMPLES_PASSED 0x8C2F
#endif

#define HENKA_OPENGL_OCCLUSION_QUERY_CAPACITY 256U

typedef struct henka_opengl_tool_window_target
{
    henka_window_id id;
    SDL_Window* window;
    SDL_GLContext gl_context;
    GLuint ui_program;
    GLuint ui_vertex_array;
    GLuint ui_vertex_buffer;
} henka_opengl_tool_window_target;

#define HENKA_OPENGL_TRANSPARENT_SORT_CAPACITY 4096U
#define HENKA_OPENGL_SCENE_DRAW_BUDGET 8192U
#define HENKA_OPENGL_INSTANCE_CAPACITY 256U

typedef struct henka_opengl_transparent_sort_item
{
    size_t entity_index;
    float view_depth;
} henka_opengl_transparent_sort_item;

typedef struct henka_opengl_instance_data
{
    henka_mat4 model;
    henka_mat4 previous_model;
} henka_opengl_instance_data;

typedef struct henka_opengl_shader_data
{
    GLuint program;
    SDL_GLContext context;
    henka_shader_contract_type contract_type;
    uint32_t contract_version;
    uint64_t source_hash;
    uint64_t generation;
    size_t location_count;
    struct henka_opengl_uniform_location_entry
    {
        char name[64];
        GLint location;
    } locations[96];
} henka_opengl_shader_data;

typedef struct henka_opengl_renderer_state
{
    SDL_Window* window;
    SDL_GLContext gl_context;
    GLuint ui_program;
    GLuint ui_vertex_array;
    GLuint ui_vertex_buffer;
    GLuint viewport_program;
    henka_opengl_shader_data viewport_shader_data;
    GLuint tone_program;
    henka_opengl_shader_data tone_shader_data;
    GLuint bloom_extract_program;
    henka_opengl_shader_data bloom_extract_shader_data;
    GLuint bloom_blur_program;
    henka_opengl_shader_data bloom_blur_shader_data;
    GLuint ibl_conversion_program;
    henka_opengl_shader_data ibl_conversion_shader_data;
    GLuint ibl_irradiance_program;
    henka_opengl_shader_data ibl_irradiance_shader_data;
    GLuint ibl_prefilter_program;
    henka_opengl_shader_data ibl_prefilter_shader_data;
    GLuint ibl_brdf_program;
    henka_opengl_shader_data ibl_brdf_shader_data;
    GLuint environment_program;
    henka_opengl_shader_data environment_shader_data;
    GLuint tone_vertex_array;
    GLuint hdr_framebuffer;
    GLuint hdr_color_texture;
    GLuint hdr_motion_texture;
    GLuint hdr_reactive_texture;
    GLuint hdr_depth_buffer;
    int hdr_width;
    int hdr_height;
    int hdr_requested_width;
    int hdr_requested_height;
    uint64_t hdr_generation;
    bool hdr_framebuffer_complete;
    char hdr_failure_reason[64];
    GLuint bloom_framebuffer;
    GLuint bloom_blur_framebuffer;
    GLuint bloom_color_texture;
    GLuint bloom_blur_texture;
    int bloom_width;
    int bloom_height;
    bool bloom_ready;
    char bloom_failure_reason[64];
    GLuint temporal_history_texture;
    int temporal_history_width;
    int temporal_history_height;
    bool temporal_history_ready;
    bool temporal_history_valid;
    bool temporal_fallback_active;
    uint32_t temporal_invalidation_count;
    char temporal_invalidation_reason[64];
    bool temporal_jitter_enabled;
    uint64_t temporal_jitter_index;
    float temporal_jitter_x;
    float temporal_jitter_y;
    henka_mat4 current_projection;
    henka_mat4 previous_view_projection;
    bool previous_view_projection_valid;
    GLuint ibl_framebuffer;
    GLuint ibl_environment_cube;
    GLuint ibl_irradiance_cube;
    GLuint ibl_prefilter_cube;
    GLuint ibl_brdf_lut;
    bool ibl_ready;
    char ibl_failure_reason[64];
    const henka_texture* ibl_source_texture;
    uint64_t ibl_source_revision;
    float ibl_source_rotation;
    GLuint reflection_probe_cubes[HENKA_SCENE_MAX_REFLECTION_PROBES];
    bool reflection_probe_capture_ready[HENKA_SCENE_MAX_REFLECTION_PROBES];
    uint64_t reflection_probe_captured_scene_revision[HENKA_SCENE_MAX_REFLECTION_PROBES];
    henka_scene_reflection_probe_desc reflection_probe_captured_desc[HENKA_SCENE_MAX_REFLECTION_PROBES];
    GLuint reflection_probe_framebuffer;
    GLuint reflection_probe_depth_buffer;
    uint32_t reflection_probe_capture_cursor;
    bool reflection_probe_capture_active;
    uint32_t reflection_probe_capture_index;
    uint32_t reflection_probe_enabled_count;
    uint32_t reflection_probe_captured_count;
    uint64_t reflection_probe_capture_generation;
    uint32_t reflection_probe_capture_failure_count;
    GLuint shadow_program;
    henka_opengl_shader_data shadow_shader_data;
    GLuint shadow_framebuffer;
    GLuint shadow_depth_texture;
    int shadow_resolution;
    uint64_t shadow_generation;
    bool shadow_framebuffer_complete;
    char shadow_failure_reason[64];
    GLuint cascade_shadow_framebuffer;
    GLuint cascade_shadow_depth_texture;
    int cascade_shadow_resolution;
    uint64_t cascade_shadow_generation;
    bool cascade_shadow_framebuffer_complete;
    char cascade_shadow_failure_reason[64];
    GLuint local_shadow_framebuffer;
    GLuint local_shadow_depth_texture;
    int local_shadow_resolution;
    uint64_t local_shadow_generation;
    bool local_shadow_framebuffer_complete;
    char local_shadow_failure_reason[64];
    GLuint point_shadow_framebuffer;
    GLuint point_shadow_depth_texture;
    int point_shadow_resolution;
    uint64_t point_shadow_generation;
    bool point_shadow_framebuffer_complete;
    char point_shadow_failure_reason[64];
    uint32_t scene_draw_calls;
    uint32_t scene_visible_entities;
    uint32_t scene_culled_entities;
    uint32_t scene_budget_dropped_entities;
    uint32_t scene_lod_entities;
    uint32_t scene_lod_fallback_entities;
    uint32_t scene_instanced_draw_calls;
    uint32_t scene_instanced_entities;
    uint32_t scene_occlusion_tested_entities;
    uint32_t scene_occlusion_culled_entities;
    uint32_t transparent_sort_overflow_entities;
    double scene_cpu_time_milliseconds;
    double scene_gpu_time_milliseconds;
    bool gpu_timing_available;
    GLuint scene_gpu_query;
    bool scene_gpu_query_pending;
    uint64_t tracked_gpu_bytes;
    uint64_t tracked_gpu_peak_bytes;
    uint64_t tracked_mesh_bytes;
    uint64_t tracked_texture_bytes;
    uint64_t tracked_render_target_bytes;
    bool memory_overflow;
    uint32_t tracked_mesh_count;
    uint32_t tracked_texture_count;
    henka_opengl_transparent_sort_item transparent_sort_items[HENKA_OPENGL_TRANSPARENT_SORT_CAPACITY];
    size_t transparent_sort_count;
    bool transparent_sort_enabled;
    GLuint instance_buffer;
    bool instancing_available;
    henka_opengl_instance_data instance_data[HENKA_OPENGL_INSTANCE_CAPACITY];
    GLuint occlusion_queries[HENKA_OPENGL_OCCLUSION_QUERY_CAPACITY];
    bool occlusion_query_valid[HENKA_OPENGL_OCCLUSION_QUERY_CAPACITY];
    uint64_t occlusion_query_scene_revision[HENKA_OPENGL_OCCLUSION_QUERY_CAPACITY];
    henka_mat4 occlusion_query_view_projection;
    bool occlusion_history_valid;
    henka_opengl_tool_window_target tool_targets[HENKA_MAX_TOOL_WINDOWS];
} henka_opengl_renderer_state;

static bool henka_opengl_memory_add(uint64_t* value, uint64_t amount)
{
    if (value == NULL || UINT64_MAX - *value < amount)
    {
        return false;
    }
    *value += amount;
    return true;
}

static bool henka_opengl_memory_subtract(uint64_t* value, uint64_t amount)
{
    if (value == NULL || *value < amount)
    {
        return false;
    }
    *value -= amount;
    return true;
}

static float henka_opengl_temporal_halton(uint64_t index, uint32_t base)
{
    float fraction = 1.0f;
    float result = 0.0f;

    while (index > 0U)
    {
        fraction /= (float)base;
        result += fraction * (float)(index % (uint64_t)base);
        index /= (uint64_t)base;
    }
    return result;
}

static void henka_opengl_invalidate_temporal_history(
    henka_opengl_renderer_state* state,
    const char* reason)
{
    if (state == NULL)
    {
        return;
    }
    state->temporal_history_valid = false;
    state->previous_view_projection_valid = false;
    state->temporal_fallback_active = true;
    if (state->temporal_invalidation_count < UINT32_MAX)
    {
        ++state->temporal_invalidation_count;
    }
    (void)snprintf(
        state->temporal_invalidation_reason,
        sizeof(state->temporal_invalidation_reason),
        "%s",
        reason != NULL && reason[0] != '\0' ? reason : "history invalidated");
}

static bool henka_opengl_temporal_matrix_is_cut(
    henka_mat4 previous,
    henka_mat4 current)
{
    float maximum_delta = 0.0f;
    size_t index;

    for (index = 0U; index < sizeof(previous.m) / sizeof(previous.m[0]); ++index)
    {
        float delta = fabsf(previous.m[index] - current.m[index]);
        if (!isfinite(delta))
        {
            return true;
        }
        maximum_delta = fmaxf(maximum_delta, delta);
    }
    return maximum_delta > 0.75f;
}

static void henka_opengl_memory_refresh(
    henka_opengl_renderer_state* state)
{
    uint64_t total = 0U;

    if (state == NULL || state->memory_overflow ||
        !henka_opengl_memory_add(&total, state->tracked_mesh_bytes) ||
        !henka_opengl_memory_add(&total, state->tracked_texture_bytes) ||
        !henka_opengl_memory_add(&total, state->tracked_render_target_bytes))
    {
        if (state != NULL)
        {
            state->memory_overflow = true;
            state->tracked_gpu_bytes = UINT64_MAX;
        }
        return;
    }
    state->tracked_gpu_bytes = total;
    if (state->tracked_gpu_peak_bytes < total)
    {
        state->tracked_gpu_peak_bytes = total;
    }
}

static void henka_opengl_memory_add_category(
    henka_opengl_renderer_state* state,
    uint64_t* category,
    uint64_t amount)
{
    if (state == NULL || !henka_opengl_memory_add(category, amount))
    {
        if (state != NULL)
        {
            state->memory_overflow = true;
            state->tracked_gpu_bytes = UINT64_MAX;
        }
        return;
    }
    henka_opengl_memory_refresh(state);
}

static void henka_opengl_memory_remove_category(
    henka_opengl_renderer_state* state,
    uint64_t* category,
    uint64_t amount)
{
    if (state == NULL || !henka_opengl_memory_subtract(category, amount))
    {
        if (state != NULL)
        {
            state->memory_overflow = true;
            state->tracked_gpu_bytes = UINT64_MAX;
        }
        return;
    }
    henka_opengl_memory_refresh(state);
}

static bool henka_opengl_calculate_texture_bytes(
    int width,
    int height,
    size_t decoded_bytes,
    bool generate_mipmaps,
    uint64_t* out_bytes)
{
    uint64_t base_pixels;
    uint64_t bytes_per_pixel;
    uint64_t total = 0U;
    int mip_width;
    int mip_height;

    if (width <= 0 || height <= 0 || out_bytes == NULL)
    {
        return false;
    }
    if ((uint64_t)width > UINT64_MAX / (uint64_t)height)
    {
        return false;
    }
    base_pixels = (uint64_t)width * (uint64_t)height;
    if (base_pixels == 0U || (uint64_t)decoded_bytes < base_pixels ||
        (uint64_t)decoded_bytes % base_pixels != 0U)
    {
        return false;
    }
    bytes_per_pixel = (uint64_t)decoded_bytes / base_pixels;
    mip_width = width;
    mip_height = height;
    do
    {
        uint64_t level_pixels;
        uint64_t level_bytes;

        if ((uint64_t)mip_width > UINT64_MAX / (uint64_t)mip_height)
        {
            return false;
        }
        level_pixels = (uint64_t)mip_width * (uint64_t)mip_height;
        if (bytes_per_pixel > UINT64_MAX / level_pixels)
        {
            return false;
        }
        level_bytes = level_pixels * bytes_per_pixel;
        if (!henka_opengl_memory_add(&total, level_bytes))
        {
            return false;
        }
        if (!generate_mipmaps || (mip_width == 1 && mip_height == 1))
        {
            break;
        }
        mip_width = mip_width > 1 ? mip_width / 2 : 1;
        mip_height = mip_height > 1 ? mip_height / 2 : 1;
    } while (true);

    *out_bytes = total;
    return true;
}

typedef struct henka_opengl_functions
{
    PFNGLCREATESHADERPROC CreateShader;
    PFNGLSHADERSOURCEPROC ShaderSource;
    PFNGLCOMPILESHADERPROC CompileShader;
    PFNGLGETSHADERIVPROC GetShaderiv;
    PFNGLGETSHADERINFOLOGPROC GetShaderInfoLog;
    PFNGLDELETESHADERPROC DeleteShader;
    PFNGLCREATEPROGRAMPROC CreateProgram;
    PFNGLATTACHSHADERPROC AttachShader;
    PFNGLLINKPROGRAMPROC LinkProgram;
    PFNGLGETPROGRAMIVPROC GetProgramiv;
    PFNGLGETPROGRAMINFOLOGPROC GetProgramInfoLog;
    PFNGLDELETEPROGRAMPROC DeleteProgram;
    PFNGLUSEPROGRAMPROC UseProgram;
    PFNGLGETUNIFORMLOCATIONPROC GetUniformLocation;
    PFNGLUNIFORMMATRIX4FVPROC UniformMatrix4fv;
    PFNGLUNIFORM4FPROC Uniform4f;
    PFNGLUNIFORM4FVPROC Uniform4fv;
    PFNGLUNIFORM3FPROC Uniform3f;
    PFNGLUNIFORM2FPROC Uniform2f;
    PFNGLUNIFORM1IPROC Uniform1i;
    PFNGLUNIFORM1FPROC Uniform1f;
    PFNGLGENVERTEXARRAYSPROC GenVertexArrays;
    PFNGLGENBUFFERSPROC GenBuffers;
    PFNGLBINDVERTEXARRAYPROC BindVertexArray;
    PFNGLBINDBUFFERPROC BindBuffer;
    PFNGLBUFFERDATAPROC BufferData;
    PFNGLENABLEVERTEXATTRIBARRAYPROC EnableVertexAttribArray;
    PFNGLDISABLEVERTEXATTRIBARRAYPROC DisableVertexAttribArray;
    PFNGLVERTEXATTRIBPOINTERPROC VertexAttribPointer;
    PFNGLVERTEXATTRIBDIVISORPROC VertexAttribDivisor;
    PFNGLDRAWELEMENTSINSTANCEDPROC DrawElementsInstanced;
    PFNGLDELETEBUFFERSPROC DeleteBuffers;
    PFNGLDELETEVERTEXARRAYSPROC DeleteVertexArrays;
    PFNGLACTIVETEXTUREPROC ActiveTexture;
    PFNGLGENERATEMIPMAPPROC GenerateMipmap;
    PFNGLCOMPRESSEDTEXIMAGE2DPROC CompressedTexImage2D;
    PFNGLGENFRAMEBUFFERSPROC GenFramebuffers;
    PFNGLBINDFRAMEBUFFERPROC BindFramebuffer;
    PFNGLFRAMEBUFFERTEXTURE2DPROC FramebufferTexture2D;
    PFNGLDRAWBUFFERSPROC DrawBuffers;
    PFNGLCHECKFRAMEBUFFERSTATUSPROC CheckFramebufferStatus;
    PFNGLDELETEFRAMEBUFFERSPROC DeleteFramebuffers;
    PFNGLGENRENDERBUFFERSPROC GenRenderbuffers;
    PFNGLBINDRENDERBUFFERPROC BindRenderbuffer;
    PFNGLRENDERBUFFERSTORAGEPROC RenderbufferStorage;
    PFNGLFRAMEBUFFERRENDERBUFFERPROC FramebufferRenderbuffer;
    PFNGLDELETERENDERBUFFERSPROC DeleteRenderbuffers;
    PFNGLGENQUERIESPROC GenQueries;
    PFNGLDELETEQUERIESPROC DeleteQueries;
    PFNGLBEGINQUERYPROC BeginQuery;
    PFNGLENDQUERYPROC EndQuery;
    PFNGLGETQUERYOBJECTIVPROC GetQueryObjectiv;
    PFNGLGETQUERYOBJECTUI64VPROC GetQueryObjectui64v;
} henka_opengl_functions;

typedef struct henka_opengl_mesh_data
{
    GLuint vao;
    GLuint vertex_buffer;
    GLuint index_buffer;
    GLenum primitive_mode;
    GLsizei index_count;
    uint64_t tracked_gpu_bytes;
} henka_opengl_mesh_data;

typedef struct henka_opengl_texture_data
{
    GLuint texture_id;
    uint64_t tracked_gpu_bytes;
} henka_opengl_texture_data;

static henka_opengl_functions g_gl;

#if defined(HENKA_WITH_KTX2_TRANSCODER)
static bool henka_opengl_has_extension(const char* extensions, const char* name)
{
    const char* cursor;
    size_t name_length;

    if (extensions == NULL || name == NULL || name[0] == '\0')
        return false;
    name_length = strlen(name);
    cursor = extensions;
    while ((cursor = strstr(cursor, name)) != NULL)
    {
        if ((cursor == extensions || cursor[-1] == ' ') &&
            (cursor[name_length] == '\0' || cursor[name_length] == ' '))
            return true;
        cursor += name_length;
    }
    return false;
}

static uint32_t henka_opengl_ktx2_capabilities(void)
{
    const char* extensions = (const char*)glGetString(GL_EXTENSIONS);
    uint32_t capabilities = 0U;

    if (henka_opengl_has_extension(extensions, "GL_EXT_texture_compression_s3tc") ||
        henka_opengl_has_extension(extensions, "GL_EXT_texture_compression_dxt1"))
        capabilities |= HENKA_KTX2_CAPABILITY_BC1_3;
    if (henka_opengl_has_extension(extensions, "GL_EXT_texture_compression_rgtc") ||
        henka_opengl_has_extension(extensions, "GL_ARB_texture_compression_rgtc"))
        capabilities |= HENKA_KTX2_CAPABILITY_BC5;
    if (henka_opengl_has_extension(extensions, "GL_ARB_texture_compression_bptc"))
        capabilities |= HENKA_KTX2_CAPABILITY_BC7;
    if (henka_opengl_has_extension(extensions, "GL_ARB_ES3_compatibility") ||
        henka_opengl_has_extension(extensions, "GL_OES_compressed_ETC2_RGB8_texture"))
        capabilities |= HENKA_KTX2_CAPABILITY_ETC2;
    if (henka_opengl_has_extension(extensions, "GL_KHR_texture_compression_astc_ldr"))
        capabilities |= HENKA_KTX2_CAPABILITY_ASTC_4X4;
    return capabilities;
}

static GLenum henka_opengl_ktx2_internal_format(
    henka_ktx2_gpu_format format,
    bool is_srgb)
{
    switch (format)
    {
        case HENKA_KTX2_GPU_FORMAT_BC1:
            return is_srgb ? 0x8C4C : GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
        case HENKA_KTX2_GPU_FORMAT_BC3:
            return is_srgb ? 0x8C4F : GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
        case HENKA_KTX2_GPU_FORMAT_BC5:
            return GL_COMPRESSED_RG_RGTC2;
        case HENKA_KTX2_GPU_FORMAT_BC7:
            return is_srgb ? GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM : GL_COMPRESSED_RGBA_BPTC_UNORM;
        case HENKA_KTX2_GPU_FORMAT_ETC2_RGB:
            return is_srgb ? GL_COMPRESSED_SRGB8_ETC2 : GL_COMPRESSED_RGB8_ETC2;
        case HENKA_KTX2_GPU_FORMAT_ETC2_RGBA:
            return is_srgb ? GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC : GL_COMPRESSED_RGBA8_ETC2_EAC;
        case HENKA_KTX2_GPU_FORMAT_ETC2_RG:
            return GL_COMPRESSED_RG11_EAC;
        case HENKA_KTX2_GPU_FORMAT_ASTC_4X4:
            return is_srgb ? GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR : GL_COMPRESSED_RGBA_ASTC_4x4_KHR;
        default:
            return 0U;
    }
}
#endif

static GLint henka_opengl_uniform_location(GLuint program, const char* name)
{
    if (SDL_GL_GetCurrentContext() == NULL || program == 0U || name == NULL || name[0] == '\0')
    {
        return -1;
    }
    return g_gl.GetUniformLocation(program, name);
}

bool henka_opengl_renderer_is_hdr_ready(const struct henka_renderer* renderer)
{
    const henka_opengl_renderer_state* state;

    if (renderer == NULL || renderer->backend_state == NULL)
    {
        return false;
    }
    state = (const henka_opengl_renderer_state*)renderer->backend_state;
    return state->hdr_framebuffer != 0U && state->hdr_color_texture != 0U &&
        state->hdr_framebuffer_complete && state->hdr_width > 0 && state->hdr_height > 0;
}

bool henka_opengl_renderer_is_shadow_ready(const struct henka_renderer* renderer)
{
    const henka_opengl_renderer_state* state;

    if (renderer == NULL || renderer->backend_state == NULL)
    {
        return false;
    }
    state = (const henka_opengl_renderer_state*)renderer->backend_state;
    return state->shadow_framebuffer != 0U && state->shadow_depth_texture != 0U &&
        state->shadow_framebuffer_complete && state->shadow_resolution > 0;
}

typedef struct henka_ui_vertex
{
    float x;
    float y;
    float r;
    float g;
    float b;
    float a;
} henka_ui_vertex;

static henka_result henka_opengl_restore_main_context(
    henka_opengl_renderer_state* state,
    const char* operation)
{
    if (state == NULL ||
        state->window == NULL ||
        state->gl_context == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    if (!SDL_GL_MakeCurrent(state->window, state->gl_context))
    {
        HENKA_LOG_ERROR(
            "could not restore the main OpenGL context after %s: %s",
            operation != NULL ? operation : "tool-window work",
            SDL_GetError());
        return HENKA_ERROR_RENDERER;
    }

    return HENKA_SUCCESS;
}

SDL_Window* henka_platform_get_sdl_window(struct henka_platform* platform);

static bool henka_compile_shader(GLuint shader, const char* source, const char* label);
static bool henka_link_program(GLuint program);
static bool henka_validate_shader_contract(
    GLuint program,
    const char* label,
    henka_shader_contract_type contract_type,
    uint32_t contract_version,
    henka_opengl_shader_data* out_shader_data);

static void henka_apply_full_framebuffer_viewport(const struct henka_renderer* renderer)
{
    if (renderer == NULL)
    {
        return;
    }

    glDisable(GL_SCISSOR_TEST);
    glViewport(0, 0, renderer->framebuffer_width, renderer->framebuffer_height);
}

static void henka_apply_scene_viewport(struct henka_renderer* renderer)
{
    int64_t gl_y_value;
    henka_viewport viewport;

    if (renderer == NULL)
    {
        return;
    }

    viewport = henka_renderer_get_scene_viewport(renderer);
    gl_y_value =
        (int64_t)renderer->framebuffer_height -
        (int64_t)viewport.y -
        (int64_t)viewport.height;
    if (gl_y_value < 0)
    {
        gl_y_value = 0;
    }
    if (gl_y_value > (int64_t)INT_MAX)
    {
        gl_y_value = (int64_t)INT_MAX;
    }

    glEnable(GL_SCISSOR_TEST);
    glViewport(
        (GLint)viewport.x,
        (GLint)gl_y_value,
        (GLsizei)viewport.width,
        (GLsizei)viewport.height);
    glScissor(
        (GLint)viewport.x,
        (GLint)gl_y_value,
        (GLsizei)viewport.width,
        (GLsizei)viewport.height);
}

static void henka_apply_scene_target_viewport(const struct henka_renderer* renderer)
{
    henka_viewport viewport;

    if (renderer == NULL)
    {
        return;
    }

    viewport = henka_renderer_get_scene_viewport(renderer);
    glDisable(GL_SCISSOR_TEST);
    glViewport(0, 0, (GLsizei)viewport.width, (GLsizei)viewport.height);
}

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

static bool henka_opengl_load_functions(void)
{
#define HENKA_GL_LOAD(name)                                                                 \
    do                                                                                      \
    {                                                                                       \
        SDL_FunctionPointer proc_address;                                                   \
        proc_address = SDL_GL_GetProcAddress("gl" #name);                                   \
        if (proc_address == NULL)                                                           \
        {                                                                                   \
            HENKA_LOG_ERROR("failed to load OpenGL function gl%s", #name);                  \
            return false;                                                                   \
        }                                                                                   \
        memcpy(&g_gl.name, &proc_address, sizeof(proc_address));                            \
    } while (0)

    HENKA_GL_LOAD(CreateShader);
    HENKA_GL_LOAD(ShaderSource);
    HENKA_GL_LOAD(CompileShader);
    HENKA_GL_LOAD(GetShaderiv);
    HENKA_GL_LOAD(GetShaderInfoLog);
    HENKA_GL_LOAD(DeleteShader);
    HENKA_GL_LOAD(CreateProgram);
    HENKA_GL_LOAD(AttachShader);
    HENKA_GL_LOAD(LinkProgram);
    HENKA_GL_LOAD(GetProgramiv);
    HENKA_GL_LOAD(GetProgramInfoLog);
    HENKA_GL_LOAD(DeleteProgram);
    HENKA_GL_LOAD(UseProgram);
    HENKA_GL_LOAD(GetUniformLocation);
    HENKA_GL_LOAD(UniformMatrix4fv);
    HENKA_GL_LOAD(Uniform4f);
    HENKA_GL_LOAD(Uniform4fv);
    HENKA_GL_LOAD(Uniform3f);
    HENKA_GL_LOAD(Uniform2f);
    HENKA_GL_LOAD(Uniform1i);
    HENKA_GL_LOAD(Uniform1f);
    HENKA_GL_LOAD(GenVertexArrays);
    HENKA_GL_LOAD(GenBuffers);
    HENKA_GL_LOAD(BindVertexArray);
    HENKA_GL_LOAD(BindBuffer);
    HENKA_GL_LOAD(BufferData);
    HENKA_GL_LOAD(EnableVertexAttribArray);
    {
        SDL_FunctionPointer proc_address;
        proc_address = SDL_GL_GetProcAddress("glDisableVertexAttribArray");
        memcpy(&g_gl.DisableVertexAttribArray, &proc_address, sizeof(proc_address));
        proc_address = SDL_GL_GetProcAddress("glVertexAttribDivisor");
        memcpy(&g_gl.VertexAttribDivisor, &proc_address, sizeof(proc_address));
        proc_address = SDL_GL_GetProcAddress("glDrawElementsInstanced");
        memcpy(&g_gl.DrawElementsInstanced, &proc_address, sizeof(proc_address));
    }
    HENKA_GL_LOAD(VertexAttribPointer);
    HENKA_GL_LOAD(DeleteBuffers);
    HENKA_GL_LOAD(DeleteVertexArrays);
    HENKA_GL_LOAD(ActiveTexture);
    HENKA_GL_LOAD(GenerateMipmap);
    HENKA_GL_LOAD(CompressedTexImage2D);
    HENKA_GL_LOAD(GenFramebuffers);
    HENKA_GL_LOAD(BindFramebuffer);
    HENKA_GL_LOAD(FramebufferTexture2D);
    HENKA_GL_LOAD(DrawBuffers);
    HENKA_GL_LOAD(CheckFramebufferStatus);
    HENKA_GL_LOAD(DeleteFramebuffers);
    HENKA_GL_LOAD(GenRenderbuffers);
    HENKA_GL_LOAD(BindRenderbuffer);
    HENKA_GL_LOAD(RenderbufferStorage);
    HENKA_GL_LOAD(FramebufferRenderbuffer);
    HENKA_GL_LOAD(DeleteRenderbuffers);

    {
        SDL_FunctionPointer proc_address;
        proc_address = SDL_GL_GetProcAddress("glGenQueries");
        memcpy(&g_gl.GenQueries, &proc_address, sizeof(proc_address));
        proc_address = SDL_GL_GetProcAddress("glDeleteQueries");
        memcpy(&g_gl.DeleteQueries, &proc_address, sizeof(proc_address));
        proc_address = SDL_GL_GetProcAddress("glBeginQuery");
        memcpy(&g_gl.BeginQuery, &proc_address, sizeof(proc_address));
        proc_address = SDL_GL_GetProcAddress("glEndQuery");
        memcpy(&g_gl.EndQuery, &proc_address, sizeof(proc_address));
        proc_address = SDL_GL_GetProcAddress("glGetQueryObjectiv");
        memcpy(&g_gl.GetQueryObjectiv, &proc_address, sizeof(proc_address));
        proc_address = SDL_GL_GetProcAddress("glGetQueryObjectui64v");
        memcpy(&g_gl.GetQueryObjectui64v, &proc_address, sizeof(proc_address));
    }

#undef HENKA_GL_LOAD
    return true;
}

static GLenum henka_mesh_primitive_to_gl(henka_mesh_primitive primitive)
{
    switch (primitive)
    {
        case HENKA_MESH_PRIMITIVE_LINES:
            return GL_LINES;
        case HENKA_MESH_PRIMITIVE_TRIANGLES:
        default:
            return GL_TRIANGLES;
    }
}

static char* henka_read_text_file(const char* path)
{
    char* buffer;
    FILE* file;
    long file_length;
    size_t allocation_size;
    size_t bytes_read;
    size_t length;

    if (path == NULL)
    {
        return NULL;
    }

    file = NULL;
    if (fopen_s(&file, path, "rb") != 0 || file == NULL)
    {
        return NULL;
    }

    if (fseek(file, 0L, SEEK_END) != 0)
    {
        fclose(file);
        return NULL;
    }

    file_length = ftell(file);
    if (file_length < 0L || (size_t)file_length > HENKA_MAX_SHADER_SOURCE_BYTES)
    {
        fclose(file);
        return NULL;
    }

    if (fseek(file, 0L, SEEK_SET) != 0)
    {
        fclose(file);
        return NULL;
    }

    length = (size_t)file_length;
    if (!henka_checked_size_add(length, 1U, &allocation_size))
    {
        fclose(file);
        return NULL;
    }

    buffer = henka_malloc(allocation_size);
    if (buffer == NULL)
    {
        fclose(file);
        return NULL;
    }

    bytes_read = fread(buffer, 1U, length, file);
    if (bytes_read != length || ferror(file))
    {
        fclose(file);
        henka_free(buffer);
        return NULL;
    }

    if (fclose(file) != 0)
    {
        henka_free(buffer);
        return NULL;
    }

    buffer[length] = '\0';
    return buffer;
}

static bool henka_compile_program_from_source(
    const char* vertex_source,
    const char* fragment_source,
    const char* vertex_label,
    const char* fragment_label,
    GLuint* out_program)
{
    GLuint fragment_shader;
    GLuint program;
    GLuint vertex_shader;

    if (vertex_source == NULL || fragment_source == NULL || out_program == NULL)
    {
        return false;
    }

    *out_program = 0U;
    vertex_shader = g_gl.CreateShader(GL_VERTEX_SHADER);
    fragment_shader = g_gl.CreateShader(GL_FRAGMENT_SHADER);
    if (!henka_compile_shader(vertex_shader, vertex_source, vertex_label) ||
        !henka_compile_shader(fragment_shader, fragment_source, fragment_label))
    {
        g_gl.DeleteShader(vertex_shader);
        g_gl.DeleteShader(fragment_shader);
        return false;
    }

    program = g_gl.CreateProgram();
    g_gl.AttachShader(program, vertex_shader);
    g_gl.AttachShader(program, fragment_shader);
    if (!henka_link_program(program))
    {
        g_gl.DeleteProgram(program);
        g_gl.DeleteShader(vertex_shader);
        g_gl.DeleteShader(fragment_shader);
        return false;
    }

    g_gl.DeleteShader(vertex_shader);
    g_gl.DeleteShader(fragment_shader);
    *out_program = program;
    return true;
}

static bool henka_compile_shader(GLuint shader, const char* source, const char* label)
{
    GLint compile_status;
    char info_log[1024];

    g_gl.ShaderSource(shader, 1, &source, NULL);
    g_gl.CompileShader(shader);
    g_gl.GetShaderiv(shader, GL_COMPILE_STATUS, &compile_status);
    if (compile_status == GL_FALSE)
    {
        g_gl.GetShaderInfoLog(shader, (GLsizei)sizeof(info_log), NULL, info_log);
        HENKA_LOG_ERROR("%s compile failed: %s", label, info_log);
        return false;
    }

    return true;
}

static bool henka_link_program(GLuint program)
{
    GLint link_status;
    char info_log[1024];

    g_gl.LinkProgram(program);
    g_gl.GetProgramiv(program, GL_LINK_STATUS, &link_status);
    if (link_status == GL_FALSE)
    {
        g_gl.GetProgramInfoLog(program, (GLsizei)sizeof(info_log), NULL, info_log);
        HENKA_LOG_ERROR("shader link failed: %s", info_log);
        return false;
    }

    return true;
}

static uint64_t henka_shader_source_hash(
    const char* vertex_source,
    const char* fragment_source)
{
    const unsigned char* bytes;
    uint64_t hash = UINT64_C(1469598103934665603);

    bytes = (const unsigned char*)vertex_source;
    while (bytes != NULL && *bytes != 0U)
    {
        hash ^= (uint64_t)*bytes++;
        hash *= UINT64_C(1099511628211);
    }
    hash ^= UINT64_C(0xff);
    bytes = (const unsigned char*)fragment_source;
    while (bytes != NULL && *bytes != 0U)
    {
        hash ^= (uint64_t)*bytes++;
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static bool henka_shader_location_table_add(
    henka_opengl_shader_data* shader_data,
    const char* name,
    GLint location)
{
    size_t index;

    if (shader_data == NULL || name == NULL || location < 0)
    {
        return false;
    }
    for (index = 0U; index < shader_data->location_count; ++index)
    {
        if (strcmp(shader_data->locations[index].name, name) == 0)
        {
            return shader_data->locations[index].location == location;
        }
    }
    if (shader_data->location_count >=
        sizeof(shader_data->locations) / sizeof(shader_data->locations[0]) ||
        strlen(name) >= sizeof(shader_data->locations[0].name))
    {
        return false;
    }
    (void)snprintf(
        shader_data->locations[shader_data->location_count].name,
        sizeof(shader_data->locations[shader_data->location_count].name),
        "%s",
        name);
    shader_data->locations[shader_data->location_count].location = location;
    ++shader_data->location_count;
    return true;
}

static bool henka_populate_shader_location_table(
    GLuint program,
    const char* label,
    henka_shader_contract_type contract_type,
    uint32_t contract_version,
    const char* const* names,
    size_t name_count,
    henka_opengl_shader_data* out_shader_data)
{
    size_t index;

    if (program == 0U || out_shader_data == NULL || names == NULL ||
        SDL_GL_GetCurrentContext() == NULL || contract_version != 1U)
    {
        return false;
    }
    memset(out_shader_data, 0, sizeof(*out_shader_data));
    out_shader_data->program = program;
    out_shader_data->context = SDL_GL_GetCurrentContext();
    out_shader_data->contract_type = contract_type;
    out_shader_data->contract_version = contract_version;
    for (index = 0U; index < name_count; ++index)
    {
        GLint location;

        if (names[index] == NULL || names[index][0] == '\0')
        {
            HENKA_LOG_ERROR(
                "shader contract rejected for '%s': empty location name",
                label != NULL ? label : "shader");
            return false;
        }
        location = g_gl.GetUniformLocation(program, names[index]);
        if (location < 0 || !henka_shader_location_table_add(
                out_shader_data,
                names[index],
                location))
        {
            HENKA_LOG_ERROR(
                "shader contract rejected for '%s': required uniform '%s' is missing or location table capacity was exceeded",
                label != NULL ? label : "shader",
                names[index]);
            return false;
        }
    }
    return true;
}

static bool henka_validate_shader_contract(
    GLuint program,
    const char* label,
    henka_shader_contract_type contract_type,
    uint32_t contract_version,
    henka_opengl_shader_data* out_shader_data)
{
    static const char* minimal_uniforms[] =
    {
        "model",
        "view",
        "projection",
        "baseColor"
    };
    static const char* material_uniforms[] =
    {
        "model", "view", "projection", "lightMatrix", "baseColor",
        "baseColorTexture", "useTexture", "useVertexColor", "cameraPosition",
        "lightDirection", "lightColor", "lightIntensity", "ambientColor",
        "useLighting", "useEnvironment", "environmentGroundColor",
        "environmentHorizonColor", "environmentZenithColor", "environmentIntensity",
        "fogEnabled", "fogMode", "fogColor", "fogStartDistance", "fogEndDistance",
        "fogDensity", "normalTexture", "metallicRoughnessTexture", "occlusionTexture",
        "emissiveTexture",
        "useNormalTexture", "useMetallicRoughnessTexture",
        "useOcclusionTexture", "useEmissiveTexture", "metallic", "roughness",
        "normalScale", "occlusionStrength", "emissiveColor", "emissiveStrength",
        "specularFactor", "specularColor", "ior", "transmission",
        "clearcoat", "clearcoatRoughness", "sheenColor", "sheenRoughness",
        "alphaMode", "alphaCutoff", "shadowMap", "useShadowMap",
        "localShadowMap", "useLocalShadowMap", "localShadowMatrix",
        "cascadeShadowMap", "useCascadeShadowMap", "cascadeShadowMatrix", "cascadeSplitDistance",
        "pointShadowMap", "usePointShadowMap", "pointShadowLightPosition", "pointShadowFarPlane",
        "environmentTexture", "useEnvironmentTexture", "environmentRotation",
        "localLightCount", "localLightPositionRange[0]",
        "localLightColorIntensity[0]", "localLightDirectionInner[0]",
        "localLightOuterType[0]"
    };
    const char* const* required_uniforms = contract_type == HENKA_SHADER_CONTRACT_MATERIAL ?
        material_uniforms : minimal_uniforms;
    size_t required_count = contract_type == HENKA_SHADER_CONTRACT_MATERIAL ?
        sizeof(material_uniforms) / sizeof(material_uniforms[0]) :
        sizeof(minimal_uniforms) / sizeof(minimal_uniforms[0]);
    if (program == 0U || out_shader_data == NULL ||
        contract_type < HENKA_SHADER_CONTRACT_MINIMAL_GEOMETRY ||
        contract_type > HENKA_SHADER_CONTRACT_MATERIAL ||
        contract_version != 1U)
    {
        return false;
    }
    return henka_populate_shader_location_table(
        program,
        label,
        contract_type,
        contract_version,
        required_uniforms,
        required_count,
        out_shader_data);
}

static void henka_add_optional_shader_locations(
    GLuint program,
    henka_opengl_shader_data* shader_data)
{
    static const char* optional_names[] =
    {
        "iblIrradianceMap", "iblPrefilterMap", "iblBrdfLut", "useIBL",
        "reflectionProbePosition", "reflectionProbeExtents", "useReflectionProbe",
        "reflectionProbeMap", "useReflectionProbeMap", "doubleSided",
        "previousViewProjection", "previousModel", "useMotionVectors",
        "useInstancing", "thickness", "attenuationDistance", "attenuationColor"
    };
    size_t index;

    if (program == 0U || shader_data == NULL)
        return;
    for (index = 0U; index < sizeof(optional_names) / sizeof(optional_names[0]); ++index)
    {
        GLint location = g_gl.GetUniformLocation(program, optional_names[index]);
        if (location >= 0)
            (void)henka_shader_location_table_add(shader_data, optional_names[index], location);
    }
}

static void henka_set_uniform_mat4(GLuint program, const char* name, henka_mat4 value)
{
    GLint location;

    location = henka_opengl_uniform_location(program, name);
    if (location >= 0)
    {
        g_gl.UniformMatrix4fv(location, 1, GL_FALSE, value.m);
    }
}

static void henka_set_uniform_vec4(GLuint program, const char* name, henka_vec4 value)
{
    GLint location;

    location = henka_opengl_uniform_location(program, name);
    if (location >= 0)
    {
        g_gl.Uniform4f(location, value.x, value.y, value.z, value.w);
    }
}

static void henka_set_uniform_vec3(GLuint program, const char* name, henka_vec3 value)
{
    GLint location;

    location = henka_opengl_uniform_location(program, name);
    if (location >= 0)
    {
        g_gl.Uniform3f(location, value.x, value.y, value.z);
    }
}

static void henka_set_uniform_bool(GLuint program, const char* name, bool value)
{
    GLint location;

    location = henka_opengl_uniform_location(program, name);
    if (location >= 0)
    {
        g_gl.Uniform1i(location, value ? 1 : 0);
    }
}

static void henka_set_uniform_int(GLuint program, const char* name, int value)
{
    GLint location;

    location = henka_opengl_uniform_location(program, name);
    if (location >= 0)
    {
        g_gl.Uniform1i(location, value);
    }
}

static void henka_set_uniform_float(GLuint program, const char* name, float value)
{
    GLint location = henka_opengl_uniform_location(program, name);
    if (location >= 0)
    {
        g_gl.Uniform1f(location, value);
    }
}

static GLint henka_opengl_shader_uniform_location(
    GLuint program,
    const henka_opengl_shader_data* shader_data,
    const char* name)
{
    size_t index;

    if (shader_data != NULL && shader_data->program == program &&
        shader_data->context == SDL_GL_GetCurrentContext())
    {
        for (index = 0U; index < shader_data->location_count; ++index)
        {
            if (strcmp(shader_data->locations[index].name, name) == 0)
            {
                return shader_data->locations[index].location;
            }
        }
        return -1;
    }
    return henka_opengl_uniform_location(program, name);
}

static void henka_set_uniform_mat4_owned(
    GLuint program,
    const henka_opengl_shader_data* shader_data,
    const char* name,
    henka_mat4 value)
{
    GLint location = henka_opengl_shader_uniform_location(program, shader_data, name);
    if (location >= 0)
    {
        g_gl.UniformMatrix4fv(location, 1, GL_FALSE, value.m);
    }
}

static void henka_set_uniform_vec4_owned(
    GLuint program,
    const henka_opengl_shader_data* shader_data,
    const char* name,
    henka_vec4 value)
{
    GLint location = henka_opengl_shader_uniform_location(program, shader_data, name);
    if (location >= 0)
    {
        g_gl.Uniform4f(location, value.x, value.y, value.z, value.w);
    }
}

static void henka_set_uniform_vec2_owned(
    GLuint program,
    const henka_opengl_shader_data* shader_data,
    const char* name,
    henka_vec2 value)
{
    GLint location = henka_opengl_shader_uniform_location(program, shader_data, name);
    if (location >= 0)
    {
        g_gl.Uniform2f(location, value.x, value.y);
    }
}

static void henka_set_uniform_vec4_array_owned(
    GLuint program,
    const henka_opengl_shader_data* shader_data,
    const char* name,
    const float* values,
    int count)
{
    GLint location = henka_opengl_shader_uniform_location(program, shader_data, name);
    if (location >= 0 && values != NULL && count > 0)
    {
        g_gl.Uniform4fv(location, count, values);
    }
}

static void henka_set_uniform_vec3_owned(
    GLuint program,
    const henka_opengl_shader_data* shader_data,
    const char* name,
    henka_vec3 value)
{
    GLint location = henka_opengl_shader_uniform_location(program, shader_data, name);
    if (location >= 0)
    {
        g_gl.Uniform3f(location, value.x, value.y, value.z);
    }
}

static void henka_set_uniform_bool_owned(
    GLuint program,
    const henka_opengl_shader_data* shader_data,
    const char* name,
    bool value)
{
    GLint location = henka_opengl_shader_uniform_location(program, shader_data, name);
    if (location >= 0)
    {
        g_gl.Uniform1i(location, value ? 1 : 0);
    }
}

static void henka_set_uniform_int_owned(
    GLuint program,
    const henka_opengl_shader_data* shader_data,
    const char* name,
    int value)
{
    GLint location = henka_opengl_shader_uniform_location(program, shader_data, name);
    if (location >= 0)
    {
        g_gl.Uniform1i(location, value);
    }
}

static void henka_set_uniform_float_owned(
    GLuint program,
    const henka_opengl_shader_data* shader_data,
    const char* name,
    float value)
{
    GLint location = henka_opengl_shader_uniform_location(program, shader_data, name);
    if (location >= 0)
    {
        g_gl.Uniform1f(location, value);
    }
}

static henka_result henka_opengl_renderer_create_ui_resources(
    GLuint* out_program,
    GLuint* out_vertex_array,
    GLuint* out_vertex_buffer)
{
    static const char* g_ui_vertex_shader_source =
        "#version 330 core\n"
        "layout(location = 0) in vec2 inPosition;\n"
        "layout(location = 1) in vec4 inColor;\n"
        "uniform vec2 framebufferSize;\n"
        "out vec4 vertexColor;\n"
        "void main(void)\n"
        "{\n"
        "    vec2 clip = vec2((inPosition.x / framebufferSize.x) * 2.0 - 1.0,\n"
        "                     1.0 - (inPosition.y / framebufferSize.y) * 2.0);\n"
        "    gl_Position = vec4(clip, 0.0, 1.0);\n"
        "    vertexColor = inColor;\n"
        "}\n";
    static const char* g_ui_fragment_shader_source =
        "#version 330 core\n"
        "in vec4 vertexColor;\n"
        "out vec4 fragmentColor;\n"
        "void main(void)\n"
        "{\n"
        "    fragmentColor = vertexColor;\n"
        "}\n";

    if (!henka_compile_program_from_source(
            g_ui_vertex_shader_source,
            g_ui_fragment_shader_source,
            "ui overlay vertex shader",
            "ui overlay fragment shader",
            out_program))
    {
        return HENKA_ERROR_RENDERER;
    }

    g_gl.GenVertexArrays(1, out_vertex_array);
    g_gl.GenBuffers(1, out_vertex_buffer);
    g_gl.BindVertexArray(*out_vertex_array);
    g_gl.BindBuffer(GL_ARRAY_BUFFER, *out_vertex_buffer);
    g_gl.EnableVertexAttribArray(0);
    g_gl.VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(henka_ui_vertex), (const void*)offsetof(henka_ui_vertex, x));
    g_gl.EnableVertexAttribArray(1);
    g_gl.VertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(henka_ui_vertex), (const void*)offsetof(henka_ui_vertex, r));
    g_gl.BindVertexArray(0);
    g_gl.BindBuffer(GL_ARRAY_BUFFER, 0);
    return HENKA_SUCCESS;
}

static henka_result
henka_opengl_renderer_create_viewport_program(
    GLuint* out_program)
{
    static const char* vertex_source =
        "#version 330 core\n"
        "layout(location = 0) in vec3 inPosition;\n"
        "layout(location = 1) in vec3 inNormal;\n"
        "layout(location = 2) in vec2 inUv;\n"
        "uniform mat4 model;\n"
        "uniform mat4 view;\n"
        "uniform mat4 projection;\n"
        "out vec3 fragNormal;\n"
        "out vec2 fragUv;\n"
        "void main(void)\n"
        "{\n"
        "    mat3 normalMatrix = transpose(inverse(mat3(model)));\n"
        "    fragNormal = normalMatrix * inNormal;\n"
        "    fragUv = inUv;\n"
        "    gl_Position = projection * view * model * vec4(inPosition, 1.0);\n"
        "}\n";
    static const char* fragment_source =
        "#version 330 core\n"
        "in vec3 fragNormal;\n"
        "in vec2 fragUv;\n"
        "uniform vec4 baseColor;\n"
        "uniform sampler2D baseColorTexture;\n"
        "uniform bool useTexture;\n"
        "uniform vec3 lightDirection;\n"
        "uniform vec3 ambientColor;\n"
        "uniform bool useLighting;\n"
        "out vec4 outColor;\n"
        "void main(void)\n"
        "{\n"
        "    vec4 surfaceColor = baseColor;\n"
        "    vec3 lighting = vec3(1.0);\n"
        "    if (useTexture)\n"
        "    {\n"
        "        surfaceColor *= texture(baseColorTexture, fragUv);\n"
        "    }\n"
        "    if (useLighting)\n"
        "    {\n"
        "        vec3 normal = normalize(fragNormal);\n"
        "        vec3 lightDir = normalize(-lightDirection);\n"
        "        float diffuse = max(dot(normal, lightDir), 0.0);\n"
        "        lighting = ambientColor + vec3(diffuse);\n"
        "    }\n"
        "    outColor = vec4(surfaceColor.rgb * lighting, surfaceColor.a);\n"
        "}\n";

    if (out_program == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    *out_program = 0U;
    if (!henka_compile_program_from_source(
            vertex_source,
            fragment_source,
            "viewport editor vertex shader",
            "viewport editor fragment shader",
            out_program))
    {
        return HENKA_ERROR_RENDERER;
    }

    return HENKA_SUCCESS;
}

static void henka_opengl_delete_hdr_target(henka_opengl_renderer_state* state)
{
    uint64_t target_bytes = 0U;

    if (state != NULL && state->hdr_width > 0 && state->hdr_height > 0)
    {
        target_bytes = (uint64_t)state->hdr_width * (uint64_t)state->hdr_height * 21U;
        henka_opengl_memory_remove_category(
            state,
            &state->tracked_render_target_bytes,
            target_bytes);
    }
    if (state->hdr_depth_buffer != 0U)
    {
        glDeleteTextures(1, &state->hdr_depth_buffer);
    }
    if (state->hdr_color_texture != 0U)
    {
        glDeleteTextures(1, &state->hdr_color_texture);
    }
    if (state->hdr_motion_texture != 0U)
    {
        glDeleteTextures(1, &state->hdr_motion_texture);
    }
    if (state->hdr_reactive_texture != 0U)
    {
        glDeleteTextures(1, &state->hdr_reactive_texture);
    }
    if (state->hdr_framebuffer != 0U)
    {
        g_gl.DeleteFramebuffers(1, &state->hdr_framebuffer);
    }
    state->hdr_depth_buffer = 0U;
    state->hdr_color_texture = 0U;
    state->hdr_motion_texture = 0U;
    state->hdr_reactive_texture = 0U;
    state->hdr_framebuffer = 0U;
    state->hdr_width = 0;
    state->hdr_height = 0;
}

static void henka_opengl_delete_bloom_target(henka_opengl_renderer_state* state)
{
    uint64_t target_bytes = 0U;

    if (state != NULL && state->bloom_width > 0 && state->bloom_height > 0)
    {
        target_bytes = (uint64_t)state->bloom_width *
            (uint64_t)state->bloom_height * 16U;
        henka_opengl_memory_remove_category(
            state,
            &state->tracked_render_target_bytes,
            target_bytes);
    }
    if (state != NULL)
    {
        if (state->bloom_color_texture != 0U)
            glDeleteTextures(1, &state->bloom_color_texture);
        if (state->bloom_blur_texture != 0U)
            glDeleteTextures(1, &state->bloom_blur_texture);
        if (state->bloom_framebuffer != 0U)
            g_gl.DeleteFramebuffers(1, &state->bloom_framebuffer);
        if (state->bloom_blur_framebuffer != 0U)
            g_gl.DeleteFramebuffers(1, &state->bloom_blur_framebuffer);
        state->bloom_color_texture = 0U;
        state->bloom_blur_texture = 0U;
        state->bloom_framebuffer = 0U;
        state->bloom_blur_framebuffer = 0U;
        state->bloom_width = 0;
        state->bloom_height = 0;
        state->bloom_ready = false;
    }
}

static void henka_opengl_delete_temporal_history(henka_opengl_renderer_state* state)
{
    if (state == NULL)
    {
        return;
    }
    if (state->temporal_history_width > 0 && state->temporal_history_height > 0)
    {
        henka_opengl_memory_remove_category(
            state,
            &state->tracked_render_target_bytes,
            (uint64_t)state->temporal_history_width *
                (uint64_t)state->temporal_history_height * 4U);
    }
    if (state->temporal_history_texture != 0U)
    {
        glDeleteTextures(1, &state->temporal_history_texture);
    }
    state->temporal_history_texture = 0U;
    state->temporal_history_width = 0;
    state->temporal_history_height = 0;
    state->temporal_history_ready = false;
    state->temporal_history_valid = false;
    state->temporal_fallback_active = true;
    state->temporal_jitter_enabled = false;
    state->temporal_jitter_index = 0U;
    state->temporal_jitter_x = 0.0f;
    state->temporal_jitter_y = 0.0f;
    state->previous_view_projection_valid = false;
    (void)snprintf(
        state->temporal_invalidation_reason,
        sizeof(state->temporal_invalidation_reason),
        "history unavailable");
}

static henka_result henka_opengl_create_temporal_history(
    henka_opengl_renderer_state* state,
    int width,
    int height)
{
    GLuint texture = 0U;
    GLint previous_texture = 0;

    if (state == NULL || width <= 0 || height <= 0 || width > 8192 || height > 8192)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous_texture);
    glGenTextures(1, &texture);
    if (texture == 0U)
    {
        glBindTexture(GL_TEXTURE_2D, (GLuint)previous_texture);
        return HENKA_ERROR_RENDERER;
    }
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glBindTexture(GL_TEXTURE_2D, (GLuint)previous_texture);
    henka_opengl_delete_temporal_history(state);
    state->temporal_history_texture = texture;
    state->temporal_history_width = width;
    state->temporal_history_height = height;
    state->temporal_history_ready = true;
    state->temporal_history_valid = false;
    state->temporal_fallback_active = true;
    (void)snprintf(
        state->temporal_invalidation_reason,
        sizeof(state->temporal_invalidation_reason),
        "awaiting first frame");
    henka_opengl_memory_add_category(
        state,
        &state->tracked_render_target_bytes,
        (uint64_t)width * (uint64_t)height * 4U);
    return HENKA_SUCCESS;
}

static henka_result henka_opengl_create_bloom_target(
    henka_opengl_renderer_state* state,
    int width,
    int height)
{
    GLuint color_texture = 0U;
    GLuint blur_texture = 0U;
    GLuint framebuffer = 0U;
    GLuint blur_framebuffer = 0U;
    GLint previous_framebuffer = 0;
    GLint previous_texture = 0;
    int bloom_width;
    int bloom_height;

    if (state == NULL || width <= 0 || height <= 0 || width > 8192 || height > 8192)
    {
        if (state != NULL)
        {
            state->bloom_ready = false;
            (void)snprintf(state->bloom_failure_reason, sizeof(state->bloom_failure_reason), "invalid bloom target size");
        }
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    bloom_width = width > 1 ? width / 2 : 1;
    bloom_height = height > 1 ? height / 2 : 1;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previous_framebuffer);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous_texture);
    g_gl.GenFramebuffers(1, &framebuffer);
    g_gl.GenFramebuffers(1, &blur_framebuffer);
    glGenTextures(1, &color_texture);
    glGenTextures(1, &blur_texture);
    if (framebuffer == 0U || blur_framebuffer == 0U || color_texture == 0U || blur_texture == 0U)
    {
        if (color_texture != 0U) glDeleteTextures(1, &color_texture);
        if (blur_texture != 0U) glDeleteTextures(1, &blur_texture);
        if (framebuffer != 0U) g_gl.DeleteFramebuffers(1, &framebuffer);
        if (blur_framebuffer != 0U) g_gl.DeleteFramebuffers(1, &blur_framebuffer);
        state->bloom_ready = false;
        (void)snprintf(state->bloom_failure_reason, sizeof(state->bloom_failure_reason), "GPU object allocation failed");
        return HENKA_ERROR_RENDERER;
    }
    glBindTexture(GL_TEXTURE_2D, color_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, bloom_width, bloom_height, 0, GL_RGBA, GL_HALF_FLOAT, NULL);
    glBindTexture(GL_TEXTURE_2D, blur_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, bloom_width, bloom_height, 0, GL_RGBA, GL_HALF_FLOAT, NULL);
    g_gl.BindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    g_gl.FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color_texture, 0);
    if (g_gl.CheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        goto bloom_target_failure;
    g_gl.BindFramebuffer(GL_FRAMEBUFFER, blur_framebuffer);
    g_gl.FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, blur_texture, 0);
    if (g_gl.CheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        goto bloom_target_failure;
    g_gl.BindFramebuffer(GL_FRAMEBUFFER, (GLuint)previous_framebuffer);
    glBindTexture(GL_TEXTURE_2D, (GLuint)previous_texture);
    henka_opengl_delete_bloom_target(state);
    state->bloom_framebuffer = framebuffer;
    state->bloom_blur_framebuffer = blur_framebuffer;
    state->bloom_color_texture = color_texture;
    state->bloom_blur_texture = blur_texture;
    state->bloom_width = bloom_width;
    state->bloom_height = bloom_height;
    state->bloom_ready = true;
    state->bloom_failure_reason[0] = '\0';
    henka_opengl_memory_add_category(
        state,
        &state->tracked_render_target_bytes,
        (uint64_t)bloom_width * (uint64_t)bloom_height * 16U);
    return HENKA_SUCCESS;

bloom_target_failure:
    g_gl.BindFramebuffer(GL_FRAMEBUFFER, (GLuint)previous_framebuffer);
    glBindTexture(GL_TEXTURE_2D, (GLuint)previous_texture);
    glDeleteTextures(1, &color_texture);
    glDeleteTextures(1, &blur_texture);
    g_gl.DeleteFramebuffers(1, &framebuffer);
    g_gl.DeleteFramebuffers(1, &blur_framebuffer);
    state->bloom_ready = false;
    (void)snprintf(state->bloom_failure_reason, sizeof(state->bloom_failure_reason), "incomplete bloom framebuffer");
    return HENKA_ERROR_RENDERER;
}

#define HENKA_IBL_ENVIRONMENT_RESOLUTION 128
#define HENKA_IBL_IRRADIANCE_RESOLUTION 32
#define HENKA_IBL_PREFILTER_LEVELS 5
#define HENKA_IBL_BRDF_RESOLUTION 128
#define HENKA_REFLECTION_PROBE_RESOLUTION 64

static void henka_opengl_delete_reflection_probe_resources(
    henka_opengl_renderer_state* state)
{
    uint32_t index;

    if (state == NULL)
    {
        return;
    }
    for (index = 0U; index < HENKA_SCENE_MAX_REFLECTION_PROBES; ++index)
    {
        if (state->reflection_probe_cubes[index] != 0U)
        {
            glDeleteTextures(1, &state->reflection_probe_cubes[index]);
            henka_opengl_memory_remove_category(
                state,
                &state->tracked_render_target_bytes,
                (uint64_t)HENKA_REFLECTION_PROBE_RESOLUTION *
                    (uint64_t)HENKA_REFLECTION_PROBE_RESOLUTION * 6U * 8U);
        }
        state->reflection_probe_cubes[index] = 0U;
        state->reflection_probe_capture_ready[index] = false;
        state->reflection_probe_captured_scene_revision[index] = 0U;
        memset(&state->reflection_probe_captured_desc[index], 0,
            sizeof(state->reflection_probe_captured_desc[index]));
    }
    if (state->reflection_probe_framebuffer != 0U)
    {
        g_gl.DeleteFramebuffers(1, &state->reflection_probe_framebuffer);
        state->reflection_probe_framebuffer = 0U;
    }
    if (state->reflection_probe_depth_buffer != 0U)
    {
        g_gl.DeleteRenderbuffers(1, &state->reflection_probe_depth_buffer);
        henka_opengl_memory_remove_category(
            state,
            &state->tracked_render_target_bytes,
            (uint64_t)HENKA_REFLECTION_PROBE_RESOLUTION *
                (uint64_t)HENKA_REFLECTION_PROBE_RESOLUTION * 4U);
        state->reflection_probe_depth_buffer = 0U;
    }
    state->reflection_probe_capture_cursor = 0U;
    state->reflection_probe_capture_active = false;
    state->reflection_probe_capture_index = UINT32_MAX;
    state->reflection_probe_enabled_count = 0U;
    state->reflection_probe_captured_count = 0U;
    state->reflection_probe_capture_generation = 0U;
    state->reflection_probe_capture_failure_count = 0U;
}

static void henka_opengl_delete_ibl_resources(henka_opengl_renderer_state* state)
{
    uint64_t bytes = 0U;
    int mip;

    if (state == NULL)
        return;
    for (mip = 0; mip < HENKA_IBL_PREFILTER_LEVELS; ++mip)
    {
        int size = HENKA_IBL_ENVIRONMENT_RESOLUTION >> mip;
        bytes += (uint64_t)size * (uint64_t)size * 6U * 8U;
    }
    bytes += (uint64_t)HENKA_IBL_ENVIRONMENT_RESOLUTION * HENKA_IBL_ENVIRONMENT_RESOLUTION * 6U * 8U;
    bytes += (uint64_t)HENKA_IBL_IRRADIANCE_RESOLUTION * HENKA_IBL_IRRADIANCE_RESOLUTION * 6U * 8U;
    bytes += (uint64_t)HENKA_IBL_BRDF_RESOLUTION * HENKA_IBL_BRDF_RESOLUTION * 4U;
    if (state->ibl_environment_cube != 0U || state->ibl_irradiance_cube != 0U ||
        state->ibl_prefilter_cube != 0U || state->ibl_brdf_lut != 0U)
    {
        henka_opengl_memory_remove_category(
            state,
            &state->tracked_render_target_bytes,
            bytes);
    }
    if (state->ibl_environment_cube != 0U) glDeleteTextures(1, &state->ibl_environment_cube);
    if (state->ibl_irradiance_cube != 0U) glDeleteTextures(1, &state->ibl_irradiance_cube);
    if (state->ibl_prefilter_cube != 0U) glDeleteTextures(1, &state->ibl_prefilter_cube);
    if (state->ibl_brdf_lut != 0U) glDeleteTextures(1, &state->ibl_brdf_lut);
    if (state->ibl_framebuffer != 0U) g_gl.DeleteFramebuffers(1, &state->ibl_framebuffer);
    state->ibl_environment_cube = 0U;
    state->ibl_irradiance_cube = 0U;
    state->ibl_prefilter_cube = 0U;
    state->ibl_brdf_lut = 0U;
    state->ibl_framebuffer = 0U;
    state->ibl_ready = false;
}

static bool henka_opengl_allocate_ibl_cube(GLuint* out_texture, int resolution, int levels)
{
    GLuint texture = 0U;
    int face;
    int mip;

    if (out_texture == NULL || resolution <= 0 || levels <= 0)
        return false;
    glGenTextures(1, &texture);
    if (texture == 0U)
        return false;
    glBindTexture(GL_TEXTURE_CUBE_MAP, texture);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER,
        levels > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_LEVEL, levels - 1);
    for (mip = 0; mip < levels; ++mip)
    {
        int mip_resolution = resolution >> mip;
        if (mip_resolution < 1) mip_resolution = 1;
        for (face = 0; face < 6; ++face)
        {
            glTexImage2D(
                GL_TEXTURE_CUBE_MAP_POSITIVE_X + face,
                mip,
                GL_RGBA16F,
                mip_resolution,
                mip_resolution,
                0,
                GL_RGBA,
                GL_HALF_FLOAT,
                NULL);
        }
    }
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0U);
    *out_texture = texture;
    return true;
}

static henka_result henka_opengl_build_ibl_resources(
    henka_opengl_renderer_state* state,
    const henka_scene* scene)
{
    static const henka_vec3 face_directions[6] =
    {
        {1.0f, 0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f}, {0.0f, -1.0f, 0.0f},
        {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, -1.0f}
    };
    static const henka_vec3 face_ups[6] =
    {
        {0.0f, -1.0f, 0.0f}, {0.0f, -1.0f, 0.0f},
        {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, -1.0f},
        {0.0f, -1.0f, 0.0f}, {0.0f, -1.0f, 0.0f}
    };
    GLuint framebuffer = 0U;
    GLuint environment_cube = 0U;
    GLuint irradiance_cube = 0U;
    GLuint prefilter_cube = 0U;
    GLuint brdf_lut = 0U;
    GLint previous_framebuffer = 0;
    GLint previous_texture = 0;
    const henka_opengl_texture_data* source_data;
    henka_mat4 projection;
    int face;
    int mip;

    if (state == NULL || scene == NULL || scene->environment.hdr_texture == NULL ||
        scene->environment.hdr_texture->backend_data == NULL ||
        state->ibl_conversion_program == 0U || state->ibl_irradiance_program == 0U ||
        state->ibl_prefilter_program == 0U || state->ibl_brdf_program == 0U)
        return HENKA_ERROR_RENDERER;
    source_data = (const henka_opengl_texture_data*)scene->environment.hdr_texture->backend_data;
    if (source_data->texture_id == 0U)
        return HENKA_ERROR_RENDERER;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previous_framebuffer);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous_texture);
    g_gl.GenFramebuffers(1, &framebuffer);
    if (framebuffer == 0U ||
        !henka_opengl_allocate_ibl_cube(&environment_cube, HENKA_IBL_ENVIRONMENT_RESOLUTION, 1) ||
        !henka_opengl_allocate_ibl_cube(&irradiance_cube, HENKA_IBL_IRRADIANCE_RESOLUTION, 1) ||
        !henka_opengl_allocate_ibl_cube(&prefilter_cube, HENKA_IBL_ENVIRONMENT_RESOLUTION, HENKA_IBL_PREFILTER_LEVELS))
        goto ibl_failure;
    glGenTextures(1, &brdf_lut);
    if (brdf_lut == 0U)
        goto ibl_failure;
    glBindTexture(GL_TEXTURE_2D, brdf_lut);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, HENKA_IBL_BRDF_RESOLUTION, HENKA_IBL_BRDF_RESOLUTION, 0, GL_RG, GL_HALF_FLOAT, NULL);
    projection = henka_mat4_perspective(3.14159265359f * 0.5f, 1.0f, 0.1f, 10.0f);
    g_gl.BindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glViewport(0, 0, HENKA_IBL_ENVIRONMENT_RESOLUTION, HENKA_IBL_ENVIRONMENT_RESOLUTION);
    g_gl.UseProgram(state->ibl_conversion_program);
    henka_set_uniform_int_owned(state->ibl_conversion_program, &state->ibl_conversion_shader_data, "equirectangularTexture", 0);
    henka_set_uniform_float_owned(state->ibl_conversion_program, &state->ibl_conversion_shader_data, "rotation", scene->environment.hdr_rotation);
    g_gl.ActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, source_data->texture_id);
    for (face = 0; face < 6; ++face)
    {
        henka_mat4 view_projection = henka_mat4_multiply(
            projection,
            henka_mat4_look_at((henka_vec3){0.0f, 0.0f, 0.0f}, face_directions[face], face_ups[face]));
        g_gl.FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, environment_cube, 0);
        if (g_gl.CheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            (void)snprintf(state->ibl_failure_reason, sizeof(state->ibl_failure_reason), "environment cube framebuffer incomplete");
            goto ibl_failure;
        }
        henka_set_uniform_mat4_owned(state->ibl_conversion_program, &state->ibl_conversion_shader_data, "viewProjection", view_projection);
        g_gl.BindVertexArray(state->tone_vertex_array);
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }
    g_gl.UseProgram(state->ibl_irradiance_program);
    henka_set_uniform_int_owned(state->ibl_irradiance_program, &state->ibl_irradiance_shader_data, "environmentCube", 0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, environment_cube);
    glViewport(0, 0, HENKA_IBL_IRRADIANCE_RESOLUTION, HENKA_IBL_IRRADIANCE_RESOLUTION);
    for (face = 0; face < 6; ++face)
    {
        henka_mat4 view_projection = henka_mat4_multiply(
            projection,
            henka_mat4_look_at((henka_vec3){0.0f, 0.0f, 0.0f}, face_directions[face], face_ups[face]));
        g_gl.FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, irradiance_cube, 0);
        if (g_gl.CheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            (void)snprintf(state->ibl_failure_reason, sizeof(state->ibl_failure_reason), "irradiance cube framebuffer incomplete");
            goto ibl_failure;
        }
        henka_set_uniform_mat4_owned(state->ibl_irradiance_program, &state->ibl_irradiance_shader_data, "viewProjection", view_projection);
        g_gl.BindVertexArray(state->tone_vertex_array);
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }
    g_gl.UseProgram(state->ibl_prefilter_program);
    henka_set_uniform_int_owned(state->ibl_prefilter_program, &state->ibl_prefilter_shader_data, "environmentCube", 0);
    for (mip = 0; mip < HENKA_IBL_PREFILTER_LEVELS; ++mip)
    {
        int resolution = HENKA_IBL_ENVIRONMENT_RESOLUTION >> mip;
        float roughness = (float)mip / (float)(HENKA_IBL_PREFILTER_LEVELS - 1);
        if (resolution < 1) resolution = 1;
        glViewport(0, 0, resolution, resolution);
        henka_set_uniform_float_owned(state->ibl_prefilter_program, &state->ibl_prefilter_shader_data, "roughness", roughness);
        for (face = 0; face < 6; ++face)
        {
            henka_mat4 view_projection = henka_mat4_multiply(
                projection,
                henka_mat4_look_at((henka_vec3){0.0f, 0.0f, 0.0f}, face_directions[face], face_ups[face]));
            g_gl.FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, prefilter_cube, mip);
            if (g_gl.CheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            {
                (void)snprintf(state->ibl_failure_reason, sizeof(state->ibl_failure_reason), "prefilter cube framebuffer incomplete");
                goto ibl_failure;
            }
            henka_set_uniform_mat4_owned(state->ibl_prefilter_program, &state->ibl_prefilter_shader_data, "viewProjection", view_projection);
            g_gl.BindVertexArray(state->tone_vertex_array);
            glDrawArrays(GL_TRIANGLES, 0, 3);
        }
    }
    g_gl.BindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    g_gl.FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, brdf_lut, 0);
    if (g_gl.CheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        (void)snprintf(state->ibl_failure_reason, sizeof(state->ibl_failure_reason), "BRDF LUT framebuffer incomplete");
        goto ibl_failure;
    }
    glViewport(0, 0, HENKA_IBL_BRDF_RESOLUTION, HENKA_IBL_BRDF_RESOLUTION);
    g_gl.UseProgram(state->ibl_brdf_program);
    g_gl.BindVertexArray(state->tone_vertex_array);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    g_gl.BindVertexArray(0);
    g_gl.UseProgram(0);
    g_gl.BindFramebuffer(GL_FRAMEBUFFER, (GLuint)previous_framebuffer);
    glBindTexture(GL_TEXTURE_2D, (GLuint)previous_texture);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0U);
    henka_opengl_delete_ibl_resources(state);
    state->ibl_framebuffer = framebuffer;
    state->ibl_environment_cube = environment_cube;
    state->ibl_irradiance_cube = irradiance_cube;
    state->ibl_prefilter_cube = prefilter_cube;
    state->ibl_brdf_lut = brdf_lut;
    state->ibl_ready = true;
    {
        uint64_t bytes = 0U;
        for (mip = 0; mip < HENKA_IBL_PREFILTER_LEVELS; ++mip)
        {
            int size = HENKA_IBL_ENVIRONMENT_RESOLUTION >> mip;
            bytes += (uint64_t)size * (uint64_t)size * 6U * 8U;
        }
        bytes += (uint64_t)HENKA_IBL_ENVIRONMENT_RESOLUTION * HENKA_IBL_ENVIRONMENT_RESOLUTION * 6U * 8U;
        bytes += (uint64_t)HENKA_IBL_IRRADIANCE_RESOLUTION * HENKA_IBL_IRRADIANCE_RESOLUTION * 6U * 8U;
        bytes += (uint64_t)HENKA_IBL_BRDF_RESOLUTION * HENKA_IBL_BRDF_RESOLUTION * 4U;
        henka_opengl_memory_add_category(state, &state->tracked_render_target_bytes, bytes);
    }
    return HENKA_SUCCESS;

ibl_failure:
    g_gl.BindFramebuffer(GL_FRAMEBUFFER, (GLuint)previous_framebuffer);
    glBindTexture(GL_TEXTURE_2D, (GLuint)previous_texture);
    if (environment_cube != 0U) glDeleteTextures(1, &environment_cube);
    if (irradiance_cube != 0U) glDeleteTextures(1, &irradiance_cube);
    if (prefilter_cube != 0U) glDeleteTextures(1, &prefilter_cube);
    if (brdf_lut != 0U) glDeleteTextures(1, &brdf_lut);
    if (framebuffer != 0U) g_gl.DeleteFramebuffers(1, &framebuffer);
    g_gl.BindVertexArray(0);
    g_gl.UseProgram(0);
    if (state->ibl_failure_reason[0] == '\0')
        (void)snprintf(state->ibl_failure_reason, sizeof(state->ibl_failure_reason), "derived IBL allocation or render failed");
    return HENKA_ERROR_RENDERER;
}

static void henka_opengl_sync_ibl_resources(
    henka_opengl_renderer_state* state,
    const henka_scene* scene)
{
    const henka_texture* source;
    uint64_t revision;
    float rotation;

    if (state == NULL || scene == NULL)
        return;
    source = scene->environment.hdr_texture;
    revision = source != NULL ? source->content_revision : 0U;
    rotation = scene->environment.hdr_rotation;
    if (source == NULL || source->backend_data == NULL)
    {
        if (state->ibl_ready || state->ibl_environment_cube != 0U)
            henka_opengl_delete_ibl_resources(state);
        state->ibl_source_texture = NULL;
        state->ibl_source_revision = 0U;
        state->ibl_source_rotation = 0.0f;
        state->ibl_failure_reason[0] = '\0';
        return;
    }
    if (state->ibl_source_texture == source && state->ibl_source_revision == revision &&
        state->ibl_source_rotation == rotation &&
        (state->ibl_ready || state->ibl_failure_reason[0] != '\0'))
        return;
    state->ibl_source_texture = source;
    state->ibl_source_revision = revision;
    state->ibl_source_rotation = rotation;
    state->ibl_failure_reason[0] = '\0';
    state->ibl_ready = false;
    if (henka_opengl_build_ibl_resources(state, scene) != HENKA_SUCCESS)
    {
        state->ibl_ready = false;
        if (state->ibl_failure_reason[0] == '\0')
            (void)snprintf(state->ibl_failure_reason, sizeof(state->ibl_failure_reason), "derived IBL target unavailable");
    }
    else
    {
        state->ibl_failure_reason[0] = '\0';
    }
}

static void henka_opengl_delete_shadow_target(henka_opengl_renderer_state* state)
{
    uint64_t target_bytes = 0U;

    if (state != NULL && state->shadow_resolution > 0)
    {
        target_bytes = (uint64_t)state->shadow_resolution *
            (uint64_t)state->shadow_resolution * 4U;
        henka_opengl_memory_remove_category(
            state,
            &state->tracked_render_target_bytes,
            target_bytes);
    }
    if (state->shadow_depth_texture != 0U)
    {
        glDeleteTextures(1, &state->shadow_depth_texture);
    }
    if (state->shadow_framebuffer != 0U)
    {
        g_gl.DeleteFramebuffers(1, &state->shadow_framebuffer);
    }
    state->shadow_depth_texture = 0U;
    state->shadow_framebuffer = 0U;
    state->shadow_resolution = 0;
    state->shadow_framebuffer_complete = false;
}

static void henka_opengl_delete_local_shadow_target(henka_opengl_renderer_state* state)
{
    uint64_t target_bytes = 0U;

    if (state != NULL && state->local_shadow_resolution > 0)
    {
        target_bytes = (uint64_t)state->local_shadow_resolution *
            (uint64_t)state->local_shadow_resolution * 4U;
        henka_opengl_memory_remove_category(
            state,
            &state->tracked_render_target_bytes,
            target_bytes);
    }
    if (state != NULL && state->local_shadow_depth_texture != 0U)
    {
        glDeleteTextures(1, &state->local_shadow_depth_texture);
    }
    if (state != NULL && state->local_shadow_framebuffer != 0U)
    {
        g_gl.DeleteFramebuffers(1, &state->local_shadow_framebuffer);
    }
    if (state != NULL)
    {
        state->local_shadow_depth_texture = 0U;
        state->local_shadow_framebuffer = 0U;
        state->local_shadow_resolution = 0;
        state->local_shadow_framebuffer_complete = false;
    }
}

static void henka_opengl_delete_cascade_shadow_target(henka_opengl_renderer_state* state)
{
    uint64_t target_bytes = 0U;

    if (state != NULL && state->cascade_shadow_resolution > 0)
    {
        target_bytes = (uint64_t)state->cascade_shadow_resolution *
            (uint64_t)state->cascade_shadow_resolution * 4U;
        henka_opengl_memory_remove_category(
            state,
            &state->tracked_render_target_bytes,
            target_bytes);
    }
    if (state != NULL && state->cascade_shadow_depth_texture != 0U)
    {
        glDeleteTextures(1, &state->cascade_shadow_depth_texture);
    }
    if (state != NULL && state->cascade_shadow_framebuffer != 0U)
    {
        g_gl.DeleteFramebuffers(1, &state->cascade_shadow_framebuffer);
    }
    if (state != NULL)
    {
        state->cascade_shadow_depth_texture = 0U;
        state->cascade_shadow_framebuffer = 0U;
        state->cascade_shadow_resolution = 0;
        state->cascade_shadow_framebuffer_complete = false;
    }
}

static void henka_opengl_delete_point_shadow_target(henka_opengl_renderer_state* state)
{
    uint64_t target_bytes = 0U;

    if (state != NULL && state->point_shadow_resolution > 0)
    {
        target_bytes = (uint64_t)state->point_shadow_resolution *
            (uint64_t)state->point_shadow_resolution * 6U * 4U;
        henka_opengl_memory_remove_category(
            state,
            &state->tracked_render_target_bytes,
            target_bytes);
    }
    if (state != NULL && state->point_shadow_depth_texture != 0U)
    {
        glDeleteTextures(1, &state->point_shadow_depth_texture);
    }
    if (state != NULL && state->point_shadow_framebuffer != 0U)
    {
        g_gl.DeleteFramebuffers(1, &state->point_shadow_framebuffer);
    }
    if (state != NULL)
    {
        state->point_shadow_depth_texture = 0U;
        state->point_shadow_framebuffer = 0U;
        state->point_shadow_resolution = 0;
        state->point_shadow_framebuffer_complete = false;
    }
}

static henka_result henka_opengl_create_hdr_target(
    henka_opengl_renderer_state* state,
    int width,
    int height)
{
    GLuint color_texture = 0U;
    GLuint motion_texture = 0U;
    GLuint reactive_texture = 0U;
    GLuint depth_buffer = 0U;
    GLuint framebuffer = 0U;
    GLint previous_framebuffer = 0;
    GLint previous_texture = 0;

    if (state == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    state->hdr_requested_width = width;
    state->hdr_requested_height = height;
    if (width <= 0 || height <= 0)
    {
        (void)snprintf(state->hdr_failure_reason, sizeof(state->hdr_failure_reason), "invalid target size");
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previous_framebuffer);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous_texture);
    g_gl.GenFramebuffers(1, &framebuffer);
    glGenTextures(1, &color_texture);
    glGenTextures(1, &motion_texture);
    glGenTextures(1, &depth_buffer);
    glGenTextures(1, &reactive_texture);
    if (framebuffer == 0U || color_texture == 0U || motion_texture == 0U ||
        reactive_texture == 0U || depth_buffer == 0U)
    {
        if (depth_buffer != 0U) glDeleteTextures(1, &depth_buffer);
        if (color_texture != 0U) glDeleteTextures(1, &color_texture);
        if (motion_texture != 0U) glDeleteTextures(1, &motion_texture);
        if (reactive_texture != 0U) glDeleteTextures(1, &reactive_texture);
        if (framebuffer != 0U) g_gl.DeleteFramebuffers(1, &framebuffer);
        (void)snprintf(state->hdr_failure_reason, sizeof(state->hdr_failure_reason), "GPU object allocation failed");
        return HENKA_ERROR_RENDERER;
    }
    g_gl.BindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glBindTexture(GL_TEXTURE_2D, color_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_HALF_FLOAT, NULL);
    g_gl.FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color_texture, 0);
    glBindTexture(GL_TEXTURE_2D, motion_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, width, height, 0, GL_RG, GL_HALF_FLOAT, NULL);
    g_gl.FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, motion_texture, 0);
    glBindTexture(GL_TEXTURE_2D, reactive_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
    g_gl.FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, reactive_texture, 0);
    {
        static const GLenum draw_buffers[] = {
            GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2};
        if (g_gl.DrawBuffers != NULL)
            g_gl.DrawBuffers(3, draw_buffers);
    }
    glBindTexture(GL_TEXTURE_2D, depth_buffer);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, NULL);
    g_gl.FramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth_buffer, 0);
    if (g_gl.CheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        (void)snprintf(state->hdr_failure_reason, sizeof(state->hdr_failure_reason), "incomplete HDR framebuffer");
        g_gl.BindFramebuffer(GL_FRAMEBUFFER, (GLuint)previous_framebuffer);
        glBindTexture(GL_TEXTURE_2D, (GLuint)previous_texture);
        glDeleteTextures(1, &depth_buffer);
        glDeleteTextures(1, &color_texture);
        glDeleteTextures(1, &motion_texture);
        glDeleteTextures(1, &reactive_texture);
        g_gl.DeleteFramebuffers(1, &framebuffer);
        return HENKA_ERROR_RENDERER;
    }
    g_gl.BindFramebuffer(GL_FRAMEBUFFER, (GLuint)previous_framebuffer);
    glBindTexture(GL_TEXTURE_2D, (GLuint)previous_texture);
    henka_opengl_delete_hdr_target(state);
    state->hdr_framebuffer = framebuffer;
    state->hdr_color_texture = color_texture;
    state->hdr_motion_texture = motion_texture;
    state->hdr_reactive_texture = reactive_texture;
    state->hdr_depth_buffer = depth_buffer;
    state->hdr_width = width;
    state->hdr_height = height;
    state->hdr_generation = state->hdr_generation == UINT64_MAX ? 1U : state->hdr_generation + 1U;
    state->hdr_framebuffer_complete = true;
    state->hdr_failure_reason[0] = '\0';
    {
        uint64_t target_bytes = (uint64_t)width * (uint64_t)height * 21U;
        henka_opengl_memory_add_category(
            state,
            &state->tracked_render_target_bytes,
            target_bytes);
    }
    return HENKA_SUCCESS;
}

static henka_result henka_opengl_create_shadow_target(
    henka_opengl_renderer_state* state,
    int resolution)
{
    GLuint depth_texture = 0U;
    GLuint framebuffer = 0U;
    GLint previous_framebuffer = 0;
    GLint previous_texture = 0;
    GLint previous_draw_buffer = GL_BACK;
    GLint previous_read_buffer = GL_BACK;

    if (state == NULL || resolution <= 0)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (resolution > 4096)
    {
        (void)snprintf(state->shadow_failure_reason, sizeof(state->shadow_failure_reason), "shadow resolution exceeds limit");
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previous_framebuffer);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous_texture);
    glGetIntegerv(GL_DRAW_BUFFER, &previous_draw_buffer);
    glGetIntegerv(GL_READ_BUFFER, &previous_read_buffer);
    g_gl.GenFramebuffers(1, &framebuffer);
    glGenTextures(1, &depth_texture);
    if (framebuffer == 0U || depth_texture == 0U)
    {
        if (depth_texture != 0U) glDeleteTextures(1, &depth_texture);
        if (framebuffer != 0U) g_gl.DeleteFramebuffers(1, &framebuffer);
        (void)snprintf(state->shadow_failure_reason, sizeof(state->shadow_failure_reason), "GPU object allocation failed");
        return HENKA_ERROR_RENDERER;
    }
    g_gl.BindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glBindTexture(GL_TEXTURE_2D, depth_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    {
        const float border_color[] = {1.0f, 1.0f, 1.0f, 1.0f};
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border_color);
    }
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, resolution, resolution, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, NULL);
    g_gl.FramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth_texture, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    if (g_gl.CheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        (void)snprintf(state->shadow_failure_reason, sizeof(state->shadow_failure_reason), "incomplete shadow framebuffer");
        g_gl.BindFramebuffer(GL_FRAMEBUFFER, (GLuint)previous_framebuffer);
        glBindTexture(GL_TEXTURE_2D, (GLuint)previous_texture);
        glDrawBuffer((GLenum)previous_draw_buffer);
        glReadBuffer((GLenum)previous_read_buffer);
        glDeleteTextures(1, &depth_texture);
        g_gl.DeleteFramebuffers(1, &framebuffer);
        return HENKA_ERROR_RENDERER;
    }
    g_gl.BindFramebuffer(GL_FRAMEBUFFER, (GLuint)previous_framebuffer);
    glBindTexture(GL_TEXTURE_2D, (GLuint)previous_texture);
    glDrawBuffer((GLenum)previous_draw_buffer);
    glReadBuffer((GLenum)previous_read_buffer);
    henka_opengl_delete_shadow_target(state);
    state->shadow_framebuffer = framebuffer;
    state->shadow_depth_texture = depth_texture;
    state->shadow_resolution = resolution;
    state->shadow_generation = state->shadow_generation == UINT64_MAX ? 1U : state->shadow_generation + 1U;
    state->shadow_framebuffer_complete = true;
    state->shadow_failure_reason[0] = '\0';
    {
        uint64_t target_bytes = (uint64_t)resolution * (uint64_t)resolution * 4U;
        henka_opengl_memory_add_category(
            state,
            &state->tracked_render_target_bytes,
            target_bytes);
    }
    return HENKA_SUCCESS;
}

static henka_result henka_opengl_create_local_shadow_target(
    henka_opengl_renderer_state* state,
    int resolution)
{
    GLuint depth_texture = 0U;
    GLuint framebuffer = 0U;
    GLint previous_framebuffer = 0;
    GLint previous_texture = 0;
    GLint previous_draw_buffer = 0;
    GLint previous_read_buffer = 0;

    if (state == NULL || resolution <= 0 || resolution > 2048)
    {
        if (state != NULL)
        {
            (void)snprintf(state->local_shadow_failure_reason,
                sizeof(state->local_shadow_failure_reason),
                "local shadow resolution exceeds limit");
        }
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previous_framebuffer);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous_texture);
    glGetIntegerv(GL_DRAW_BUFFER, &previous_draw_buffer);
    glGetIntegerv(GL_READ_BUFFER, &previous_read_buffer);
    g_gl.GenFramebuffers(1, &framebuffer);
    glGenTextures(1, &depth_texture);
    if (framebuffer == 0U || depth_texture == 0U)
    {
        if (depth_texture != 0U) glDeleteTextures(1, &depth_texture);
        if (framebuffer != 0U) g_gl.DeleteFramebuffers(1, &framebuffer);
        (void)snprintf(state->local_shadow_failure_reason,
            sizeof(state->local_shadow_failure_reason),
            "local shadow GPU allocation failed");
        return HENKA_ERROR_RENDERER;
    }
    g_gl.BindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glBindTexture(GL_TEXTURE_2D, depth_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, resolution, resolution, 0,
        GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, NULL);
    g_gl.FramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth_texture, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    if (g_gl.CheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        (void)snprintf(state->local_shadow_failure_reason,
            sizeof(state->local_shadow_failure_reason),
            "incomplete local shadow framebuffer");
        g_gl.BindFramebuffer(GL_FRAMEBUFFER, (GLuint)previous_framebuffer);
        glBindTexture(GL_TEXTURE_2D, (GLuint)previous_texture);
        glDrawBuffer((GLenum)previous_draw_buffer);
        glReadBuffer((GLenum)previous_read_buffer);
        glDeleteTextures(1, &depth_texture);
        g_gl.DeleteFramebuffers(1, &framebuffer);
        return HENKA_ERROR_RENDERER;
    }
    g_gl.BindFramebuffer(GL_FRAMEBUFFER, (GLuint)previous_framebuffer);
    glBindTexture(GL_TEXTURE_2D, (GLuint)previous_texture);
    glDrawBuffer((GLenum)previous_draw_buffer);
    glReadBuffer((GLenum)previous_read_buffer);
    henka_opengl_delete_local_shadow_target(state);
    state->local_shadow_framebuffer = framebuffer;
    state->local_shadow_depth_texture = depth_texture;
    state->local_shadow_resolution = resolution;
    state->local_shadow_generation = state->local_shadow_generation == UINT64_MAX ?
        1U : state->local_shadow_generation + 1U;
    state->local_shadow_framebuffer_complete = true;
    state->local_shadow_failure_reason[0] = '\0';
    henka_opengl_memory_add_category(
        state,
        &state->tracked_render_target_bytes,
        (uint64_t)resolution * (uint64_t)resolution * 4U);
    return HENKA_SUCCESS;
}

static henka_result henka_opengl_create_cascade_shadow_target(
    henka_opengl_renderer_state* state,
    int resolution)
{
    GLuint depth_texture = 0U;
    GLuint framebuffer = 0U;
    GLint previous_framebuffer = 0;
    GLint previous_texture = 0;
    GLint previous_draw_buffer = 0;
    GLint previous_read_buffer = 0;

    if (state == NULL || resolution <= 0 || resolution > 4096)
    {
        if (state != NULL)
        {
            (void)snprintf(state->cascade_shadow_failure_reason,
                sizeof(state->cascade_shadow_failure_reason),
                "cascade shadow resolution exceeds limit");
        }
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previous_framebuffer);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous_texture);
    glGetIntegerv(GL_DRAW_BUFFER, &previous_draw_buffer);
    glGetIntegerv(GL_READ_BUFFER, &previous_read_buffer);
    g_gl.GenFramebuffers(1, &framebuffer);
    glGenTextures(1, &depth_texture);
    if (framebuffer == 0U || depth_texture == 0U)
    {
        if (depth_texture != 0U) glDeleteTextures(1, &depth_texture);
        if (framebuffer != 0U) g_gl.DeleteFramebuffers(1, &framebuffer);
        (void)snprintf(state->cascade_shadow_failure_reason,
            sizeof(state->cascade_shadow_failure_reason),
            "cascade shadow GPU allocation failed");
        return HENKA_ERROR_RENDERER;
    }
    g_gl.BindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glBindTexture(GL_TEXTURE_2D, depth_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    {
        const float border_color[] = {1.0f, 1.0f, 1.0f, 1.0f};
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border_color);
    }
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, resolution, resolution, 0,
        GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, NULL);
    g_gl.FramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth_texture, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    if (g_gl.CheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        (void)snprintf(state->cascade_shadow_failure_reason,
            sizeof(state->cascade_shadow_failure_reason),
            "incomplete cascade shadow framebuffer");
        g_gl.BindFramebuffer(GL_FRAMEBUFFER, (GLuint)previous_framebuffer);
        glBindTexture(GL_TEXTURE_2D, (GLuint)previous_texture);
        glDrawBuffer((GLenum)previous_draw_buffer);
        glReadBuffer((GLenum)previous_read_buffer);
        glDeleteTextures(1, &depth_texture);
        g_gl.DeleteFramebuffers(1, &framebuffer);
        return HENKA_ERROR_RENDERER;
    }
    g_gl.BindFramebuffer(GL_FRAMEBUFFER, (GLuint)previous_framebuffer);
    glBindTexture(GL_TEXTURE_2D, (GLuint)previous_texture);
    glDrawBuffer((GLenum)previous_draw_buffer);
    glReadBuffer((GLenum)previous_read_buffer);
    henka_opengl_delete_cascade_shadow_target(state);
    state->cascade_shadow_framebuffer = framebuffer;
    state->cascade_shadow_depth_texture = depth_texture;
    state->cascade_shadow_resolution = resolution;
    state->cascade_shadow_generation = state->cascade_shadow_generation == UINT64_MAX ?
        1U : state->cascade_shadow_generation + 1U;
    state->cascade_shadow_framebuffer_complete = true;
    state->cascade_shadow_failure_reason[0] = '\0';
    henka_opengl_memory_add_category(
        state,
        &state->tracked_render_target_bytes,
        (uint64_t)resolution * (uint64_t)resolution * 4U);
    return HENKA_SUCCESS;
}

static henka_result henka_opengl_create_point_shadow_target(
    henka_opengl_renderer_state* state,
    int resolution)
{
    GLuint depth_texture = 0U;
    GLuint framebuffer = 0U;
    GLint previous_framebuffer = 0;
    GLint previous_texture = 0;

    if (state == NULL || resolution <= 0 || resolution > 1024)
    {
        if (state != NULL)
        {
            (void)snprintf(state->point_shadow_failure_reason,
                sizeof(state->point_shadow_failure_reason),
                "point shadow resolution exceeds limit");
        }
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previous_framebuffer);
    glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP, &previous_texture);
    g_gl.GenFramebuffers(1, &framebuffer);
    glGenTextures(1, &depth_texture);
    if (framebuffer == 0U || depth_texture == 0U)
    {
        if (depth_texture != 0U) glDeleteTextures(1, &depth_texture);
        if (framebuffer != 0U) g_gl.DeleteFramebuffers(1, &framebuffer);
        (void)snprintf(state->point_shadow_failure_reason,
            sizeof(state->point_shadow_failure_reason),
            "point shadow GPU allocation failed");
        return HENKA_ERROR_RENDERER;
    }
    glBindTexture(GL_TEXTURE_CUBE_MAP, depth_texture);
    for (int face = 0; face < 6; ++face)
    {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, GL_DEPTH_COMPONENT24,
            resolution, resolution, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, NULL);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    g_gl.BindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    g_gl.FramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
        GL_TEXTURE_CUBE_MAP_POSITIVE_X, depth_texture, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    if (g_gl.CheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        (void)snprintf(state->point_shadow_failure_reason,
            sizeof(state->point_shadow_failure_reason),
            "incomplete point shadow framebuffer");
        g_gl.BindFramebuffer(GL_FRAMEBUFFER, (GLuint)previous_framebuffer);
        glBindTexture(GL_TEXTURE_CUBE_MAP, (GLuint)previous_texture);
        glDeleteTextures(1, &depth_texture);
        g_gl.DeleteFramebuffers(1, &framebuffer);
        return HENKA_ERROR_RENDERER;
    }
    g_gl.BindFramebuffer(GL_FRAMEBUFFER, (GLuint)previous_framebuffer);
    glBindTexture(GL_TEXTURE_CUBE_MAP, (GLuint)previous_texture);
    henka_opengl_delete_point_shadow_target(state);
    state->point_shadow_framebuffer = framebuffer;
    state->point_shadow_depth_texture = depth_texture;
    state->point_shadow_resolution = resolution;
    state->point_shadow_generation = state->point_shadow_generation == UINT64_MAX ?
        1U : state->point_shadow_generation + 1U;
    state->point_shadow_framebuffer_complete = true;
    state->point_shadow_failure_reason[0] = '\0';
    henka_opengl_memory_add_category(
        state,
        &state->tracked_render_target_bytes,
        (uint64_t)resolution * (uint64_t)resolution * 6U * 4U);
    return HENKA_SUCCESS;
}

static henka_result henka_opengl_create_render_programs(
    henka_opengl_renderer_state* state)
{
    static const char* tone_uniforms[] = {"hdrTexture", "bloomTexture", "historyTexture", "motionTexture", "reactiveTexture", "depthTexture", "projection", "exposure", "useBloom", "bloomStrength", "useTemporalHistory", "useMotionVectors", "useReactiveMask", "useAmbientOcclusion", "aoRadius", "aoThickness", "aoFalloff", "aoBias", "aoIntensity", "ssrThickness", "ssrMaxDistance", "ssrRoughness", "ssrEdgeFade", "temporalBlend", "sharpenStrength", "useRenderedGrade", "useScreenSpaceReflections"};
    static const char* bloom_extract_uniforms[] = {"hdrTexture", "threshold"};
    static const char* bloom_blur_uniforms[] = {"sourceTexture", "direction"};
    static const char* ibl_conversion_uniforms[] = {"equirectangularTexture", "rotation", "viewProjection"};
    static const char* ibl_cube_uniforms[] = {"environmentCube", "viewProjection"};
    static const char* ibl_prefilter_uniforms[] = {"environmentCube", "roughness", "viewProjection"};
    static const char* ibl_brdf_uniforms[] = {"brdfScale"};
    static const char* environment_uniforms[] =
        {"groundColor", "horizonColor", "zenithColor", "intensity",
         "environmentTexture", "useEnvironmentTexture", "environmentRotation",
         "environmentCube", "useIBLCube"};
    static const char* shadow_uniforms[] =
        {"model", "lightMatrix", "baseColor", "baseColorTexture", "baseColorUvSet", "useTexture", "alphaMode", "alphaCutoff", "pointLightPosition", "pointLightFarPlane", "pointShadowPass"};
    static const char* tone_vertex =
        "#version 330 core\n"
        "out vec2 uv;\n"
        "void main(){ vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2); uv = p; gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0); }\n";
    static const char* tone_fragment =
        "#version 330 core\n"
        "in vec2 uv; uniform sampler2D hdrTexture; uniform sampler2D bloomTexture; uniform sampler2D historyTexture; uniform sampler2D motionTexture; uniform sampler2D reactiveTexture; uniform sampler2D depthTexture; uniform mat4 projection; uniform float exposure; uniform bool useBloom; uniform float bloomStrength; uniform bool useTemporalHistory; uniform bool useMotionVectors; uniform bool useReactiveMask; uniform bool useAmbientOcclusion; uniform float aoRadius; uniform float aoThickness; uniform float aoFalloff; uniform float aoBias; uniform float aoIntensity; uniform float ssrThickness; uniform float ssrMaxDistance; uniform float ssrRoughness; uniform float ssrEdgeFade; uniform float temporalBlend; uniform float sharpenStrength; uniform bool useRenderedGrade; uniform bool useScreenSpaceReflections; out vec4 outColor;\n"
        "vec3 aces(vec3 x){ return clamp((x*(2.51*x+0.03))/(x*(2.43*x+0.59)+0.14),0.0,1.0); }\n"
        "vec3 presentColor(vec3 hdr){ return pow(aces(max(hdr * exp2(exposure), vec3(0.0))), vec3(1.0/2.2)); }\n"
        "vec3 clampHistory(vec3 history, vec2 at, vec2 texel){ vec3 lower=vec3(1e6); vec3 upper=vec3(-1e6); for(int y=-1;y<=1;++y){ for(int x=-1;x<=1;++x){ vec2 sampleUv=clamp(at+vec2(x,y)*texel,vec2(0.001),vec2(0.999)); vec3 sampleColor=presentColor(texture(hdrTexture,sampleUv).rgb); lower=min(lower,sampleColor); upper=max(upper,sampleColor); } } return clamp(history,lower,upper); }\n"
        "float depthNeighborhoodConfidence(vec2 at, vec2 texel, float depth){ float lower=1.0; float upper=0.0; for(int y=-1;y<=1;++y){ for(int x=-1;x<=1;++x){ float sampleDepth=texture(depthTexture,clamp(at+vec2(x,y)*texel,vec2(0.001),vec2(0.999))).r; lower=min(lower,sampleDepth); upper=max(upper,sampleDepth); } } float tolerance=0.012+0.04*max(abs(depth-0.5),0.0); return depth >= lower-tolerance && depth <= upper+tolerance ? 1.0 : 0.0; }\n"
        "vec3 viewPosition(vec2 at, float depth){ vec4 view=inverse(projection)*vec4(at*2.0-1.0,depth*2.0-1.0,1.0); return view.xyz/max(view.w,0.0001); }\n"
        "float normalAwareOcclusion(vec2 at, vec2 texel, float depth){ if(depth>=0.9999) return 0.0; vec3 position=viewPosition(at,depth); vec2 xUv=clamp(at+vec2(texel.x,0.0),vec2(0.001),vec2(0.999)); vec2 yUv=clamp(at+vec2(0.0,texel.y),vec2(0.001),vec2(0.999)); vec3 px=viewPosition(xUv,texture(depthTexture,xUv).r); vec3 py=viewPosition(yUv,texture(depthTexture,yUv).r); vec3 normal=normalize(cross(py-position,px-position)); const vec2 directions[4]=vec2[4](vec2(1.0,0.0),vec2(0.0,1.0),vec2(0.7071,0.7071),vec2(-0.7071,0.7071)); float occluded=0.0; for(int directionIndex=0;directionIndex<4;++directionIndex){ float directionOcclusion=0.0; for(int side=0;side<2;++side){ vec2 direction=directions[directionIndex]*(side==0?1.0:-1.0); float horizon=0.0; for(int step=1;step<=4;++step){ vec2 sampleUv=clamp(at+direction*texel*(2.0*float(step)),vec2(0.001),vec2(0.999)); float sampleDepth=texture(depthTexture,sampleUv).r; if(sampleDepth>=0.9999||sampleDepth>depth-aoBias) continue; vec3 delta=viewPosition(sampleUv,sampleDepth)-position; float distanceToSample=length(delta); float thicknessWeight=1.0-clamp(abs(delta.z)/max(aoThickness,0.001),0.0,1.0); float radiusWeight=pow(1.0-clamp(distanceToSample/max(aoRadius,0.001),0.0,1.0),max(aoFalloff,0.1)); float horizonAngle=max(dot(normal,normalize(delta)),0.0); horizon=max(horizon,horizonAngle*thicknessWeight*radiusWeight); } directionOcclusion+=horizon; } occluded+=directionOcclusion*0.5; } return clamp(occluded*0.25*clamp(aoIntensity,0.0,2.0),0.0,1.0); }\n"
        "vec3 screenReflection(vec2 at, vec2 texel, float depth, vec3 base){ if(depth>=0.9999) return base; vec3 position=viewPosition(at,depth); vec3 px=viewPosition(clamp(at+vec2(texel.x,0.0),vec2(0.001),vec2(0.999)),texture(depthTexture,clamp(at+vec2(texel.x,0.0),vec2(0.001),vec2(0.999))).r); vec3 py=viewPosition(clamp(at+vec2(0.0,texel.y),vec2(0.001),vec2(0.999)),texture(depthTexture,clamp(at+vec2(0.0,texel.y),vec2(0.001),vec2(0.999))).r); vec3 normal=normalize(cross(py-position,px-position)); vec3 direction=normalize(reflect(normalize(-position),normal)); vec3 reflected=base; float confidence=0.0; float hitDistance=0.0; float maxDistance=clamp(ssrMaxDistance,0.25,32.0); for(int step=1;step<=16;++step){ float distanceAlongRay=maxDistance*(float(step)/16.0); vec3 rayPosition=position+direction*distanceAlongRay; vec4 clip=projection*vec4(rayPosition,1.0); if(clip.w<=0.0) break; vec2 sampleUv=clip.xy/clip.w*0.5+0.5; if(sampleUv.x<0.002||sampleUv.x>0.998||sampleUv.y<0.002||sampleUv.y>0.998) break; float sceneDepth=texture(depthTexture,sampleUv).r; if(sceneDepth>=0.9999) continue; vec3 scenePosition=viewPosition(sampleUv,sceneDepth); float depthDelta=abs(scenePosition.z-rayPosition.z); if(depthDelta<=max(ssrThickness,0.001)){ reflected=presentColor(texture(hdrTexture,sampleUv).rgb); hitDistance=distanceAlongRay; float edgeDistance=min(min(sampleUv.x,1.0-sampleUv.x),min(sampleUv.y,1.0-sampleUv.y)); float edgeConfidence=clamp(edgeDistance/max(ssrEdgeFade,0.001),0.0,1.0); float roughnessConfidence=1.0-clamp(ssrRoughness,0.0,1.0)*0.65; confidence=0.45*edgeConfidence*roughnessConfidence*(1.0-clamp(hitDistance/maxDistance,0.0,1.0)*0.5); break; } } return mix(base,reflected,clamp(confidence,0.0,0.45)); }\n"
        "vec3 sharpen(vec2 at, vec2 texel, vec3 color, float strength){ vec3 crossNeighborhood = (presentColor(texture(hdrTexture, clamp(at+vec2(texel.x,0.0),vec2(0.001),vec2(0.999))).rgb) + presentColor(texture(hdrTexture, clamp(at-vec2(texel.x,0.0),vec2(0.001),vec2(0.999))).rgb) + presentColor(texture(hdrTexture, clamp(at+vec2(0.0,texel.y),vec2(0.001),vec2(0.999))).rgb) + presentColor(texture(hdrTexture, clamp(at-vec2(0.0,texel.y),vec2(0.001),vec2(0.999))).rgb)) * 0.25; return clamp(color + (color-crossNeighborhood)*clamp(strength,0.0,0.2),vec3(0.0),vec3(1.0)); }\n"
        "void main(){ vec3 hdrColor = texture(hdrTexture, uv).rgb; if (useBloom) hdrColor += texture(bloomTexture, uv).rgb * max(bloomStrength, 0.0); vec3 color = presentColor(hdrColor); vec2 texel = 1.0 / vec2(textureSize(depthTexture, 0)); float currentDepth = texture(depthTexture, uv).r; if (useScreenSpaceReflections) color = screenReflection(uv, texel, currentDepth, color); if (useRenderedGrade) color = clamp(pow(max(color, vec3(0.0)), vec3(0.92)) * vec3(1.02, 1.0, 0.98), vec3(0.0), vec3(1.0)); if (useAmbientOcclusion) color *= mix(1.0, 0.92, normalAwareOcclusion(uv, texel, currentDepth)); if (useTemporalHistory && currentDepth < 0.9999) { vec2 historyUv = uv; vec2 motion = useMotionVectors ? texture(motionTexture, uv).rg : vec2(0.0); if (useMotionVectors) historyUv = clamp(uv - motion, vec2(0.001), vec2(0.999)); float reprojectedDepth = texture(depthTexture, historyUv).r; float depthTolerance = 0.012 + 0.04 * max(abs(currentDepth-0.5),0.0); float depthConfidence = reprojectedDepth < 0.9999 && abs(currentDepth - reprojectedDepth) <= depthTolerance ? depthNeighborhoodConfidence(historyUv, texel, reprojectedDepth) : 0.0; float reactive = useReactiveMask ? texture(reactiveTexture, uv).r : 0.0; float motionResponsiveness = useMotionVectors ? clamp(length(motion) * 12.0, 0.0, 0.75) : 0.0; float historyWeight = clamp(temporalBlend, 0.0, 0.25) * depthConfidence * (1.0-reactive) * (1.0-motionResponsiveness); vec3 historyColor = clampHistory(texture(historyTexture, historyUv).rgb, uv, 1.0 / vec2(textureSize(hdrTexture, 0))); color = mix(color, historyColor, historyWeight); } color = sharpen(uv, texel, color, useTemporalHistory ? sharpenStrength : 0.0); outColor = vec4(color, 1.0); }\n";
    static const char* bloom_extract_fragment =
        "#version 330 core\n"
        "in vec2 uv; uniform sampler2D hdrTexture; uniform float threshold; out vec4 outColor;\n"
        "void main(){ vec3 color = max(texture(hdrTexture, uv).rgb, vec3(0.0)); float brightness = max(max(color.r, color.g), color.b); outColor = vec4(brightness > threshold ? color - vec3(threshold) : vec3(0.0), 1.0); }\n";
    static const char* bloom_blur_fragment =
        "#version 330 core\n"
        "in vec2 uv; uniform sampler2D sourceTexture; uniform vec2 direction; out vec4 outColor;\n"
        "void main(){ vec3 color = texture(sourceTexture, uv).rgb * 0.227027; color += texture(sourceTexture, uv + direction * 1.384615).rgb * 0.316216; color += texture(sourceTexture, uv - direction * 1.384615).rgb * 0.316216; color += texture(sourceTexture, uv + direction * 3.230769).rgb * 0.070270; color += texture(sourceTexture, uv - direction * 3.230769).rgb * 0.070270; outColor = vec4(color, 1.0); }\n";
    static const char* ibl_conversion_fragment =
        "#version 330 core\n"
        "in vec2 uv; uniform sampler2D equirectangularTexture; uniform float rotation; uniform mat4 viewProjection; out vec4 outColor; const float PI=3.14159265359;\n"
        "void main(){ vec2 ndc=uv*2.0-1.0; vec4 world=inverse(viewProjection)*vec4(ndc,1.0,1.0); vec3 direction=normalize(world.xyz/world.w); float longitude=atan(direction.z,direction.x)/(2.0*PI)+0.5+rotation/(2.0*PI); float latitude=acos(clamp(direction.y,-1.0,1.0))/PI; outColor=vec4(texture(equirectangularTexture,vec2(fract(longitude),latitude)).rgb,1.0); }\n";
    static const char* ibl_irradiance_fragment =
        "#version 330 core\n"
        "in vec2 uv; uniform samplerCube environmentCube; uniform mat4 viewProjection; out vec4 outColor;\n"
        "void main(){ vec2 ndc=uv*2.0-1.0; vec4 world=inverse(viewProjection)*vec4(ndc,1.0,1.0); vec3 n=normalize(world.xyz/world.w); vec3 up=abs(n.y)<0.95?vec3(0.0,1.0,0.0):vec3(1.0,0.0,0.0); vec3 t=normalize(cross(up,n)); vec3 b=cross(n,t); vec3 color=texture(environmentCube,n).rgb; color+=texture(environmentCube,normalize(n+t*0.5)).rgb; color+=texture(environmentCube,normalize(n-t*0.5)).rgb; color+=texture(environmentCube,normalize(n+b*0.5)).rgb; color+=texture(environmentCube,normalize(n-b*0.5)).rgb; outColor=vec4(color/5.0,1.0); }\n";
    static const char* ibl_prefilter_fragment =
        "#version 330 core\n"
        "in vec2 uv; uniform samplerCube environmentCube; uniform float roughness; uniform mat4 viewProjection; out vec4 outColor;\n"
        "void main(){ vec2 ndc=uv*2.0-1.0; vec4 world=inverse(viewProjection)*vec4(ndc,1.0,1.0); vec3 n=normalize(world.xyz/world.w); vec3 color=texture(environmentCube,n).rgb; vec3 up=abs(n.y)<0.95?vec3(0.0,1.0,0.0):vec3(1.0,0.0,0.0); vec3 t=normalize(cross(up,n)); vec3 b=cross(n,t); color+=texture(environmentCube,normalize(n+t*roughness)).rgb; color+=texture(environmentCube,normalize(n-t*roughness)).rgb; color+=texture(environmentCube,normalize(n+b*roughness)).rgb; color+=texture(environmentCube,normalize(n-b*roughness)).rgb; outColor=vec4(color/5.0,1.0); }\n";
    static const char* ibl_brdf_fragment =
        "#version 330 core\n"
        "in vec2 uv; uniform float brdfScale; out vec2 outColor;\n"
        "void main(){ float nDotV=clamp(uv.x,0.0,1.0); float roughness=clamp(1.0-uv.y,0.0,1.0); float fresnel=pow(1.0-nDotV,5.0); outColor=vec2((1.0-fresnel)*(1.0-0.5*roughness),fresnel)*brdfScale; }\n";
    static const char* environment_fragment =
        "#version 330 core\n"
        "in vec2 uv; uniform vec3 groundColor; uniform vec3 horizonColor; uniform vec3 zenithColor; uniform float intensity; uniform sampler2D environmentTexture; uniform samplerCube environmentCube; uniform bool useEnvironmentTexture; uniform bool useIBLCube; uniform float environmentRotation; out vec4 outColor; const float PI=3.14159265359;\n"
        "void main(){ float height = clamp(uv.y, 0.0, 1.0); float horizon = smoothstep(0.04, 0.48, height); vec3 lower = mix(groundColor, horizonColor, horizon); vec3 gradient = mix(lower, zenithColor, smoothstep(0.48, 1.0, height)); float longitude = fract(uv.x + environmentRotation / 6.28318530718); float latitude=height*PI; vec3 direction=vec3(sin(latitude)*cos(longitude*2.0*PI),cos(latitude),sin(latitude)*sin(longitude*2.0*PI)); vec3 hdr = useIBLCube ? texture(environmentCube,direction).rgb : texture(environmentTexture, vec2(longitude, height)).rgb; vec3 color = (useEnvironmentTexture || useIBLCube) ? hdr : gradient; outColor = vec4(max(color * max(intensity, 0.0), vec3(0.0)), 1.0); }\n";
    static const char* shadow_vertex =
        "#version 330 core\n"
        "layout(location=0) in vec3 inPosition; layout(location=2) in vec2 inUv; layout(location=5) in vec2 inUv1; out vec2 fragUv; out vec2 fragUv1; out vec3 fragWorldPosition; uniform mat4 model; uniform mat4 lightMatrix;\n"
        "void main(){ vec4 worldPosition = model * vec4(inPosition,1.0); fragUv = inUv; fragUv1 = inUv1; fragWorldPosition = worldPosition.xyz; gl_Position = lightMatrix * worldPosition; }\n";
    static const char* shadow_fragment =
        "#version 330 core\n"
        "in vec2 fragUv; in vec2 fragUv1; in vec3 fragWorldPosition; uniform vec4 baseColor; uniform sampler2D baseColorTexture; uniform int baseColorUvSet; uniform bool useTexture; uniform int alphaMode; uniform float alphaCutoff; uniform vec3 pointLightPosition; uniform float pointLightFarPlane; uniform bool pointShadowPass;\n"
        "void main(){ vec2 uv = baseColorUvSet == 1 ? fragUv1 : fragUv; if(alphaMode == 1 && baseColor.a * (useTexture ? texture(baseColorTexture, uv).a : 1.0) < alphaCutoff) discard; if(pointShadowPass) gl_FragDepth = clamp(length(fragWorldPosition - pointLightPosition) / max(pointLightFarPlane, 0.0001), 0.0, 1.0); }\n";

    if (state == NULL ||
        !henka_compile_program_from_source(tone_vertex, tone_fragment, "tone-map vertex", "tone-map fragment", &state->tone_program) ||
        !henka_compile_program_from_source(tone_vertex, bloom_extract_fragment, "bloom extract vertex", "bloom extract fragment", &state->bloom_extract_program) ||
        !henka_compile_program_from_source(tone_vertex, bloom_blur_fragment, "bloom blur vertex", "bloom blur fragment", &state->bloom_blur_program) ||
        !henka_compile_program_from_source(tone_vertex, ibl_conversion_fragment, "IBL conversion vertex", "IBL conversion fragment", &state->ibl_conversion_program) ||
        !henka_compile_program_from_source(tone_vertex, ibl_irradiance_fragment, "IBL irradiance vertex", "IBL irradiance fragment", &state->ibl_irradiance_program) ||
        !henka_compile_program_from_source(tone_vertex, ibl_prefilter_fragment, "IBL prefilter vertex", "IBL prefilter fragment", &state->ibl_prefilter_program) ||
        !henka_compile_program_from_source(tone_vertex, ibl_brdf_fragment, "BRDF LUT vertex", "BRDF LUT fragment", &state->ibl_brdf_program) ||
        !henka_compile_program_from_source(tone_vertex, environment_fragment, "environment vertex", "environment fragment", &state->environment_program) ||
        !henka_compile_program_from_source(shadow_vertex, shadow_fragment, "shadow vertex", "shadow fragment", &state->shadow_program))
    {
        if (state->bloom_blur_program != 0U) g_gl.DeleteProgram(state->bloom_blur_program);
        if (state->bloom_extract_program != 0U) g_gl.DeleteProgram(state->bloom_extract_program);
        if (state->tone_program != 0U) g_gl.DeleteProgram(state->tone_program);
        if (state->ibl_brdf_program != 0U) g_gl.DeleteProgram(state->ibl_brdf_program);
        if (state->ibl_prefilter_program != 0U) g_gl.DeleteProgram(state->ibl_prefilter_program);
        if (state->ibl_irradiance_program != 0U) g_gl.DeleteProgram(state->ibl_irradiance_program);
        if (state->ibl_conversion_program != 0U) g_gl.DeleteProgram(state->ibl_conversion_program);
        state->bloom_blur_program = 0U;
        state->bloom_extract_program = 0U;
        state->tone_program = 0U;
        return HENKA_ERROR_RENDERER;
    }
    if (!henka_populate_shader_location_table(
            state->tone_program,
            "tone-map",
            HENKA_SHADER_CONTRACT_TONE_MAP,
            1U,
            tone_uniforms,
            sizeof(tone_uniforms) / sizeof(tone_uniforms[0]),
            &state->tone_shader_data) ||
        !henka_populate_shader_location_table(
            state->bloom_extract_program,
            "bloom extract",
            HENKA_SHADER_CONTRACT_BLOOM,
            1U,
            bloom_extract_uniforms,
            sizeof(bloom_extract_uniforms) / sizeof(bloom_extract_uniforms[0]),
            &state->bloom_extract_shader_data) ||
        !henka_populate_shader_location_table(
            state->bloom_blur_program,
            "bloom blur",
            HENKA_SHADER_CONTRACT_BLOOM,
            1U,
            bloom_blur_uniforms,
            sizeof(bloom_blur_uniforms) / sizeof(bloom_blur_uniforms[0]),
            &state->bloom_blur_shader_data) ||
        !henka_populate_shader_location_table(
            state->ibl_conversion_program,
            "IBL conversion",
            HENKA_SHADER_CONTRACT_IBL_CONVERSION,
            1U,
            ibl_conversion_uniforms,
            sizeof(ibl_conversion_uniforms) / sizeof(ibl_conversion_uniforms[0]),
            &state->ibl_conversion_shader_data) ||
        !henka_populate_shader_location_table(
            state->ibl_irradiance_program,
            "IBL irradiance",
            HENKA_SHADER_CONTRACT_IBL_IRRADIANCE,
            1U,
            ibl_cube_uniforms,
            sizeof(ibl_cube_uniforms) / sizeof(ibl_cube_uniforms[0]),
            &state->ibl_irradiance_shader_data) ||
        !henka_populate_shader_location_table(
            state->ibl_prefilter_program,
            "IBL prefilter",
            HENKA_SHADER_CONTRACT_IBL_PREFILTER,
            1U,
            ibl_prefilter_uniforms,
            sizeof(ibl_prefilter_uniforms) / sizeof(ibl_prefilter_uniforms[0]),
            &state->ibl_prefilter_shader_data) ||
        !henka_populate_shader_location_table(
            state->ibl_brdf_program,
            "BRDF LUT",
            HENKA_SHADER_CONTRACT_BRDF_LUT,
            1U,
            ibl_brdf_uniforms,
            sizeof(ibl_brdf_uniforms) / sizeof(ibl_brdf_uniforms[0]),
            &state->ibl_brdf_shader_data) ||
        !henka_populate_shader_location_table(
            state->environment_program,
            "environment",
            HENKA_SHADER_CONTRACT_ENVIRONMENT,
            1U,
            environment_uniforms,
            sizeof(environment_uniforms) / sizeof(environment_uniforms[0]),
            &state->environment_shader_data) ||
        !henka_populate_shader_location_table(
            state->shadow_program,
            "shadow",
            HENKA_SHADER_CONTRACT_SHADOW_MASKED,
            1U,
            shadow_uniforms,
            sizeof(shadow_uniforms) / sizeof(shadow_uniforms[0]),
            &state->shadow_shader_data))
    {
        return HENKA_ERROR_RENDERER;
    }
    g_gl.GenVertexArrays(1, &state->tone_vertex_array);
    if (state->tone_vertex_array == 0U)
    {
        return HENKA_ERROR_RENDERER;
    }
    return HENKA_SUCCESS;
}

static void henka_opengl_draw_environment(
    henka_opengl_renderer_state* state,
    struct henka_renderer* renderer,
    const henka_scene* scene)
{
    henka_viewport viewport;

    if (state == NULL || renderer == NULL || scene == NULL ||
        state->environment_program == 0U)
    {
        return;
    }

    viewport = henka_renderer_get_scene_viewport(renderer);
    glDisable(GL_SCISSOR_TEST);
    glViewport(0, 0, viewport.width, viewport.height);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    g_gl.UseProgram(state->environment_program);
    henka_set_uniform_vec3_owned(state->environment_program, &state->environment_shader_data, "groundColor", scene->environment.ground_color);
    henka_set_uniform_vec3_owned(state->environment_program, &state->environment_shader_data, "horizonColor", scene->environment.horizon_color);
    henka_set_uniform_vec3_owned(state->environment_program, &state->environment_shader_data, "zenithColor", scene->environment.zenith_color);
    henka_set_uniform_float_owned(state->environment_program, &state->environment_shader_data, "intensity", scene->environment.intensity);
    henka_set_uniform_int_owned(state->environment_program, &state->environment_shader_data, "environmentTexture", 6);
    henka_set_uniform_int_owned(state->environment_program, &state->environment_shader_data, "environmentCube", 7);
    henka_set_uniform_bool_owned(
        state->environment_program,
        &state->environment_shader_data,
        "useEnvironmentTexture",
        scene->environment.hdr_texture != NULL &&
        scene->environment.hdr_texture->backend_data != NULL);
    henka_set_uniform_bool_owned(
        state->environment_program,
        &state->environment_shader_data,
        "useIBLCube",
        state->ibl_ready);
    henka_set_uniform_float_owned(
        state->environment_program,
        &state->environment_shader_data,
        "environmentRotation",
        scene->environment.hdr_rotation);
    g_gl.ActiveTexture(GL_TEXTURE6);
    glBindTexture(
        GL_TEXTURE_2D,
        scene->environment.hdr_texture != NULL &&
        scene->environment.hdr_texture->backend_data != NULL ?
        ((const henka_opengl_texture_data*)scene->environment.hdr_texture->backend_data)->texture_id : 0U);
    g_gl.ActiveTexture(GL_TEXTURE7);
    glBindTexture(GL_TEXTURE_CUBE_MAP, state->ibl_ready ? state->ibl_environment_cube : 0U);
    g_gl.BindVertexArray(state->tone_vertex_array);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    g_gl.BindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0U);
    g_gl.ActiveTexture(GL_TEXTURE7);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0U);
    g_gl.ActiveTexture(GL_TEXTURE0);
    g_gl.UseProgram(0);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
}

static void henka_opengl_draw_bloom(
    henka_opengl_renderer_state* state)
{
    if (state == NULL || !state->bloom_ready || state->hdr_color_texture == 0U ||
        state->bloom_framebuffer == 0U || state->bloom_blur_framebuffer == 0U ||
        state->bloom_extract_program == 0U || state->bloom_blur_program == 0U ||
        state->tone_vertex_array == 0U)
    {
        return;
    }
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glViewport(0, 0, state->bloom_width, state->bloom_height);

    g_gl.BindFramebuffer(GL_FRAMEBUFFER, state->bloom_framebuffer);
    g_gl.UseProgram(state->bloom_extract_program);
    henka_set_uniform_int_owned(state->bloom_extract_program, &state->bloom_extract_shader_data, "hdrTexture", 0);
    henka_set_uniform_float_owned(state->bloom_extract_program, &state->bloom_extract_shader_data, "threshold", 1.0f);
    g_gl.ActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, state->hdr_color_texture);
    g_gl.BindVertexArray(state->tone_vertex_array);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    g_gl.BindFramebuffer(GL_FRAMEBUFFER, state->bloom_blur_framebuffer);
    g_gl.UseProgram(state->bloom_blur_program);
    henka_set_uniform_int_owned(state->bloom_blur_program, &state->bloom_blur_shader_data, "sourceTexture", 0);
    henka_set_uniform_vec2_owned(state->bloom_blur_program, &state->bloom_blur_shader_data, "direction",
        (henka_vec2){1.0f / (float)state->bloom_width, 0.0f});
    glBindTexture(GL_TEXTURE_2D, state->bloom_color_texture);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    g_gl.BindFramebuffer(GL_FRAMEBUFFER, state->bloom_framebuffer);
    henka_set_uniform_vec2_owned(state->bloom_blur_program, &state->bloom_blur_shader_data, "direction",
        (henka_vec2){0.0f, 1.0f / (float)state->bloom_height});
    glBindTexture(GL_TEXTURE_2D, state->bloom_blur_texture);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    g_gl.BindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0U);
    g_gl.UseProgram(0);
    g_gl.BindFramebuffer(GL_FRAMEBUFFER, 0U);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
}

static henka_mat4 henka_opengl_get_light_matrix(
    const henka_scene* scene,
    float shadow_extent,
    float shadow_distance)
{
    henka_vec3 direction = henka_vec3_normalize(scene->light_direction);
    henka_vec3 up = fabsf(direction.y) > 0.94f ?
        (henka_vec3){1.0f, 0.0f, 0.0f} :
        (henka_vec3){0.0f, 1.0f, 0.0f};
    henka_vec3 light_right = henka_vec3_normalize(henka_vec3_cross(direction, up));
    henka_vec3 target;
    henka_vec3 eye;
    float texel_size = (shadow_extent * 2.0f) / 1024.0f;
    float target_right;
    float target_up;

    /* Fit the map to the active camera frustum and quantize its center to one
     * shadow texel. This keeps the coverage useful while preventing camera
     * sub-pixel movement from making the directional shadow shimmer. */
    target = henka_vec3_add(
        scene->camera.position,
        henka_vec3_scale(henka_camera_get_forward(&scene->camera),
            fminf(fmaxf(scene->camera.far_plane * 0.25f, 6.0f), 18.0f)));
    target_right = henka_vec3_dot(target, light_right);
    target_up = henka_vec3_dot(target, up);
    if (texel_size > 0.0f)
    {
        target = henka_vec3_add(
            target,
            henka_vec3_scale(light_right,
                floorf(target_right / texel_size + 0.5f) * texel_size - target_right));
        target = henka_vec3_add(
            target,
            henka_vec3_scale(up,
                floorf(target_up / texel_size + 0.5f) * texel_size - target_up));
    }
    eye = henka_vec3_subtract(target, henka_vec3_scale(direction, shadow_distance));

    return henka_mat4_multiply(
        henka_mat4_orthographic(-shadow_extent, shadow_extent, -shadow_extent, shadow_extent, 0.1f, shadow_distance * 2.0f),
        henka_mat4_look_at(eye, target, up));
}

static henka_mat4 henka_opengl_get_spot_light_matrix(const henka_scene_light_desc* light)
{
    henka_vec3 direction;
    henka_vec3 up;
    float outer_cosine;
    float field_of_view;
    float range;

    if (light == NULL)
    {
        return henka_mat4_identity();
    }
    direction = henka_vec3_normalize(light->direction);
    up = fabsf(direction.y) > 0.94f ?
        (henka_vec3){1.0f, 0.0f, 0.0f} :
        (henka_vec3){0.0f, 1.0f, 0.0f};
    outer_cosine = fmaxf(-0.999f, fminf(0.999f, light->outer_cone_cosine));
    field_of_view = fmaxf(0.35f, fminf(2.4f, 2.0f * acosf(outer_cosine) + 0.08f));
    range = fmaxf(0.1f, fminf(light->range, 10000.0f));
    return henka_mat4_multiply(
        henka_mat4_perspective(field_of_view, 1.0f, 0.1f, range),
        henka_mat4_look_at(
            light->position,
            henka_vec3_add(light->position, direction),
            up));
}

static void henka_opengl_draw_shadow_pass(
    henka_opengl_renderer_state* state,
    const henka_scene* scene,
    henka_mat4 light_matrix,
    GLuint framebuffer,
    int resolution)
{
    size_t index;

    if (framebuffer == 0U || resolution <= 0 || state->shadow_program == 0U)
    {
        return;
    }
    g_gl.BindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glViewport(0, 0, resolution, resolution);
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);
    g_gl.UseProgram(state->shadow_program);
    for (index = 0U; index < scene->entity_capacity; ++index)
    {
        const henka_scene_entity_record* entity = &scene->entities[index];
        const henka_opengl_mesh_data* mesh_data;

        if (!entity->active || !entity->visible || entity->mesh == NULL ||
            !entity->material.cast_shadows ||
            entity->material.alpha_mode == HENKA_MATERIAL_ALPHA_BLENDED ||
            (entity->flags & HENKA_SCENE_ENTITY_FLAG_HELPER) != 0U)
        {
            continue;
        }
        mesh_data = (const henka_opengl_mesh_data*)entity->mesh->backend_data;
        if (mesh_data == NULL)
        {
            continue;
        }
        if (entity->material.double_sided)
        {
            glDisable(GL_CULL_FACE);
        }
        else
        {
            glEnable(GL_CULL_FACE);
            glCullFace(GL_FRONT);
        }
        henka_set_uniform_mat4_owned(
            state->shadow_program,
            &state->shadow_shader_data,
            "model",
            henka_transform_to_mat4(entity->transform));
        henka_set_uniform_mat4_owned(state->shadow_program, &state->shadow_shader_data, "lightMatrix", light_matrix);
        henka_set_uniform_vec4_owned(state->shadow_program, &state->shadow_shader_data, "baseColor", entity->material.base_color);
        henka_set_uniform_int_owned(state->shadow_program, &state->shadow_shader_data, "baseColorTexture", 0);
        henka_set_uniform_int_owned(state->shadow_program, &state->shadow_shader_data, "baseColorUvSet", entity->material.base_color_uv_set);
        henka_set_uniform_bool_owned(state->shadow_program, &state->shadow_shader_data, "useTexture",
            entity->material.use_texture && entity->material.base_color_texture != NULL &&
            entity->material.base_color_texture->backend_data != NULL);
        henka_set_uniform_int_owned(state->shadow_program, &state->shadow_shader_data, "alphaMode", (int)entity->material.alpha_mode);
        henka_set_uniform_float_owned(state->shadow_program, &state->shadow_shader_data, "alphaCutoff", entity->material.alpha_cutoff);
        g_gl.ActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D,
            entity->material.base_color_texture != NULL && entity->material.base_color_texture->backend_data != NULL ?
            ((const henka_opengl_texture_data*)entity->material.base_color_texture->backend_data)->texture_id : 0U);
        g_gl.BindVertexArray(mesh_data->vao);
        glDrawElements(mesh_data->primitive_mode, mesh_data->index_count, GL_UNSIGNED_INT, 0);
    }
    g_gl.BindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0U);
    g_gl.UseProgram(0);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    g_gl.BindFramebuffer(GL_FRAMEBUFFER, 0U);
}

static void henka_opengl_draw_point_shadow_pass(
    henka_opengl_renderer_state* state,
    const henka_scene* scene,
    const henka_scene_light_desc* light)
{
    static const henka_vec3 directions[6] = {
        {1.0f, 0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f}, {0.0f, -1.0f, 0.0f},
        {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, -1.0f}};
    static const henka_vec3 ups[6] = {
        {0.0f, -1.0f, 0.0f}, {0.0f, -1.0f, 0.0f},
        {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, -1.0f},
        {0.0f, -1.0f, 0.0f}, {0.0f, -1.0f, 0.0f}};
    float far_plane;

    if (state == NULL || scene == NULL || light == NULL ||
        state->point_shadow_framebuffer == 0U || state->shadow_program == 0U ||
        !state->point_shadow_framebuffer_complete)
    {
        return;
    }
    far_plane = fmaxf(0.1f, fminf(light->range, 10000.0f));
    g_gl.UseProgram(state->shadow_program);
    glViewport(0, 0, state->point_shadow_resolution, state->point_shadow_resolution);
    glEnable(GL_DEPTH_TEST);
    for (int face = 0; face < 6; ++face)
    {
        henka_mat4 light_matrix = henka_mat4_multiply(
            henka_mat4_perspective(3.14159265359f * 0.5f, 1.0f, 0.1f, far_plane),
            henka_mat4_look_at(light->position,
                henka_vec3_add(light->position, directions[face]), ups[face]));
        g_gl.BindFramebuffer(GL_FRAMEBUFFER, state->point_shadow_framebuffer);
        g_gl.FramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
            GL_TEXTURE_CUBE_MAP_POSITIVE_X + face,
            state->point_shadow_depth_texture, 0);
        glClear(GL_DEPTH_BUFFER_BIT);
        glCullFace(GL_FRONT);
        for (size_t index = 0U; index < scene->entity_capacity; ++index)
        {
            const henka_scene_entity_record* entity = &scene->entities[index];
            const henka_opengl_mesh_data* mesh_data;
            if (!entity->active || !entity->visible || entity->mesh == NULL ||
                !entity->material.cast_shadows ||
                entity->material.alpha_mode == HENKA_MATERIAL_ALPHA_BLENDED ||
                (entity->flags & HENKA_SCENE_ENTITY_FLAG_HELPER) != 0U)
            {
                continue;
            }
            mesh_data = (const henka_opengl_mesh_data*)entity->mesh->backend_data;
            if (mesh_data == NULL) continue;
            if (entity->material.double_sided) glDisable(GL_CULL_FACE);
            else { glEnable(GL_CULL_FACE); glCullFace(GL_FRONT); }
            henka_set_uniform_mat4_owned(state->shadow_program, &state->shadow_shader_data,
                "model", henka_transform_to_mat4(entity->transform));
            henka_set_uniform_mat4_owned(state->shadow_program, &state->shadow_shader_data,
                "lightMatrix", light_matrix);
            henka_set_uniform_vec4_owned(state->shadow_program, &state->shadow_shader_data,
                "baseColor", entity->material.base_color);
            henka_set_uniform_int_owned(state->shadow_program, &state->shadow_shader_data,
                "baseColorTexture", 0);
            henka_set_uniform_int_owned(state->shadow_program, &state->shadow_shader_data,
                "baseColorUvSet", entity->material.base_color_uv_set);
            henka_set_uniform_bool_owned(state->shadow_program, &state->shadow_shader_data,
                "useTexture", entity->material.use_texture && entity->material.base_color_texture != NULL &&
                entity->material.base_color_texture->backend_data != NULL);
            henka_set_uniform_int_owned(state->shadow_program, &state->shadow_shader_data,
                "alphaMode", (int)entity->material.alpha_mode);
            henka_set_uniform_float_owned(state->shadow_program, &state->shadow_shader_data,
                "alphaCutoff", entity->material.alpha_cutoff);
            henka_set_uniform_vec3_owned(state->shadow_program, &state->shadow_shader_data,
                "pointLightPosition", light->position);
            henka_set_uniform_float_owned(state->shadow_program, &state->shadow_shader_data,
                "pointLightFarPlane", far_plane);
            henka_set_uniform_bool_owned(state->shadow_program, &state->shadow_shader_data,
                "pointShadowPass", true);
            g_gl.ActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D,
                entity->material.base_color_texture != NULL && entity->material.base_color_texture->backend_data != NULL ?
                ((const henka_opengl_texture_data*)entity->material.base_color_texture->backend_data)->texture_id : 0U);
            g_gl.BindVertexArray(mesh_data->vao);
            glDrawElements(mesh_data->primitive_mode, mesh_data->index_count, GL_UNSIGNED_INT, 0);
        }
    }
    henka_set_uniform_bool_owned(state->shadow_program, &state->shadow_shader_data,
        "pointShadowPass", false);
    g_gl.BindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0U);
    g_gl.UseProgram(0);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    g_gl.BindFramebuffer(GL_FRAMEBUFFER, 0U);
}

static void henka_opengl_present_hdr(
    struct henka_renderer* renderer,
    henka_opengl_renderer_state* state,
    bool use_rendered_post_processing)
{
    henka_viewport viewport;

    if (state->hdr_color_texture == 0U || state->tone_program == 0U)
    {
        return;
    }
    viewport = henka_renderer_get_scene_viewport(renderer);
    if (state->hdr_width != viewport.width || state->hdr_height != viewport.height)
    {
        return;
    }
    glDisable(GL_SCISSOR_TEST);
    glViewport(viewport.x, renderer->framebuffer_height - viewport.y - viewport.height, viewport.width, viewport.height);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    g_gl.UseProgram(state->tone_program);
    henka_set_uniform_int_owned(state->tone_program, &state->tone_shader_data, "hdrTexture", 0);
    henka_set_uniform_int_owned(state->tone_program, &state->tone_shader_data, "bloomTexture", 1);
    henka_set_uniform_int_owned(state->tone_program, &state->tone_shader_data, "historyTexture", 2);
    henka_set_uniform_int_owned(state->tone_program, &state->tone_shader_data, "motionTexture", 3);
    henka_set_uniform_int_owned(state->tone_program, &state->tone_shader_data, "reactiveTexture", 5);
    henka_set_uniform_int_owned(state->tone_program, &state->tone_shader_data, "depthTexture", 4);
    henka_set_uniform_float_owned(state->tone_program, &state->tone_shader_data, "exposure", renderer->exposure);
    henka_set_uniform_mat4_owned(
        state->tone_program,
        &state->tone_shader_data,
        "projection",
        state->current_projection);
    henka_set_uniform_bool_owned(
        state->tone_program,
        &state->tone_shader_data,
        "useBloom",
        use_rendered_post_processing && state->bloom_ready);
    henka_set_uniform_float_owned(state->tone_program, &state->tone_shader_data, "bloomStrength", 0.14f);
    henka_set_uniform_bool_owned(
        state->tone_program,
        &state->tone_shader_data,
        "useRenderedGrade",
        use_rendered_post_processing);
    henka_set_uniform_bool_owned(
        state->tone_program,
        &state->tone_shader_data,
        "useTemporalHistory",
        use_rendered_post_processing && state->temporal_history_ready && state->temporal_history_valid &&
            state->temporal_history_width == viewport.width &&
            state->temporal_history_height == viewport.height);
    henka_set_uniform_float_owned(state->tone_program, &state->tone_shader_data, "temporalBlend", 0.08f);
    henka_set_uniform_float_owned(state->tone_program, &state->tone_shader_data, "sharpenStrength", 0.08f);
    henka_set_uniform_bool_owned(
        state->tone_program,
        &state->tone_shader_data,
        "useMotionVectors",
        use_rendered_post_processing && state->hdr_motion_texture != 0U && state->previous_view_projection_valid);
    henka_set_uniform_bool_owned(
        state->tone_program,
        &state->tone_shader_data,
        "useReactiveMask",
        use_rendered_post_processing && state->hdr_reactive_texture != 0U);
    henka_set_uniform_bool_owned(
        state->tone_program,
        &state->tone_shader_data,
        "useAmbientOcclusion",
        use_rendered_post_processing && state->hdr_depth_buffer != 0U);
    henka_set_uniform_float_owned(state->tone_program, &state->tone_shader_data, "aoRadius", 1.25f);
    henka_set_uniform_float_owned(state->tone_program, &state->tone_shader_data, "aoThickness", 0.45f);
    henka_set_uniform_float_owned(state->tone_program, &state->tone_shader_data, "aoFalloff", 1.35f);
    henka_set_uniform_float_owned(state->tone_program, &state->tone_shader_data, "aoBias", 0.0025f);
    henka_set_uniform_float_owned(state->tone_program, &state->tone_shader_data, "aoIntensity", 0.35f);
    henka_set_uniform_float_owned(state->tone_program, &state->tone_shader_data, "ssrThickness", 0.10f);
    henka_set_uniform_float_owned(state->tone_program, &state->tone_shader_data, "ssrMaxDistance", 8.0f);
    henka_set_uniform_float_owned(state->tone_program, &state->tone_shader_data, "ssrRoughness", 0.45f);
    henka_set_uniform_float_owned(state->tone_program, &state->tone_shader_data, "ssrEdgeFade", 0.08f);
    henka_set_uniform_bool_owned(
        state->tone_program,
        &state->tone_shader_data,
        "useScreenSpaceReflections",
        use_rendered_post_processing && state->hdr_depth_buffer != 0U && state->ibl_ready);
    g_gl.ActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, state->hdr_color_texture);
    g_gl.ActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D,
        use_rendered_post_processing && state->bloom_ready ? state->bloom_color_texture : 0U);
    g_gl.ActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D,
        use_rendered_post_processing && state->temporal_history_ready ? state->temporal_history_texture : 0U);
    g_gl.ActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, state->hdr_motion_texture);
    g_gl.ActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, state->hdr_depth_buffer);
    g_gl.ActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D,
        use_rendered_post_processing && state->hdr_reactive_texture != 0U ?
        state->hdr_reactive_texture : 0U);
    g_gl.BindVertexArray(state->tone_vertex_array);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    g_gl.BindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0U);
    g_gl.ActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0U);
    g_gl.ActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, 0U);
    g_gl.ActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, 0U);
    g_gl.ActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, 0U);
    g_gl.ActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D, 0U);
    g_gl.ActiveTexture(GL_TEXTURE0);
    g_gl.UseProgram(0);
    if (state->temporal_history_ready &&
        state->temporal_history_width == viewport.width &&
        state->temporal_history_height == viewport.height)
    {
        g_gl.ActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, state->temporal_history_texture);
        glCopyTexSubImage2D(
            GL_TEXTURE_2D,
            0,
            0,
            0,
            viewport.x,
            renderer->framebuffer_height - viewport.y - viewport.height,
            viewport.width,
            viewport.height);
        glBindTexture(GL_TEXTURE_2D, 0U);
        g_gl.ActiveTexture(GL_TEXTURE0);
        state->temporal_history_valid = true;
        state->temporal_fallback_active = false;
        (void)snprintf(
            state->temporal_invalidation_reason,
            sizeof(state->temporal_invalidation_reason),
            "valid");
    }
}
henka_result henka_opengl_renderer_create(struct henka_renderer* renderer, struct henka_platform* platform, bool enable_vsync)
{
    henka_opengl_renderer_state* state;
    int framebuffer_height;
    int framebuffer_width;
    henka_result result;

    if (renderer == NULL || platform == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    result = henka_renderer_configure_gl_attributes();
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    state = henka_calloc(1U, sizeof(*state));
    if (state == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }

    state->window = henka_platform_get_sdl_window(platform);
    state->gl_context = SDL_GL_CreateContext(state->window);
    if (state->gl_context == NULL)
    {
        HENKA_LOG_ERROR("SDL_GL_CreateContext failed: %s", SDL_GetError());
        henka_free(state);
        return HENKA_ERROR_RENDERER;
    }

    if (!SDL_GL_MakeCurrent(state->window, state->gl_context))
    {
        HENKA_LOG_ERROR("SDL_GL_MakeCurrent failed: %s", SDL_GetError());
        SDL_GL_DestroyContext(state->gl_context);
        henka_free(state);
        return HENKA_ERROR_RENDERER;
    }

    renderer->backend_state = state;
    renderer->platform = platform;

    if (!henka_opengl_load_functions())
    {
        SDL_GL_DestroyContext(state->gl_context);
        henka_free(state);
        renderer->backend_state = NULL;
        return HENKA_ERROR_RENDERER;
    }
    if (g_gl.GenQueries != NULL && g_gl.DeleteQueries != NULL &&
        g_gl.BeginQuery != NULL && g_gl.EndQuery != NULL &&
        g_gl.GetQueryObjectiv != NULL && g_gl.GetQueryObjectui64v != NULL)
    {
        g_gl.GenQueries(1, &state->scene_gpu_query);
        state->gpu_timing_available = state->scene_gpu_query != 0U;
    }

    if (henka_platform_get_framebuffer_size(platform, &framebuffer_width, &framebuffer_height))
    {
        renderer->framebuffer_width = framebuffer_width;
        renderer->framebuffer_height = framebuffer_height;
        renderer->scene_viewport = (henka_viewport){0, 0, framebuffer_width, framebuffer_height};
        renderer->scene_view.viewport = renderer->scene_viewport;
        glViewport(0, 0, framebuffer_width, framebuffer_height);
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    result = henka_opengl_renderer_set_vsync(renderer, enable_vsync);
    if (result != HENKA_SUCCESS)
    {
        SDL_GL_DestroyContext(state->gl_context);
        henka_free(state);
        renderer->backend_state = NULL;
        return result;
    }

    result = henka_opengl_renderer_create_ui_resources(
        &state->ui_program,
        &state->ui_vertex_array,
        &state->ui_vertex_buffer);
    if (result != HENKA_SUCCESS)
    {
        SDL_GL_DestroyContext(state->gl_context);
        henka_free(state);
        renderer->backend_state = NULL;
        return result;
    }

    result = henka_opengl_renderer_create_viewport_program(
        &state->viewport_program);
    if (result == HENKA_SUCCESS)
    {
        static const char* viewport_uniforms[] =
        {
            "model", "view", "projection", "baseColor", "baseColorTexture",
            "useTexture", "lightDirection", "ambientColor", "useLighting"
        };
        if (!henka_populate_shader_location_table(
                state->viewport_program,
                "viewport",
                HENKA_SHADER_CONTRACT_SOLID,
                1U,
                viewport_uniforms,
                sizeof(viewport_uniforms) / sizeof(viewport_uniforms[0]),
                &state->viewport_shader_data))
        {
            result = HENKA_ERROR_RENDERER;
        }
    }
    if (result != HENKA_SUCCESS)
    {
        g_gl.DeleteBuffers(1, &state->ui_vertex_buffer);
        g_gl.DeleteVertexArrays(1, &state->ui_vertex_array);
        g_gl.DeleteProgram(state->ui_program);
        if (state->scene_gpu_query != 0U)
            g_gl.DeleteQueries(1, &state->scene_gpu_query);
        SDL_GL_DestroyContext(state->gl_context);
        henka_free(state);
        renderer->backend_state = NULL;
        return result;
    }

    result = henka_opengl_create_render_programs(state);
    if (result != HENKA_SUCCESS ||
        henka_opengl_create_hdr_target(
            state,
            renderer->scene_viewport.width,
            renderer->scene_viewport.height) != HENKA_SUCCESS ||
        henka_opengl_create_bloom_target(
            state,
            renderer->scene_viewport.width,
            renderer->scene_viewport.height) != HENKA_SUCCESS ||
        henka_opengl_create_shadow_target(state, 1024) != HENKA_SUCCESS ||
        henka_opengl_create_cascade_shadow_target(state, 1024) != HENKA_SUCCESS ||
        henka_opengl_create_local_shadow_target(state, 512) != HENKA_SUCCESS ||
        henka_opengl_create_point_shadow_target(state, 256) != HENKA_SUCCESS)
    {
        henka_opengl_delete_hdr_target(state);
        henka_opengl_delete_bloom_target(state);
        henka_opengl_delete_temporal_history(state);
        henka_opengl_delete_shadow_target(state);
        henka_opengl_delete_cascade_shadow_target(state);
        henka_opengl_delete_local_shadow_target(state);
        henka_opengl_delete_point_shadow_target(state);
        henka_opengl_delete_ibl_resources(state);
        henka_opengl_delete_reflection_probe_resources(state);
        if (state->shadow_program != 0U) g_gl.DeleteProgram(state->shadow_program);
        if (state->tone_program != 0U) g_gl.DeleteProgram(state->tone_program);
        if (state->bloom_blur_program != 0U) g_gl.DeleteProgram(state->bloom_blur_program);
        if (state->bloom_extract_program != 0U) g_gl.DeleteProgram(state->bloom_extract_program);
        if (state->ibl_brdf_program != 0U) g_gl.DeleteProgram(state->ibl_brdf_program);
        if (state->ibl_prefilter_program != 0U) g_gl.DeleteProgram(state->ibl_prefilter_program);
        if (state->ibl_irradiance_program != 0U) g_gl.DeleteProgram(state->ibl_irradiance_program);
        if (state->ibl_conversion_program != 0U) g_gl.DeleteProgram(state->ibl_conversion_program);
        if (state->environment_program != 0U) g_gl.DeleteProgram(state->environment_program);
        if (state->tone_vertex_array != 0U) g_gl.DeleteVertexArrays(1, &state->tone_vertex_array);
        g_gl.DeleteProgram(state->viewport_program);
        g_gl.DeleteBuffers(1, &state->ui_vertex_buffer);
        g_gl.DeleteVertexArrays(1, &state->ui_vertex_array);
        g_gl.DeleteProgram(state->ui_program);
        if (state->scene_gpu_query != 0U)
            g_gl.DeleteQueries(1, &state->scene_gpu_query);
        SDL_GL_DestroyContext(state->gl_context);
        henka_free(state);
        renderer->backend_state = NULL;
        return HENKA_ERROR_RENDERER;
    }

    if (henka_opengl_create_temporal_history(
            state,
            renderer->scene_viewport.width,
            renderer->scene_viewport.height) != HENKA_SUCCESS)
    {
        HENKA_LOG_WARN("temporal history allocation failed; presentation will use the non-temporal path");
    }

    state->instancing_available =
        g_gl.DisableVertexAttribArray != NULL &&
        g_gl.VertexAttribDivisor != NULL &&
        g_gl.DrawElementsInstanced != NULL;
    if (state->instancing_available)
    {
        g_gl.GenBuffers(1, &state->instance_buffer);
        if (state->instance_buffer != 0U)
        {
            g_gl.BindBuffer(GL_ARRAY_BUFFER, state->instance_buffer);
            g_gl.BufferData(
                GL_ARRAY_BUFFER,
                (GLsizeiptr)sizeof(state->instance_data),
                NULL,
                GL_STREAM_DRAW);
            g_gl.BindBuffer(GL_ARRAY_BUFFER, 0U);
            henka_opengl_memory_add_category(
                state,
                &state->tracked_render_target_bytes,
                (uint64_t)sizeof(state->instance_data));
        }
        else
        {
            state->instancing_available = false;
        }
    }
    if (g_gl.GenQueries != NULL && g_gl.DeleteQueries != NULL &&
        g_gl.BeginQuery != NULL && g_gl.EndQuery != NULL &&
        g_gl.GetQueryObjectiv != NULL)
    {
        g_gl.GenQueries(
            HENKA_OPENGL_OCCLUSION_QUERY_CAPACITY,
            state->occlusion_queries);
    }

    HENKA_LOG_INFO("renderer initialized with OpenGL backend");
    return HENKA_SUCCESS;
}

void henka_opengl_renderer_destroy(struct henka_renderer* renderer)
{
    henka_opengl_renderer_state* state;
    size_t index;
    bool main_context_current;

    if (renderer == NULL || renderer->backend_state == NULL)
    {
        return;
    }

    state = (henka_opengl_renderer_state*)renderer->backend_state;
    for (index = 0U; index < HENKA_MAX_TOOL_WINDOWS; ++index)
    {
        if (state->tool_targets[index].id != HENKA_INVALID_WINDOW_ID)
        {
            henka_opengl_renderer_destroy_tool_window_target(
                renderer,
                state->tool_targets[index].id);
        }
    }

    main_context_current =
        SDL_GL_MakeCurrent(state->window, state->gl_context);
    if (!main_context_current)
    {
        HENKA_LOG_WARN(
            "could not make the main OpenGL context current during renderer destruction: %s",
            SDL_GetError());
    }
    else
    {
        if (state->viewport_program != 0U)
        {
            g_gl.DeleteProgram(state->viewport_program);
        }
        henka_opengl_delete_hdr_target(state);
        henka_opengl_delete_bloom_target(state);
        henka_opengl_delete_temporal_history(state);
        henka_opengl_delete_shadow_target(state);
        henka_opengl_delete_cascade_shadow_target(state);
        henka_opengl_delete_local_shadow_target(state);
        henka_opengl_delete_point_shadow_target(state);
        henka_opengl_delete_ibl_resources(state);
        henka_opengl_delete_reflection_probe_resources(state);
        if (state->tone_program != 0U)
        {
            g_gl.DeleteProgram(state->tone_program);
        }
        if (state->bloom_blur_program != 0U)
        {
            g_gl.DeleteProgram(state->bloom_blur_program);
        }
        if (state->bloom_extract_program != 0U)
        {
            g_gl.DeleteProgram(state->bloom_extract_program);
        }
        if (state->ibl_brdf_program != 0U)
        {
            g_gl.DeleteProgram(state->ibl_brdf_program);
        }
        if (state->ibl_prefilter_program != 0U)
        {
            g_gl.DeleteProgram(state->ibl_prefilter_program);
        }
        if (state->ibl_irradiance_program != 0U)
        {
            g_gl.DeleteProgram(state->ibl_irradiance_program);
        }
        if (state->ibl_conversion_program != 0U)
        {
            g_gl.DeleteProgram(state->ibl_conversion_program);
        }
        if (state->environment_program != 0U)
        {
            g_gl.DeleteProgram(state->environment_program);
        }
        if (state->shadow_program != 0U)
        {
            g_gl.DeleteProgram(state->shadow_program);
        }
        if (state->tone_vertex_array != 0U)
        {
            g_gl.DeleteVertexArrays(1, &state->tone_vertex_array);
        }
        if (state->ui_vertex_buffer != 0U)
        {
            g_gl.DeleteBuffers(1, &state->ui_vertex_buffer);
        }
        if (state->ui_vertex_array != 0U)
        {
            g_gl.DeleteVertexArrays(1, &state->ui_vertex_array);
        }
        if (state->ui_program != 0U)
        {
            g_gl.DeleteProgram(state->ui_program);
        }
        if (state->scene_gpu_query != 0U)
        {
            g_gl.DeleteQueries(1, &state->scene_gpu_query);
        }
        if (state->instance_buffer != 0U)
        {
            henka_opengl_memory_remove_category(
                state,
                &state->tracked_render_target_bytes,
                (uint64_t)sizeof(state->instance_data));
            g_gl.DeleteBuffers(1, &state->instance_buffer);
        }
        if (state->occlusion_queries[0] != 0U && g_gl.DeleteQueries != NULL)
        {
            g_gl.DeleteQueries(
                HENKA_OPENGL_OCCLUSION_QUERY_CAPACITY,
                state->occlusion_queries);
        }
    }

    if (state->gl_context != NULL)
    {
        SDL_GL_DestroyContext(state->gl_context);
    }

    henka_free(state);
    renderer->backend_state = NULL;
}

henka_result henka_opengl_renderer_begin_frame(struct henka_renderer* renderer)
{
    henka_opengl_renderer_state* state;

    if (renderer == NULL || renderer->backend_state == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    state = (henka_opengl_renderer_state*)renderer->backend_state;
    if (!SDL_GL_MakeCurrent(state->window, state->gl_context))
    {
        HENKA_LOG_ERROR("SDL_GL_MakeCurrent failed during begin frame: %s", SDL_GetError());
        return HENKA_ERROR_RENDERER;
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    return HENKA_SUCCESS;
}

henka_result henka_opengl_renderer_abort_frame(
    struct henka_renderer* renderer)
{
    henka_opengl_renderer_state* state;

    if (renderer == NULL || renderer->backend_state == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    state = (henka_opengl_renderer_state*)renderer->backend_state;
    if (!SDL_GL_MakeCurrent(state->window, state->gl_context))
    {
        HENKA_LOG_ERROR(
            "SDL_GL_MakeCurrent failed during frame abort: %s",
            SDL_GetError());
        return HENKA_ERROR_RENDERER;
    }

    g_gl.BindBuffer(GL_ARRAY_BUFFER, 0);
    g_gl.BindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    g_gl.UseProgram(0);
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glDisable(GL_SCISSOR_TEST);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    return HENKA_SUCCESS;
}

void henka_opengl_renderer_clear_frame(struct henka_renderer* renderer)
{
    henka_apply_full_framebuffer_viewport(renderer);
    glClearColor(0.08f, 0.09f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

static bool henka_opengl_bounds_in_camera(
    const henka_camera* camera,
    henka_mat4 view,
    henka_bounds bounds)
{
    henka_vec3 center;
    float radius;
    float view_depth;
    float vertical_limit;
    float horizontal_limit;

    if (camera == NULL ||
        !isfinite(bounds.center.x) || !isfinite(bounds.center.y) || !isfinite(bounds.center.z) ||
        !isfinite(bounds.extents.x) || !isfinite(bounds.extents.y) || !isfinite(bounds.extents.z) ||
        bounds.extents.x < 0.0f || bounds.extents.y < 0.0f || bounds.extents.z < 0.0f)
    {
        return true;
    }

    center.x = view.m[0] * bounds.center.x + view.m[4] * bounds.center.y + view.m[8] * bounds.center.z + view.m[12];
    center.y = view.m[1] * bounds.center.x + view.m[5] * bounds.center.y + view.m[9] * bounds.center.z + view.m[13];
    center.z = view.m[2] * bounds.center.x + view.m[6] * bounds.center.y + view.m[10] * bounds.center.z + view.m[14];
    radius = sqrtf(
        bounds.extents.x * bounds.extents.x +
        bounds.extents.y * bounds.extents.y +
        bounds.extents.z * bounds.extents.z);
    if (!isfinite(center.x) || !isfinite(center.y) || !isfinite(center.z) || !isfinite(radius))
    {
        return true;
    }

    view_depth = -center.z;
    if (view_depth + radius < camera->near_plane || view_depth - radius > camera->far_plane)
    {
        return false;
    }
    if (camera->projection_mode == HENKA_CAMERA_PROJECTION_ORTHOGRAPHIC)
    {
        vertical_limit = camera->orthographic_height * 0.5f;
        horizontal_limit = vertical_limit * camera->aspect_ratio;
    }
    else
    {
        float half_fov_tangent = tanf(camera->field_of_view_radians * 0.5f);
        if (!isfinite(half_fov_tangent) || half_fov_tangent <= 0.0f)
        {
            return true;
        }
        vertical_limit = fmaxf(view_depth, camera->near_plane) * half_fov_tangent;
        horizontal_limit = vertical_limit * camera->aspect_ratio;
    }
    if (!isfinite(vertical_limit) || !isfinite(horizontal_limit) ||
        horizontal_limit <= 0.0f || vertical_limit <= 0.0f)
    {
        return true;
    }
    return fabsf(center.x) <= horizontal_limit + radius &&
        fabsf(center.y) <= vertical_limit + radius;
}

static float henka_opengl_entity_view_depth(
    const henka_scene* scene,
    henka_mat4 view,
    size_t index)
{
    const henka_scene_entity_record* entity;
    henka_bounds bounds;
    henka_entity entity_id;
    henka_vec3 center;
    float depth;

    entity = &scene->entities[index];
    center = entity->transform.position;
    if (entity->has_local_bounds)
    {
        entity_id = henka_scene_get_entity_at_index(scene, index);
        if (entity_id != HENKA_INVALID_ENTITY &&
            henka_scene_get_entity_world_bounds(scene, entity_id, &bounds) == HENKA_SUCCESS)
        {
            center = bounds.center;
        }
    }
    depth = -(view.m[2] * center.x +
        view.m[6] * center.y +
        view.m[10] * center.z +
        view.m[14]);
    return isfinite(depth) ? depth : 0.0f;
}

static bool henka_opengl_transparent_item_is_nearer(
    henka_opengl_transparent_sort_item left,
    henka_opengl_transparent_sort_item right)
{
    if (left.view_depth != right.view_depth)
    {
        return left.view_depth < right.view_depth;
    }
    return left.entity_index > right.entity_index;
}

static void henka_opengl_transparent_sift_down(
    henka_opengl_transparent_sort_item* items,
    size_t count,
    size_t root)
{
    size_t child;
    henka_opengl_transparent_sort_item temporary;

    for (;;)
    {
        if (root > (SIZE_MAX - 1U) / 2U)
        {
            return;
        }
        child = root * 2U + 1U;
        if (child >= count)
        {
            return;
        }
        if (child + 1U < count &&
            henka_opengl_transparent_item_is_nearer(items[child + 1U], items[child]))
        {
            ++child;
        }
        if (!henka_opengl_transparent_item_is_nearer(items[child], items[root]))
        {
            return;
        }
        temporary = items[root];
        items[root] = items[child];
        items[child] = temporary;
        root = child;
    }
}

static void henka_opengl_sort_transparent_items(
    henka_opengl_transparent_sort_item* items,
    size_t count)
{
    size_t index;
    henka_opengl_transparent_sort_item temporary;

    if (items == NULL || count < 2U)
    {
        return;
    }
    for (index = count / 2U; index > 0U; --index)
    {
        henka_opengl_transparent_sift_down(items, count, index - 1U);
    }
    for (index = count; index > 1U; --index)
    {
        temporary = items[0U];
        items[0U] = items[index - 1U];
        items[index - 1U] = temporary;
        henka_opengl_transparent_sift_down(items, index - 1U, 0U);
    }
}

static void henka_opengl_prepare_transparent_sort(
    henka_opengl_renderer_state* state,
    const henka_scene* scene,
    henka_mat4 view)
{
    size_t index;

    state->transparent_sort_count = 0U;
    state->transparent_sort_enabled = true;
    state->transparent_sort_overflow_entities = 0U;
    for (index = 0U; index < scene->entity_capacity; ++index)
    {
        const henka_scene_entity_record* entity = &scene->entities[index];

        if (!entity->active || !entity->visible || entity->mesh == NULL ||
            entity->material.shader == NULL ||
            entity->material.alpha_mode != HENKA_MATERIAL_ALPHA_BLENDED)
        {
            continue;
        }
        if (state->transparent_sort_count >= HENKA_OPENGL_TRANSPARENT_SORT_CAPACITY)
        {
            state->transparent_sort_enabled = false;
            if (state->transparent_sort_overflow_entities < UINT32_MAX)
            {
                ++state->transparent_sort_overflow_entities;
            }
            continue;
        }
        state->transparent_sort_items[state->transparent_sort_count++] =
            (henka_opengl_transparent_sort_item){
                index,
                henka_opengl_entity_view_depth(scene, view, index)};
    }

    henka_opengl_sort_transparent_items(
        state->transparent_sort_items,
        state->transparent_sort_count);
}

static bool henka_opengl_select_reflection_probe(
    const henka_scene* scene,
    henka_vec3 position,
    henka_scene_reflection_probe_desc* out_probe,
    uint32_t* out_index)
{
    bool found = false;
    float best_score = FLT_MAX;
    uint32_t best_index = UINT32_MAX;
    uint32_t index;

    if (scene == NULL || out_probe == NULL || out_index == NULL)
    {
        return false;
    }
    for (index = 0U; index < HENKA_SCENE_MAX_REFLECTION_PROBES; ++index)
    {
        const henka_scene_reflection_probe_desc* probe;
        henka_vec3 offset;
        float normalized_distance;
        float score;

        if (!scene->reflection_probe_active[index] ||
            !scene->reflection_probes[index].enabled)
        {
            continue;
        }
        probe = &scene->reflection_probes[index];
        offset = (henka_vec3){
            position.x - probe->position.x,
            position.y - probe->position.y,
            position.z - probe->position.z};
        if (fabsf(offset.x) > probe->extents.x ||
            fabsf(offset.y) > probe->extents.y ||
            fabsf(offset.z) > probe->extents.z)
        {
            continue;
        }
        normalized_distance =
            (offset.x * offset.x) / (probe->extents.x * probe->extents.x) +
            (offset.y * offset.y) / (probe->extents.y * probe->extents.y) +
            (offset.z * offset.z) / (probe->extents.z * probe->extents.z);
        score = normalized_distance / fmaxf(probe->influence, 0.0001f);
        if (!isfinite(score))
        {
            continue;
        }
        if (!found || score < best_score - 0.000001f ||
            (fabsf(score - best_score) <= 0.000001f && index < best_index))
        {
            found = true;
            best_score = score;
            best_index = index;
            *out_probe = *probe;
            *out_index = index;
        }
    }
    return found;
}

static bool henka_opengl_reflection_probe_desc_equal(
    const henka_scene_reflection_probe_desc* left,
    const henka_scene_reflection_probe_desc* right)
{
    return left != NULL && right != NULL &&
        memcmp(left, right, sizeof(*left)) == 0;
}

static bool henka_opengl_ensure_reflection_probe_target(
    henka_opengl_renderer_state* state)
{
    if (state == NULL)
    {
        return false;
    }
    if (state->reflection_probe_framebuffer == 0U)
    {
        g_gl.GenFramebuffers(1, &state->reflection_probe_framebuffer);
    }
    if (state->reflection_probe_depth_buffer == 0U)
    {
        g_gl.GenRenderbuffers(1, &state->reflection_probe_depth_buffer);
        if (state->reflection_probe_depth_buffer != 0U)
        {
            g_gl.BindRenderbuffer(GL_RENDERBUFFER, state->reflection_probe_depth_buffer);
            g_gl.RenderbufferStorage(
                GL_RENDERBUFFER,
                GL_DEPTH_COMPONENT24,
                HENKA_REFLECTION_PROBE_RESOLUTION,
                HENKA_REFLECTION_PROBE_RESOLUTION);
            henka_opengl_memory_add_category(
                state,
                &state->tracked_render_target_bytes,
                (uint64_t)HENKA_REFLECTION_PROBE_RESOLUTION *
                    (uint64_t)HENKA_REFLECTION_PROBE_RESOLUTION * 4U);
            g_gl.BindRenderbuffer(GL_RENDERBUFFER, 0U);
        }
    }
    return state->reflection_probe_framebuffer != 0U &&
        state->reflection_probe_depth_buffer != 0U;
}

static bool henka_opengl_allocate_reflection_probe_cube(GLuint* out_texture)
{
    GLuint texture = 0U;
    int face;

    if (out_texture == NULL)
    {
        return false;
    }
    glGenTextures(1, &texture);
    if (texture == 0U)
    {
        return false;
    }
    glBindTexture(GL_TEXTURE_CUBE_MAP, texture);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_LEVEL, 0);
    for (face = 0; face < 6; ++face)
    {
        glTexImage2D(
            GL_TEXTURE_CUBE_MAP_POSITIVE_X + face,
            0,
            GL_RGBA16F,
            HENKA_REFLECTION_PROBE_RESOLUTION,
            HENKA_REFLECTION_PROBE_RESOLUTION,
            0,
            GL_RGBA,
            GL_HALF_FLOAT,
            NULL);
    }
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0U);
    *out_texture = texture;
    return true;
}

static void henka_opengl_capture_next_reflection_probe(
    struct henka_renderer* renderer,
    const henka_scene* scene)
{
    static const henka_vec3 face_directions[6] =
    {
        {1.0f, 0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f}, {0.0f, -1.0f, 0.0f},
        {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, -1.0f}
    };
    static const henka_vec3 face_ups[6] =
    {
        {0.0f, -1.0f, 0.0f}, {0.0f, -1.0f, 0.0f},
        {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, -1.0f},
        {0.0f, -1.0f, 0.0f}, {0.0f, -1.0f, 0.0f}
    };
    henka_opengl_renderer_state* state;
    henka_scene_reflection_probe_desc probe = {0};
    henka_scene capture_scene;
    GLuint candidate = 0U;
    GLint previous_framebuffer = 0;
    GLint previous_renderbuffer = 0;
    GLint previous_viewport[4] = {0, 0, 0, 0};
    uint32_t probe_index = UINT32_MAX;
    uint32_t offset;
    int face;
    bool success = false;

    if (renderer == NULL || renderer->backend_state == NULL || scene == NULL)
    {
        return;
    }
    state = (henka_opengl_renderer_state*)renderer->backend_state;
    if (state->reflection_probe_capture_active || !state->ibl_ready)
    {
        return;
    }
    for (offset = 0U; offset < HENKA_SCENE_MAX_REFLECTION_PROBES; ++offset)
    {
        uint32_t index = (state->reflection_probe_capture_cursor + offset) %
            HENKA_SCENE_MAX_REFLECTION_PROBES;
        if (!scene->reflection_probe_active[index] ||
            !scene->reflection_probes[index].enabled)
        {
            continue;
        }
        if (state->reflection_probe_capture_ready[index] &&
            state->reflection_probe_captured_scene_revision[index] == scene->render_revision &&
            henka_opengl_reflection_probe_desc_equal(
                &state->reflection_probe_captured_desc[index],
                &scene->reflection_probes[index]) )
        {
            continue;
        }
        probe_index = index;
        probe = scene->reflection_probes[index];
        break;
    }
    state->reflection_probe_capture_cursor =
        (probe_index == UINT32_MAX) ? 0U :
        (probe_index + 1U) % HENKA_SCENE_MAX_REFLECTION_PROBES;
    if (probe_index == UINT32_MAX)
    {
        return;
    }
    if (!henka_opengl_ensure_reflection_probe_target(state) ||
        !henka_opengl_allocate_reflection_probe_cube(&candidate))
    {
        if (state->reflection_probe_capture_failure_count < UINT32_MAX)
            ++state->reflection_probe_capture_failure_count;
        return;
    }

    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previous_framebuffer);
    glGetIntegerv(GL_RENDERBUFFER_BINDING, &previous_renderbuffer);
    glGetIntegerv(GL_VIEWPORT, previous_viewport);
    capture_scene = *scene;
    capture_scene.has_camera = true;
    capture_scene.camera = henka_camera_create_perspective(
        3.14159265359f * 0.5f,
        1.0f,
        0.1f,
        65536.0f);
    capture_scene.camera.position = probe.position;
    state->reflection_probe_capture_active = true;
    state->reflection_probe_capture_index = probe_index;
    g_gl.BindFramebuffer(GL_FRAMEBUFFER, state->reflection_probe_framebuffer);
    g_gl.BindRenderbuffer(GL_RENDERBUFFER, state->reflection_probe_depth_buffer);
    g_gl.FramebufferRenderbuffer(
        GL_FRAMEBUFFER,
        GL_DEPTH_ATTACHMENT,
        GL_RENDERBUFFER,
        state->reflection_probe_depth_buffer);
    for (face = 0; face < 6; ++face)
    {
        g_gl.FramebufferTexture2D(
            GL_FRAMEBUFFER,
            GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_CUBE_MAP_POSITIVE_X + face,
            candidate,
            0);
        if (g_gl.CheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE ||
            !henka_camera_look_at(
                &capture_scene.camera,
                henka_vec3_add(probe.position, face_directions[face])))
        {
            break;
        }
        glViewport(
            0,
            0,
            HENKA_REFLECTION_PROBE_RESOLUTION,
            HENKA_REFLECTION_PROBE_RESOLUTION);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        if (henka_opengl_renderer_draw_scene(renderer, &capture_scene) != HENKA_SUCCESS)
        {
            break;
        }
    }
    success = face == 6;
    state->reflection_probe_capture_active = false;
    state->reflection_probe_capture_index = UINT32_MAX;
    g_gl.BindFramebuffer(GL_FRAMEBUFFER, (GLuint)previous_framebuffer);
    g_gl.BindRenderbuffer(GL_RENDERBUFFER, (GLuint)previous_renderbuffer);
    glViewport(
        previous_viewport[0], previous_viewport[1],
        previous_viewport[2], previous_viewport[3]);
    if (success)
    {
        if (state->reflection_probe_cubes[probe_index] != 0U)
        {
            glDeleteTextures(1, &state->reflection_probe_cubes[probe_index]);
            henka_opengl_memory_remove_category(
                state,
                &state->tracked_render_target_bytes,
                (uint64_t)HENKA_REFLECTION_PROBE_RESOLUTION *
                    (uint64_t)HENKA_REFLECTION_PROBE_RESOLUTION * 6U * 8U);
        }
        state->reflection_probe_cubes[probe_index] = candidate;
        state->reflection_probe_capture_ready[probe_index] = true;
        state->reflection_probe_captured_scene_revision[probe_index] = scene->render_revision;
        state->reflection_probe_captured_desc[probe_index] = probe;
        if (state->reflection_probe_capture_generation < UINT64_MAX)
            ++state->reflection_probe_capture_generation;
        henka_opengl_memory_add_category(
            state,
            &state->tracked_render_target_bytes,
            (uint64_t)HENKA_REFLECTION_PROBE_RESOLUTION *
                (uint64_t)HENKA_REFLECTION_PROBE_RESOLUTION * 6U * 8U);
    }
    else if (candidate != 0U)
    {
        glDeleteTextures(1, &candidate);
        if (state->reflection_probe_capture_failure_count < UINT32_MAX)
            ++state->reflection_probe_capture_failure_count;
    }
}

static bool henka_opengl_material_batch_compatible(
    const henka_material* a,
    const henka_material* b)
{
    if (a == NULL || b == NULL)
    {
        return false;
    }
    return a->type == b->type &&
        a->shader == b->shader &&
        a->base_color_texture == b->base_color_texture &&
        a->normal_texture == b->normal_texture &&
        a->metallic_roughness_texture == b->metallic_roughness_texture &&
        a->occlusion_texture == b->occlusion_texture &&
        a->emissive_texture == b->emissive_texture &&
        a->base_color_uv_set == b->base_color_uv_set &&
        a->normal_uv_set == b->normal_uv_set &&
        a->metallic_roughness_uv_set == b->metallic_roughness_uv_set &&
        a->occlusion_uv_set == b->occlusion_uv_set &&
        a->emissive_uv_set == b->emissive_uv_set &&
        a->base_color.x == b->base_color.x &&
        a->base_color.y == b->base_color.y &&
        a->base_color.z == b->base_color.z &&
        a->base_color.w == b->base_color.w &&
        a->emissive_color.x == b->emissive_color.x &&
        a->emissive_color.y == b->emissive_color.y &&
        a->emissive_color.z == b->emissive_color.z &&
        a->metallic == b->metallic &&
        a->roughness == b->roughness &&
        a->specular_factor == b->specular_factor &&
        a->specular_color.x == b->specular_color.x &&
        a->specular_color.y == b->specular_color.y &&
        a->specular_color.z == b->specular_color.z &&
        a->ior == b->ior &&
        a->transmission == b->transmission &&
        a->thickness == b->thickness &&
        a->attenuation_distance == b->attenuation_distance &&
        a->attenuation_color.x == b->attenuation_color.x &&
        a->attenuation_color.y == b->attenuation_color.y &&
        a->attenuation_color.z == b->attenuation_color.z &&
        a->normal_scale == b->normal_scale &&
        a->occlusion_strength == b->occlusion_strength &&
        a->emissive_strength == b->emissive_strength &&
        a->clearcoat == b->clearcoat &&
        a->clearcoat_roughness == b->clearcoat_roughness &&
        a->alpha_cutoff == b->alpha_cutoff &&
        a->use_texture == b->use_texture &&
        a->use_lighting == b->use_lighting &&
        a->depth_test == b->depth_test &&
        a->alpha_mode == b->alpha_mode &&
        a->double_sided == b->double_sided &&
        a->cast_shadows == b->cast_shadows &&
        a->receive_shadows == b->receive_shadows &&
        a->sheen_color.x == b->sheen_color.x &&
        a->sheen_color.y == b->sheen_color.y &&
        a->sheen_color.z == b->sheen_color.z &&
        a->sheen_roughness == b->sheen_roughness;
}

static bool henka_opengl_scene_has_active_reflection_probe(const henka_scene* scene)
{
    size_t index;

    if (scene == NULL)
    {
        return false;
    }
    for (index = 0U; index < HENKA_SCENE_MAX_REFLECTION_PROBES; ++index)
    {
        if (scene->reflection_probe_active[index] &&
            scene->reflection_probes[index].enabled)
        {
            return true;
        }
    }
    return false;
}

static bool henka_opengl_mat4_nearly_equal(henka_mat4 a, henka_mat4 b)
{
    size_t index;
    for (index = 0U; index < 16U; ++index)
    {
        if (fabsf(a.m[index] - b.m[index]) > 0.0001f)
        {
            return false;
        }
    }
    return true;
}

static bool henka_opengl_can_reuse_occlusion_result(
    const henka_opengl_renderer_state* state,
    const henka_scene* scene,
    size_t entity_index,
    const henka_scene_entity_record* entity,
    henka_mat4 current_view_projection)
{
    if (state == NULL || scene == NULL || entity == NULL ||
        entity_index >= HENKA_OPENGL_OCCLUSION_QUERY_CAPACITY ||
        state->occlusion_queries[entity_index] == 0U ||
        !state->occlusion_history_valid ||
        !state->occlusion_query_valid[entity_index] ||
        state->occlusion_query_scene_revision[entity_index] != scene->render_revision ||
        !entity->previous_transform_valid ||
        !henka_opengl_mat4_nearly_equal(
            henka_transform_to_mat4(entity->transform),
            henka_transform_to_mat4(entity->previous_transform)) ||
        !henka_opengl_mat4_nearly_equal(
            current_view_projection,
            state->occlusion_query_view_projection))
    {
        return false;
    }
    return true;
}

static bool henka_opengl_entity_can_join_instance_batch(
    const henka_scene* scene,
    const henka_scene_entity_record* current,
    const henka_scene_entity_record* candidate,
    size_t candidate_index,
    henka_mesh* selected_mesh,
    henka_mat4 view)
{
    if (scene == NULL || current == NULL || candidate == NULL ||
        selected_mesh == NULL || !candidate->active || !candidate->visible ||
        candidate->mesh != selected_mesh || candidate->lod.level_count != 0U ||
        (candidate->flags & HENKA_SCENE_ENTITY_FLAG_HELPER) != 0U ||
        candidate->material.alpha_mode == HENKA_MATERIAL_ALPHA_BLENDED ||
        !henka_opengl_material_batch_compatible(&current->material, &candidate->material))
    {
        return false;
    }
    if (candidate->has_local_bounds)
    {
        henka_entity entity_id = henka_scene_get_entity_at_index(scene, candidate_index);
        henka_bounds world_bounds;
        if (entity_id != HENKA_INVALID_ENTITY &&
            henka_scene_get_entity_world_bounds(scene, entity_id, &world_bounds) == HENKA_SUCCESS &&
            !henka_opengl_bounds_in_camera(&scene->camera, view, world_bounds))
        {
            return false;
        }
    }
    return true;
}

static void henka_opengl_prepare_instance_attributes(
    henka_opengl_renderer_state* state,
    const henka_opengl_mesh_data* mesh_data)
{
    unsigned int column;
    if (state == NULL || mesh_data == NULL || state->instance_buffer == 0U)
    {
        return;
    }
    g_gl.BindVertexArray(mesh_data->vao);
    g_gl.BindBuffer(GL_ARRAY_BUFFER, state->instance_buffer);
    for (column = 0U; column < 4U; ++column)
    {
        GLuint model_location = 6U + column;
        GLuint previous_location = 10U + column;
        g_gl.EnableVertexAttribArray(model_location);
        g_gl.VertexAttribPointer(
            model_location,
            4,
            GL_FLOAT,
            GL_FALSE,
            (GLsizei)sizeof(henka_opengl_instance_data),
            (const void*)(offsetof(henka_opengl_instance_data, model) + sizeof(float) * 4U * column));
        g_gl.VertexAttribDivisor(model_location, 1U);
        g_gl.EnableVertexAttribArray(previous_location);
        g_gl.VertexAttribPointer(
            previous_location,
            4,
            GL_FLOAT,
            GL_FALSE,
            (GLsizei)sizeof(henka_opengl_instance_data),
            (const void*)(offsetof(henka_opengl_instance_data, previous_model) + sizeof(float) * 4U * column));
        g_gl.VertexAttribDivisor(previous_location, 1U);
    }
}

static void henka_opengl_reset_instance_attributes(
    const henka_opengl_mesh_data* mesh_data)
{
    unsigned int location;
    if (mesh_data == NULL)
    {
        return;
    }
    g_gl.BindVertexArray(mesh_data->vao);
    for (location = 6U; location < 14U; ++location)
    {
        g_gl.VertexAttribDivisor(location, 0U);
        g_gl.DisableVertexAttribArray(location);
    }
}

henka_result henka_opengl_renderer_draw_scene(
    struct henka_renderer* renderer,
    const struct henka_scene* scene)
{
    static const henka_vec4 solid_color =
        {0.64f, 0.68f, 0.74f, 1.0f};
    static const henka_vec4 wire_color =
        {0.78f, 0.82f, 0.88f, 1.0f};
    static const henka_vec3 preview_light_direction =
        {0.35f, -0.85f, 0.40f};
    static const henka_vec3 preview_light_color =
        {1.0f, 0.94f, 0.88f};
    static const henka_vec3 preview_ambient =
        {0.24f, 0.26f, 0.30f};
    henka_mat4 projection;
    henka_mat4 light_matrix;
    henka_mat4 cascade_shadow_matrix;
    henka_mat4 local_shadow_matrix;
    henka_mat4 current_view_projection;
    henka_viewport_render_policy policy;
    henka_opengl_renderer_state* state;
    henka_mat4 view;
    bool rendered;
    size_t index;
    size_t pass;
    size_t pass_count;
    float local_light_position_range[HENKA_SCENE_MAX_LOCAL_LIGHTS * 4U] = {0.0f};
    float local_light_color_intensity[HENKA_SCENE_MAX_LOCAL_LIGHTS * 4U] = {0.0f};
    float local_light_direction_inner[HENKA_SCENE_MAX_LOCAL_LIGHTS * 4U] = {0.0f};
    float local_light_outer_type[HENKA_SCENE_MAX_LOCAL_LIGHTS * 4U] = {0.0f};
    int local_light_count = 0;
    int local_shadow_light_index = -1;
    int point_shadow_light_index = -1;
    Uint64 cpu_start_ticks;
    bool gpu_query_active = false;

    if (renderer == NULL ||
        scene == NULL ||
        !scene->has_camera)
    {
        return HENKA_SUCCESS;
    }

    if (henka_viewport_render_policy_resolve(
            henka_renderer_get_viewport_shading_mode(renderer),
            &policy) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    state =
        (henka_opengl_renderer_state*)
            renderer->backend_state;
    if (state == NULL ||
        state->viewport_program == 0U)
    {
        return HENKA_ERROR_RENDERER;
    }
    state->reflection_probe_enabled_count = 0U;
    state->reflection_probe_captured_count = 0U;
    for (index = 0U; index < HENKA_SCENE_MAX_REFLECTION_PROBES; ++index)
    {
        if (scene->reflection_probe_active[index] && scene->reflection_probes[index].enabled)
        {
            if (state->reflection_probe_enabled_count < UINT32_MAX)
                ++state->reflection_probe_enabled_count;
            if (state->reflection_probe_capture_ready[index] &&
                state->reflection_probe_cubes[index] != 0U &&
                state->reflection_probe_captured_scene_revision[index] == scene->render_revision)
            {
                if (state->reflection_probe_captured_count < UINT32_MAX)
                    ++state->reflection_probe_captured_count;
            }
        }
    }
    cpu_start_ticks = SDL_GetPerformanceCounter();
    if (state->gpu_timing_available && state->scene_gpu_query_pending)
    {
        GLint query_available = GL_FALSE;
        state->gpu_timing_available = state->gpu_timing_available &&
            state->scene_gpu_query != 0U;
        g_gl.GetQueryObjectiv(
            state->scene_gpu_query,
            GL_QUERY_RESULT_AVAILABLE,
            &query_available);
        if (query_available == GL_TRUE)
        {
            GLuint64 elapsed_nanoseconds = 0U;
            g_gl.GetQueryObjectui64v(
                state->scene_gpu_query,
                GL_QUERY_RESULT,
                &elapsed_nanoseconds);
            state->scene_gpu_time_milliseconds =
                (double)elapsed_nanoseconds / 1000000.0;
            state->scene_gpu_query_pending = false;
        }
    }
    state->scene_draw_calls = 0U;
    state->scene_visible_entities = 0U;
    state->scene_culled_entities = 0U;
    state->scene_budget_dropped_entities = 0U;
    state->scene_lod_entities = 0U;
    state->scene_lod_fallback_entities = 0U;
    state->scene_instanced_draw_calls = 0U;
    state->scene_instanced_entities = 0U;
    state->scene_occlusion_tested_entities = 0U;
    state->scene_occlusion_culled_entities = 0U;
    for (index = 0U; index < HENKA_SCENE_MAX_LOCAL_LIGHTS; ++index)
    {
        const henka_scene_light_desc* light = &scene->local_lights[index];
        if (!scene->local_light_active[index] || !light->enabled)
        {
            continue;
        }
        local_light_position_range[local_light_count * 4U + 0U] = light->position.x;
        local_light_position_range[local_light_count * 4U + 1U] = light->position.y;
        local_light_position_range[local_light_count * 4U + 2U] = light->position.z;
        local_light_position_range[local_light_count * 4U + 3U] = light->range;
        local_light_color_intensity[local_light_count * 4U + 0U] = light->color.x;
        local_light_color_intensity[local_light_count * 4U + 1U] = light->color.y;
        local_light_color_intensity[local_light_count * 4U + 2U] = light->color.z;
        local_light_color_intensity[local_light_count * 4U + 3U] = light->intensity;
        local_light_direction_inner[local_light_count * 4U + 0U] = light->direction.x;
        local_light_direction_inner[local_light_count * 4U + 1U] = light->direction.y;
        local_light_direction_inner[local_light_count * 4U + 2U] = light->direction.z;
        local_light_direction_inner[local_light_count * 4U + 3U] = light->inner_cone_cosine;
        local_light_outer_type[local_light_count * 4U + 0U] = light->outer_cone_cosine;
        local_light_outer_type[local_light_count * 4U + 1U] = light->type == HENKA_SCENE_LIGHT_SPOT ? 1.0f : 0.0f;
        if (local_shadow_light_index < 0 && light->type == HENKA_SCENE_LIGHT_SPOT)
        {
            local_shadow_light_index = local_light_count;
        }
        if (point_shadow_light_index < 0 && light->type == HENKA_SCENE_LIGHT_POINT)
        {
            point_shadow_light_index = local_light_count;
        }
        ++local_light_count;
    }

    rendered = henka_renderer_get_viewport_shading_mode(renderer) ==
        HENKA_VIEWPORT_SHADING_RENDERED;
    light_matrix = henka_opengl_get_light_matrix(scene, 24.0f, 36.0f);
    cascade_shadow_matrix = henka_opengl_get_light_matrix(scene, 72.0f, 96.0f);
    local_shadow_matrix = henka_mat4_identity();
    if (local_shadow_light_index >= 0)
    {
        size_t shadow_index = 0U;
        for (; shadow_index < HENKA_SCENE_MAX_LOCAL_LIGHTS; ++shadow_index)
        {
            const henka_scene_light_desc* light = &scene->local_lights[shadow_index];
            if (scene->local_light_active[shadow_index] && light->enabled &&
                light->type == HENKA_SCENE_LIGHT_SPOT)
            {
                local_shadow_matrix = henka_opengl_get_spot_light_matrix(light);
                break;
            }
        }
    }
    if (local_shadow_light_index >= 0)
    {
        local_light_outer_type[local_shadow_light_index * 4U + 2U] = 1.0f;
    }
    if (point_shadow_light_index >= 0)
    {
        local_light_outer_type[point_shadow_light_index * 4U + 2U] = 2.0f;
    }
    if (policy.use_hdr_presentation && !state->reflection_probe_capture_active)
    {
        henka_viewport scene_viewport = henka_renderer_get_scene_viewport(renderer);

        if (state->hdr_width != scene_viewport.width ||
            state->hdr_height != scene_viewport.height)
        {
            henka_opengl_renderer_sync_scene_target(renderer);
        }
        if (rendered)
        {
            henka_opengl_draw_shadow_pass(
                state, scene, light_matrix,
                state->shadow_framebuffer, state->shadow_resolution);
            if (state->cascade_shadow_framebuffer_complete)
            {
                henka_opengl_draw_shadow_pass(
                    state, scene, cascade_shadow_matrix,
                    state->cascade_shadow_framebuffer, state->cascade_shadow_resolution);
            }
            if (local_shadow_light_index >= 0 && state->local_shadow_framebuffer_complete)
            {
                henka_opengl_draw_shadow_pass(
                    state, scene, local_shadow_matrix,
                    state->local_shadow_framebuffer, state->local_shadow_resolution);
            }
            if (point_shadow_light_index >= 0)
            {
                size_t point_index = 0U;
                for (; point_index < HENKA_SCENE_MAX_LOCAL_LIGHTS; ++point_index)
                {
                    const henka_scene_light_desc* light = &scene->local_lights[point_index];
                    if (scene->local_light_active[point_index] && light->enabled &&
                        light->type == HENKA_SCENE_LIGHT_POINT)
                    {
                        henka_opengl_draw_point_shadow_pass(state, scene, light);
                        break;
                    }
                }
            }
        }
        if (!henka_opengl_renderer_is_hdr_ready(renderer))
        {
            return HENKA_ERROR_RENDERER;
        }
        g_gl.BindFramebuffer(GL_FRAMEBUFFER, state->hdr_framebuffer);
        henka_apply_scene_target_viewport(renderer);
        glClearColor(0.075f, 0.09f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        henka_opengl_sync_ibl_resources(state, scene);
        henka_opengl_capture_next_reflection_probe(renderer, scene);
        henka_opengl_draw_environment(state, renderer, scene);
    }
    else if (!state->reflection_probe_capture_active)
    {
        henka_apply_scene_viewport(renderer);
    }
    if (state->gpu_timing_available && !state->scene_gpu_query_pending)
    {
        g_gl.BeginQuery(GL_TIME_ELAPSED, state->scene_gpu_query);
        gpu_query_active = true;
    }
    projection =
        henka_camera_get_projection_matrix(&scene->camera);
    view = henka_camera_get_view_matrix(&scene->camera);
    {
        bool use_temporal_jitter = rendered &&
            policy.use_hdr_presentation && state->temporal_history_ready;
        if (state->temporal_jitter_enabled != use_temporal_jitter)
        {
            henka_opengl_invalidate_temporal_history(
                state,
                use_temporal_jitter ? "rendered temporal path enabled" : "shading mode changed");
        }
        state->temporal_jitter_enabled = use_temporal_jitter;
        if (use_temporal_jitter)
        {
            henka_viewport temporal_viewport = henka_renderer_get_scene_viewport(renderer);
            uint64_t sample_index = (state->temporal_jitter_index % 8U) + 1U;
            state->temporal_jitter_index = (state->temporal_jitter_index + 1U) % 8U;
            state->temporal_jitter_x = temporal_viewport.width > 0 ?
                (henka_opengl_temporal_halton(sample_index, 2U) - 0.5f) /
                    (float)temporal_viewport.width : 0.0f;
            state->temporal_jitter_y = temporal_viewport.height > 0 ?
                (henka_opengl_temporal_halton(sample_index, 3U) - 0.5f) /
                    (float)temporal_viewport.height : 0.0f;
            /* The perspective projection stores the clip-space offset in
             * the z-column entries; this shifts the sample without changing
             * the camera's logical transform or scene materials. */
            projection.m[8] += 2.0f * state->temporal_jitter_x;
            projection.m[9] += 2.0f * state->temporal_jitter_y;
        }
        else
        {
            state->temporal_jitter_x = 0.0f;
            state->temporal_jitter_y = 0.0f;
        }
    }
    current_view_projection = henka_mat4_multiply(projection, view);
    state->current_projection = projection;
    if (state->previous_view_projection_valid &&
        henka_opengl_temporal_matrix_is_cut(
            state->previous_view_projection,
            current_view_projection))
    {
        henka_opengl_invalidate_temporal_history(state, "camera or projection cut");
    }
    if (!state->previous_view_projection_valid)
        state->previous_view_projection = current_view_projection;
    henka_opengl_prepare_transparent_sort(state, scene, view);
    g_gl.ActiveTexture(GL_TEXTURE0);

    pass_count = 2U;
    for (pass = 0U; pass < pass_count; ++pass)
    {
        const size_t pass_entity_count = pass == 0U ||
            !state->transparent_sort_enabled ?
            scene->entity_capacity : state->transparent_sort_count;

        for (index = 0U; index < pass_entity_count; ++index)
        {
        size_t draw_index = pass == 0U || !state->transparent_sort_enabled ? index :
            state->transparent_sort_items[index].entity_index;
        const henka_scene_entity_record* entity;
        const henka_opengl_mesh_data* mesh_data;
        const henka_opengl_shader_data* shader_data;
        const henka_opengl_texture_data* texture_data;
        const henka_opengl_texture_data* normal_texture_data;
        const henka_opengl_texture_data* metallic_roughness_texture_data;
        const henka_opengl_texture_data* occlusion_texture_data;
        const henka_opengl_texture_data* emissive_texture_data;
        henka_vec3 ambient_color;
        henka_vec4 base_color;
        bool editor_surface;
        bool helper_entity;
        henka_vec3 light_direction;
        henka_vec3 light_color;
        float light_intensity;
        GLuint program;
        bool use_lighting;
        bool use_texture;
        henka_mat4 model;
        henka_mat4 previous_model;
        henka_bounds world_bounds;
        henka_entity entity_id = HENKA_INVALID_ENTITY;
        henka_mesh* selected_mesh;
        henka_vec3 lod_center;
        float lod_distance;
        uint32_t lod_index;
        henka_scene_reflection_probe_desc reflection_probe = {0};
        henka_vec3 reflection_probe_center;
        bool use_reflection_probe;
        uint32_t reflection_probe_index = UINT32_MAX;
        bool use_reflection_probe_map;
        size_t instance_count = 1U;
        bool occlusion_query_active = false;

        entity = &scene->entities[draw_index];
        if (!entity->active ||
            !entity->visible ||
            entity->mesh == NULL ||
            entity->material.shader == NULL)
        {
            continue;
        }

        if ((pass == 0U && entity->material.alpha_mode == HENKA_MATERIAL_ALPHA_BLENDED) ||
            (pass == 1U && entity->material.alpha_mode != HENKA_MATERIAL_ALPHA_BLENDED))
        {
            continue;
        }

        if ((entity->flags & HENKA_SCENE_ENTITY_FLAG_HELPER) == 0U &&
            entity->has_local_bounds)
        {
            entity_id = henka_scene_get_entity_at_index(scene, draw_index);
            if (entity_id != HENKA_INVALID_ENTITY &&
                henka_scene_get_entity_world_bounds(scene, entity_id, &world_bounds) == HENKA_SUCCESS &&
                !henka_opengl_bounds_in_camera(&scene->camera, view, world_bounds))
            {
                state->scene_culled_entities += 1U;
                continue;
            }
        }

        selected_mesh = entity->mesh;
        lod_center = entity->transform.position;
        if (entity->has_local_bounds &&
            entity_id != HENKA_INVALID_ENTITY &&
            henka_scene_get_entity_world_bounds(scene, entity_id, &world_bounds) == HENKA_SUCCESS)
        {
            lod_center = world_bounds.center;
        }
        lod_distance = henka_vec3_length(henka_vec3_subtract(lod_center, scene->camera.position));
        if (entity->lod.level_count > 0U && isfinite(lod_distance))
        {
            state->scene_lod_entities += 1U;
            selected_mesh = entity->lod.meshes[entity->lod.level_count - 1U];
            for (lod_index = 0U; lod_index < entity->lod.level_count; ++lod_index)
            {
                if (lod_distance <= entity->lod.max_distances[lod_index])
                {
                    selected_mesh = entity->lod.meshes[lod_index];
                    break;
                }
            }
            if (selected_mesh == NULL || selected_mesh->backend_data == NULL)
            {
                state->scene_lod_fallback_entities += 1U;
                selected_mesh = entity->mesh;
            }
        }

        if (pass == 0U && entity->lod.level_count == 0U &&
            (entity->flags & HENKA_SCENE_ENTITY_FLAG_HELPER) == 0U &&
            draw_index < HENKA_OPENGL_OCCLUSION_QUERY_CAPACITY &&
            henka_opengl_can_reuse_occlusion_result(
                state,
                scene,
                draw_index,
                entity,
                current_view_projection))
        {
            GLint query_available = GL_FALSE;
            g_gl.GetQueryObjectiv(
                state->occlusion_queries[draw_index],
                GL_QUERY_RESULT_AVAILABLE,
                &query_available);
            if (query_available == GL_TRUE)
            {
                GLint samples_passed = GL_TRUE;
                g_gl.GetQueryObjectiv(
                    state->occlusion_queries[draw_index],
                    GL_QUERY_RESULT,
                    &samples_passed);
                state->scene_occlusion_tested_entities += 1U;
                if (samples_passed == GL_FALSE)
                {
                    state->scene_occlusion_culled_entities += 1U;
                    continue;
                }
            }
        }

        if (state->scene_draw_calls >= HENKA_OPENGL_SCENE_DRAW_BUDGET)
        {
            state->scene_budget_dropped_entities += 1U;
            continue;
        }

        mesh_data =
            (const henka_opengl_mesh_data*)
                selected_mesh->backend_data;
        shader_data =
            (const henka_opengl_shader_data*)
                entity->material.shader->backend_data;
        if (mesh_data == NULL || shader_data == NULL)
        {
            continue;
        }

        helper_entity =
            (entity->flags &
                HENKA_SCENE_ENTITY_FLAG_HELPER) != 0U;
        reflection_probe_center = entity->transform.position;
        if (entity->has_local_bounds &&
            entity_id != HENKA_INVALID_ENTITY &&
            henka_scene_get_entity_world_bounds(scene, entity_id, &world_bounds) == HENKA_SUCCESS)
        {
            reflection_probe_center = world_bounds.center;
        }
        use_reflection_probe = !helper_entity && !state->reflection_probe_capture_active &&
            state->ibl_ready && henka_opengl_select_reflection_probe(
                scene,
                reflection_probe_center,
                &reflection_probe,
                &reflection_probe_index);
        use_reflection_probe_map = use_reflection_probe &&
            reflection_probe_index < HENKA_SCENE_MAX_REFLECTION_PROBES &&
            state->reflection_probe_capture_ready[reflection_probe_index] &&
            state->reflection_probe_cubes[reflection_probe_index] != 0U;
        use_reflection_probe = use_reflection_probe && reflection_probe.box_projection;
        editor_surface =
            !helper_entity &&
            henka_renderer_get_viewport_shading_mode(
                renderer) <= HENKA_VIEWPORT_SHADING_SOLID;
        if (editor_surface)
        {
            shader_data = &state->viewport_shader_data;
        }
        program =
            editor_surface ?
                state->viewport_program :
                shader_data->program;
        base_color = entity->material.base_color;
        light_direction = scene->light_direction;
        light_color = scene->light_color;
        light_intensity = scene->light_intensity;
        ambient_color = scene->ambient_color;
        use_lighting = entity->material.use_lighting;

        if (!helper_entity)
        {
            if (!policy.use_material_base_color)
            {
                base_color =
                    policy.polygon_wireframe ?
                    wire_color :
                    solid_color;
            }
            if (policy.use_preview_lighting)
            {
                light_direction =
                    preview_light_direction;
                light_color = preview_light_color;
                light_intensity = 2.5f;
                ambient_color = preview_ambient;
            }
            if (policy.force_unlit)
            {
                use_lighting = false;
            }
            else if (entity->material.type == HENKA_MATERIAL_TYPE_UNLIT)
            {
                use_lighting = false;
            }
            else if (!policy.use_scene_lighting &&
                     !policy.use_preview_lighting)
            {
                use_lighting = false;
            }
            else if (!policy.use_material_base_color)
            {
                use_lighting = true;
            }
        }

        texture_data = NULL;
        if (entity->material.use_texture &&
            entity->material.base_color_texture != NULL)
        {
            texture_data =
                (const henka_opengl_texture_data*)
                    entity->material
                        .base_color_texture->backend_data;
        }
        normal_texture_data = entity->material.normal_texture != NULL ?
            (const henka_opengl_texture_data*)entity->material.normal_texture->backend_data : NULL;
        metallic_roughness_texture_data = entity->material.metallic_roughness_texture != NULL ?
            (const henka_opengl_texture_data*)entity->material.metallic_roughness_texture->backend_data : NULL;
        occlusion_texture_data = entity->material.occlusion_texture != NULL ?
            (const henka_opengl_texture_data*)entity->material.occlusion_texture->backend_data : NULL;
        emissive_texture_data = entity->material.emissive_texture != NULL ?
            (const henka_opengl_texture_data*)entity->material.emissive_texture->backend_data : NULL;

        use_texture =
            texture_data != NULL &&
            texture_data->texture_id != 0U &&
            (helper_entity ||
                policy.sample_material_texture);

        glPolygonMode(
            GL_FRONT_AND_BACK,
            !helper_entity &&
                policy.polygon_wireframe ?
                GL_LINE :
                GL_FILL);

        if (entity->material.depth_test)
        {
            glEnable(GL_DEPTH_TEST);
        }
        else
        {
            glDisable(GL_DEPTH_TEST);
        }

        model =
            henka_transform_to_mat4(entity->transform);
        previous_model = entity->previous_transform_valid ?
            henka_transform_to_mat4(entity->previous_transform) : model;
        if (state->instancing_available &&
            pass == 0U &&
            entity->lod.level_count == 0U &&
            (entity->flags & HENKA_SCENE_ENTITY_FLAG_HELPER) == 0U &&
            entity->material.alpha_mode != HENKA_MATERIAL_ALPHA_BLENDED &&
            !henka_opengl_scene_has_active_reflection_probe(scene) &&
            henka_opengl_shader_uniform_location(program, shader_data, "useInstancing") >= 0)
        {
            state->instance_data[0U].model = model;
            state->instance_data[0U].previous_model = previous_model;
            while (instance_count < HENKA_OPENGL_INSTANCE_CAPACITY &&
                   instance_count < scene->entity_capacity - draw_index &&
                   henka_opengl_entity_can_join_instance_batch(
                       scene,
                       entity,
                       &scene->entities[draw_index + instance_count],
                       draw_index + instance_count,
                       selected_mesh,
                       view))
            {
                const henka_scene_entity_record* candidate =
                    &scene->entities[draw_index + instance_count];
                state->instance_data[instance_count].model =
                    henka_transform_to_mat4(candidate->transform);
                state->instance_data[instance_count].previous_model =
                    candidate->previous_transform_valid ?
                    henka_transform_to_mat4(candidate->previous_transform) :
                    state->instance_data[instance_count].model;
                ++instance_count;
            }
        }
        g_gl.UseProgram(program);
#define henka_set_uniform_mat4(program_value, name_value, value_value) \
    henka_set_uniform_mat4_owned(program_value, shader_data, name_value, value_value)
#define henka_set_uniform_vec4(program_value, name_value, value_value) \
    henka_set_uniform_vec4_owned(program_value, shader_data, name_value, value_value)
#define henka_set_uniform_vec3(program_value, name_value, value_value) \
    henka_set_uniform_vec3_owned(program_value, shader_data, name_value, value_value)
#define henka_set_uniform_bool(program_value, name_value, value_value) \
    henka_set_uniform_bool_owned(program_value, shader_data, name_value, value_value)
#define henka_set_uniform_int(program_value, name_value, value_value) \
    henka_set_uniform_int_owned(program_value, shader_data, name_value, value_value)
#define henka_set_uniform_float(program_value, name_value, value_value) \
    henka_set_uniform_float_owned(program_value, shader_data, name_value, value_value)
        henka_set_uniform_mat4(
            program,
            "model",
            model);
        henka_set_uniform_mat4(
            program,
            "view",
            view);
        henka_set_uniform_mat4(
            program,
            "projection",
            projection);
        henka_set_uniform_mat4_owned(
            program,
            shader_data,
            "previousViewProjection",
            state->previous_view_projection);
        henka_set_uniform_mat4_owned(
            program,
            shader_data,
            "previousModel",
            previous_model);
        henka_set_uniform_bool_owned(
            program,
            shader_data,
            "useMotionVectors",
            rendered && !helper_entity && state->previous_view_projection_valid);
        henka_set_uniform_bool_owned(program, shader_data, "useInstancing", false);
        henka_set_uniform_mat4(program, "lightMatrix", light_matrix);
        henka_set_uniform_mat4(program, "cascadeShadowMatrix", cascade_shadow_matrix);
        henka_set_uniform_mat4(program, "localShadowMatrix", local_shadow_matrix);
        henka_set_uniform_vec4(
            program,
            "baseColor",
            base_color);
        henka_set_uniform_vec3(
            program,
            "lightDirection",
            light_direction);
        henka_set_uniform_vec3(program, "lightColor", light_color);
        henka_set_uniform_float(program, "lightIntensity", light_intensity);
        henka_set_uniform_vec3(program, "cameraPosition", scene->camera.position);
        henka_set_uniform_vec3(
            program,
            "ambientColor",
            ambient_color);
        henka_set_uniform_bool(
            program,
            "useEnvironment",
            !helper_entity && policy.use_hdr_presentation);
        henka_set_uniform_vec3(
            program,
            "environmentGroundColor",
            scene->environment.ground_color);
        henka_set_uniform_vec3(
            program,
            "environmentHorizonColor",
            scene->environment.horizon_color);
        henka_set_uniform_vec3(
            program,
            "environmentZenithColor",
            scene->environment.zenith_color);
        henka_set_uniform_float(program, "environmentIntensity", scene->environment.intensity);
        henka_set_uniform_int(program, "environmentTexture", 6);
        henka_set_uniform_bool(
            program,
            "useEnvironmentTexture",
            !helper_entity &&
            scene->environment.hdr_texture != NULL &&
            scene->environment.hdr_texture->backend_data != NULL);
        henka_set_uniform_float(program, "environmentRotation", scene->environment.hdr_rotation);
        henka_set_uniform_int_owned(program, shader_data, "iblIrradianceMap", 7);
        henka_set_uniform_int_owned(program, shader_data, "iblPrefilterMap", 8);
        henka_set_uniform_int_owned(program, shader_data, "iblBrdfLut", 9);
        henka_set_uniform_bool_owned(program, shader_data, "useIBL", !helper_entity && state->ibl_ready);
        henka_set_uniform_vec3_owned(
            program,
            shader_data,
            "reflectionProbePosition",
            use_reflection_probe ? reflection_probe.position : (henka_vec3){0.0f, 0.0f, 0.0f});
        henka_set_uniform_vec3_owned(
            program,
            shader_data,
            "reflectionProbeExtents",
            use_reflection_probe ? reflection_probe.extents : (henka_vec3){1.0f, 1.0f, 1.0f});
        henka_set_uniform_bool_owned(program, shader_data, "useReflectionProbe", use_reflection_probe);
        henka_set_uniform_int_owned(program, shader_data, "reflectionProbeMap", 10);
        henka_set_uniform_bool_owned(program, shader_data, "useReflectionProbeMap", use_reflection_probe_map);
        henka_set_uniform_int(program, "localLightCount", local_light_count);
        henka_set_uniform_vec4_array_owned(
            program,
            shader_data,
            "localLightPositionRange[0]",
            local_light_position_range,
            (int)HENKA_SCENE_MAX_LOCAL_LIGHTS);
        henka_set_uniform_vec4_array_owned(
            program,
            shader_data,
            "localLightColorIntensity[0]",
            local_light_color_intensity,
            (int)HENKA_SCENE_MAX_LOCAL_LIGHTS);
        henka_set_uniform_vec4_array_owned(
            program,
            shader_data,
            "localLightDirectionInner[0]",
            local_light_direction_inner,
            (int)HENKA_SCENE_MAX_LOCAL_LIGHTS);
        henka_set_uniform_vec4_array_owned(
            program,
            shader_data,
            "localLightOuterType[0]",
            local_light_outer_type,
            (int)HENKA_SCENE_MAX_LOCAL_LIGHTS);
        henka_set_uniform_bool(program, "fogEnabled", !helper_entity && scene->fog.enabled);
        henka_set_uniform_int(program, "fogMode", (int)scene->fog.mode);
        henka_set_uniform_vec3(program, "fogColor", scene->fog.color);
        henka_set_uniform_float(program, "fogStartDistance", scene->fog.start_distance);
        henka_set_uniform_float(program, "fogEndDistance", scene->fog.end_distance);
        henka_set_uniform_float(program, "fogDensity", scene->fog.density);
        henka_set_uniform_bool(
            program,
            "useTexture",
            use_texture);
        henka_set_uniform_bool(
            program,
            "useVertexColor",
            !helper_entity &&
                entity->material.type == HENKA_MATERIAL_TYPE_VERTEX_COLOR &&
                policy.use_material_base_color);
        henka_set_uniform_bool(
            program,
            "useLighting",
            use_lighting);
        henka_set_uniform_int(
            program,
            "baseColorTexture",
            0);
        henka_set_uniform_int(program, "baseColorUvSet", entity->material.base_color_uv_set);
        henka_set_uniform_int(program, "normalTexture", 1);
        henka_set_uniform_int(program, "normalUvSet", entity->material.normal_uv_set);
        henka_set_uniform_int(program, "metallicRoughnessTexture", 2);
        henka_set_uniform_int(program, "metallicRoughnessUvSet", entity->material.metallic_roughness_uv_set);
        henka_set_uniform_int(program, "occlusionTexture", 3);
        henka_set_uniform_int(program, "occlusionUvSet", entity->material.occlusion_uv_set);
        henka_set_uniform_int(program, "emissiveTexture", 4);
        henka_set_uniform_int(program, "emissiveUvSet", entity->material.emissive_uv_set);
        henka_set_uniform_bool(program, "useNormalTexture",
            normal_texture_data != NULL && normal_texture_data->texture_id != 0U);
        henka_set_uniform_bool(program, "useMetallicRoughnessTexture",
            metallic_roughness_texture_data != NULL && metallic_roughness_texture_data->texture_id != 0U);
        henka_set_uniform_bool(program, "useOcclusionTexture",
            occlusion_texture_data != NULL && occlusion_texture_data->texture_id != 0U);
        henka_set_uniform_bool(program, "useEmissiveTexture",
            emissive_texture_data != NULL && emissive_texture_data->texture_id != 0U);
        henka_set_uniform_float(program, "metallic", entity->material.metallic);
        henka_set_uniform_float(program, "roughness", entity->material.roughness);
        henka_set_uniform_float(program, "specularFactor", entity->material.specular_factor);
        henka_set_uniform_vec3(program, "specularColor", entity->material.specular_color);
        henka_set_uniform_float(program, "ior", entity->material.ior);
        henka_set_uniform_float(program, "transmission", entity->material.transmission);
        henka_set_uniform_float(program, "thickness", entity->material.thickness);
        henka_set_uniform_float(program, "attenuationDistance", entity->material.attenuation_distance);
        henka_set_uniform_vec3(program, "attenuationColor", entity->material.attenuation_color);
        henka_set_uniform_float(program, "normalScale", entity->material.normal_scale);
        henka_set_uniform_float(program, "occlusionStrength", entity->material.occlusion_strength);
        henka_set_uniform_vec3(program, "emissiveColor", entity->material.emissive_color);
        henka_set_uniform_float(program, "emissiveStrength", entity->material.emissive_strength);
        henka_set_uniform_float(program, "clearcoat", entity->material.clearcoat);
        henka_set_uniform_float(program, "clearcoatRoughness", entity->material.clearcoat_roughness);
        henka_set_uniform_vec3(program, "sheenColor", entity->material.sheen_color);
        henka_set_uniform_float(program, "sheenRoughness", entity->material.sheen_roughness);
        henka_set_uniform_int(program, "alphaMode", (int)entity->material.alpha_mode);
        henka_set_uniform_float(program, "alphaCutoff", entity->material.alpha_cutoff);
        henka_set_uniform_bool(program, "doubleSided", entity->material.double_sided);
        henka_set_uniform_int(program, "shadowMap", 5);
        henka_set_uniform_bool(program, "useShadowMap",
            rendered && state->shadow_depth_texture != 0U && entity->material.receive_shadows);
        henka_set_uniform_int(program, "cascadeShadowMap", 12);
        henka_set_uniform_bool(program, "useCascadeShadowMap",
            rendered && state->cascade_shadow_depth_texture != 0U && entity->material.receive_shadows);
        henka_set_uniform_float(program, "cascadeSplitDistance", 18.0f);
        henka_set_uniform_int(program, "pointShadowMap", 13);
        henka_set_uniform_bool(program, "usePointShadowMap",
            rendered && point_shadow_light_index >= 0 &&
            state->point_shadow_depth_texture != 0U && entity->material.receive_shadows);
        {
            henka_vec3 point_shadow_position = {0.0f, 0.0f, 0.0f};
            float point_shadow_far_plane = 1.0f;
            if (point_shadow_light_index >= 0)
            {
                size_t point_index = 0U;
                for (; point_index < HENKA_SCENE_MAX_LOCAL_LIGHTS; ++point_index)
                {
                    const henka_scene_light_desc* light = &scene->local_lights[point_index];
                    if (scene->local_light_active[point_index] && light->enabled &&
                        light->type == HENKA_SCENE_LIGHT_POINT)
                    {
                        point_shadow_position = light->position;
                        point_shadow_far_plane = fmaxf(0.1f, fminf(light->range, 10000.0f));
                        break;
                    }
                }
            }
            henka_set_uniform_vec3(program, "pointShadowLightPosition", point_shadow_position);
            henka_set_uniform_float(program, "pointShadowFarPlane", point_shadow_far_plane);
        }
        henka_set_uniform_int(program, "localShadowMap", 11);
        henka_set_uniform_bool(program, "useLocalShadowMap",
            rendered && local_shadow_light_index >= 0 &&
            state->local_shadow_depth_texture != 0U && entity->material.receive_shadows);
#undef henka_set_uniform_mat4
#undef henka_set_uniform_vec4
#undef henka_set_uniform_vec3
#undef henka_set_uniform_bool
#undef henka_set_uniform_int
#undef henka_set_uniform_float

        if (entity->material.double_sided)
        {
            glDisable(GL_CULL_FACE);
        }
        else
        {
            glEnable(GL_CULL_FACE);
        }
        if (entity->material.alpha_mode == HENKA_MATERIAL_ALPHA_BLENDED)
        {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDepthMask(GL_FALSE);
        }
        else
        {
            glDisable(GL_BLEND);
            glDepthMask(GL_TRUE);
        }

        glBindTexture(
            GL_TEXTURE_2D,
            use_texture ?
                texture_data->texture_id :
                0U);
        g_gl.ActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D,
            normal_texture_data != NULL ? normal_texture_data->texture_id : 0U);
        g_gl.ActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D,
            metallic_roughness_texture_data != NULL ? metallic_roughness_texture_data->texture_id : 0U);
        g_gl.ActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D,
            occlusion_texture_data != NULL ? occlusion_texture_data->texture_id : 0U);
        g_gl.ActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_2D,
            emissive_texture_data != NULL ? emissive_texture_data->texture_id : 0U);
        g_gl.ActiveTexture(GL_TEXTURE5);
        glBindTexture(GL_TEXTURE_2D,
            rendered ? state->shadow_depth_texture : 0U);
        g_gl.ActiveTexture(GL_TEXTURE6);
        glBindTexture(
            GL_TEXTURE_2D,
            !helper_entity && scene->environment.hdr_texture != NULL &&
            scene->environment.hdr_texture->backend_data != NULL ?
            ((const henka_opengl_texture_data*)scene->environment.hdr_texture->backend_data)->texture_id : 0U);
        g_gl.ActiveTexture(GL_TEXTURE7);
        glBindTexture(GL_TEXTURE_CUBE_MAP,
            !helper_entity && state->ibl_ready ? state->ibl_irradiance_cube : 0U);
        g_gl.ActiveTexture(GL_TEXTURE8);
        glBindTexture(GL_TEXTURE_CUBE_MAP,
            !helper_entity && state->ibl_ready ? state->ibl_prefilter_cube : 0U);
        g_gl.ActiveTexture(GL_TEXTURE9);
        glBindTexture(GL_TEXTURE_2D,
            !helper_entity && state->ibl_ready ? state->ibl_brdf_lut : 0U);
        g_gl.ActiveTexture(GL_TEXTURE10);
        glBindTexture(GL_TEXTURE_CUBE_MAP,
            use_reflection_probe_map ? state->reflection_probe_cubes[reflection_probe_index] : 0U);
        g_gl.ActiveTexture(GL_TEXTURE11);
        glBindTexture(GL_TEXTURE_2D,
            rendered && local_shadow_light_index >= 0 ? state->local_shadow_depth_texture : 0U);
        g_gl.ActiveTexture(GL_TEXTURE12);
        glBindTexture(GL_TEXTURE_2D,
            rendered ? state->cascade_shadow_depth_texture : 0U);
        g_gl.ActiveTexture(GL_TEXTURE13);
        glBindTexture(GL_TEXTURE_CUBE_MAP,
            rendered && point_shadow_light_index >= 0 ? state->point_shadow_depth_texture : 0U);
        g_gl.ActiveTexture(GL_TEXTURE0);
        if (instance_count == 1U && pass == 0U && !helper_entity &&
            draw_index < HENKA_OPENGL_OCCLUSION_QUERY_CAPACITY &&
            state->occlusion_queries[draw_index] != 0U)
        {
            g_gl.BeginQuery(
                GL_ANY_SAMPLES_PASSED,
                state->occlusion_queries[draw_index]);
            occlusion_query_active = true;
        }
        if (instance_count > 1U)
        {
            g_gl.BindBuffer(GL_ARRAY_BUFFER, state->instance_buffer);
            g_gl.BufferData(
                GL_ARRAY_BUFFER,
                (GLsizeiptr)(instance_count * sizeof(state->instance_data[0])),
                state->instance_data,
                GL_STREAM_DRAW);
            henka_opengl_prepare_instance_attributes(state, mesh_data);
            henka_set_uniform_bool_owned(program, shader_data, "useInstancing", true);
            g_gl.DrawElementsInstanced(
                mesh_data->primitive_mode,
                mesh_data->index_count,
                GL_UNSIGNED_INT,
                0,
                (GLsizei)instance_count);
            henka_opengl_reset_instance_attributes(mesh_data);
            state->scene_instanced_draw_calls += 1U;
            state->scene_instanced_entities += (uint32_t)instance_count;
        }
        else
        {
            g_gl.BindVertexArray(mesh_data->vao);
            glDrawElements(
                mesh_data->primitive_mode,
                mesh_data->index_count,
                GL_UNSIGNED_INT,
                0);
        }
        if (occlusion_query_active)
        {
            g_gl.EndQuery(GL_ANY_SAMPLES_PASSED);
            state->occlusion_query_valid[draw_index] = true;
            state->occlusion_query_scene_revision[draw_index] = scene->render_revision;
        }
        state->scene_draw_calls += 1U;
        state->scene_visible_entities += (uint32_t)instance_count;
        if (pass == 0U && instance_count > 1U)
        {
            index += instance_count - 1U;
        }
        }
    }

    if (state->occlusion_queries[0] != 0U)
    {
        state->occlusion_query_view_projection = current_view_projection;
        state->occlusion_history_valid = true;
    }
    if (policy.use_hdr_presentation && !state->reflection_probe_capture_active)
    {
        if (policy.use_rendered_post_processing)
        {
            henka_opengl_draw_bloom(state);
        }
        g_gl.BindFramebuffer(GL_FRAMEBUFFER, 0U);
        henka_opengl_present_hdr(renderer, state, policy.use_rendered_post_processing);
    }
    if (gpu_query_active)
    {
        g_gl.EndQuery(GL_TIME_ELAPSED);
        state->scene_gpu_query_pending = true;
    }
    {
        Uint64 cpu_end_ticks = SDL_GetPerformanceCounter();
        Uint64 frequency = SDL_GetPerformanceFrequency();
        state->scene_cpu_time_milliseconds = frequency > 0U ?
            (double)(cpu_end_ticks - cpu_start_ticks) * 1000.0 / (double)frequency : 0.0;
    }
    g_gl.BindBuffer(GL_ARRAY_BUFFER, 0);
    g_gl.BindVertexArray(0);
    g_gl.ActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
    g_gl.ActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0);
    g_gl.ActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, 0);
    g_gl.ActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, 0);
    g_gl.ActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, 0);
    g_gl.ActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D, 0);
    g_gl.ActiveTexture(GL_TEXTURE6);
    glBindTexture(GL_TEXTURE_2D, 0);
    g_gl.ActiveTexture(GL_TEXTURE7);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    g_gl.ActiveTexture(GL_TEXTURE8);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    g_gl.ActiveTexture(GL_TEXTURE9);
    glBindTexture(GL_TEXTURE_2D, 0);
    g_gl.ActiveTexture(GL_TEXTURE10);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    g_gl.ActiveTexture(GL_TEXTURE11);
    glBindTexture(GL_TEXTURE_2D, 0);
    g_gl.ActiveTexture(GL_TEXTURE12);
    glBindTexture(GL_TEXTURE_2D, 0);
    g_gl.ActiveTexture(GL_TEXTURE13);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    g_gl.ActiveTexture(GL_TEXTURE0);
    g_gl.UseProgram(0);
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glDisable(GL_SCISSOR_TEST);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    if (!state->reflection_probe_capture_active)
    {
        for (index = 0U; index < scene->entity_capacity; ++index)
        {
            henka_scene_entity_record* entity = &scene->entities[index];
            if (entity->active)
            {
                entity->previous_transform = entity->transform;
                entity->previous_transform_valid = true;
            }
        }
        state->previous_view_projection = current_view_projection;
        state->previous_view_projection_valid = true;
    }
    return HENKA_SUCCESS;
}

void henka_opengl_renderer_get_scene_diagnostics(
    const struct henka_renderer* renderer,
    uint32_t* out_draw_calls,
    uint32_t* out_visible_entities,
    uint32_t* out_culled_entities,
    uint32_t* out_budget_dropped_entities,
    uint32_t* out_lod_entities,
    uint32_t* out_lod_fallback_entities,
    uint32_t* out_instanced_draw_calls,
    uint32_t* out_instanced_entities,
    uint32_t* out_occlusion_tested_entities,
    uint32_t* out_occlusion_culled_entities,
    uint32_t* out_transparent_sort_overflow_entities,
    double* out_cpu_time_milliseconds,
    double* out_gpu_time_milliseconds,
    bool* out_gpu_timing_available)
{
    const henka_opengl_renderer_state* state = renderer != NULL ?
        (const henka_opengl_renderer_state*)renderer->backend_state : NULL;

    if (out_draw_calls != NULL)
    {
        *out_draw_calls = state != NULL ? state->scene_draw_calls : 0U;
    }
    if (out_visible_entities != NULL)
    {
        *out_visible_entities = state != NULL ? state->scene_visible_entities : 0U;
    }
    if (out_culled_entities != NULL)
    {
        *out_culled_entities = state != NULL ? state->scene_culled_entities : 0U;
    }
    if (out_budget_dropped_entities != NULL)
    {
        *out_budget_dropped_entities = state != NULL ?
            state->scene_budget_dropped_entities : 0U;
    }
    if (out_lod_entities != NULL)
    {
        *out_lod_entities = state != NULL ? state->scene_lod_entities : 0U;
    }
    if (out_lod_fallback_entities != NULL)
    {
        *out_lod_fallback_entities = state != NULL ? state->scene_lod_fallback_entities : 0U;
    }
    if (out_instanced_draw_calls != NULL)
    {
        *out_instanced_draw_calls = state != NULL ? state->scene_instanced_draw_calls : 0U;
    }
    if (out_instanced_entities != NULL)
    {
        *out_instanced_entities = state != NULL ? state->scene_instanced_entities : 0U;
    }
    if (out_occlusion_tested_entities != NULL)
    {
        *out_occlusion_tested_entities = state != NULL ? state->scene_occlusion_tested_entities : 0U;
    }
    if (out_occlusion_culled_entities != NULL)
    {
        *out_occlusion_culled_entities = state != NULL ? state->scene_occlusion_culled_entities : 0U;
    }
    if (out_transparent_sort_overflow_entities != NULL)
    {
        *out_transparent_sort_overflow_entities = state != NULL ?
            state->transparent_sort_overflow_entities : 0U;
    }
    if (out_cpu_time_milliseconds != NULL)
    {
        *out_cpu_time_milliseconds = state != NULL ? state->scene_cpu_time_milliseconds : 0.0;
    }
    if (out_gpu_time_milliseconds != NULL)
    {
        *out_gpu_time_milliseconds = state != NULL ? state->scene_gpu_time_milliseconds : 0.0;
    }
    if (out_gpu_timing_available != NULL)
    {
        *out_gpu_timing_available = state != NULL && state->gpu_timing_available;
    }
}

void henka_opengl_renderer_get_memory_diagnostics(
    const struct henka_renderer* renderer,
    uint64_t* out_gpu_bytes,
    uint64_t* out_gpu_peak_bytes,
    uint64_t* out_mesh_bytes,
    uint64_t* out_texture_bytes,
    uint64_t* out_render_target_bytes,
    uint32_t* out_mesh_count,
    uint32_t* out_texture_count,
    bool* out_overflow)
{
    const henka_opengl_renderer_state* state = renderer != NULL ?
        (const henka_opengl_renderer_state*)renderer->backend_state : NULL;

    if (out_gpu_bytes != NULL)
    {
        *out_gpu_bytes = state != NULL ? state->tracked_gpu_bytes : 0U;
    }
    if (out_gpu_peak_bytes != NULL)
    {
        *out_gpu_peak_bytes = state != NULL ? state->tracked_gpu_peak_bytes : 0U;
    }
    if (out_mesh_bytes != NULL)
    {
        *out_mesh_bytes = state != NULL ? state->tracked_mesh_bytes : 0U;
    }
    if (out_texture_bytes != NULL)
    {
        *out_texture_bytes = state != NULL ? state->tracked_texture_bytes : 0U;
    }
    if (out_render_target_bytes != NULL)
    {
        *out_render_target_bytes = state != NULL ? state->tracked_render_target_bytes : 0U;
    }
    if (out_mesh_count != NULL)
    {
        *out_mesh_count = state != NULL ? state->tracked_mesh_count : 0U;
    }
    if (out_texture_count != NULL)
    {
        *out_texture_count = state != NULL ? state->tracked_texture_count : 0U;
    }
    if (out_overflow != NULL)
    {
        *out_overflow = state != NULL && state->memory_overflow;
    }
}

static henka_result henka_opengl_renderer_draw_ui_resources(
    const struct henka_ui_context* ui_context,
    GLuint ui_program,
    GLuint ui_vertex_array,
    GLuint ui_vertex_buffer,
    int framebuffer_width,
    int framebuffer_height)
{
    henka_ui_vertex* vertices;
    size_t index;
    int gl_vertex_count;
    size_t line_vertex_count;
    size_t rect_vertex_count;
    size_t vertex_bytes;
    size_t vertex_count;

    if (ui_context == NULL || ui_context->frame_active ||
        framebuffer_width <= 0 || framebuffer_height <= 0)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    if (ui_context->draw_rect_count == 0U && ui_context->draw_line_count == 0U)
    {
        return HENKA_SUCCESS;
    }

    glDisable(GL_SCISSOR_TEST);
    glViewport(0, 0, framebuffer_width, framebuffer_height);
    if (!henka_checked_size_multiply(ui_context->draw_rect_count, 6U, &rect_vertex_count) ||
        !henka_checked_size_multiply(ui_context->draw_line_count, 6U, &line_vertex_count) ||
        !henka_checked_size_add(rect_vertex_count, line_vertex_count, &vertex_count) ||
        vertex_count > HENKA_MAX_MESH_ELEMENTS ||
        !henka_checked_size_to_int(vertex_count, &gl_vertex_count) ||
        !henka_checked_size_multiply(vertex_count, sizeof(*vertices), &vertex_bytes) ||
        vertex_bytes > (size_t)PTRDIFF_MAX)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    vertices = henka_malloc(vertex_bytes);
    if (vertices == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }

    for (index = 0U; index < ui_context->draw_rect_count; ++index)
    {
        const henka_ui_draw_rect* draw_rect;
        henka_vec4 color;
        float x0;
        float x1;
        float y0;
        float y1;
        size_t base_index;

        draw_rect = &ui_context->draw_rects[index];
        x0 = draw_rect->bounds.x;
        y0 = draw_rect->bounds.y;
        x1 = draw_rect->bounds.x + draw_rect->bounds.width;
        y1 = draw_rect->bounds.y + draw_rect->bounds.height;
        color = draw_rect->color;
        base_index = index * 6U;

        vertices[base_index + 0U] = (henka_ui_vertex){x0, y0, color.x, color.y, color.z, color.w};
        vertices[base_index + 1U] = (henka_ui_vertex){x1, y0, color.x, color.y, color.z, color.w};
        vertices[base_index + 2U] = (henka_ui_vertex){x1, y1, color.x, color.y, color.z, color.w};
        vertices[base_index + 3U] = (henka_ui_vertex){x0, y0, color.x, color.y, color.z, color.w};
        vertices[base_index + 4U] = (henka_ui_vertex){x1, y1, color.x, color.y, color.z, color.w};
        vertices[base_index + 5U] = (henka_ui_vertex){x0, y1, color.x, color.y, color.z, color.w};
    }

    for (index = 0U; index < ui_context->draw_line_count; ++index)
    {
        const henka_ui_draw_line* draw_line;
        henka_vec4 color;
        double dx;
        double dy;
        double half_thickness;
        double inv_length;
        double line_length;
        double normal_x;
        double normal_y;
        double x0;
        double y0;
        double x1;
        double y1;
        double x2;
        double y2;
        double x3;
        double y3;
        size_t base_index;

        draw_line = &ui_context->draw_lines[index];
        dx = (double)draw_line->end.x - (double)draw_line->start.x;
        dy = (double)draw_line->end.y - (double)draw_line->start.y;
        half_thickness = (double)draw_line->thickness * 0.5;
        color = draw_line->color;
        line_length = hypot(dx, dy);

        if (!isfinite(line_length))
        {
            henka_free(vertices);
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        if (line_length <= 0.01)
        {
            dx = (double)draw_line->thickness;
            dy = 0.0;
            line_length = (double)draw_line->thickness;
        }

        inv_length = 1.0 / line_length;
        normal_x = -dy * inv_length * half_thickness;
        normal_y = dx * inv_length * half_thickness;

        x0 = (double)draw_line->start.x + normal_x;
        y0 = (double)draw_line->start.y + normal_y;
        x1 = (double)draw_line->end.x + normal_x;
        y1 = (double)draw_line->end.y + normal_y;
        x2 = (double)draw_line->end.x - normal_x;
        y2 = (double)draw_line->end.y - normal_y;
        x3 = (double)draw_line->start.x - normal_x;
        y3 = (double)draw_line->start.y - normal_y;

        if (!isfinite(x0) || !isfinite(y0) ||
            !isfinite(x1) || !isfinite(y1) ||
            !isfinite(x2) || !isfinite(y2) ||
            !isfinite(x3) || !isfinite(y3) ||
            x0 < -(double)FLT_MAX || x0 > (double)FLT_MAX ||
            y0 < -(double)FLT_MAX || y0 > (double)FLT_MAX ||
            x1 < -(double)FLT_MAX || x1 > (double)FLT_MAX ||
            y1 < -(double)FLT_MAX || y1 > (double)FLT_MAX ||
            x2 < -(double)FLT_MAX || x2 > (double)FLT_MAX ||
            y2 < -(double)FLT_MAX || y2 > (double)FLT_MAX ||
            x3 < -(double)FLT_MAX || x3 > (double)FLT_MAX ||
            y3 < -(double)FLT_MAX || y3 > (double)FLT_MAX)
        {
            henka_free(vertices);
            return HENKA_ERROR_INVALID_ARGUMENT;
        }

        base_index = rect_vertex_count + index * 6U;
        vertices[base_index + 0U] = (henka_ui_vertex){(float)x0, (float)y0, color.x, color.y, color.z, color.w};
        vertices[base_index + 1U] = (henka_ui_vertex){(float)x1, (float)y1, color.x, color.y, color.z, color.w};
        vertices[base_index + 2U] = (henka_ui_vertex){(float)x2, (float)y2, color.x, color.y, color.z, color.w};
        vertices[base_index + 3U] = (henka_ui_vertex){(float)x0, (float)y0, color.x, color.y, color.z, color.w};
        vertices[base_index + 4U] = (henka_ui_vertex){(float)x2, (float)y2, color.x, color.y, color.z, color.w};
        vertices[base_index + 5U] = (henka_ui_vertex){(float)x3, (float)y3, color.x, color.y, color.z, color.w};
    }

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    g_gl.UseProgram(ui_program);
    {
        GLint location;

        location = g_gl.GetUniformLocation(ui_program, "framebufferSize");
        if (location >= 0)
        {
            g_gl.Uniform2f(location, (GLfloat)framebuffer_width, (GLfloat)framebuffer_height);
        }
    }
    g_gl.BindVertexArray(ui_vertex_array);
    g_gl.BindBuffer(GL_ARRAY_BUFFER, ui_vertex_buffer);
    g_gl.BufferData(GL_ARRAY_BUFFER, (GLsizeiptr)vertex_bytes, vertices, GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)gl_vertex_count);
    g_gl.BindBuffer(GL_ARRAY_BUFFER, 0);
    g_gl.BindVertexArray(0);
    g_gl.UseProgram(0);
    glDisable(GL_BLEND);
    henka_free(vertices);
    return HENKA_SUCCESS;
}

henka_result henka_opengl_renderer_draw_ui(struct henka_renderer* renderer, const struct henka_ui_context* ui_context)
{
    const henka_opengl_renderer_state* state;

    if (renderer == NULL || renderer->backend_state == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    state = (const henka_opengl_renderer_state*)renderer->backend_state;
    return henka_opengl_renderer_draw_ui_resources(
        ui_context,
        state->ui_program,
        state->ui_vertex_array,
        state->ui_vertex_buffer,
        renderer->framebuffer_width,
        renderer->framebuffer_height);
}

henka_result henka_opengl_renderer_end_frame(
    struct henka_renderer* renderer)
{
    henka_opengl_renderer_state* state;

    if (renderer == NULL || renderer->backend_state == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    state =
        (henka_opengl_renderer_state*)renderer->backend_state;
    if (henka_opengl_restore_main_context(
            state,
            "main-window presentation") != HENKA_SUCCESS)
    {
        return HENKA_ERROR_RENDERER;
    }

    if (!SDL_GL_SwapWindow(state->window))
    {
        HENKA_LOG_ERROR(
            "SDL_GL_SwapWindow failed for the main window: %s",
            SDL_GetError());
        return HENKA_ERROR_RENDERER;
    }

    return HENKA_SUCCESS;
}

henka_result henka_opengl_renderer_create_tool_window_target(
    struct henka_renderer* renderer,
    henka_window_id window_id)
{
    henka_opengl_renderer_state* state;
    SDL_Window* window;
    size_t index;
    henka_result result;

    if (renderer == NULL ||
        renderer->backend_state == NULL ||
        window_id == HENKA_INVALID_WINDOW_ID)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    state =
        (henka_opengl_renderer_state*)renderer->backend_state;
    window = (SDL_Window*)henka_platform_get_native_tool_window(
        renderer->platform,
        window_id);
    if (window == NULL)
    {
        return HENKA_ERROR_PLATFORM;
    }

    for (index = 0U; index < HENKA_MAX_TOOL_WINDOWS; ++index)
    {
        henka_opengl_tool_window_target* target;

        if (state->tool_targets[index].id !=
            HENKA_INVALID_WINDOW_ID)
        {
            continue;
        }

        target = &state->tool_targets[index];
        target->id = window_id;
        target->window = window;
        target->gl_context = SDL_GL_CreateContext(window);
        if (target->gl_context == NULL)
        {
            HENKA_LOG_ERROR(
                "SDL_GL_CreateContext failed for a detached window: %s",
                SDL_GetError());
            memset(target, 0, sizeof(*target));
            return HENKA_ERROR_RENDERER;
        }

        if (!SDL_GL_MakeCurrent(window, target->gl_context))
        {
            HENKA_LOG_ERROR(
                "could not make a new detached OpenGL context current: %s",
                SDL_GetError());
            SDL_GL_DestroyContext(target->gl_context);
            memset(target, 0, sizeof(*target));
            result = henka_opengl_restore_main_context(
                state,
                "detached-window context creation failure");
            return result != HENKA_SUCCESS ?
                result : HENKA_ERROR_RENDERER;
        }

        result = henka_opengl_renderer_create_ui_resources(
            &target->ui_program,
            &target->ui_vertex_array,
            &target->ui_vertex_buffer);
        if (result != HENKA_SUCCESS)
        {
            henka_result restore_result;

            restore_result = henka_opengl_restore_main_context(
                state,
                "detached-window target resource creation failure");
            SDL_GL_DestroyContext(target->gl_context);
            memset(target, 0, sizeof(*target));
            return restore_result != HENKA_SUCCESS ?
                restore_result : result;
        }

        result = henka_opengl_restore_main_context(
            state,
            "detached-window target creation");
        if (result != HENKA_SUCCESS)
        {
            henka_result cleanup_restore_result;

            if (SDL_GL_MakeCurrent(window, target->gl_context))
            {
                if (target->ui_vertex_buffer != 0U)
                {
                    g_gl.DeleteBuffers(
                        1,
                        &target->ui_vertex_buffer);
                }
                if (target->ui_vertex_array != 0U)
                {
                    g_gl.DeleteVertexArrays(
                        1,
                        &target->ui_vertex_array);
                }
                if (target->ui_program != 0U)
                {
                    g_gl.DeleteProgram(target->ui_program);
                }
            }
            else
            {
                HENKA_LOG_ERROR(
                    "could not make the detached OpenGL context current for creation rollback: %s",
                    SDL_GetError());
            }

            cleanup_restore_result =
                henka_opengl_restore_main_context(
                    state,
                    "detached-window target creation rollback");
            SDL_GL_DestroyContext(target->gl_context);
            memset(target, 0, sizeof(*target));
            return cleanup_restore_result != HENKA_SUCCESS ?
                cleanup_restore_result : result;
        }

        return HENKA_SUCCESS;
    }

    return HENKA_ERROR_RENDERER;
}

void henka_opengl_renderer_destroy_tool_window_target(
    struct henka_renderer* renderer,
    henka_window_id window_id)
{
    henka_opengl_renderer_state* state;
    size_t index;

    if (renderer == NULL ||
        renderer->backend_state == NULL ||
        window_id == HENKA_INVALID_WINDOW_ID)
    {
        return;
    }

    state = (henka_opengl_renderer_state*)renderer->backend_state;
    for (index = 0U; index < HENKA_MAX_TOOL_WINDOWS; ++index)
    {
        henka_opengl_tool_window_target* target;

        if (state->tool_targets[index].id != window_id)
        {
            continue;
        }

        target = &state->tool_targets[index];
        if (SDL_GL_MakeCurrent(target->window, target->gl_context))
        {
            if (target->ui_vertex_buffer != 0U)
            {
                g_gl.DeleteBuffers(1, &target->ui_vertex_buffer);
            }
            if (target->ui_vertex_array != 0U)
            {
                g_gl.DeleteVertexArrays(1, &target->ui_vertex_array);
            }
            if (target->ui_program != 0U)
            {
                g_gl.DeleteProgram(target->ui_program);
            }
        }
        else
        {
            HENKA_LOG_WARN(
                "could not make tool window context current during destruction: %s",
                SDL_GetError());
        }

        SDL_GL_DestroyContext(target->gl_context);
        memset(target, 0, sizeof(*target));
        if (!SDL_GL_MakeCurrent(state->window, state->gl_context))
        {
            HENKA_LOG_WARN(
                "could not restore main OpenGL context after tool window destruction: %s",
                SDL_GetError());
        }
        return;
    }
}

henka_result henka_opengl_renderer_draw_tool_window_ui(
    struct henka_renderer* renderer,
    henka_window_id window_id,
    const struct henka_ui_context* ui_context)
{
    henka_opengl_renderer_state* state;
    henka_opengl_tool_window_target* target;
    henka_result draw_result;
    henka_result restore_result;
    henka_tool_window_state window_state;
    size_t index;

    if (renderer == NULL ||
        renderer->backend_state == NULL ||
        ui_context == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    if (!henka_platform_get_tool_window_state(
            renderer->platform,
            window_id,
            &window_state) ||
        !window_state.open)
    {
        return HENKA_ERROR_PLATFORM;
    }

    state =
        (henka_opengl_renderer_state*)renderer->backend_state;
    target = NULL;
    for (index = 0U; index < HENKA_MAX_TOOL_WINDOWS; ++index)
    {
        if (state->tool_targets[index].id == window_id)
        {
            target = &state->tool_targets[index];
            break;
        }
    }

    if (target == NULL)
    {
        return HENKA_ERROR_RENDERER;
    }

    if (!SDL_GL_MakeCurrent(
            target->window,
            target->gl_context))
    {
        HENKA_LOG_ERROR(
            "could not make the detached OpenGL context current: %s",
            SDL_GetError());
        return HENKA_ERROR_RENDERER;
    }

    glDisable(GL_SCISSOR_TEST);
    glViewport(
        0,
        0,
        window_state.width,
        window_state.height);
    glClearColor(0.06f, 0.08f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    draw_result = henka_opengl_renderer_draw_ui_resources(
        ui_context,
        target->ui_program,
        target->ui_vertex_array,
        target->ui_vertex_buffer,
        window_state.width,
        window_state.height);
    if (draw_result != HENKA_SUCCESS)
    {
        restore_result = henka_opengl_restore_main_context(
            state,
            "detached-window UI failure");
        return restore_result != HENKA_SUCCESS ?
            restore_result : draw_result;
    }

    if (!SDL_GL_SwapWindow(target->window))
    {
        HENKA_LOG_ERROR(
            "SDL_GL_SwapWindow failed for a detached window: %s",
            SDL_GetError());
        restore_result = henka_opengl_restore_main_context(
            state,
            "detached-window presentation failure");
        return restore_result != HENKA_SUCCESS ?
            restore_result : HENKA_ERROR_RENDERER;
    }

    return henka_opengl_restore_main_context(
        state,
        "detached-window presentation");
}

void henka_opengl_renderer_resize_viewport(struct henka_renderer* renderer, int width, int height)
{
    if (renderer == NULL || renderer->backend_state == NULL || width <= 0 || height <= 0)
    {
        return;
    }
    glDisable(GL_SCISSOR_TEST);
    glViewport(0, 0, width, height);
    henka_opengl_renderer_sync_scene_target(renderer);
}

void henka_opengl_renderer_sync_scene_target(struct henka_renderer* renderer)
{
    henka_opengl_renderer_state* state;
    henka_viewport viewport;

    if (renderer == NULL || renderer->backend_state == NULL)
    {
        return;
    }
    viewport = henka_renderer_get_scene_viewport(renderer);
    if (viewport.width <= 0 || viewport.height <= 0)
    {
        return;
    }
    state = (henka_opengl_renderer_state*)renderer->backend_state;
    if (state->hdr_width == viewport.width && state->hdr_height == viewport.height &&
        state->hdr_framebuffer != 0U && state->hdr_framebuffer_complete &&
        state->temporal_history_ready && state->temporal_history_width == viewport.width &&
        state->temporal_history_height == viewport.height)
    {
        return;
    }
    if (henka_opengl_create_hdr_target(state, viewport.width, viewport.height) != HENKA_SUCCESS)
    {
        HENKA_LOG_ERROR(
            "Scene View HDR target resize failed for %dx%d; previous target remains active (%s)",
            viewport.width,
            viewport.height,
            state->hdr_failure_reason[0] != '\0' ? state->hdr_failure_reason : "unknown reason");
        return;
    }
    if (henka_opengl_create_bloom_target(state, viewport.width, viewport.height) != HENKA_SUCCESS)
    {
        HENKA_LOG_ERROR(
            "Scene View bloom target resize failed for %dx%d; HDR presentation remains active without bloom (%s)",
            viewport.width,
            viewport.height,
            state->bloom_failure_reason[0] != '\0' ? state->bloom_failure_reason : "unknown reason");
    }
    if (henka_opengl_create_temporal_history(state, viewport.width, viewport.height) != HENKA_SUCCESS)
    {
        HENKA_LOG_ERROR(
            "Scene View temporal history resize failed for %dx%d; temporal accumulation disabled",
            viewport.width,
            viewport.height);
    }
}

void henka_opengl_renderer_get_hdr_diagnostics(
    const struct henka_renderer* renderer,
    int* out_requested_width,
    int* out_requested_height,
    int* out_allocated_width,
    int* out_allocated_height,
    uint64_t* out_generation,
    bool* out_complete,
    char* out_failure,
    size_t failure_capacity)
{
    const henka_opengl_renderer_state* state = NULL;

    if (renderer != NULL && renderer->backend_state != NULL)
    {
        state = (const henka_opengl_renderer_state*)renderer->backend_state;
    }
    if (out_requested_width != NULL) *out_requested_width = state != NULL ? state->hdr_requested_width : 0;
    if (out_requested_height != NULL) *out_requested_height = state != NULL ? state->hdr_requested_height : 0;
    if (out_allocated_width != NULL) *out_allocated_width = state != NULL ? state->hdr_width : 0;
    if (out_allocated_height != NULL) *out_allocated_height = state != NULL ? state->hdr_height : 0;
    if (out_generation != NULL) *out_generation = state != NULL ? state->hdr_generation : 0U;
    if (out_complete != NULL) *out_complete = state != NULL && state->hdr_framebuffer_complete;
    if (out_failure != NULL && failure_capacity > 0U)
    {
        (void)snprintf(
            out_failure,
            failure_capacity,
            "%s",
            state != NULL && state->hdr_failure_reason[0] != '\0' ? state->hdr_failure_reason : "");
    }
}

void henka_opengl_renderer_get_shadow_diagnostics(
    const struct henka_renderer* renderer,
    int* out_resolution,
    uint64_t* out_generation,
    bool* out_complete,
    char* out_failure,
    size_t failure_capacity)
{
    const henka_opengl_renderer_state* state = NULL;

    if (renderer != NULL && renderer->backend_state != NULL)
    {
        state = (const henka_opengl_renderer_state*)renderer->backend_state;
    }
    if (out_resolution != NULL) *out_resolution = state != NULL ? state->shadow_resolution : 0;
    if (out_generation != NULL) *out_generation = state != NULL ? state->shadow_generation : 0U;
    if (out_complete != NULL) *out_complete = state != NULL && state->shadow_framebuffer_complete;
    if (out_failure != NULL && failure_capacity > 0U)
    {
        (void)snprintf(
            out_failure,
            failure_capacity,
            "%s",
            state != NULL && state->shadow_failure_reason[0] != '\0' ? state->shadow_failure_reason : "");
    }
}

void henka_opengl_renderer_get_cascade_shadow_diagnostics(
    const struct henka_renderer* renderer,
    int* out_resolution,
    uint64_t* out_generation,
    bool* out_complete,
    char* out_failure,
    size_t failure_capacity)
{
    const henka_opengl_renderer_state* state = NULL;

    if (renderer != NULL && renderer->backend_state != NULL)
    {
        state = (const henka_opengl_renderer_state*)renderer->backend_state;
    }
    if (out_resolution != NULL) *out_resolution = state != NULL ? state->cascade_shadow_resolution : 0;
    if (out_generation != NULL) *out_generation = state != NULL ? state->cascade_shadow_generation : 0U;
    if (out_complete != NULL) *out_complete = state != NULL && state->cascade_shadow_framebuffer_complete;
    if (out_failure != NULL && failure_capacity > 0U)
    {
        (void)snprintf(
            out_failure,
            failure_capacity,
            "%s",
            state != NULL && state->cascade_shadow_failure_reason[0] != '\0' ?
                state->cascade_shadow_failure_reason : "");
    }
}

void henka_opengl_renderer_get_point_shadow_diagnostics(
    const struct henka_renderer* renderer,
    int* out_resolution,
    uint64_t* out_generation,
    bool* out_complete,
    char* out_failure,
    size_t failure_capacity)
{
    const henka_opengl_renderer_state* state = NULL;

    if (renderer != NULL && renderer->backend_state != NULL)
    {
        state = (const henka_opengl_renderer_state*)renderer->backend_state;
    }
    if (out_resolution != NULL) *out_resolution = state != NULL ? state->point_shadow_resolution : 0;
    if (out_generation != NULL) *out_generation = state != NULL ? state->point_shadow_generation : 0U;
    if (out_complete != NULL) *out_complete = state != NULL && state->point_shadow_framebuffer_complete;
    if (out_failure != NULL && failure_capacity > 0U)
    {
        (void)snprintf(
            out_failure,
            failure_capacity,
            "%s",
            state != NULL && state->point_shadow_failure_reason[0] != '\0' ?
                state->point_shadow_failure_reason : "");
    }
}

void henka_opengl_renderer_get_bloom_diagnostics(
    const struct henka_renderer* renderer,
    int* out_width,
    int* out_height,
    bool* out_ready,
    char* out_failure,
    size_t failure_capacity)
{
    const henka_opengl_renderer_state* state = renderer != NULL && renderer->backend_state != NULL ?
        (const henka_opengl_renderer_state*)renderer->backend_state : NULL;

    if (out_width != NULL) *out_width = state != NULL ? state->bloom_width : 0;
    if (out_height != NULL) *out_height = state != NULL ? state->bloom_height : 0;
    if (out_ready != NULL) *out_ready = state != NULL && state->bloom_ready;
    if (out_failure != NULL && failure_capacity > 0U)
    {
        (void)snprintf(
            out_failure,
            failure_capacity,
            "%s",
            state != NULL && state->bloom_failure_reason[0] != '\0' ? state->bloom_failure_reason : "");
    }
}

void henka_opengl_renderer_get_ibl_diagnostics(
    const struct henka_renderer* renderer,
    bool* out_ready,
    char* out_failure,
    size_t failure_capacity)
{
    const henka_opengl_renderer_state* state = renderer != NULL && renderer->backend_state != NULL ?
        (const henka_opengl_renderer_state*)renderer->backend_state : NULL;

    if (out_ready != NULL) *out_ready = state != NULL && state->ibl_ready;
    if (out_failure != NULL && failure_capacity > 0U)
    {
        (void)snprintf(
            out_failure,
            failure_capacity,
            "%s",
            state != NULL && state->ibl_failure_reason[0] != '\0' ? state->ibl_failure_reason : "");
    }
}

void henka_opengl_renderer_get_temporal_diagnostics(
    const struct henka_renderer* renderer,
    bool* out_history_ready,
    bool* out_history_valid,
    bool* out_fallback_active,
    uint32_t* out_invalidation_count,
    char* out_invalidation_reason,
    size_t invalidation_reason_capacity,
    bool* out_motion_vectors_ready,
    bool* out_jitter_enabled,
    float* out_jitter_x,
    float* out_jitter_y)
{
    const henka_opengl_renderer_state* state = renderer != NULL && renderer->backend_state != NULL ?
        (const henka_opengl_renderer_state*)renderer->backend_state : NULL;

    if (out_history_ready != NULL)
        *out_history_ready = state != NULL && state->temporal_history_ready;
    if (out_history_valid != NULL)
        *out_history_valid = state != NULL && state->temporal_history_valid;
    if (out_fallback_active != NULL)
        *out_fallback_active = state == NULL || state->temporal_fallback_active;
    if (out_invalidation_count != NULL)
        *out_invalidation_count = state != NULL ? state->temporal_invalidation_count : 0U;
    if (out_invalidation_reason != NULL && invalidation_reason_capacity > 0U)
    {
        (void)snprintf(
            out_invalidation_reason,
            invalidation_reason_capacity,
            "%s",
            state != NULL && state->temporal_invalidation_reason[0] != '\0' ?
                state->temporal_invalidation_reason : "renderer unavailable");
    }
    if (out_motion_vectors_ready != NULL)
        *out_motion_vectors_ready = state != NULL && state->hdr_motion_texture != 0U;
    if (out_jitter_enabled != NULL)
        *out_jitter_enabled = state != NULL && state->temporal_jitter_enabled;
    if (out_jitter_x != NULL)
        *out_jitter_x = state != NULL ? state->temporal_jitter_x : 0.0f;
    if (out_jitter_y != NULL)
        *out_jitter_y = state != NULL ? state->temporal_jitter_y : 0.0f;
}

void henka_opengl_renderer_get_reflection_probe_diagnostics(
    const struct henka_renderer* renderer,
    uint32_t* out_enabled_count,
    uint32_t* out_captured_count,
    bool* out_capture_active,
    uint32_t* out_capture_index,
    uint64_t* out_capture_generation,
    uint32_t* out_capture_failure_count)
{
    const henka_opengl_renderer_state* state = renderer != NULL && renderer->backend_state != NULL ?
        (const henka_opengl_renderer_state*)renderer->backend_state : NULL;

    if (out_enabled_count != NULL)
        *out_enabled_count = state != NULL ? state->reflection_probe_enabled_count : 0U;
    if (out_captured_count != NULL)
        *out_captured_count = state != NULL ? state->reflection_probe_captured_count : 0U;
    if (out_capture_active != NULL)
        *out_capture_active = state != NULL && state->reflection_probe_capture_active;
    if (out_capture_index != NULL)
        *out_capture_index = state != NULL ? state->reflection_probe_capture_index : UINT32_MAX;
    if (out_capture_generation != NULL)
        *out_capture_generation = state != NULL ? state->reflection_probe_capture_generation : 0U;
    if (out_capture_failure_count != NULL)
        *out_capture_failure_count = state != NULL ? state->reflection_probe_capture_failure_count : 0U;
}

henka_result henka_opengl_renderer_set_vsync(struct henka_renderer* renderer, bool enabled)
{
    return henka_platform_set_vsync(renderer->platform, enabled);
}

henka_result henka_opengl_renderer_set_wireframe(
    struct henka_renderer* renderer,
    bool enabled)
{
    (void)renderer;
    (void)enabled;
    return HENKA_SUCCESS;
}
henka_result henka_opengl_renderer_create_mesh_from_data(
    struct henka_renderer* renderer,
    const henka_vertex* vertices,
    int vertex_count,
    const unsigned int* indices,
    int index_count,
    henka_mesh_primitive primitive,
    struct henka_mesh** out_mesh)
{
    int index;
    size_t index_bytes;
    int vertex_index;
    henka_mesh* mesh;
    henka_opengl_mesh_data* mesh_data;
    henka_vec3* bitangents;
    henka_vec3* tangents;
    henka_vertex* upload_vertices;
    size_t vertex_bytes;

    if (renderer == NULL || vertices == NULL || indices == NULL || out_mesh == NULL ||
        vertex_count <= 0 || index_count <= 0 ||
        (primitive != HENKA_MESH_PRIMITIVE_TRIANGLES && primitive != HENKA_MESH_PRIMITIVE_LINES) ||
        (primitive == HENKA_MESH_PRIMITIVE_TRIANGLES && (index_count % 3) != 0) ||
        (primitive == HENKA_MESH_PRIMITIVE_LINES && (index_count % 2) != 0) ||
        (size_t)vertex_count > HENKA_MAX_MESH_ELEMENTS ||
        (size_t)index_count > HENKA_MAX_MESH_ELEMENTS ||
        !henka_checked_size_multiply(sizeof(*vertices), (size_t)vertex_count, &vertex_bytes) ||
        !henka_checked_size_multiply(sizeof(*indices), (size_t)index_count, &index_bytes) ||
        vertex_bytes > (size_t)PTRDIFF_MAX || index_bytes > (size_t)PTRDIFF_MAX)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    for (vertex_index = 0; vertex_index < vertex_count; ++vertex_index)
    {
        if (!isfinite(vertices[vertex_index].position.x) ||
            !isfinite(vertices[vertex_index].position.y) ||
            !isfinite(vertices[vertex_index].position.z) ||
            !isfinite(vertices[vertex_index].normal.x) ||
            !isfinite(vertices[vertex_index].normal.y) ||
            !isfinite(vertices[vertex_index].normal.z) ||
            !isfinite(vertices[vertex_index].uv.x) ||
            !isfinite(vertices[vertex_index].uv.y) ||
            !isfinite(vertices[vertex_index].uv1.x) ||
            !isfinite(vertices[vertex_index].uv1.y) ||
            (vertices[vertex_index].color_valid &&
                (!isfinite(vertices[vertex_index].color.x) ||
                 !isfinite(vertices[vertex_index].color.y) ||
                 !isfinite(vertices[vertex_index].color.z) ||
                 !isfinite(vertices[vertex_index].color.w) ||
                 vertices[vertex_index].color.x < 0.0f ||
                 vertices[vertex_index].color.x > 1.0f ||
                 vertices[vertex_index].color.y < 0.0f ||
                 vertices[vertex_index].color.y > 1.0f ||
                 vertices[vertex_index].color.z < 0.0f ||
                 vertices[vertex_index].color.z > 1.0f ||
                 vertices[vertex_index].color.w < 0.0f ||
                 vertices[vertex_index].color.w > 1.0f))
            || (vertices[vertex_index].tangent_valid &&
                (!isfinite(vertices[vertex_index].tangent.x) ||
                 !isfinite(vertices[vertex_index].tangent.y) ||
                 !isfinite(vertices[vertex_index].tangent.z) ||
                 !isfinite(vertices[vertex_index].tangent.w) ||
                 henka_vec3_length((henka_vec3){
                     vertices[vertex_index].tangent.x,
                     vertices[vertex_index].tangent.y,
                     vertices[vertex_index].tangent.z}) <= 0.000001f ||
                 fabsf(vertices[vertex_index].tangent.w) < 0.5f)))
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
    }

    for (index = 0; index < index_count; ++index)
    {
        if (indices[index] >= (unsigned int)vertex_count)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
    }

    *out_mesh = NULL;

    upload_vertices = henka_malloc(vertex_bytes);
    tangents = henka_calloc((size_t)vertex_count, sizeof(*tangents));
    bitangents = henka_calloc((size_t)vertex_count, sizeof(*bitangents));
    if (upload_vertices == NULL || tangents == NULL || bitangents == NULL)
    {
        henka_free(bitangents);
        henka_free(tangents);
        henka_free(upload_vertices);
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    memcpy(upload_vertices, vertices, vertex_bytes);
    for (vertex_index = 0; vertex_index < vertex_count; ++vertex_index)
    {
        if (!upload_vertices[vertex_index].color_valid)
        {
            upload_vertices[vertex_index].color =
                (henka_vec4){1.0f, 1.0f, 1.0f, 1.0f};
            upload_vertices[vertex_index].color_valid = true;
        }
        tangents[vertex_index] = (henka_vec3){0.0f, 0.0f, 0.0f};
    }
    if (primitive == HENKA_MESH_PRIMITIVE_TRIANGLES)
    {
        for (index = 0; index < index_count; index += 3)
        {
            unsigned int i0 = indices[index + 0U];
            unsigned int i1 = indices[index + 1U];
            unsigned int i2 = indices[index + 2U];
            henka_vec3 edge1 = henka_vec3_subtract(
                vertices[i1].position, vertices[i0].position);
            henka_vec3 edge2 = henka_vec3_subtract(
                vertices[i2].position, vertices[i0].position);
            float du1 = vertices[i1].uv.x - vertices[i0].uv.x;
            float dv1 = vertices[i1].uv.y - vertices[i0].uv.y;
            float du2 = vertices[i2].uv.x - vertices[i0].uv.x;
            float dv2 = vertices[i2].uv.y - vertices[i0].uv.y;
            float denominator = du1 * dv2 - du2 * dv1;

            if (isfinite(denominator) && fabsf(denominator) > 0.000001f)
            {
                float inverse = 1.0f / denominator;
                henka_vec3 tangent = henka_vec3_scale(
                    henka_vec3_subtract(
                        henka_vec3_scale(edge1, dv2),
                        henka_vec3_scale(edge2, dv1)),
                    inverse);
                henka_vec3 bitangent = henka_vec3_scale(
                    henka_vec3_subtract(
                        henka_vec3_scale(edge2, du1),
                        henka_vec3_scale(edge1, du2)),
                    inverse);
                tangents[i0] = henka_vec3_add(tangents[i0], tangent);
                tangents[i1] = henka_vec3_add(tangents[i1], tangent);
                tangents[i2] = henka_vec3_add(tangents[i2], tangent);
                bitangents[i0] = henka_vec3_add(bitangents[i0], bitangent);
                bitangents[i1] = henka_vec3_add(bitangents[i1], bitangent);
                bitangents[i2] = henka_vec3_add(bitangents[i2], bitangent);
            }
        }
    }
    for (vertex_index = 0; vertex_index < vertex_count; ++vertex_index)
    {
        henka_vec3 normal = henka_vec3_normalize(vertices[vertex_index].normal);
        henka_vec3 tangent = vertices[vertex_index].tangent_valid ?
            (henka_vec3){
                vertices[vertex_index].tangent.x,
                vertices[vertex_index].tangent.y,
                vertices[vertex_index].tangent.z} : tangents[vertex_index];
        float handedness = vertices[vertex_index].tangent_valid ?
            (vertices[vertex_index].tangent.w < 0.0f ? -1.0f : 1.0f) : 1.0f;

        tangent = henka_vec3_subtract(
            tangent,
            henka_vec3_scale(normal, henka_vec3_dot(normal, tangent)));
        if (!isfinite(henka_vec3_length(tangent)) || henka_vec3_length(tangent) <= 0.000001f)
        {
            if (vertices[vertex_index].tangent_valid)
            {
                henka_free(bitangents);
                henka_free(tangents);
                henka_free(upload_vertices);
                return HENKA_ERROR_INVALID_ARGUMENT;
            }
            henka_vec3 fallback_axis = fabsf(normal.y) < 0.9f ?
                (henka_vec3){0.0f, 1.0f, 0.0f} :
                (henka_vec3){1.0f, 0.0f, 0.0f};
            tangent = henka_vec3_cross(normal, fallback_axis);
        }
        tangent = henka_vec3_normalize(tangent);
        if (!vertices[vertex_index].tangent_valid)
        {
            handedness = henka_vec3_dot(
                henka_vec3_cross(normal, tangent),
                bitangents[vertex_index]) < 0.0f ? -1.0f : 1.0f;
        }
        upload_vertices[vertex_index].tangent =
            (henka_vec4){tangent.x, tangent.y, tangent.z, handedness};
    }

    mesh = henka_calloc(1U, sizeof(*mesh));
    mesh_data = henka_calloc(1U, sizeof(*mesh_data));
    if (mesh == NULL || mesh_data == NULL)
    {
        henka_free(mesh_data);
        henka_free(bitangents);
        henka_free(tangents);
        henka_free(upload_vertices);
        henka_free(mesh);
        return HENKA_ERROR_OUT_OF_MEMORY;
    }

    g_gl.GenVertexArrays(1, &mesh_data->vao);
    g_gl.GenBuffers(1, &mesh_data->vertex_buffer);
    g_gl.GenBuffers(1, &mesh_data->index_buffer);

    g_gl.BindVertexArray(mesh_data->vao);
    g_gl.BindBuffer(GL_ARRAY_BUFFER, mesh_data->vertex_buffer);
    g_gl.BufferData(GL_ARRAY_BUFFER, (GLsizeiptr)vertex_bytes, upload_vertices, GL_STATIC_DRAW);
    g_gl.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh_data->index_buffer);
    g_gl.BufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)index_bytes, indices, GL_STATIC_DRAW);

    g_gl.EnableVertexAttribArray(0);
    g_gl.VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(henka_vertex), (const void*)offsetof(henka_vertex, position));
    g_gl.EnableVertexAttribArray(1);
    g_gl.VertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(henka_vertex), (const void*)offsetof(henka_vertex, normal));
    g_gl.EnableVertexAttribArray(2);
    g_gl.VertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(henka_vertex), (const void*)offsetof(henka_vertex, uv));
    g_gl.EnableVertexAttribArray(3);
    g_gl.VertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(henka_vertex), (const void*)offsetof(henka_vertex, tangent));
    g_gl.EnableVertexAttribArray(4);
    g_gl.VertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(henka_vertex), (const void*)offsetof(henka_vertex, color));
    g_gl.EnableVertexAttribArray(5);
    g_gl.VertexAttribPointer(5, 2, GL_FLOAT, GL_FALSE, sizeof(henka_vertex), (const void*)offsetof(henka_vertex, uv1));
    g_gl.BindVertexArray(0);

    henka_free(bitangents);
    henka_free(tangents);
    henka_free(upload_vertices);

    mesh_data->primitive_mode = henka_mesh_primitive_to_gl(primitive);
    mesh_data->index_count = (GLsizei)index_count;

    mesh->renderer = renderer;
    mesh->primitive = primitive;
    mesh->vertex_count = vertex_count;
    mesh->index_count = index_count;
    mesh->backend_data = mesh_data;
    mesh_data->tracked_gpu_bytes = (uint64_t)vertex_bytes + (uint64_t)index_bytes;
    {
        henka_opengl_renderer_state* memory_state =
            (henka_opengl_renderer_state*)renderer->backend_state;
        if (memory_state != NULL)
        {
            henka_opengl_memory_add_category(
                memory_state,
                &memory_state->tracked_mesh_bytes,
                mesh_data->tracked_gpu_bytes);
            if (memory_state->tracked_mesh_count < UINT32_MAX)
            {
                ++memory_state->tracked_mesh_count;
            }
        }
    }

    *out_mesh = mesh;
    return HENKA_SUCCESS;
}

void henka_opengl_renderer_destroy_mesh(struct henka_mesh* mesh)
{
    henka_opengl_mesh_data* mesh_data;

    if (mesh == NULL || mesh->backend_data == NULL)
    {
        return;
    }

    mesh_data = (henka_opengl_mesh_data*)mesh->backend_data;
    if (mesh->renderer != NULL && mesh->renderer->backend_state != NULL)
    {
        henka_opengl_renderer_state* memory_state =
            (henka_opengl_renderer_state*)mesh->renderer->backend_state;
        henka_opengl_memory_remove_category(
            memory_state,
            &memory_state->tracked_mesh_bytes,
            mesh_data->tracked_gpu_bytes);
        if (memory_state->tracked_mesh_count > 0U)
        {
            --memory_state->tracked_mesh_count;
        }
    }
    g_gl.DeleteBuffers(1, &mesh_data->index_buffer);
    g_gl.DeleteBuffers(1, &mesh_data->vertex_buffer);
    g_gl.DeleteVertexArrays(1, &mesh_data->vao);
    henka_free(mesh_data);
    henka_free(mesh);
}

henka_result henka_opengl_renderer_create_shader_from_files(
    struct henka_renderer* renderer,
    const char* vertex_path,
    const char* fragment_path,
    struct henka_shader** out_shader)
{
    henka_shader_contract_desc contract =
        henka_shader_contract_desc_default(HENKA_SHADER_CONTRACT_MINIMAL_GEOMETRY);

    return henka_opengl_renderer_create_shader_from_files_with_contract(
        renderer,
        vertex_path,
        fragment_path,
        &contract,
        out_shader);
}

henka_result henka_opengl_renderer_create_shader_from_files_with_contract(
    struct henka_renderer* renderer,
    const char* vertex_path,
    const char* fragment_path,
    const henka_shader_contract_desc* contract,
    struct henka_shader** out_shader)
{
    char* fragment_source;
    char* vertex_source;
    GLuint fragment_shader;
    GLuint program;
    henka_shader* shader;
    henka_opengl_shader_data* shader_data;
    henka_opengl_shader_data location_data;
    uint64_t source_hash;
    GLuint vertex_shader;

    if (renderer == NULL || vertex_path == NULL || fragment_path == NULL ||
        contract == NULL || out_shader == NULL ||
        henka_shader_contract_desc_validate(contract) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    *out_shader = NULL;

    vertex_source = henka_read_text_file(vertex_path);
    fragment_source = henka_read_text_file(fragment_path);
    if (vertex_source == NULL || fragment_source == NULL)
    {
        HENKA_LOG_ERROR("failed to read shader files '%s' and '%s'", vertex_path, fragment_path);
        henka_free(vertex_source);
        henka_free(fragment_source);
        return HENKA_ERROR_RENDERER;
    }

    vertex_shader = g_gl.CreateShader(GL_VERTEX_SHADER);
    fragment_shader = g_gl.CreateShader(GL_FRAGMENT_SHADER);
    if (!henka_compile_shader(vertex_shader, vertex_source, vertex_path) ||
        !henka_compile_shader(fragment_shader, fragment_source, fragment_path))
    {
        g_gl.DeleteShader(vertex_shader);
        g_gl.DeleteShader(fragment_shader);
        henka_free(vertex_source);
        henka_free(fragment_source);
        return HENKA_ERROR_RENDERER;
    }

    program = g_gl.CreateProgram();
    g_gl.AttachShader(program, vertex_shader);
    g_gl.AttachShader(program, fragment_shader);
    if (!henka_link_program(program))
    {
        g_gl.DeleteProgram(program);
        g_gl.DeleteShader(vertex_shader);
        g_gl.DeleteShader(fragment_shader);
        henka_free(vertex_source);
        henka_free(fragment_source);
        return HENKA_ERROR_RENDERER;
    }

    if (!henka_validate_shader_contract(
            program,
            fragment_path,
            contract->type,
            contract->version,
            &location_data))
    {
        g_gl.DeleteProgram(program);
        g_gl.DeleteShader(vertex_shader);
        g_gl.DeleteShader(fragment_shader);
        henka_free(vertex_source);
        henka_free(fragment_source);
        return HENKA_ERROR_RENDERER;
    }
    henka_add_optional_shader_locations(program, &location_data);
    source_hash = henka_shader_source_hash(vertex_source, fragment_source);

    g_gl.DeleteShader(vertex_shader);
    g_gl.DeleteShader(fragment_shader);
    henka_free(vertex_source);
    henka_free(fragment_source);

    shader = henka_calloc(1U, sizeof(*shader));
    shader_data = henka_calloc(1U, sizeof(*shader_data));
    if (shader == NULL || shader_data == NULL)
    {
        g_gl.DeleteProgram(program);
        henka_free(shader_data);
        henka_free(shader);
        return HENKA_ERROR_OUT_OF_MEMORY;
    }

    *shader_data = location_data;
    shader_data->source_hash = source_hash;
    shader_data->generation = 1U;
    shader->renderer = renderer;
    shader->backend_data = shader_data;

    *out_shader = shader;
    return HENKA_SUCCESS;
}

void henka_opengl_renderer_destroy_shader(struct henka_shader* shader)
{
    henka_opengl_shader_data* shader_data;

    if (shader == NULL || shader->backend_data == NULL)
    {
        return;
    }

    shader_data = (henka_opengl_shader_data*)shader->backend_data;
    g_gl.DeleteProgram(shader_data->program);
    henka_free(shader_data);
    henka_free(shader);
}

typedef struct henka_opengl_texture_context_guard
{
    SDL_Window* previous_window;
    SDL_GLContext previous_context;
    bool restore_previous;
} henka_opengl_texture_context_guard;

static henka_result henka_opengl_begin_texture_context(
    henka_opengl_renderer_state* state,
    henka_opengl_texture_context_guard* guard,
    const char* operation)
{
    if (state == NULL ||
        state->window == NULL ||
        state->gl_context == NULL ||
        guard == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    memset(guard, 0, sizeof(*guard));
    guard->previous_window = SDL_GL_GetCurrentWindow();
    guard->previous_context = SDL_GL_GetCurrentContext();
    if ((guard->previous_window == NULL) !=
        (guard->previous_context == NULL))
    {
        HENKA_LOG_ERROR(
            "OpenGL reported an incomplete current context before %s",
            operation != NULL ? operation : "texture work");
        return HENKA_ERROR_RENDERER;
    }

    if (guard->previous_window == state->window &&
        guard->previous_context == state->gl_context)
    {
        return HENKA_SUCCESS;
    }

    guard->restore_previous =
        guard->previous_window != NULL &&
        guard->previous_context != NULL;
    if (!SDL_GL_MakeCurrent(
            state->window,
            state->gl_context))
    {
        HENKA_LOG_ERROR(
            "could not make the main OpenGL context current for %s: %s",
            operation != NULL ? operation : "texture work",
            SDL_GetError());
        memset(guard, 0, sizeof(*guard));
        return HENKA_ERROR_RENDERER;
    }

    return HENKA_SUCCESS;
}

static henka_result henka_opengl_end_texture_context(
    henka_opengl_renderer_state* state,
    const henka_opengl_texture_context_guard* guard,
    const char* operation)
{
    if (state == NULL || guard == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    if (!guard->restore_previous)
    {
        return HENKA_SUCCESS;
    }

    if (SDL_GL_MakeCurrent(
            guard->previous_window,
            guard->previous_context))
    {
        return HENKA_SUCCESS;
    }

    HENKA_LOG_ERROR(
        "could not restore the previous OpenGL context after %s: %s",
        operation != NULL ? operation : "texture work",
        SDL_GetError());
    if (!SDL_GL_MakeCurrent(
            state->window,
            state->gl_context))
    {
        HENKA_LOG_ERROR(
            "could not recover the main OpenGL context after texture context restoration failed: %s",
            SDL_GetError());
    }
    return HENKA_ERROR_RENDERER;
}

static void henka_opengl_discard_prior_texture_errors(
    const char* operation)
{
    GLenum error;

    while ((error = glGetError()) != GL_NO_ERROR)
    {
        HENKA_LOG_WARN(
            "discarding pre-existing OpenGL error 0x%04x before %s",
            (unsigned int)error,
            operation != NULL ? operation : "texture work");
    }
}

static henka_result henka_opengl_collect_texture_errors(
    const char* operation)
{
    GLenum error;
    henka_result result;

    result = HENKA_SUCCESS;
    while ((error = glGetError()) != GL_NO_ERROR)
    {
        HENKA_LOG_ERROR(
            "OpenGL texture operation '%s' failed with error 0x%04x",
            operation != NULL ? operation : "unknown",
            (unsigned int)error);
        if (error == GL_OUT_OF_MEMORY)
        {
            result = HENKA_ERROR_OUT_OF_MEMORY;
        }
        else if (result == HENKA_SUCCESS)
        {
            result = HENKA_ERROR_RENDERER;
        }
    }

    return result;
}

static henka_result henka_opengl_restore_texture_binding(
    GLint previous_active_texture,
    GLint previous_texture_binding,
    GLint previous_unpack_alignment)
{
    g_gl.ActiveTexture(GL_TEXTURE0);
    glBindTexture(
        GL_TEXTURE_2D,
        (GLuint)previous_texture_binding);
    glPixelStorei(GL_UNPACK_ALIGNMENT, previous_unpack_alignment);
    g_gl.ActiveTexture((GLenum)previous_active_texture);
    return henka_opengl_collect_texture_errors(
        "texture binding restoration");
}

static henka_result henka_opengl_create_texture_from_pixels(
    struct henka_renderer* renderer,
    int width,
    int height,
    const void* pixels,
    size_t decoded_bytes,
    GLint internal_format,
    GLenum pixel_type,
    henka_texture_source_class source_class,
    const henka_texture_descriptor* descriptor,
    struct henka_texture** out_texture);

henka_result henka_opengl_renderer_create_texture_from_rgba8(
    struct henka_renderer* renderer,
    int width,
    int height,
    const unsigned char* pixels,
    struct henka_texture** out_texture)
{
    henka_texture_descriptor descriptor = henka_texture_descriptor_default_color();

    return henka_opengl_renderer_create_texture_from_rgba8_with_descriptor(
        renderer,
        width,
        height,
        pixels,
        &descriptor,
        out_texture);
}

henka_result henka_opengl_renderer_create_texture_from_rgba8_with_descriptor(
    struct henka_renderer* renderer,
    int width,
    int height,
    const unsigned char* pixels,
    const henka_texture_descriptor* descriptor,
    struct henka_texture** out_texture)
{
    size_t decoded_bytes;

    if (!henka_checked_rgba8_size(width, height, &decoded_bytes))
    {
        if (out_texture != NULL)
        {
            *out_texture = NULL;
        }
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return henka_opengl_create_texture_from_pixels(
        renderer,
        width,
        height,
        pixels,
        decoded_bytes,
        descriptor != NULL &&
            descriptor->color_space == HENKA_TEXTURE_COLOR_SPACE_SRGB ?
            GL_SRGB8_ALPHA8 : GL_RGBA8,
        GL_UNSIGNED_BYTE,
        HENKA_TEXTURE_SOURCE_CLASS_LDR_8_BIT,
        descriptor,
        out_texture);
}

henka_result henka_opengl_renderer_create_texture_from_rgba32f_with_descriptor(
    struct henka_renderer* renderer,
    int width,
    int height,
    const float* pixels,
    const henka_texture_descriptor* descriptor,
    struct henka_texture** out_texture)
{
    uint64_t pixel_count;
    uint64_t decoded_bytes;
    uint64_t logical_bytes;

    if (out_texture != NULL)
    {
        *out_texture = NULL;
    }
    if (width <= 0 || height <= 0 ||
        (uint64_t)width > UINT64_MAX / (uint64_t)height)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    pixel_count = (uint64_t)width * (uint64_t)height;
    if (pixel_count > UINT64_MAX / 4U ||
        pixel_count * 4U > UINT64_MAX / sizeof(float))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    decoded_bytes = pixel_count * 4U * sizeof(float);
    if (decoded_bytes > (uint64_t)SIZE_MAX)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (pixel_count > UINT64_MAX / 4U ||
        pixel_count * 4U > UINT64_MAX / 2U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    logical_bytes = pixel_count * 4U * 2U;
    return henka_opengl_create_texture_from_pixels(
        renderer,
        width,
        height,
        pixels,
        (size_t)logical_bytes,
        GL_RGBA16F,
        GL_FLOAT,
        HENKA_TEXTURE_SOURCE_CLASS_HDR,
        descriptor,
        out_texture);
}

static henka_result henka_opengl_create_texture_from_pixels(
    struct henka_renderer* renderer,
    int width,
    int height,
    const void* pixels,
    size_t decoded_bytes,
    GLint internal_format,
    GLenum pixel_type,
    henka_texture_source_class source_class,
    const henka_texture_descriptor* descriptor,
    struct henka_texture** out_texture)
{
    henka_opengl_texture_context_guard context_guard;
    henka_result context_result;
    henka_opengl_renderer_state* state;
    uint64_t logical_texture_bytes;
    henka_result operation_result;
    GLint previous_active_texture;
    GLint previous_texture_binding;
    GLint previous_unpack_alignment;
    henka_result restore_result;
    henka_texture* texture;
    henka_opengl_texture_data* texture_data;

    if (out_texture != NULL)
    {
        *out_texture = NULL;
    }

    if (renderer == NULL ||
        renderer->backend_state == NULL ||
        pixels == NULL ||
        out_texture == NULL || descriptor == NULL ||
        decoded_bytes == 0U ||
        internal_format == 0 ||
        (pixel_type != GL_UNSIGNED_BYTE && pixel_type != GL_FLOAT) ||
        source_class == HENKA_TEXTURE_SOURCE_CLASS_UNKNOWN ||
        henka_texture_descriptor_validate(descriptor) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (!henka_opengl_calculate_texture_bytes(
            width,
            height,
            decoded_bytes,
            descriptor->generate_mipmaps,
            &logical_texture_bytes))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    state = (henka_opengl_renderer_state*)renderer->backend_state;
    operation_result = henka_opengl_begin_texture_context(
        state,
        &context_guard,
        "texture creation");
    if (operation_result != HENKA_SUCCESS)
    {
        return operation_result;
    }

    henka_opengl_discard_prior_texture_errors(
        "texture creation");
    previous_active_texture = GL_TEXTURE0;
    previous_texture_binding = 0;
    previous_unpack_alignment = 4;
    glGetIntegerv(
        GL_ACTIVE_TEXTURE,
        &previous_active_texture);
    g_gl.ActiveTexture(GL_TEXTURE0);
    glGetIntegerv(
        GL_TEXTURE_BINDING_2D,
        &previous_texture_binding);
    glGetIntegerv(
        GL_UNPACK_ALIGNMENT,
        &previous_unpack_alignment);
    operation_result = henka_opengl_collect_texture_errors(
        "texture state capture");
    if (operation_result != HENKA_SUCCESS)
    {
        (void)henka_opengl_restore_texture_binding(
            previous_active_texture,
            previous_texture_binding,
            previous_unpack_alignment);
        context_result = henka_opengl_end_texture_context(
            state,
            &context_guard,
            "failed texture state capture");
        return context_result != HENKA_SUCCESS ?
            context_result : operation_result;
    }

    texture = henka_calloc(1U, sizeof(*texture));
    texture_data = henka_calloc(1U, sizeof(*texture_data));
    if (texture == NULL || texture_data == NULL)
    {
        henka_free(texture_data);
        henka_free(texture);
        (void)henka_opengl_restore_texture_binding(
            previous_active_texture,
            previous_texture_binding,
            previous_unpack_alignment);
        context_result = henka_opengl_end_texture_context(
            state,
            &context_guard,
            "failed texture allocation");
        return context_result != HENKA_SUCCESS ?
            context_result : HENKA_ERROR_OUT_OF_MEMORY;
    }

    glGenTextures(1, &texture_data->texture_id);
    if (texture_data->texture_id != 0U)
    {
        glBindTexture(
            GL_TEXTURE_2D,
            texture_data->texture_id);
        glTexParameteri(
            GL_TEXTURE_2D,
            GL_TEXTURE_MIN_FILTER,
            descriptor->min_filter == HENKA_TEXTURE_FILTER_NEAREST ?
                GL_NEAREST :
                descriptor->min_filter == HENKA_TEXTURE_FILTER_LINEAR ?
                GL_LINEAR : GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(
            GL_TEXTURE_2D,
            GL_TEXTURE_MAG_FILTER,
            descriptor->mag_filter == HENKA_TEXTURE_FILTER_NEAREST ?
                GL_NEAREST : GL_LINEAR);
        glTexParameteri(
            GL_TEXTURE_2D,
            GL_TEXTURE_WRAP_S,
            descriptor->wrap_u == HENKA_TEXTURE_WRAP_CLAMP_TO_EDGE ?
                GL_CLAMP_TO_EDGE :
                descriptor->wrap_u == HENKA_TEXTURE_WRAP_MIRRORED_REPEAT ?
                GL_MIRRORED_REPEAT : GL_REPEAT);
        glTexParameteri(
            GL_TEXTURE_2D,
            GL_TEXTURE_WRAP_T,
            descriptor->wrap_v == HENKA_TEXTURE_WRAP_CLAMP_TO_EDGE ?
                GL_CLAMP_TO_EDGE :
                descriptor->wrap_v == HENKA_TEXTURE_WRAP_MIRRORED_REPEAT ?
                GL_MIRRORED_REPEAT : GL_REPEAT);
        if (descriptor->anisotropy > 0.0f &&
            glGetString(GL_EXTENSIONS) != NULL &&
            strstr((const char*)glGetString(GL_EXTENSIONS),
                "GL_EXT_texture_filter_anisotropic") != NULL)
        {
            GLfloat maximum_anisotropy = 1.0f;
            glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maximum_anisotropy);
            if (isfinite(maximum_anisotropy) && maximum_anisotropy >= 1.0f)
            {
                GLfloat requested_anisotropy = descriptor->anisotropy < maximum_anisotropy ?
                    descriptor->anisotropy : maximum_anisotropy;
                if (requested_anisotropy >= 1.0f)
                {
                    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, requested_anisotropy);
                }
            }
        }
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            internal_format,
            width,
            height,
            0,
            GL_RGBA,
            pixel_type,
            pixels);
        if (descriptor->generate_mipmaps)
        {
            g_gl.GenerateMipmap(GL_TEXTURE_2D);
        }
    }

    operation_result = henka_opengl_collect_texture_errors(
        "texture allocation and upload");
    if (texture_data->texture_id == 0U &&
        operation_result == HENKA_SUCCESS)
    {
        HENKA_LOG_ERROR(
            "OpenGL returned texture identifier zero");
        operation_result = HENKA_ERROR_RENDERER;
    }

    if (operation_result != HENKA_SUCCESS &&
        texture_data->texture_id != 0U)
    {
        glBindTexture(GL_TEXTURE_2D, 0U);
        glDeleteTextures(
            1,
            &texture_data->texture_id);
        texture_data->texture_id = 0U;
        (void)henka_opengl_collect_texture_errors(
            "failed texture cleanup");
    }

    restore_result = henka_opengl_restore_texture_binding(
        previous_active_texture,
        previous_texture_binding,
        previous_unpack_alignment);
    if (operation_result == HENKA_SUCCESS &&
        restore_result != HENKA_SUCCESS)
    {
        operation_result = restore_result;
        g_gl.ActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, 0U);
        if (texture_data->texture_id != 0U)
        {
            glDeleteTextures(
                1,
                &texture_data->texture_id);
            texture_data->texture_id = 0U;
        }
        g_gl.ActiveTexture(
            (GLenum)previous_active_texture);
    }

    context_result = henka_opengl_end_texture_context(
        state,
        &context_guard,
        "texture creation");
    if (operation_result == HENKA_SUCCESS &&
        context_result != HENKA_SUCCESS)
    {
        operation_result = context_result;
        if (SDL_GL_MakeCurrent(
                state->window,
                state->gl_context))
        {
            if (texture_data->texture_id != 0U)
            {
                glDeleteTextures(
                    1,
                    &texture_data->texture_id);
                texture_data->texture_id = 0U;
            }
        }
    }

    if (operation_result != HENKA_SUCCESS)
    {
        henka_free(texture_data);
        henka_free(texture);
        return operation_result;
    }

    texture->renderer = renderer;
    texture->backend_data = texture_data;
    texture->owns_backend = true;
    texture->width = width;
    texture->height = height;
    texture->descriptor = *descriptor;
    texture->source_class = source_class;
    texture->alpha_mode = HENKA_TEXTURE_ALPHA_OPAQUE;
    texture->last_failure = HENKA_TEXTURE_FAILURE_NONE;
    texture->content_revision = 1U;
    texture->gpu_compressed = false;
    texture->gpu_format = HENKA_TEXTURE_GPU_FORMAT_RGBA8;
    texture->resident_gpu_bytes = logical_texture_bytes;
    texture->resident_mip_count = 1U;
    texture->mip_count = 1U;
    if (descriptor->generate_mipmaps)
    {
        int mip_width = width;
        int mip_height = height;
        while (mip_width > 1 || mip_height > 1)
        {
            mip_width = mip_width > 1 ? mip_width / 2 : 1;
            mip_height = mip_height > 1 ? mip_height / 2 : 1;
            if (texture->mip_count < UINT32_MAX)
                ++texture->mip_count;
        }
        texture->resident_mip_count = texture->mip_count;
    }
    texture_data->tracked_gpu_bytes = logical_texture_bytes;
    {
        henka_opengl_renderer_state* memory_state =
            (henka_opengl_renderer_state*)renderer->backend_state;
        if (memory_state != NULL)
        {
            henka_opengl_memory_add_category(
                memory_state,
                &memory_state->tracked_texture_bytes,
                texture_data->tracked_gpu_bytes);
            if (memory_state->tracked_texture_count < UINT32_MAX)
            {
                ++memory_state->tracked_texture_count;
            }
        }
    }

    *out_texture = texture;
    return HENKA_SUCCESS;
}

henka_result henka_opengl_renderer_create_texture_from_ktx2_memory_with_mip_limit(
    struct henka_renderer* renderer,
    const unsigned char* data,
    size_t data_size,
    const henka_texture_descriptor* descriptor,
    uint32_t max_resident_mips,
    struct henka_texture** out_texture)
{
#if defined(HENKA_WITH_KTX2_TRANSCODER)
    henka_opengl_texture_context_guard context_guard;
    henka_ktx2_upload upload;
    henka_opengl_renderer_state* state;
    henka_opengl_texture_data* texture_data = NULL;
    henka_texture* texture = NULL;
    henka_result operation_result;
    henka_result context_result;
    henka_result restore_result;
    GLint previous_active_texture = GL_TEXTURE0;
    GLint previous_texture_binding = 0;
    GLint previous_unpack_alignment = 4;
    uint32_t level;
    GLenum internal_format;
    uint64_t logical_texture_bytes;

    memset(&upload, 0, sizeof(upload));
    if (out_texture != NULL)
        *out_texture = NULL;
    if (renderer == NULL || renderer->backend_state == NULL || data == NULL ||
        data_size == 0U || descriptor == NULL || out_texture == NULL ||
        henka_texture_descriptor_validate(descriptor) != HENKA_SUCCESS)
        return HENKA_ERROR_INVALID_ARGUMENT;

    state = (henka_opengl_renderer_state*)renderer->backend_state;
    operation_result = henka_opengl_begin_texture_context(
        state, &context_guard, "KTX2 texture creation");
    if (operation_result != HENKA_SUCCESS)
        return operation_result;
    operation_result = henka_ktx2_prepare_upload_with_mip_limit(
        data,
        data_size,
        descriptor->usage,
        descriptor->color_space,
        henka_opengl_ktx2_capabilities(),
        max_resident_mips,
        &upload);
    if (operation_result != HENKA_SUCCESS)
    {
        context_result = henka_opengl_end_texture_context(
            state, &context_guard, "failed KTX2 preparation");
        return context_result != HENKA_SUCCESS ? context_result : operation_result;
    }
    logical_texture_bytes = (uint64_t)upload.data_size;
    internal_format = upload.compressed ?
        henka_opengl_ktx2_internal_format(upload.format, upload.is_srgb) :
        (upload.is_srgb ? GL_SRGB8_ALPHA8 : GL_RGBA8);
    if (internal_format == 0U)
    {
        henka_ktx2_upload_dispose(&upload);
        context_result = henka_opengl_end_texture_context(
            state, &context_guard, "unsupported KTX2 format");
        return context_result != HENKA_SUCCESS ? context_result : HENKA_ERROR_ASSET_SOURCE;
    }

    henka_opengl_discard_prior_texture_errors("KTX2 texture creation");
    glGetIntegerv(GL_ACTIVE_TEXTURE, &previous_active_texture);
    g_gl.ActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous_texture_binding);
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &previous_unpack_alignment);
    operation_result = henka_opengl_collect_texture_errors("KTX2 texture state capture");
    if (operation_result == HENKA_SUCCESS)
    {
        texture = henka_calloc(1U, sizeof(*texture));
        texture_data = henka_calloc(1U, sizeof(*texture_data));
        if (texture == NULL || texture_data == NULL)
            operation_result = HENKA_ERROR_OUT_OF_MEMORY;
    }
    if (operation_result == HENKA_SUCCESS)
    {
        glGenTextures(1, &texture_data->texture_id);
        if (texture_data->texture_id == 0U)
            operation_result = HENKA_ERROR_RENDERER;
    }
    if (operation_result == HENKA_SUCCESS)
    {
        glBindTexture(GL_TEXTURE_2D, texture_data->texture_id);
        glTexParameteri(
            GL_TEXTURE_2D,
            GL_TEXTURE_MIN_FILTER,
            upload.level_count <= 1U ?
                (descriptor->min_filter == HENKA_TEXTURE_FILTER_NEAREST ? GL_NEAREST : GL_LINEAR) :
                (descriptor->min_filter == HENKA_TEXTURE_FILTER_NEAREST ? GL_NEAREST :
                    descriptor->min_filter == HENKA_TEXTURE_FILTER_LINEAR ? GL_LINEAR :
                    GL_LINEAR_MIPMAP_LINEAR));
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, (GLint)upload.level_count - 1);
        glTexParameteri(
            GL_TEXTURE_2D,
            GL_TEXTURE_MAG_FILTER,
            descriptor->mag_filter == HENKA_TEXTURE_FILTER_NEAREST ? GL_NEAREST : GL_LINEAR);
        glTexParameteri(
            GL_TEXTURE_2D,
            GL_TEXTURE_WRAP_S,
            descriptor->wrap_u == HENKA_TEXTURE_WRAP_CLAMP_TO_EDGE ? GL_CLAMP_TO_EDGE :
                descriptor->wrap_u == HENKA_TEXTURE_WRAP_MIRRORED_REPEAT ? GL_MIRRORED_REPEAT : GL_REPEAT);
        glTexParameteri(
            GL_TEXTURE_2D,
            GL_TEXTURE_WRAP_T,
            descriptor->wrap_v == HENKA_TEXTURE_WRAP_CLAMP_TO_EDGE ? GL_CLAMP_TO_EDGE :
                descriptor->wrap_v == HENKA_TEXTURE_WRAP_MIRRORED_REPEAT ? GL_MIRRORED_REPEAT : GL_REPEAT);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        for (level = 0U; level < upload.level_count; ++level)
        {
            const henka_ktx2_upload_level* upload_level = &upload.levels[level];
            const unsigned char* level_data = upload.data + upload_level->offset;
            if (upload.compressed)
            {
                g_gl.CompressedTexImage2D(
                    GL_TEXTURE_2D,
                    (GLint)level,
                    internal_format,
                    upload_level->width,
                    upload_level->height,
                    0,
                    (GLsizei)upload_level->size,
                    level_data);
            }
            else
            {
                glTexImage2D(
                    GL_TEXTURE_2D,
                    (GLint)level,
                    (GLint)internal_format,
                    upload_level->width,
                    upload_level->height,
                    0,
                    GL_RGBA,
                    GL_UNSIGNED_BYTE,
                    level_data);
            }
        }
        operation_result = henka_opengl_collect_texture_errors(
            "KTX2 texture upload");
    }
    restore_result = henka_opengl_restore_texture_binding(
        previous_active_texture, previous_texture_binding, previous_unpack_alignment);
    if (operation_result == HENKA_SUCCESS && restore_result != HENKA_SUCCESS)
        operation_result = restore_result;
    if (operation_result != HENKA_SUCCESS && texture_data != NULL && texture_data->texture_id != 0U)
    {
        g_gl.ActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, 0U);
        glDeleteTextures(1, &texture_data->texture_id);
        texture_data->texture_id = 0U;
        (void)henka_opengl_collect_texture_errors("failed KTX2 texture cleanup");
    }
    context_result = henka_opengl_end_texture_context(
        state, &context_guard, "KTX2 texture creation");
    if (operation_result == HENKA_SUCCESS && context_result != HENKA_SUCCESS)
    {
        operation_result = context_result;
        if (SDL_GL_MakeCurrent(state->window, state->gl_context) &&
            texture_data != NULL && texture_data->texture_id != 0U)
        {
            glDeleteTextures(1, &texture_data->texture_id);
            texture_data->texture_id = 0U;
        }
    }
    if (operation_result != HENKA_SUCCESS)
    {
        henka_free(texture_data);
        henka_free(texture);
        henka_ktx2_upload_dispose(&upload);
        return operation_result;
    }

    texture->renderer = renderer;
    texture->backend_data = texture_data;
    texture->owns_backend = true;
    texture->width = upload.width;
    texture->height = upload.height;
    texture->descriptor = *descriptor;
    texture->source_class = HENKA_TEXTURE_SOURCE_CLASS_LDR_8_BIT;
    texture->alpha_mode = HENKA_TEXTURE_ALPHA_OPAQUE;
    texture->last_failure = HENKA_TEXTURE_FAILURE_NONE;
    texture->content_revision = 1U;
    texture->source_byte_size = data_size;
    texture->original_channel_count = 4;
    texture->gpu_compressed = upload.compressed;
    texture->gpu_format = (henka_texture_gpu_format)upload.format + 1;
    texture->resident_gpu_bytes = logical_texture_bytes;
    texture->resident_mip_count = upload.level_count;
    texture->mip_count = upload.total_level_count;
    texture_data->tracked_gpu_bytes = logical_texture_bytes;
    henka_opengl_memory_add_category(
        state, &state->tracked_texture_bytes, texture_data->tracked_gpu_bytes);
    if (state->tracked_texture_count < UINT32_MAX)
        ++state->tracked_texture_count;
    *out_texture = texture;
    henka_ktx2_upload_dispose(&upload);
    return HENKA_SUCCESS;
#else
    (void)renderer;
    (void)data;
    (void)data_size;
    (void)descriptor;
    if (out_texture != NULL)
        *out_texture = NULL;
    return HENKA_ERROR_ASSET_SOURCE;
#endif
}

henka_result henka_opengl_renderer_create_texture_from_ktx2_memory(
    struct henka_renderer* renderer,
    const unsigned char* data,
    size_t data_size,
    const henka_texture_descriptor* descriptor,
    struct henka_texture** out_texture)
{
    return henka_opengl_renderer_create_texture_from_ktx2_memory_with_mip_limit(
        renderer,
        data,
        data_size,
        descriptor,
        0U,
        out_texture);
}

void henka_opengl_renderer_destroy_texture(
    struct henka_texture* texture)
{
    henka_opengl_texture_context_guard context_guard;
    henka_result context_result;
    henka_opengl_texture_data* texture_data;
    henka_opengl_renderer_state* state;
    henka_result result;

    if (texture == NULL)
    {
        return;
    }

    texture_data =
        (henka_opengl_texture_data*)texture->backend_data;
    if (texture->owns_backend &&
        texture_data != NULL &&
        texture->renderer != NULL &&
        texture->renderer->backend_state != NULL)
    {
        state = (henka_opengl_renderer_state*)
            texture->renderer->backend_state;
        henka_opengl_memory_remove_category(
            state,
            &state->tracked_texture_bytes,
            texture_data->tracked_gpu_bytes);
        if (state->tracked_texture_count > 0U)
        {
            --state->tracked_texture_count;
        }
        result = henka_opengl_begin_texture_context(
            state,
            &context_guard,
            "texture destruction");
        if (result == HENKA_SUCCESS)
        {
            henka_opengl_discard_prior_texture_errors(
                "texture destruction");
            glDeleteTextures(
                1,
                &texture_data->texture_id);
            (void)henka_opengl_collect_texture_errors(
                "texture destruction");
            context_result =
                henka_opengl_end_texture_context(
                    state,
                    &context_guard,
                    "texture destruction");
            if (context_result != HENKA_SUCCESS)
            {
                HENKA_LOG_ERROR(
                    "the previous OpenGL context could not be restored after texture destruction");
            }
        }
        else
        {
            HENKA_LOG_ERROR(
                "texture backend could not be deleted because the main context was unavailable");
        }
    }

    if (texture->owns_backend)
    {
        henka_free(texture_data);
    }
    texture->backend_data = NULL;
    texture->owns_backend = false;
    henka_free(texture);
}
