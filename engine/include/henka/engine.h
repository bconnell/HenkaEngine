#ifndef HENKA_ENGINE_H
#define HENKA_ENGINE_H

#include <stdbool.h>
#include <stdint.h>

#include <henka/core.h>
#include <henka/math.h>
#include <henka/result.h>

typedef struct henka_engine henka_engine;
typedef struct henka_scene henka_scene;
typedef struct henka_asset_manager henka_asset_manager;
typedef struct henka_ui_context henka_ui_context;

#define HENKA_INVALID_WINDOW_ID 0U
#define HENKA_MAX_TOOL_WINDOWS 5U

typedef uint32_t henka_window_id;

typedef enum henka_window_event_route
{
    HENKA_WINDOW_EVENT_ROUTE_NONE = 0,
    HENKA_WINDOW_EVENT_ROUTE_MAIN,
    HENKA_WINDOW_EVENT_ROUTE_TOOL,
    HENKA_WINDOW_EVENT_ROUTE_UNKNOWN
} henka_window_event_route;

typedef struct henka_tool_window_desc
{
    const char* title;
    int width;
    int height;
    int minimum_width;
    int minimum_height;
} henka_tool_window_desc;

typedef struct henka_tool_window_state
{
    henka_window_id id;
    uint32_t native_window_id;
    bool open;
    bool focused;
    int width;
    int height;
    henka_vec2 mouse_position;
    bool mouse_left_down;
    bool mouse_left_pressed;
    bool mouse_left_released;
    bool close_requested;
    bool resized;
    char last_event[48];
} henka_tool_window_state;

typedef enum henka_package_mode
{
    HENKA_PACKAGE_MODE_AUTO = 0,
    HENKA_PACKAGE_MODE_DEVELOPMENT,
    HENKA_PACKAGE_MODE_PACKAGED
} henka_package_mode;

typedef enum henka_viewport_shading_mode
{
    HENKA_VIEWPORT_SHADING_WIREFRAME = 0,
    HENKA_VIEWPORT_SHADING_SOLID,
    HENKA_VIEWPORT_SHADING_MATERIAL_PREVIEW,
    HENKA_VIEWPORT_SHADING_RENDERED,
    HENKA_VIEWPORT_SHADING_COUNT
} henka_viewport_shading_mode;

