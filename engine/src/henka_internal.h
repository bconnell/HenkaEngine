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
#include <henka/model.h>
#include <henka/persistence.h>
#include <henka/platform.h>
#include <henka/renderer.h>
#include <henka/result.h>
#include <henka/scene.h>
#include <henka/shader.h>
#include <henka/texture.h>

#if defined(HENKA_WITH_KTX2_TRANSCODER)
typedef enum henka_ktx2_gpu_format
{
    HENKA_KTX2_GPU_FORMAT_RGBA8 = 0,
    HENKA_KTX2_GPU_FORMAT_BC1,
    HENKA_KTX2_GPU_FORMAT_BC3,
    HENKA_KTX2_GPU_FORMAT_BC5,
    HENKA_KTX2_GPU_FORMAT_BC7,
    HENKA_KTX2_GPU_FORMAT_ETC2_RGB,
    HENKA_KTX2_GPU_FORMAT_ETC2_RGBA,
    HENKA_KTX2_GPU_FORMAT_ETC2_RG,
    HENKA_KTX2_GPU_FORMAT_ASTC_4X4
} henka_ktx2_gpu_format;

#define HENKA_KTX2_CAPABILITY_BC1_3 (1U << 0)
#define HENKA_KTX2_CAPABILITY_BC5 (1U << 1)
#define HENKA_KTX2_CAPABILITY_BC7 (1U << 2)
#define HENKA_KTX2_CAPABILITY_ETC2 (1U << 3)
#define HENKA_KTX2_CAPABILITY_ASTC_4X4 (1U << 4)

typedef struct henka_ktx2_upload_level
{
    size_t offset;
    size_t size;
    int width;
    int height;
} henka_ktx2_upload_level;

typedef struct henka_ktx2_upload
{
    unsigned char* data;
    size_t data_size;
    int width;
    int height;
    uint32_t level_count;
    uint32_t total_level_count;
    bool compressed;
    bool is_srgb;
    henka_ktx2_gpu_format format;
    henka_ktx2_upload_level levels[16];
} henka_ktx2_upload;

void henka_ktx2_upload_dispose(henka_ktx2_upload* upload);
henka_result henka_ktx2_prepare_upload(
    const unsigned char* data,
    size_t data_size,
    henka_texture_usage usage,
    henka_texture_color_space color_space,
    uint32_t capabilities,
    henka_ktx2_upload* out_upload);
henka_result henka_ktx2_prepare_upload_with_mip_limit(
    const unsigned char* data,
    size_t data_size,
    henka_texture_usage usage,
    henka_texture_color_space color_space,
    uint32_t capabilities,
    uint32_t max_resident_mips,
    henka_ktx2_upload* out_upload);
henka_result henka_ktx2_decode_rgba8(
    const unsigned char* data,
    size_t data_size,
    unsigned char** out_pixels,
    size_t* out_pixel_size,
    int* out_width,
    int* out_height,
    bool* out_is_srgb);
#endif
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
    henka_vec2 uv1;
    henka_vec4 color;
    bool color_valid;
    henka_vec4 tangent;
    bool tangent_valid;
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

typedef struct henka_viewport_render_policy
{
    bool polygon_wireframe;
    bool use_material_base_color;
    bool sample_material_texture;
    bool use_scene_lighting;
    bool use_preview_lighting;
    bool use_hdr_presentation;
    bool force_unlit;
} henka_viewport_render_policy;
typedef struct henka_platform_frame_state
{
    bool close_requested;
    bool resized;
    int framebuffer_width;
    int framebuffer_height;
} henka_platform_frame_state;

typedef struct henka_platform_diagnostics
{
    bool multi_window_available;
    bool main_window_focused;
    unsigned int open_tool_window_count;
    henka_window_event_route last_event_route;
    henka_window_id last_tool_window_id;
    bool last_tool_window_close_requested;
    bool last_tool_window_resized;
} henka_platform_diagnostics;

