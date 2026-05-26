#include "henka_internal.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <SDL3/SDL_opengl_glext.h>

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <henka/log.h>
#include <henka/memory.h>

#include "../ui/ui_internal.h"

typedef struct henka_opengl_renderer_state
{
    SDL_Window* window;
    SDL_GLContext gl_context;
    GLuint ui_program;
    GLuint ui_vertex_array;
    GLuint ui_vertex_buffer;
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

typedef struct henka_ui_vertex
{
    float x;
    float y;
    float r;
    float g;
    float b;
    float a;
} henka_ui_vertex;

static henka_opengl_functions g_gl;

SDL_Window* henka_platform_get_sdl_window(struct henka_platform* platform);

static bool henka_compile_shader(GLuint shader, const char* source, const char* label);
static bool henka_link_program(GLuint program);

static void henka_apply_full_framebuffer_viewport(const struct henka_renderer* renderer)
{
    if (renderer == NULL)
    {
        return;
    }

    glDisable(GL_SCISSOR_TEST);
    glViewport(0, 0, renderer->framebuffer_width, renderer->framebuffer_height);
}

static void henka_apply_scene_viewport(const struct henka_renderer* renderer)
{
    henka_viewport viewport;
    GLint gl_y;

    if (renderer == NULL)
    {
        return;
    }

    viewport = renderer->scene_viewport;
    if (viewport.width <= 0 || viewport.height <= 0)
    {
        viewport = (henka_viewport){0, 0, renderer->framebuffer_width, renderer->framebuffer_height};
    }

    gl_y = (GLint)(renderer->framebuffer_height - (viewport.y + viewport.height));
    if (gl_y < 0)
    {
        gl_y = 0;
    }

    glEnable(GL_SCISSOR_TEST);
    glViewport((GLint)viewport.x, gl_y, (GLsizei)viewport.width, (GLsizei)viewport.height);
    glScissor((GLint)viewport.x, gl_y, (GLsizei)viewport.width, (GLsizei)viewport.height);
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
    size_t bytes_read;

    if (fopen_s(&file, path, "rb") != 0 || file == NULL)
    {
        return NULL;
    }

    fseek(file, 0L, SEEK_END);
    file_length = ftell(file);
    rewind(file);

    buffer = henka_malloc((size_t)file_length + 1U);
    if (buffer == NULL)
    {
        fclose(file);
        return NULL;
    }

    bytes_read = fread(buffer, 1U, (size_t)file_length, file);
    fclose(file);

    buffer[bytes_read] = '\0';
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

static void henka_set_uniform_mat4(GLuint program, const char* name, henka_mat4 value)
{
    GLint location;

    location = g_gl.GetUniformLocation(program, name);
    if (location >= 0)
    {
        g_gl.UniformMatrix4fv(location, 1, GL_FALSE, value.m);
    }
}

static void henka_set_uniform_vec4(GLuint program, const char* name, henka_vec4 value)
{
    GLint location;

    location = g_gl.GetUniformLocation(program, name);
    if (location >= 0)
    {
        g_gl.Uniform4f(location, value.x, value.y, value.z, value.w);
    }
}

static void henka_set_uniform_vec3(GLuint program, const char* name, henka_vec3 value)
{
    GLint location;

    location = g_gl.GetUniformLocation(program, name);
    if (location >= 0)
    {
        g_gl.Uniform3f(location, value.x, value.y, value.z);
    }
}

static void henka_set_uniform_bool(GLuint program, const char* name, bool value)
{
    GLint location;

    location = g_gl.GetUniformLocation(program, name);
    if (location >= 0)
    {
        g_gl.Uniform1i(location, value ? 1 : 0);
    }
}

static void henka_set_uniform_int(GLuint program, const char* name, int value)
{
    GLint location;

    location = g_gl.GetUniformLocation(program, name);
    if (location >= 0)
    {
        g_gl.Uniform1i(location, value);
    }
}

static henka_result henka_opengl_renderer_create_ui_resources(henka_opengl_renderer_state* state)
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
            &state->ui_program))
    {
        return HENKA_ERROR_RENDERER;
    }

    g_gl.GenVertexArrays(1, &state->ui_vertex_array);
    g_gl.GenBuffers(1, &state->ui_vertex_buffer);
    g_gl.BindVertexArray(state->ui_vertex_array);
    g_gl.BindBuffer(GL_ARRAY_BUFFER, state->ui_vertex_buffer);
    g_gl.EnableVertexAttribArray(0);
    g_gl.VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(henka_ui_vertex), (const void*)offsetof(henka_ui_vertex, x));
    g_gl.EnableVertexAttribArray(1);
    g_gl.VertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(henka_ui_vertex), (const void*)offsetof(henka_ui_vertex, r));
    g_gl.BindVertexArray(0);
    g_gl.BindBuffer(GL_ARRAY_BUFFER, 0);
    return HENKA_SUCCESS;
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

    result = henka_opengl_renderer_create_ui_resources(state);
    if (result != HENKA_SUCCESS)
    {
        SDL_GL_DestroyContext(state->gl_context);
        henka_free(state);
        renderer->backend_state = NULL;
        return result;
    }

    HENKA_LOG_INFO("renderer initialized with OpenGL backend");
    return HENKA_SUCCESS;
}