typedef struct henka_scene_view_render_desc
{
    henka_viewport viewport;
    henka_viewport_shading_mode shading_mode;
    bool overlays_visible;
    bool xray_enabled;
} henka_scene_view_render_desc;
typedef struct henka_engine_diagnostics
{
    double delta_seconds;
    double frame_time_milliseconds;
    double frames_per_second;
    uint64_t frame_index;
    int framebuffer_width;
    int framebuffer_height;
    henka_viewport_shading_mode viewport_shading_mode;
    float viewport_exposure;
    bool rendered_hdr_ready;
    bool rendered_shadow_ready;
    bool rendered_bloom_ready;
    bool rendered_ibl_ready;
    bool rendered_temporal_history_ready;
    bool rendered_temporal_history_valid;
    bool rendered_temporal_fallback_active;
    uint32_t rendered_temporal_invalidation_count;
    char rendered_temporal_invalidation_reason[64];
    uint64_t rendered_temporal_resolve_count;
    uint64_t rendered_temporal_fallback_frame_count;
    bool rendered_motion_vectors_ready;
    bool rendered_temporal_jitter_enabled;
    float rendered_temporal_jitter_x;
    float rendered_temporal_jitter_y;
    uint32_t rendered_reflection_probe_enabled_count;
    uint32_t rendered_reflection_probe_captured_count;
    bool rendered_reflection_probe_capture_active;
    uint32_t rendered_reflection_probe_capture_index;
    uint64_t rendered_reflection_probe_capture_generation;
    uint32_t rendered_reflection_probe_capture_failure_count;
    henka_viewport scene_viewport;
    int rendered_hdr_requested_width;
    int rendered_hdr_requested_height;
    int rendered_hdr_allocated_width;
    int rendered_hdr_allocated_height;
    uint64_t rendered_hdr_generation;
    bool rendered_hdr_framebuffer_complete;
    char rendered_hdr_failure[64];
    int rendered_shadow_resolution;
    uint64_t rendered_shadow_generation;
    bool rendered_shadow_framebuffer_complete;
    char rendered_shadow_failure[64];
    int rendered_cascade_shadow_resolution;
    uint64_t rendered_cascade_shadow_generation;
    bool rendered_cascade_shadow_framebuffer_complete;
    char rendered_cascade_shadow_failure[64];
    int rendered_point_shadow_resolution;
    uint64_t rendered_point_shadow_generation;
    bool rendered_point_shadow_framebuffer_complete;
    char rendered_point_shadow_failure[64];
    int rendered_bloom_width;
    int rendered_bloom_height;
    char rendered_bloom_failure[64];
    char rendered_ibl_failure[64];
    uint32_t rendered_scene_draw_calls;
    uint32_t rendered_scene_visible_entities;
    uint32_t rendered_scene_culled_entities;
    uint32_t rendered_scene_budget_dropped_entities;
    uint32_t rendered_scene_lod_entities;
    uint32_t rendered_scene_lod_fallback_entities;
    uint32_t rendered_scene_instanced_draw_calls;
    uint32_t rendered_scene_instanced_entities;
    uint32_t rendered_scene_occlusion_tested_entities;
    uint32_t rendered_scene_occlusion_culled_entities;
    uint32_t rendered_scene_transparent_sort_overflow_entities;
    double rendered_scene_cpu_time_milliseconds;
    double rendered_scene_gpu_time_milliseconds;
    bool rendered_scene_gpu_timing_available;
    uint64_t renderer_tracked_gpu_bytes;
    uint64_t renderer_tracked_gpu_peak_bytes;
    uint64_t renderer_tracked_mesh_bytes;
    uint64_t renderer_tracked_texture_bytes;
    uint64_t renderer_tracked_render_target_bytes;
    uint32_t renderer_tracked_mesh_count;
    uint32_t renderer_tracked_texture_count;
    bool renderer_memory_overflow;
    uint64_t texture_residency_budget_bytes;
    uint64_t texture_residency_resident_bytes;
    uint64_t texture_residency_uploaded_bytes;
    uint64_t texture_residency_evicted_bytes;
    uint64_t texture_residency_failed_bytes;
    uint32_t texture_residency_managed_count;
    uint32_t texture_residency_fallback_count;
    uint32_t texture_residency_queued_request_count;
    uint64_t texture_residency_completed_request_count;
    uint64_t texture_residency_failed_request_count;
    uint64_t texture_residency_cancelled_request_count;
    uint64_t texture_residency_eviction_count;
    uint64_t texture_residency_eviction_failure_count;
    uint32_t texture_residency_pinned_count;
    bool texture_residency_budget_exceeded;
    bool wireframe_enabled;
    bool mouse_captured;
    bool ui_visible;
    henka_package_mode package_mode;
    bool multi_window_available;
    bool main_window_focused;
    unsigned int open_tool_window_count;
    henka_window_event_route last_window_event_route;
    henka_window_id last_tool_window_id;
    bool last_tool_window_close_requested;
    bool last_tool_window_resized;
} henka_engine_diagnostics;

typedef henka_result (*henka_engine_initialize_fn)(henka_engine* engine, void* user_data);
typedef void (*henka_engine_update_fn)(henka_engine* engine, double delta_seconds, void* user_data);
typedef void (*henka_engine_shutdown_fn)(henka_engine* engine, void* user_data);

typedef struct henka_engine_config
{
    const char* application_name;
    int window_width;
    int window_height;
    bool enable_vsync;
    /* When NULL or empty, runtime assets resolve relative to the executable directory. */
    const char* asset_base_path;
    /* When NULL or empty, local user data resolves to a "user" folder beside the executable. */
    const char* user_data_base_path;
    henka_package_mode package_mode;
    henka_engine_initialize_fn on_initialize;
    henka_engine_update_fn on_update;
    henka_engine_shutdown_fn on_shutdown;
    void* user_data;
} henka_engine_config;