typedef struct henka_tool_window_slot
{
    henka_window_id id;
    struct henka_ui_context* ui_context;
} henka_tool_window_slot;

typedef struct henka_scene_entity_record
{
    bool active;
    uint64_t generation;
    bool visible;
    uint32_t flags;
    char* name;
    char* tag;
    henka_transform transform;
    henka_transform previous_transform;
    bool previous_transform_valid;
    henka_mesh* mesh;
    henka_scene_lod_desc lod;
    henka_material material;
    char* material_name;
    bool has_local_bounds;
    henka_bounds local_bounds;
    henka_interaction_desc interaction;
    char* interaction_prompt;
} henka_scene_entity_record;

typedef struct henka_asset_shader_entry
{
    char* vertex_key;
    char* fragment_key;
    char* source_path;
    char* display_name;
    henka_shader* shader;
    henka_shader_contract_type contract_type;
    uint32_t contract_version;
    henka_asset_metadata metadata;
} henka_asset_shader_entry;

typedef struct henka_asset_texture_entry
{
    char* key;
    char* source_path;
    char* display_name;
    henka_texture* texture;
    bool owns_texture;
    henka_texture_descriptor descriptor;
    uint64_t resident_gpu_bytes;
    henka_asset_metadata metadata;
} henka_asset_texture_entry;

typedef struct henka_asset_mesh_entry
{
    char* key;
    char* source_path;
    char* display_name;
    henka_mesh* mesh;
    bool owns_mesh;
    henka_asset_metadata metadata;
} henka_asset_mesh_entry;

struct henka_material_asset
{
    char* key;
    char* source_path;
    char* display_name;
    henka_material material;
    henka_asset_metadata metadata;
    uint64_t revision;
};

struct henka_gltf_scene_asset
{
    char* key;
    char* source_path;
    char* display_name;
    henka_shader* shader;
    henka_model_scene_data data;
    henka_mesh* primitive_meshes[HENKA_MODEL_MAX_SCENE_ITEMS];
    henka_material materials[HENKA_MODEL_MAX_SCENE_ITEMS];
    bool material_ready[HENKA_MODEL_MAX_SCENE_ITEMS];
    henka_asset_metadata metadata;
    uint64_t revision;
};

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
    henka_material_asset** material_entries;
    size_t material_count;
    size_t material_capacity;
    henka_gltf_scene_asset** gltf_scene_entries;
    size_t gltf_scene_count;
    size_t gltf_scene_capacity;
    henka_texture* white_texture;
    henka_texture* error_texture;
    henka_texture* normal_texture;
    henka_texture* metallic_roughness_texture;
    henka_texture* occlusion_texture;
    henka_texture* emissive_texture;
    henka_mesh* fallback_mesh;
    uint64_t texture_residency_budget_bytes;
    uint64_t texture_resident_bytes;
    uint32_t texture_budget_rejection_count;
    henka_texture* texture_residency_request_textures[HENKA_MAX_TEXTURE_RESIDENCY_REQUESTS];
    uint32_t texture_residency_request_mips[HENKA_MAX_TEXTURE_RESIDENCY_REQUESTS];
    size_t texture_residency_request_count;
    uint64_t texture_residency_completed_requests;
    uint64_t texture_residency_failed_requests;
    uint64_t texture_residency_eviction_count;
    uint64_t texture_residency_eviction_failure_count;
};

struct henka_scene
{
    henka_scene_entity_record* entities;
    size_t entity_capacity;
    size_t entity_count;
    henka_camera camera;
    bool has_camera;
    henka_vec3 light_direction;
    henka_vec3 light_color;
    float light_intensity;
    henka_vec3 ambient_color;
    henka_scene_environment_desc environment;
    henka_scene_reflection_probe_desc reflection_probes[HENKA_SCENE_MAX_REFLECTION_PROBES];
    bool reflection_probe_active[HENKA_SCENE_MAX_REFLECTION_PROBES];
    uint64_t render_revision;
    henka_scene_light_desc local_lights[HENKA_SCENE_MAX_LOCAL_LIGHTS];
    bool local_light_active[HENKA_SCENE_MAX_LOCAL_LIGHTS];
    henka_scene_fog_desc fog;
};