void henka_opengl_renderer_destroy(struct henka_renderer* renderer)
{
    henka_opengl_renderer_state* state;

    if (renderer == NULL || renderer->backend_state == NULL)
    {
        return;
    }

    state = (henka_opengl_renderer_state*)renderer->backend_state;
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
    glPolygonMode(GL_FRONT_AND_BACK, renderer->wireframe_enabled ? GL_LINE : GL_FILL);
    return HENKA_SUCCESS;
}

void henka_opengl_renderer_clear_frame(struct henka_renderer* renderer)
{
    henka_apply_full_framebuffer_viewport(renderer);
    glClearColor(0.08f, 0.09f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

henka_result henka_opengl_renderer_draw_scene(struct henka_renderer* renderer, const struct henka_scene* scene)
{
    henka_mat4 projection;
    henka_mat4 view;
    size_t index;

    if (renderer == NULL || scene == NULL || !scene->has_camera)
    {
        return HENKA_SUCCESS;
    }

    henka_apply_scene_viewport(renderer);
    projection = henka_camera_get_projection_matrix(&scene->camera);
    view = henka_camera_get_view_matrix(&scene->camera);

    for (index = 0U; index < scene->entity_capacity; ++index)
    {
        const henka_scene_entity_record* entity;
        const henka_opengl_mesh_data* mesh_data;
        const henka_opengl_shader_data* shader_data;
        const henka_opengl_texture_data* texture_data;
        henka_mat4 model;

        entity = &scene->entities[index];
        if (!entity->active || !entity->visible || entity->mesh == NULL || entity->material.shader == NULL)
        {
            continue;
        }

        mesh_data = (const henka_opengl_mesh_data*)entity->mesh->backend_data;
        shader_data = (const henka_opengl_shader_data*)entity->material.shader->backend_data;
        if (mesh_data == NULL || shader_data == NULL)
        {
            continue;
        }

        model = henka_transform_to_mat4(entity->transform);

        g_gl.UseProgram(shader_data->program);
        if (entity->material.depth_test)
        {
            glEnable(GL_DEPTH_TEST);
        }
        else
        {
            glDisable(GL_DEPTH_TEST);
        }
        henka_set_uniform_mat4(shader_data->program, "model", model);
        henka_set_uniform_mat4(shader_data->program, "view", view);
        henka_set_uniform_mat4(shader_data->program, "projection", projection);
        henka_set_uniform_vec4(shader_data->program, "baseColor", entity->material.base_color);
        henka_set_uniform_vec3(shader_data->program, "lightDirection", scene->light_direction);
        henka_set_uniform_vec3(shader_data->program, "ambientColor", scene->ambient_color);
        henka_set_uniform_bool(shader_data->program, "useTexture", entity->material.use_texture && entity->material.base_color_texture != NULL);
        henka_set_uniform_bool(shader_data->program, "useLighting", entity->material.use_lighting);
        henka_set_uniform_int(shader_data->program, "baseColorTexture", 0);

        if (entity->material.base_color_texture != NULL)
        {
            texture_data = (const henka_opengl_texture_data*)entity->material.base_color_texture->backend_data;
            if (texture_data != NULL)
            {
                g_gl.ActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, texture_data->texture_id);
            }
        }

        g_gl.BindVertexArray(mesh_data->vao);
        glDrawElements(mesh_data->primitive_mode, mesh_data->index_count, GL_UNSIGNED_INT, 0);
    }

    g_gl.BindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    g_gl.UseProgram(0);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_SCISSOR_TEST);
    return HENKA_SUCCESS;
}

henka_result henka_opengl_renderer_draw_ui(struct henka_renderer* renderer, const struct henka_ui_context* ui_context)
{
    const henka_opengl_renderer_state* state;
    henka_ui_vertex* vertices;
    size_t index;
    size_t vertex_count;

    if (renderer == NULL || renderer->backend_state == NULL || ui_context == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    if (ui_context->draw_rect_count == 0U)
    {
        return HENKA_SUCCESS;
    }

    henka_apply_full_framebuffer_viewport(renderer);
    state = (const henka_opengl_renderer_state*)renderer->backend_state;
    vertex_count = ui_context->draw_rect_count * 6U;
    vertices = henka_malloc(vertex_count * sizeof(*vertices));
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

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    g_gl.UseProgram(state->ui_program);
    {
        GLint location;

        location = g_gl.GetUniformLocation(state->ui_program, "framebufferSize");
        if (location >= 0)
        {
            g_gl.Uniform2f(location, (GLfloat)renderer->framebuffer_width, (GLfloat)renderer->framebuffer_height);
        }
    }
    g_gl.BindVertexArray(state->ui_vertex_array);
    g_gl.BindBuffer(GL_ARRAY_BUFFER, state->ui_vertex_buffer);
    g_gl.BufferData(GL_ARRAY_BUFFER, vertex_count * sizeof(*vertices), vertices, GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)vertex_count);
    g_gl.BindBuffer(GL_ARRAY_BUFFER, 0);
    g_gl.BindVertexArray(0);
    g_gl.UseProgram(0);
    glDisable(GL_BLEND);
    henka_free(vertices);
    return HENKA_SUCCESS;
}