/*
 * Creates and owns a new engine instance. Configuration string values are
 * copied during creation. Callback pointers and user_data remain caller-owned.
 * The caller becomes responsible for releasing the instance with
 * henka_engine_destroy.
 */
bool henka_viewport_shading_mode_is_valid(
    henka_viewport_shading_mode mode);
const char* henka_viewport_shading_mode_get_label(
    henka_viewport_shading_mode mode);
const char* henka_viewport_shading_mode_get_setting_value(
    henka_viewport_shading_mode mode);
henka_result henka_viewport_shading_mode_parse(
    const char* value,
    henka_viewport_shading_mode* out_mode);
henka_result henka_engine_create(const henka_engine_config* config, henka_engine** out_engine);
void henka_engine_destroy(henka_engine* engine);

/*
 * Runs one engine lifecycle. Reentrant calls and later repeat calls on the same
 * instance are rejected. Exit requests stop before another update or render.
 */
henka_result henka_engine_run(henka_engine* engine);
void henka_engine_request_exit(henka_engine* engine);
henka_result henka_engine_set_scene(henka_engine* engine, henka_scene* scene);
henka_result henka_engine_set_ui_context(henka_engine* engine, henka_ui_context* ui_context);
henka_result henka_engine_set_vsync(henka_engine* engine, bool enabled);
bool henka_engine_is_vsync_enabled(const henka_engine* engine);
henka_result henka_engine_set_wireframe(henka_engine* engine, bool enabled);
bool henka_engine_is_wireframe_enabled(const henka_engine* engine);
henka_result henka_engine_set_mouse_capture(henka_engine* engine, bool enabled);
bool henka_engine_is_mouse_captured(const henka_engine* engine);
double henka_engine_get_delta_time(const henka_engine* engine);
double henka_engine_get_total_time(const henka_engine* engine);
uint64_t henka_engine_get_frame_index(const henka_engine* engine);
henka_result henka_engine_get_window_size(const henka_engine* engine, int* out_width, int* out_height);
henka_result henka_engine_get_framebuffer_size(const henka_engine* engine, int* out_width, int* out_height);
henka_result henka_engine_set_scene_viewport(henka_engine* engine, henka_viewport viewport);
henka_result henka_engine_get_scene_viewport(const henka_engine* engine, henka_viewport* out_viewport);
henka_result henka_engine_set_viewport_shading_mode(
    henka_engine* engine,
    henka_viewport_shading_mode mode);
henka_viewport_shading_mode henka_engine_get_viewport_shading_mode(
    const henka_engine* engine);
henka_result henka_engine_set_viewport_exposure(henka_engine* engine, float exposure_stops);
float henka_engine_get_viewport_exposure(const henka_engine* engine);
henka_package_mode henka_engine_get_package_mode(const henka_engine* engine);
const char* henka_engine_get_package_mode_label(henka_package_mode package_mode);
henka_result henka_engine_get_diagnostics(const henka_engine* engine, henka_engine_diagnostics* out_diagnostics);
henka_result henka_engine_open_tool_window(
    henka_engine* engine,
    const henka_tool_window_desc* desc,
    henka_ui_context* ui_context,
    henka_window_id* out_window_id);
henka_result henka_engine_close_tool_window(henka_engine* engine, henka_window_id window_id);
henka_result henka_engine_get_tool_window_state(
    const henka_engine* engine,
    henka_window_id window_id,
    henka_tool_window_state* out_state);
const char* henka_window_event_route_to_string(henka_window_event_route route);
const char* henka_engine_get_asset_base_path(const henka_engine* engine);
const char* henka_engine_get_user_data_base_path(const henka_engine* engine);
henka_asset_manager* henka_engine_get_asset_manager(henka_engine* engine);
const henka_asset_manager* henka_engine_get_asset_manager_const(const henka_engine* engine);

#endif