struct henka_platform;

struct henka_renderer
{
    struct henka_platform* platform;
    void* backend_state;
    bool vsync_enabled;
    bool mouse_captured;
    bool frame_active;
    bool scene_viewport_custom;
    int framebuffer_width;
    int framebuffer_height;
    henka_viewport scene_viewport;
    henka_scene_view_render_desc scene_view;
    henka_viewport_shading_mode last_non_wireframe_mode;
    float exposure;
};

struct henka_mesh
{
    struct henka_renderer* renderer;
    bool asset_manager_owned;
    henka_mesh_primitive primitive;
    int vertex_count;
    int index_count;
    void* backend_data;
};

struct henka_shader
{
    struct henka_renderer* renderer;
    bool asset_manager_owned;
    void* backend_data;
};

struct henka_texture
{
    struct henka_renderer* renderer;
    bool asset_manager_owned;
    bool owns_backend;
    void* backend_data;
    int width;
    int height;
    int original_channel_count;
    size_t source_byte_size;
    henka_texture_descriptor descriptor;
    henka_texture_alpha_mode alpha_mode;
    henka_texture_source_class source_class;
    henka_texture_failure_category last_failure;
    bool fallback_alias;
    bool gpu_compressed;
    uint64_t resident_gpu_bytes;
    uint32_t resident_mip_count;
    uint32_t mip_count;
    uint64_t content_revision;
};

typedef enum henka_engine_run_state
{
    HENKA_ENGINE_RUN_STATE_CREATED = 0,
    HENKA_ENGINE_RUN_STATE_INITIALIZING,
    HENKA_ENGINE_RUN_STATE_RUNNING,
    HENKA_ENGINE_RUN_STATE_STOPPED
} henka_engine_run_state;

struct henka_engine
{
    henka_engine_config config;
    struct henka_platform* platform;
    struct henka_renderer* renderer;
    struct henka_asset_manager* asset_manager;
    struct henka_scene* active_scene;
    struct henka_ui_context* active_ui;
    char* application_name;
    char* asset_base_path;
    char* user_data_base_path;
    henka_package_mode package_mode;
    henka_input_state input;
    henka_tool_window_slot tool_windows[HENKA_MAX_TOOL_WINDOWS];
    henka_key action_key_bindings[HENKA_INPUT_ACTION_COUNT][HENKA_MAX_ACTION_KEY_BINDINGS];
    henka_mouse_button action_mouse_bindings[HENKA_INPUT_ACTION_COUNT][HENKA_MAX_ACTION_MOUSE_BINDINGS];
    henka_time_state time;
    henka_engine_run_state run_state;
    bool exit_requested;
    bool shutdown_callback_pending;
    bool destroying;
};

henka_result henka_engine_begin_run_transition(struct henka_engine* engine);
void henka_engine_finish_run_transition(struct henka_engine* engine);
bool henka_engine_should_continue_run(const struct henka_engine* engine);

void henka_platform_release_input_on_focus_loss(henka_input_state* input);
bool henka_platform_choose_tool_window_id(
    henka_window_id next_candidate,
    const henka_window_id* occupied_ids,
    size_t occupied_count,
    henka_window_id* out_window_id,
    henka_window_id* out_next_candidate);
henka_result henka_platform_create(const henka_platform_desc* desc, struct henka_platform** out_platform);
void henka_platform_destroy(struct henka_platform* platform);
void henka_platform_set_multi_window_available(
    struct henka_platform* platform,
    bool available);
henka_result henka_platform_poll_events(struct henka_platform* platform, henka_input_state* input, henka_platform_frame_state* out_state);
henka_result henka_platform_create_tool_window(
    struct henka_platform* platform,
    const henka_tool_window_desc* desc,
    henka_window_id* out_window_id);