henka_result henka_opengl_renderer_end_frame(struct henka_renderer* renderer)
{
    henka_opengl_renderer_state* state;

    if (renderer == NULL || renderer->backend_state == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    state = (henka_opengl_renderer_state*)renderer->backend_state;
    if (!SDL_GL_SwapWindow(state->window))
    {
        HENKA_LOG_ERROR("SDL_GL_SwapWindow failed: %s", SDL_GetError());
        return HENKA_ERROR_RENDERER;
    }

    return HENKA_SUCCESS;
}

void henka_opengl_renderer_resize_viewport(struct henka_renderer* renderer, int width, int height)
{
    (void)renderer;
    glDisable(GL_SCISSOR_TEST);
    glViewport(0, 0, width, height);
}

henka_result henka_opengl_renderer_set_vsync(struct henka_renderer* renderer, bool enabled)
{
    return henka_platform_set_vsync(renderer->platform, enabled);
}

henka_result henka_opengl_renderer_set_wireframe(struct henka_renderer* renderer, bool enabled)
{
    (void)renderer;
    glPolygonMode(GL_FRONT_AND_BACK, enabled ? GL_LINE : GL_FILL);
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
    henka_mesh* mesh;
    henka_opengl_mesh_data* mesh_data;

    if (renderer == NULL || vertices == NULL || indices == NULL || out_mesh == NULL || vertex_count <= 0 || index_count <= 0)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    *out_mesh = NULL;

    mesh = henka_calloc(1U, sizeof(*mesh));
    mesh_data = henka_calloc(1U, sizeof(*mesh_data));
    if (mesh == NULL || mesh_data == NULL)
    {
        henka_free(mesh_data);
        henka_free(mesh);
        return HENKA_ERROR_OUT_OF_MEMORY;
    }

    g_gl.GenVertexArrays(1, &mesh_data->vao);
    g_gl.GenBuffers(1, &mesh_data->vertex_buffer);
    g_gl.GenBuffers(1, &mesh_data->index_buffer);

    g_gl.BindVertexArray(mesh_data->vao);
    g_gl.BindBuffer(GL_ARRAY_BUFFER, mesh_data->vertex_buffer);
    g_gl.BufferData(GL_ARRAY_BUFFER, sizeof(*vertices) * (size_t)vertex_count, vertices, GL_STATIC_DRAW);
    g_gl.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh_data->index_buffer);
    g_gl.BufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(*indices) * (size_t)index_count, indices, GL_STATIC_DRAW);

    g_gl.EnableVertexAttribArray(0);
    g_gl.VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(henka_vertex), (const void*)offsetof(henka_vertex, position));
    g_gl.EnableVertexAttribArray(1);
    g_gl.VertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(henka_vertex), (const void*)offsetof(henka_vertex, normal));
    g_gl.EnableVertexAttribArray(2);
    g_gl.VertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(henka_vertex), (const void*)offsetof(henka_vertex, uv));
    g_gl.BindVertexArray(0);

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
    g_gl.DeleteProgram(shader_data->program);
    henka_free(shader_data);
    henka_free(shader);
}

henka_result henka_opengl_renderer_create_texture_from_rgba8(
    struct henka_renderer* renderer,
    int width,
    int height,
    const unsigned char* pixels,
    struct henka_texture** out_texture)
{
    henka_texture* texture;
    henka_opengl_texture_data* texture_data;

    if (renderer == NULL || pixels == NULL || out_texture == NULL || width <= 0 || height <= 0)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    *out_texture = NULL;

    texture = henka_calloc(1U, sizeof(*texture));
    texture_data = henka_calloc(1U, sizeof(*texture_data));
    if (texture == NULL || texture_data == NULL)
    {
        henka_free(texture_data);
        henka_free(texture);
        return HENKA_ERROR_OUT_OF_MEMORY;
    }

    glGenTextures(1, &texture_data->texture_id);
    g_gl.ActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture_data->texture_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    g_gl.GenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);

    texture->renderer = renderer;
    texture->backend_data = texture_data;
    texture->width = width;
    texture->height = height;

    *out_texture = texture;
    return HENKA_SUCCESS;
}

void henka_opengl_renderer_destroy_texture(struct henka_texture* texture)
{
    henka_opengl_texture_data* texture_data;

    if (texture == NULL || texture->backend_data == NULL)
    {
        return;
    }

    texture_data = (henka_opengl_texture_data*)texture->backend_data;
    glDeleteTextures(1, &texture_data->texture_id);
    henka_free(texture_data);
    henka_free(texture);
}
