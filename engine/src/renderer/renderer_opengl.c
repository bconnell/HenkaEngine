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

typedef struct henka_opengl_tool_window_target
{
    henka_window_id id;
    SDL_Window* window;
    SDL_GLContext gl_context;
    GLuint ui_program;
    GLuint ui_vertex_array;
    GLuint ui_vertex_buffer;
} henka_opengl_tool_window_target;

typedef struct henka_opengl_renderer_state
{
    SDL_Window* window;
    SDL_GLContext gl_context;
    GLuint ui_program;
    GLuint ui_vertex_array;
    GLuint ui_vertex_buffer;
    GLuint viewport_program;
    GLuint tone_program;
    GLuint environment_program;
    GLuint tone_vertex_array;
    GLuint hdr_framebuffer;
    GLuint hdr_color_texture;
    GLuint hdr_depth_buffer;
    int hdr_width;
    int hdr_height;
    int hdr_requested_width;
    int hdr_requested_height;
    uint64_t hdr_generation;
    bool hdr_framebuffer_complete;
    char hdr_failure_reason[64];
    GLuint shadow_program;
    GLuint shadow_framebuffer;
    GLuint shadow_depth_texture;
    int shadow_resolution;
    uint64_t shadow_generation;
    bool shadow_framebuffer_complete;
    char shadow_failure_reason[64];
    henka_opengl_tool_window_target tool_targets[HENKA_MAX_TOOL_WINDOWS];
} henka_opengl_renderer_state;

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
    PFNGLVERTEXATTRIBPOINTERPROC VertexAttribPointer;
    PFNGLDELETEBUFFERSPROC DeleteBuffers;
    PFNGLDELETEVERTEXARRAYSPROC DeleteVertexArrays;
    PFNGLACTIVETEXTUREPROC ActiveTexture;
    PFNGLGENERATEMIPMAPPROC GenerateMipmap;
    PFNGLGENFRAMEBUFFERSPROC GenFramebuffers;
    PFNGLBINDFRAMEBUFFERPROC BindFramebuffer;
    PFNGLFRAMEBUFFERTEXTURE2DPROC FramebufferTexture2D;
    PFNGLCHECKFRAMEBUFFERSTATUSPROC CheckFramebufferStatus;
    PFNGLDELETEFRAMEBUFFERSPROC DeleteFramebuffers;
    PFNGLGENRENDERBUFFERSPROC GenRenderbuffers;
    PFNGLBINDRENDERBUFFERPROC BindRenderbuffer;
    PFNGLRENDERBUFFERSTORAGEPROC RenderbufferStorage;
    PFNGLFRAMEBUFFERRENDERBUFFERPROC FramebufferRenderbuffer;
    PFNGLDELETERENDERBUFFERSPROC DeleteRenderbuffers;
} henka_opengl_functions;

typedef struct henka_opengl_mesh_data
{
    GLuint vao;
    GLuint vertex_buffer;
    GLuint index_buffer;
    GLenum primitive_mode;
    GLsizei index_count;
} henka_opengl_mesh_data;

typedef struct henka_opengl_shader_data
{
    GLuint program;
} henka_opengl_shader_data;

typedef struct henka_opengl_texture_data
{
    GLuint texture_id;
} henka_opengl_texture_data;

static henka_opengl_functions g_gl;

#define HENKA_OPENGL_UNIFORM_CACHE_CAPACITY 256U
#define HENKA_OPENGL_UNIFORM_NAME_CAPACITY 48U

typedef struct henka_opengl_uniform_cache_entry
{
    SDL_GLContext context;
    GLuint program;
    GLint location;
    char name[HENKA_OPENGL_UNIFORM_NAME_CAPACITY];
} henka_opengl_uniform_cache_entry;

static henka_opengl_uniform_cache_entry g_uniform_cache[HENKA_OPENGL_UNIFORM_CACHE_CAPACITY];
static size_t g_uniform_cache_count;

static void henka_opengl_uniform_cache_forget(GLuint program)
{
    SDL_GLContext context = SDL_GL_GetCurrentContext();
    size_t index;

    if (context == NULL)
    {
        return;
    }
    for (index = 0U; index < g_uniform_cache_count; ++index)
    {
        if (g_uniform_cache[index].context == context &&
            g_uniform_cache[index].program == program)
        {
            g_uniform_cache[index] = g_uniform_cache[g_uniform_cache_count - 1U];
            --g_uniform_cache_count;
            --index;
        }
    }
}