void henka_platform_destroy_tool_window(struct henka_platform* platform, henka_window_id window_id);
bool henka_platform_get_tool_window_state(
    const struct henka_platform* platform,
    henka_window_id window_id,
    henka_tool_window_state* out_state);
void henka_platform_get_diagnostics(const struct henka_platform* platform, henka_platform_diagnostics* out_diagnostics);
void* henka_platform_get_native_tool_window(struct henka_platform* platform, henka_window_id window_id);
henka_result henka_platform_set_vsync(struct henka_platform* platform, bool enabled);
bool henka_platform_get_framebuffer_size(struct henka_platform* platform, int* out_width, int* out_height);
bool henka_platform_get_window_size(struct henka_platform* platform, int* out_width, int* out_height);
henka_result henka_platform_set_mouse_capture(struct henka_platform* platform, bool enabled);
char* henka_platform_get_base_path_copy(void);
henka_result henka_platform_create_directory_tree(const char* path);

henka_result henka_renderer_create(struct henka_platform* platform, bool enable_vsync, struct henka_renderer** out_renderer);
void henka_renderer_destroy(struct henka_renderer* renderer);
henka_result henka_renderer_begin_frame(struct henka_renderer* renderer);
henka_result henka_renderer_abort_frame(struct henka_renderer* renderer);
void henka_renderer_clear_frame(struct henka_renderer* renderer);
henka_result henka_renderer_draw_scene(struct henka_renderer* renderer, const struct henka_scene* scene);
henka_result henka_renderer_draw_ui(struct henka_renderer* renderer, const struct henka_ui_context* ui_context);
henka_result henka_renderer_end_frame(struct henka_renderer* renderer);
henka_result henka_renderer_create_tool_window_target(struct henka_renderer* renderer, henka_window_id window_id);
void henka_renderer_destroy_tool_window_target(struct henka_renderer* renderer, henka_window_id window_id);
henka_result henka_renderer_draw_tool_window_ui(
    struct henka_renderer* renderer,
    henka_window_id window_id,
    const struct henka_ui_context* ui_context);
void henka_renderer_resize_viewport(struct henka_renderer* renderer, int width, int height);
void henka_renderer_set_scene_viewport(struct henka_renderer* renderer, henka_viewport viewport);
henka_viewport henka_renderer_get_scene_viewport(const struct henka_renderer* renderer);
henka_result henka_renderer_set_vsync(struct henka_renderer* renderer, bool enabled);
henka_result henka_renderer_set_viewport_exposure(struct henka_renderer* renderer, float exposure_stops);
float henka_renderer_get_viewport_exposure(const struct henka_renderer* renderer);
bool henka_renderer_is_hdr_ready(const struct henka_renderer* renderer);
bool henka_renderer_is_shadow_ready(const struct henka_renderer* renderer);
henka_result henka_viewport_render_policy_resolve(
    henka_viewport_shading_mode mode,
    henka_viewport_render_policy* out_policy);
henka_result henka_renderer_set_viewport_shading_mode(
    struct henka_renderer* renderer,
    henka_viewport_shading_mode mode);
henka_viewport_shading_mode henka_renderer_get_viewport_shading_mode(
    const struct henka_renderer* renderer);
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
henka_result henka_renderer_create_shader_from_files_with_contract(
    struct henka_renderer* renderer,
    const char* vertex_path,
    const char* fragment_path,
    const henka_shader_contract_desc* contract,
    struct henka_shader** out_shader);
void henka_renderer_destroy_shader(struct henka_shader* shader);
henka_result henka_texture_create_from_file_with_descriptor_and_mip_limit(
    henka_engine* engine,
    const char* path,
    const henka_texture_descriptor* descriptor,
    uint32_t max_resident_mips,
    henka_texture** out_texture);
henka_result henka_renderer_create_texture_from_rgba8(
    struct henka_renderer* renderer,
    int width,
    int height,
    const unsigned char* pixels,
    struct henka_texture** out_texture);
