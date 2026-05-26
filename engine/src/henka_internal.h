#ifndef HENKA_INTERNAL_H
#define HENKA_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

#include <henka/assets.h>
#include <henka/camera.h>
#include <henka/engine.h>
#include <henka/input.h>
#include <henka/math.h>
#include <henka/mesh.h>
#include <henka/persistence.h>
#include <henka/platform.h>
#include <henka/renderer.h>
#include <henka/result.h>
#include <henka/scene.h>
#include <henka/shader.h>
#include <henka/texture.h>
#include <henka/time.h>
#include <henka/ui.h>
#include <henka/workspace.h>

typedef enum henka_mesh_primitive
{
    HENKA_MESH_PRIMITIVE_TRIANGLES = 0,
    HENKA_MESH_PRIMITIVE_LINES
} henka_mesh_primitive;

typedef struct henka_vertex
{
    henka_vec3 position;
    henka_vec3 normal;
    henka_vec2 uv;
} henka_vertex;

typedef struct henka_input_state
{
    bool keys_down[HENKA_KEY_COUNT];
    bool keys_pressed[HENKA_KEY_COUNT];
    bool keys_released[HENKA_KEY_COUNT];
    bool mouse_buttons_down[HENKA_MOUSE_BUTTON_COUNT];
    bool mouse_buttons_pressed[HENKA_MOUSE_BUTTON_COUNT];
    bool mouse_buttons_released[HENKA_MOUSE_BUTTON_COUNT];
    henka_vec2 mouse_position;
    henka_vec2 mouse_delta;
    henka_vec2 mouse_wheel_delta;
    bool close_requested;
} henka_input_state;

typedef struct henka_platform_desc
{
    const char* application_name;
    int window_width;
    int window_height;
    bool enable_vsync;
} henka_platform_desc;

typedef struct henka_platform_frame_state
{
    bool close_requested;
    bool resized;
    int framebuffer_width;
    int framebuffer_height;
} henka_platform_frame_state;

typedef struct henka_scene_entity_record
{
    bool active;
    bool visible;
    uint32_t flags;
    char* name;
    char* tag;
    henka_transform transform;
    henka_mesh* mesh;
    henka_material material;
    bool has_local_bounds;
    henka_bounds local_bounds;
    henka_interaction_desc interaction;
} henka_scene_entity_record;

typedef struct henka_asset_shader_entry
{
    char* key;
    henka_shader* shader;
    henka_asset_metadata metadata;
} henka_asset_shader_entry;

typedef struct henka_asset_texture_entry
{
    char* key;
    henka_texture* texture;
    bool owns_texture;
    henka_asset_metadata metadata;
} henka_asset_texture_entry;

typedef struct henka_asset_mesh_entry
{
    char* key;
    henka_mesh* mesh;
    bool owns_mesh;
    henka_asset_metadata metadata;
} henka_asset_mesh_entry;

struct henka_asset_manager
{
    struct henka_engine* engine;
    henka_asset_shader_entry* shader_entries;
    size_t shader_count;
    size_t shader_capacity;
    henka_asset_texture_entry* texture_entries;
    size_t texture_count;
    size_t texture_capacity;
    henka_asset_mesh_entry* mesh_entries;
    size_t mesh_count;
    size_t mesh_capacity;
    henka_texture* white_texture;
    henka_texture* error_texture;
    henka_mesh* fallback_mesh;
};

struct henka_scene
{
    henka_scene_entity_record* entities;
    size_t entity_capacity;
    size_t entity_count;
    henka_camera camera;
    bool has_camera;
    henka_vec3 light_direction;
    henka_vec3 ambient_color;
};

struct henka_platform;

struct henka_renderer
{
    struct henka_platform* platform;
    void* backend_state;
    bool vsync_enabled;
    bool wireframe_enabled;
    bool mouse_captured;
    int framebuffer_width;
    int framebuffer_height;
    henka_viewport scene_viewport;
};

struct henka_mesh
{
    struct henka_renderer* renderer;
    henka_mesh_primitive primitive;
    int vertex_count;
    int index_count;
    void* backend_data;
};

struct henka_shader
{
    struct henka_renderer* renderer;
    void* backend_data;
};

struct henka_texture
{
    struct henka_renderer* renderer;
    void* backend_data;
    int width;
    int height;
};

struct henka_engine
{
    henka_engine_config config;
    struct henka_platform* platform;
    struct henka_renderer* renderer;
    struct henka_asset_manager* asset_manager;
    struct henka_scene* active_scene;
    struct henka_ui_context* active_ui;
    char* asset_base_path;
    char* user_data_base_path;
    henka_package_mode package_mode;
    henka_input_state input;
    henka_key action_key_bindings[HENKA_INPUT_ACTION_COUNT];
    henka_mouse_button action_mouse_bindings[HENKA_INPUT_ACTION_COUNT];
    henka_time_state time;
    bool exit_requested;
    bool initialized_callback_ran;
};