static GLint henka_opengl_uniform_location(GLuint program, const char* name)
{
    SDL_GLContext context = SDL_GL_GetCurrentContext();
    size_t index;

    if (context == NULL || program == 0U || name == NULL || name[0] == '\0')
    {
        return -1;
    }
    for (index = 0U; index < g_uniform_cache_count; ++index)
    {
        if (g_uniform_cache[index].context == context &&
            g_uniform_cache[index].program == program &&
            strncmp(g_uniform_cache[index].name, name, HENKA_OPENGL_UNIFORM_NAME_CAPACITY) == 0)
        {
            return g_uniform_cache[index].location;
        }
    }

    if (g_uniform_cache_count >= HENKA_OPENGL_UNIFORM_CACHE_CAPACITY ||
        strlen(name) >= HENKA_OPENGL_UNIFORM_NAME_CAPACITY)
    {
        HENKA_LOG_ERROR("uniform cache capacity exceeded for '%s'", name);
        return -1;
    }
    g_uniform_cache[g_uniform_cache_count].context = context;
    g_uniform_cache[g_uniform_cache_count].program = program;
    g_uniform_cache[g_uniform_cache_count].location = g_gl.GetUniformLocation(program, name);
    (void)snprintf(g_uniform_cache[g_uniform_cache_count].name,
        sizeof(g_uniform_cache[g_uniform_cache_count].name), "%s", name);
    ++g_uniform_cache_count;
    return g_uniform_cache[g_uniform_cache_count - 1U].location;
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
static bool henka_validate_shader_contract(GLuint program, const char* label);

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
    HENKA_GL_LOAD(VertexAttribPointer);
    HENKA_GL_LOAD(DeleteBuffers);
    HENKA_GL_LOAD(DeleteVertexArrays);
    HENKA_GL_LOAD(ActiveTexture);
    HENKA_GL_LOAD(GenerateMipmap);
    HENKA_GL_LOAD(GenFramebuffers);
    HENKA_GL_LOAD(BindFramebuffer);
    HENKA_GL_LOAD(FramebufferTexture2D);
    HENKA_GL_LOAD(CheckFramebufferStatus);
    HENKA_GL_LOAD(DeleteFramebuffers);
    HENKA_GL_LOAD(GenRenderbuffers);
    HENKA_GL_LOAD(BindRenderbuffer);
    HENKA_GL_LOAD(RenderbufferStorage);
    HENKA_GL_LOAD(FramebufferRenderbuffer);
    HENKA_GL_LOAD(DeleteRenderbuffers);

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
    henka_opengl_uniform_cache_forget(program);
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

static bool henka_validate_shader_contract(GLuint program, const char* label)
{
    static const char* required_uniforms[] =
    {
        "model",
        "view",
        "projection",
        "baseColor"
    };
    size_t index;

    if (program == 0U)
    {
        return false;
    }
    for (index = 0U; index < sizeof(required_uniforms) / sizeof(required_uniforms[0]); ++index)
    {
        if (g_gl.GetUniformLocation(program, required_uniforms[index]) < 0)
        {
            HENKA_LOG_ERROR(
                "shader contract rejected for '%s': required uniform '%s' is missing",
                label != NULL ? label : "shader",
                required_uniforms[index]);
            return false;
        }
    }
    return true;
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
    if (state->hdr_depth_buffer != 0U)
    {
        g_gl.DeleteRenderbuffers(1, &state->hdr_depth_buffer);
    }
    if (state->hdr_color_texture != 0U)
    {
        glDeleteTextures(1, &state->hdr_color_texture);
    }
    if (state->hdr_framebuffer != 0U)
    {
        g_gl.DeleteFramebuffers(1, &state->hdr_framebuffer);
    }
    state->hdr_depth_buffer = 0U;
    state->hdr_color_texture = 0U;
    state->hdr_framebuffer = 0U;
    state->hdr_width = 0;
    state->hdr_height = 0;
}

static void henka_opengl_delete_shadow_target(henka_opengl_renderer_state* state)
{
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

static henka_result henka_opengl_create_hdr_target(
    henka_opengl_renderer_state* state,
    int width,
    int height)
{
    GLuint color_texture = 0U;
    GLuint depth_buffer = 0U;
    GLuint framebuffer = 0U;
    GLint previous_framebuffer = 0;
    GLint previous_texture = 0;
    GLint previous_renderbuffer = 0;

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
    glGetIntegerv(GL_RENDERBUFFER_BINDING, &previous_renderbuffer);
    g_gl.GenFramebuffers(1, &framebuffer);
    glGenTextures(1, &color_texture);
    g_gl.GenRenderbuffers(1, &depth_buffer);
    if (framebuffer == 0U || color_texture == 0U || depth_buffer == 0U)
    {
        if (depth_buffer != 0U) g_gl.DeleteRenderbuffers(1, &depth_buffer);
        if (color_texture != 0U) glDeleteTextures(1, &color_texture);
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
    g_gl.BindRenderbuffer(GL_RENDERBUFFER, depth_buffer);
    g_gl.RenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
    g_gl.FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth_buffer);
    if (g_gl.CheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        (void)snprintf(state->hdr_failure_reason, sizeof(state->hdr_failure_reason), "incomplete HDR framebuffer");
        g_gl.BindFramebuffer(GL_FRAMEBUFFER, (GLuint)previous_framebuffer);
        glBindTexture(GL_TEXTURE_2D, (GLuint)previous_texture);
        g_gl.BindRenderbuffer(GL_RENDERBUFFER, (GLuint)previous_renderbuffer);
        g_gl.DeleteRenderbuffers(1, &depth_buffer);
        glDeleteTextures(1, &color_texture);
        g_gl.DeleteFramebuffers(1, &framebuffer);
        return HENKA_ERROR_RENDERER;
    }
    g_gl.BindFramebuffer(GL_FRAMEBUFFER, (GLuint)previous_framebuffer);
    glBindTexture(GL_TEXTURE_2D, (GLuint)previous_texture);
    g_gl.BindRenderbuffer(GL_RENDERBUFFER, (GLuint)previous_renderbuffer);
    henka_opengl_delete_hdr_target(state);
    state->hdr_framebuffer = framebuffer;
    state->hdr_color_texture = color_texture;
    state->hdr_depth_buffer = depth_buffer;
    state->hdr_width = width;
    state->hdr_height = height;
    state->hdr_generation = state->hdr_generation == UINT64_MAX ? 1U : state->hdr_generation + 1U;
    state->hdr_framebuffer_complete = true;
    state->hdr_failure_reason[0] = '\0';
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
    return HENKA_SUCCESS;
}

static henka_result henka_opengl_create_render_programs(
    henka_opengl_renderer_state* state)
{
    static const char* tone_vertex =
        "#version 330 core\n"
        "out vec2 uv;\n"
        "void main(){ vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2); uv = p; gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0); }\n";
    static const char* tone_fragment =
        "#version 330 core\n"
        "in vec2 uv; uniform sampler2D hdrTexture; uniform float exposure; out vec4 outColor;\n"
        "vec3 aces(vec3 x){ return clamp((x*(2.51*x+0.03))/(x*(2.43*x+0.59)+0.14),0.0,1.0); }\n"
        "void main(){ vec3 color = texture(hdrTexture, uv).rgb * exp2(exposure); color = aces(max(color, vec3(0.0))); outColor = vec4(pow(color, vec3(1.0/2.2)), 1.0); }\n";
    static const char* environment_fragment =
        "#version 330 core\n"
        "in vec2 uv; uniform vec3 groundColor; uniform vec3 horizonColor; uniform vec3 zenithColor; uniform float intensity; out vec4 outColor;\n"
        "void main(){ float height = clamp(uv.y, 0.0, 1.0); float horizon = smoothstep(0.04, 0.48, height); vec3 lower = mix(groundColor, horizonColor, horizon); vec3 color = mix(lower, zenithColor, smoothstep(0.48, 1.0, height)); outColor = vec4(max(color * max(intensity, 0.0), vec3(0.0)), 1.0); }\n";
    static const char* shadow_vertex =
        "#version 330 core\n"
        "layout(location=0) in vec3 inPosition; layout(location=2) in vec2 inUv; out vec2 fragUv; uniform mat4 model; uniform mat4 lightMatrix;\n"
        "void main(){ fragUv = inUv; gl_Position = lightMatrix * model * vec4(inPosition,1.0); }\n";
    static const char* shadow_fragment =
        "#version 330 core\n"
        "in vec2 fragUv; uniform vec4 baseColor; uniform sampler2D baseColorTexture; uniform bool useTexture; uniform int alphaMode; uniform float alphaCutoff;\n"
        "void main(){ if(alphaMode == 1 && baseColor.a * (useTexture ? texture(baseColorTexture, fragUv).a : 1.0) < alphaCutoff) discard; }\n";

    if (state == NULL ||
        !henka_compile_program_from_source(tone_vertex, tone_fragment, "tone-map vertex", "tone-map fragment", &state->tone_program) ||
        !henka_compile_program_from_source(tone_vertex, environment_fragment, "environment vertex", "environment fragment", &state->environment_program) ||
        !henka_compile_program_from_source(shadow_vertex, shadow_fragment, "shadow vertex", "shadow fragment", &state->shadow_program))
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
    struct henka_renderer* renderer)
{
    static const henka_vec3 ground_color = {0.035f, 0.045f, 0.065f};
    static const henka_vec3 horizon_color = {0.16f, 0.19f, 0.24f};
    static const henka_vec3 zenith_color = {0.055f, 0.08f, 0.14f};
    henka_viewport viewport;

    if (state == NULL || renderer == NULL || state->environment_program == 0U)
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
    henka_set_uniform_vec3(state->environment_program, "groundColor", ground_color);
    henka_set_uniform_vec3(state->environment_program, "horizonColor", horizon_color);
    henka_set_uniform_vec3(state->environment_program, "zenithColor", zenith_color);
    henka_set_uniform_float(state->environment_program, "intensity", 1.5f);
    g_gl.BindVertexArray(state->tone_vertex_array);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    g_gl.BindVertexArray(0);
    g_gl.UseProgram(0);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
}

static henka_mat4 henka_opengl_get_light_matrix(const henka_scene* scene)
{
    henka_vec3 direction = henka_vec3_normalize(scene->light_direction);
    henka_vec3 up = fabsf(direction.y) > 0.94f ?
        (henka_vec3){1.0f, 0.0f, 0.0f} :
        (henka_vec3){0.0f, 1.0f, 0.0f};
    henka_vec3 target = (henka_vec3){0.0f, 0.0f, 0.0f};
    henka_vec3 eye = henka_vec3_scale(direction, -12.0f);

    return henka_mat4_multiply(
        henka_mat4_orthographic(-12.0f, 12.0f, -12.0f, 12.0f, 0.1f, 40.0f),
        henka_mat4_look_at(eye, target, up));
}

static void henka_opengl_draw_shadow_pass(
    henka_opengl_renderer_state* state,
    const henka_scene* scene,
    henka_mat4 light_matrix)
{
    size_t index;

    if (state->shadow_framebuffer == 0U || state->shadow_program == 0U ||
        !state->shadow_framebuffer_complete)
    {
        return;
    }
    g_gl.BindFramebuffer(GL_FRAMEBUFFER, state->shadow_framebuffer);
    glViewport(0, 0, state->shadow_resolution, state->shadow_resolution);
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
        henka_set_uniform_mat4(
            state->shadow_program,
            "model",
            henka_transform_to_mat4(entity->transform));
        henka_set_uniform_mat4(state->shadow_program, "lightMatrix", light_matrix);
        henka_set_uniform_vec4(state->shadow_program, "baseColor", entity->material.base_color);
        henka_set_uniform_int(state->shadow_program, "baseColorTexture", 0);
        henka_set_uniform_bool(state->shadow_program, "useTexture",
            entity->material.use_texture && entity->material.base_color_texture != NULL &&
            entity->material.base_color_texture->backend_data != NULL);
        henka_set_uniform_int(state->shadow_program, "alphaMode", (int)entity->material.alpha_mode);
        henka_set_uniform_float(state->shadow_program, "alphaCutoff", entity->material.alpha_cutoff);
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

static void henka_opengl_present_hdr(
    struct henka_renderer* renderer,
    henka_opengl_renderer_state* state)
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
    henka_set_uniform_int(state->tone_program, "hdrTexture", 0);
    henka_set_uniform_float(state->tone_program, "exposure", renderer->exposure);
    g_gl.ActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, state->hdr_color_texture);
    g_gl.BindVertexArray(state->tone_vertex_array);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    g_gl.BindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0U);
    g_gl.UseProgram(0);
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
    if (result != HENKA_SUCCESS)
    {
        g_gl.DeleteBuffers(1, &state->ui_vertex_buffer);
        g_gl.DeleteVertexArrays(1, &state->ui_vertex_array);
        g_gl.DeleteProgram(state->ui_program);
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
        henka_opengl_create_shadow_target(state, 1024) != HENKA_SUCCESS)
    {
        henka_opengl_delete_hdr_target(state);
        henka_opengl_delete_shadow_target(state);
        if (state->shadow_program != 0U) g_gl.DeleteProgram(state->shadow_program);
        if (state->tone_program != 0U) g_gl.DeleteProgram(state->tone_program);
        if (state->environment_program != 0U) g_gl.DeleteProgram(state->environment_program);
        if (state->tone_vertex_array != 0U) g_gl.DeleteVertexArrays(1, &state->tone_vertex_array);
        g_gl.DeleteProgram(state->viewport_program);
        g_gl.DeleteBuffers(1, &state->ui_vertex_buffer);
        g_gl.DeleteVertexArrays(1, &state->ui_vertex_array);
        g_gl.DeleteProgram(state->ui_program);
        SDL_GL_DestroyContext(state->gl_context);
        henka_free(state);
        renderer->backend_state = NULL;
        return HENKA_ERROR_RENDERER;
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
        henka_opengl_delete_shadow_target(state);
        if (state->tone_program != 0U)
        {
            g_gl.DeleteProgram(state->tone_program);
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
    henka_viewport_render_policy policy;
    henka_opengl_renderer_state* state;
    henka_mat4 view;
    bool rendered;
    size_t index;

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

    rendered = henka_renderer_get_viewport_shading_mode(renderer) ==
        HENKA_VIEWPORT_SHADING_RENDERED;
    light_matrix = henka_opengl_get_light_matrix(scene);
    if (policy.use_hdr_presentation)
    {
        henka_viewport scene_viewport = henka_renderer_get_scene_viewport(renderer);

        if (state->hdr_width != scene_viewport.width ||
            state->hdr_height != scene_viewport.height)
        {
            henka_opengl_renderer_sync_scene_target(renderer);
        }
        if (rendered)
        {
            henka_opengl_draw_shadow_pass(state, scene, light_matrix);
        }
        if (!henka_opengl_renderer_is_hdr_ready(renderer))
        {
            return HENKA_ERROR_RENDERER;
        }
        g_gl.BindFramebuffer(GL_FRAMEBUFFER, state->hdr_framebuffer);
        henka_apply_scene_target_viewport(renderer);
        glClearColor(0.075f, 0.09f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        henka_opengl_draw_environment(state, renderer);
    }
    else
    {
        henka_apply_scene_viewport(renderer);
    }
    projection =
        henka_camera_get_projection_matrix(&scene->camera);
    view = henka_camera_get_view_matrix(&scene->camera);
    g_gl.ActiveTexture(GL_TEXTURE0);

    for (index = 0U;
         index < scene->entity_capacity;
         ++index)
    {
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
        henka_bounds world_bounds;
        henka_entity entity_id;

        entity = &scene->entities[index];
        if (!entity->active ||
            !entity->visible ||
            entity->mesh == NULL ||
            entity->material.shader == NULL)
        {
            continue;
        }

        if ((entity->flags & HENKA_SCENE_ENTITY_FLAG_HELPER) == 0U &&
            entity->has_local_bounds)
        {
            entity_id = henka_scene_get_entity_at_index(scene, index);
            if (entity_id != HENKA_INVALID_ENTITY &&
                henka_scene_get_entity_world_bounds(scene, entity_id, &world_bounds) == HENKA_SUCCESS &&
                !henka_opengl_bounds_in_camera(&scene->camera, view, world_bounds))
            {
                continue;
            }
        }

        mesh_data =
            (const henka_opengl_mesh_data*)
                entity->mesh->backend_data;
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
        editor_surface =
            !helper_entity &&
            henka_renderer_get_viewport_shading_mode(
                renderer) <= HENKA_VIEWPORT_SHADING_SOLID;
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
        g_gl.UseProgram(program);
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
        henka_set_uniform_mat4(program, "lightMatrix", light_matrix);
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
            (henka_vec3){0.035f, 0.045f, 0.065f});
        henka_set_uniform_vec3(
            program,
            "environmentHorizonColor",
            (henka_vec3){0.16f, 0.19f, 0.24f});
        henka_set_uniform_vec3(
            program,
            "environmentZenithColor",
            (henka_vec3){0.055f, 0.08f, 0.14f});
        henka_set_uniform_float(program, "environmentIntensity", 1.5f);
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
        henka_set_uniform_int(program, "normalTexture", 1);
        henka_set_uniform_int(program, "metallicRoughnessTexture", 2);
        henka_set_uniform_int(program, "occlusionTexture", 3);
        henka_set_uniform_int(program, "emissiveTexture", 4);
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
        henka_set_uniform_float(program, "normalScale", entity->material.normal_scale);
        henka_set_uniform_float(program, "occlusionStrength", entity->material.occlusion_strength);
        henka_set_uniform_vec3(program, "emissiveColor", entity->material.emissive_color);
        henka_set_uniform_float(program, "emissiveStrength", entity->material.emissive_strength);
        henka_set_uniform_int(program, "alphaMode", (int)entity->material.alpha_mode);
        henka_set_uniform_float(program, "alphaCutoff", entity->material.alpha_cutoff);
        henka_set_uniform_int(program, "shadowMap", 5);
        henka_set_uniform_bool(program, "useShadowMap",
            rendered && state->shadow_depth_texture != 0U && entity->material.receive_shadows);

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
        g_gl.ActiveTexture(GL_TEXTURE0);
        g_gl.BindVertexArray(mesh_data->vao);
        glDrawElements(
            mesh_data->primitive_mode,
            mesh_data->index_count,
            GL_UNSIGNED_INT,
            0);
    }

    if (policy.use_hdr_presentation)
    {
        g_gl.BindFramebuffer(GL_FRAMEBUFFER, 0U);
        henka_opengl_present_hdr(renderer, state);
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
    g_gl.ActiveTexture(GL_TEXTURE0);
    g_gl.UseProgram(0);
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glDisable(GL_SCISSOR_TEST);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    return HENKA_SUCCESS;
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
        state->hdr_framebuffer != 0U && state->hdr_framebuffer_complete)
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
                 vertices[vertex_index].color.w > 1.0f)))
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
        henka_vec3 tangent = tangents[vertex_index];
        float handedness;

        tangent = henka_vec3_subtract(
            tangent,
            henka_vec3_scale(normal, henka_vec3_dot(normal, tangent)));
        if (!isfinite(henka_vec3_length(tangent)) || henka_vec3_length(tangent) <= 0.000001f)
        {
            henka_vec3 fallback_axis = fabsf(normal.y) < 0.9f ?
                (henka_vec3){0.0f, 1.0f, 0.0f} :
                (henka_vec3){1.0f, 0.0f, 0.0f};
            tangent = henka_vec3_cross(normal, fallback_axis);
        }
        tangent = henka_vec3_normalize(tangent);
        handedness = henka_vec3_dot(
            henka_vec3_cross(normal, tangent),
            bitangents[vertex_index]) < 0.0f ? -1.0f : 1.0f;
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
    char* fragment_source;
    char* vertex_source;
    GLuint fragment_shader;
    GLuint program;
    henka_shader* shader;
    henka_opengl_shader_data* shader_data;
    GLuint vertex_shader;

    (void)renderer;

    if (vertex_path == NULL || fragment_path == NULL || out_shader == NULL)
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

    if (!henka_validate_shader_contract(program, fragment_path))
    {
        g_gl.DeleteProgram(program);
        g_gl.DeleteShader(vertex_shader);
        g_gl.DeleteShader(fragment_shader);
        henka_free(vertex_source);
        henka_free(fragment_source);
        return HENKA_ERROR_RENDERER;
    }

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

    shader_data->program = program;
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
    henka_opengl_uniform_cache_forget(shader_data->program);
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
    henka_opengl_texture_context_guard context_guard;
    henka_result context_result;
    size_t decoded_bytes;
    henka_opengl_renderer_state* state;
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
        henka_texture_descriptor_validate(descriptor) != HENKA_SUCCESS ||
        !henka_checked_rgba8_size(
            width,
            height,
            &decoded_bytes))
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
            descriptor->color_space == HENKA_TEXTURE_COLOR_SPACE_SRGB ?
                GL_SRGB8_ALPHA8 : GL_RGBA8,
            width,
            height,
            0,
            GL_RGBA,
            GL_UNSIGNED_BYTE,
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
    texture->source_class = HENKA_TEXTURE_SOURCE_CLASS_LDR_8_BIT;
    texture->alpha_mode = HENKA_TEXTURE_ALPHA_OPAQUE;
    texture->last_failure = HENKA_TEXTURE_FAILURE_NONE;
    texture->content_revision = 1U;

    *out_texture = texture;
    return HENKA_SUCCESS;
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