henka_result henka_renderer_create_texture_from_rgba8_with_descriptor(
    struct henka_renderer* renderer,
    int width,
    int height,
    const unsigned char* pixels,
    const henka_texture_descriptor* descriptor,
    struct henka_texture** out_texture);
henka_result henka_renderer_create_texture_from_rgba32f_with_descriptor(
    struct henka_renderer* renderer,
    int width,
    int height,
    const float* pixels,
    const henka_texture_descriptor* descriptor,
    struct henka_texture** out_texture);
henka_result henka_renderer_create_texture_from_ktx2_memory(
    struct henka_renderer* renderer,
    const unsigned char* data,
    size_t data_size,
    const henka_texture_descriptor* descriptor,
    struct henka_texture** out_texture);
henka_result henka_renderer_create_texture_from_ktx2_memory_with_mip_limit(
    struct henka_renderer* renderer,
    const unsigned char* data,
    size_t data_size,
    const henka_texture_descriptor* descriptor,
    uint32_t max_resident_mips,
    struct henka_texture** out_texture);
void henka_renderer_destroy_texture(struct henka_texture* texture);

henka_result henka_opengl_renderer_create(struct henka_renderer* renderer, struct henka_platform* platform, bool enable_vsync);
void henka_opengl_renderer_destroy(struct henka_renderer* renderer);
henka_result henka_opengl_renderer_begin_frame(struct henka_renderer* renderer);
henka_result henka_opengl_renderer_abort_frame(struct henka_renderer* renderer);
void henka_opengl_renderer_clear_frame(struct henka_renderer* renderer);
henka_result henka_opengl_renderer_draw_scene(struct henka_renderer* renderer, const struct henka_scene* scene);
henka_result henka_opengl_renderer_draw_ui(struct henka_renderer* renderer, const struct henka_ui_context* ui_context);
henka_result henka_opengl_renderer_end_frame(struct henka_renderer* renderer);
henka_result henka_opengl_renderer_create_tool_window_target(struct henka_renderer* renderer, henka_window_id window_id);
void henka_opengl_renderer_destroy_tool_window_target(struct henka_renderer* renderer, henka_window_id window_id);
henka_result henka_opengl_renderer_draw_tool_window_ui(
    struct henka_renderer* renderer,
    henka_window_id window_id,
    const struct henka_ui_context* ui_context);
void henka_opengl_renderer_resize_viewport(struct henka_renderer* renderer, int width, int height);
void henka_opengl_renderer_sync_scene_target(struct henka_renderer* renderer);
henka_result henka_opengl_renderer_set_vsync(struct henka_renderer* renderer, bool enabled);
bool henka_opengl_renderer_is_hdr_ready(const struct henka_renderer* renderer);
bool henka_opengl_renderer_is_shadow_ready(const struct henka_renderer* renderer);
void henka_opengl_renderer_get_hdr_diagnostics(
    const struct henka_renderer* renderer,
    int* out_requested_width,
    int* out_requested_height,
    int* out_allocated_width,
    int* out_allocated_height,
    uint64_t* out_generation,
    bool* out_complete,
    char* out_failure,
    size_t failure_capacity);
void henka_opengl_renderer_get_shadow_diagnostics(
    const struct henka_renderer* renderer,
    int* out_resolution,
    uint64_t* out_generation,
    bool* out_complete,
    char* out_failure,
    size_t failure_capacity);
void henka_opengl_renderer_get_cascade_shadow_diagnostics(
    const struct henka_renderer* renderer,
    int* out_resolution,
    uint64_t* out_generation,
    bool* out_complete,
    char* out_failure,
    size_t failure_capacity);
void henka_opengl_renderer_get_point_shadow_diagnostics(
    const struct henka_renderer* renderer,
    int* out_resolution,
    uint64_t* out_generation,
    bool* out_complete,
    char* out_failure,
    size_t failure_capacity);
void henka_opengl_renderer_get_bloom_diagnostics(
    const struct henka_renderer* renderer,
    int* out_width,
    int* out_height,
    bool* out_ready,
    char* out_failure,
    size_t failure_capacity);