henka_result henka_platform_create(const henka_platform_desc* desc, struct henka_platform** out_platform);
void henka_platform_destroy(struct henka_platform* platform);
henka_result henka_platform_poll_events(struct henka_platform* platform, henka_input_state* input, henka_platform_frame_state* out_state);
henka_result henka_platform_set_vsync(struct henka_platform* platform, bool enabled);
bool henka_platform_get_framebuffer_size(struct henka_platform* platform, int* out_width, int* out_height);
bool henka_platform_get_window_size(struct henka_platform* platform, int* out_width, int* out_height);
henka_result henka_platform_set_mouse_capture(struct henka_platform* platform, bool enabled);
char* henka_platform_get_base_path_copy(void);
henka_result henka_platform_create_directory_tree(const char* path);

henka_result henka_renderer_create(struct henka_platform* platform, bool enable_vsync, struct henka_renderer** out_renderer);
void henka_renderer_destroy(struct henka_renderer* renderer);
henka_result henka_renderer_begin_frame(struct henka_renderer* renderer);
void henka_renderer_clear_frame(struct henka_renderer* renderer);
henka_result henka_renderer_draw_scene(struct henka_renderer* renderer, const struct henka_scene* scene);
henka_result henka_renderer_draw_ui(struct henka_renderer* renderer, const struct henka_ui_context* ui_context);
henka_result henka_renderer_end_frame(struct henka_renderer* renderer);
void henka_renderer_resize_viewport(struct henka_renderer* renderer, int width, int height);
void henka_renderer_set_scene_viewport(struct henka_renderer* renderer, henka_viewport viewport);
henka_viewport henka_renderer_get_scene_viewport(const struct henka_renderer* renderer);
henka_result henka_renderer_set_vsync(struct henka_renderer* renderer, bool enabled);
henka_result henka_renderer_set_wireframe(struct henka_renderer* renderer, bool enabled);
henka_result henka_renderer_create_mesh_from_data(
    struct henka_renderer* renderer,
    const henka_vertex* vertices,
    int vertex_count,
    const unsigned int* indices,
    int index_count,
    henka_mesh_primitive primitive,
    struct henka_mesh** out_mesh);
void henka_renderer_destroy_mesh(struct henka_mesh* mesh);
henka_result henka_renderer_create_shader_from_files(
    struct henka_renderer* renderer,
    const char* vertex_path,
    const char* fragment_path,
    struct henka_shader** out_shader);
void henka_renderer_destroy_shader(struct henka_shader* shader);
henka_result henka_renderer_create_texture_from_rgba8(
    struct henka_renderer* renderer,
    int width,
    int height,
    const unsigned char* pixels,
    struct henka_texture** out_texture);
void henka_renderer_destroy_texture(struct henka_texture* texture);

henka_result henka_opengl_renderer_create(struct henka_renderer* renderer, struct henka_platform* platform, bool enable_vsync);
void henka_opengl_renderer_destroy(struct henka_renderer* renderer);
henka_result henka_opengl_renderer_begin_frame(struct henka_renderer* renderer);
void henka_opengl_renderer_clear_frame(struct henka_renderer* renderer);
henka_result henka_opengl_renderer_draw_scene(struct henka_renderer* renderer, const struct henka_scene* scene);
henka_result henka_opengl_renderer_draw_ui(struct henka_renderer* renderer, const struct henka_ui_context* ui_context);
henka_result henka_opengl_renderer_end_frame(struct henka_renderer* renderer);
void henka_opengl_renderer_resize_viewport(struct henka_renderer* renderer, int width, int height);
henka_result henka_opengl_renderer_set_vsync(struct henka_renderer* renderer, bool enabled);
henka_result henka_opengl_renderer_set_wireframe(struct henka_renderer* renderer, bool enabled);
henka_result henka_opengl_renderer_create_mesh_from_data(
    struct henka_renderer* renderer,
    const henka_vertex* vertices,
    int vertex_count,
    const unsigned int* indices,
    int index_count,
    henka_mesh_primitive primitive,
    struct henka_mesh** out_mesh);
void henka_opengl_renderer_destroy_mesh(struct henka_mesh* mesh);
henka_result henka_opengl_renderer_create_shader_from_files(
    struct henka_renderer* renderer,
    const char* vertex_path,
    const char* fragment_path,
    struct henka_shader** out_shader);
void henka_opengl_renderer_destroy_shader(struct henka_shader* shader);
henka_result henka_opengl_renderer_create_texture_from_rgba8(
    struct henka_renderer* renderer,
    int width,
    int height,
    const unsigned char* pixels,
    struct henka_texture** out_texture);
void henka_opengl_renderer_destroy_texture(struct henka_texture* texture);

henka_result henka_asset_manager_create(struct henka_engine* engine, struct henka_asset_manager** out_manager);
void henka_asset_manager_destroy(struct henka_asset_manager* manager);

#endif