void henka_opengl_renderer_get_ibl_diagnostics(
    const struct henka_renderer* renderer,
    bool* out_ready,
    char* out_failure,
    size_t failure_capacity);
void henka_opengl_renderer_get_temporal_diagnostics(
    const struct henka_renderer* renderer,
    bool* out_history_ready,
    bool* out_history_valid,
    bool* out_motion_vectors_ready);
void henka_opengl_renderer_get_reflection_probe_diagnostics(
    const struct henka_renderer* renderer,
    uint32_t* out_enabled_count,
    uint32_t* out_captured_count,
    bool* out_capture_active,
    uint32_t* out_capture_index,
    uint64_t* out_capture_generation,
    uint32_t* out_capture_failure_count);
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
    uint32_t* out_transparent_sort_overflow_entities,
    double* out_cpu_time_milliseconds,
    double* out_gpu_time_milliseconds,
    bool* out_gpu_timing_available);
void henka_opengl_renderer_get_memory_diagnostics(
    const struct henka_renderer* renderer,
    uint64_t* out_gpu_bytes,
    uint64_t* out_gpu_peak_bytes,
    uint64_t* out_mesh_bytes,
    uint64_t* out_texture_bytes,
    uint64_t* out_render_target_bytes,
    uint32_t* out_mesh_count,
    uint32_t* out_texture_count,
    bool* out_overflow);
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
henka_result henka_opengl_renderer_create_shader_from_files_with_contract(
    struct henka_renderer* renderer,
    const char* vertex_path,
    const char* fragment_path,
    const henka_shader_contract_desc* contract,
    struct henka_shader** out_shader);
void henka_opengl_renderer_destroy_shader(struct henka_shader* shader);
henka_result henka_opengl_renderer_create_texture_from_rgba8(
    struct henka_renderer* renderer,
    int width,
    int height,
    const unsigned char* pixels,
    struct henka_texture** out_texture);
henka_result henka_opengl_renderer_create_texture_from_rgba8_with_descriptor(
    struct henka_renderer* renderer,
    int width,
    int height,
    const unsigned char* pixels,
    const henka_texture_descriptor* descriptor,
    struct henka_texture** out_texture);
henka_result henka_opengl_renderer_create_texture_from_rgba32f_with_descriptor(
    struct henka_renderer* renderer,
    int width,
    int height,
    const float* pixels,
    const henka_texture_descriptor* descriptor,
    struct henka_texture** out_texture);
henka_result henka_opengl_renderer_create_texture_from_ktx2_memory(
    struct henka_renderer* renderer,
    const unsigned char* data,
    size_t data_size,
    const henka_texture_descriptor* descriptor,
    struct henka_texture** out_texture);
henka_result henka_opengl_renderer_create_texture_from_ktx2_memory_with_mip_limit(
    struct henka_renderer* renderer,
    const unsigned char* data,
    size_t data_size,
    const henka_texture_descriptor* descriptor,
    uint32_t max_resident_mips,
    struct henka_texture** out_texture);
void henka_opengl_renderer_destroy_texture(struct henka_texture* texture);

char* henka_asset_copy_display_name(const char* path);
henka_result henka_assets_make_canonical_key(
    const char* path,
    char** out_key);
void henka_mesh_destroy_owned(henka_mesh* mesh);
void henka_shader_destroy_owned(henka_shader* shader);
void henka_texture_destroy_owned(henka_texture* texture);
henka_result henka_texture_create_borrowed_alias(
    const henka_texture* source,
    henka_texture** out_texture);
henka_result henka_texture_adopt_owned_payload(
    henka_texture* target,
    henka_texture* replacement);
henka_result henka_texture_replace_owned_payload(
    henka_texture* target,
    henka_texture* replacement);
henka_result henka_asset_manager_create(struct henka_engine* engine, struct henka_asset_manager** out_manager);
void henka_asset_manager_destroy(struct henka_asset_manager* manager);

#endif
