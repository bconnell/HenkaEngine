#ifndef HENKA_ENGINE_H
#define HENKA_ENGINE_H

#include <stdbool.h>
#include <stdint.h>

#include <henka/core.h>
#include <henka/result.h>

typedef struct henka_engine henka_engine;
typedef struct henka_scene henka_scene;
typedef struct henka_asset_manager henka_asset_manager;
typedef struct henka_ui_context henka_ui_context;

#define HENKA_INVALID_WINDOW_ID 0U
#define HENKA_MAX_TOOL_WINDOWS 4U

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

typedef struct henka_engine_diagnostics
{
    double delta_seconds;
    double frame_time_milliseconds;
    double frames_per_second;
    uint64_t frame_index;
    int framebuffer_width;
    int framebuffer_height;
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
 * Creates and owns a new engine instance. The caller becomes responsible for
 * releasing it with henka_engine_destroy.
 */
henka_result henka_engine_create(const henka_engine_config* config, henka_engine** out_engine);
void henka_engine_destroy(henka_engine* engine);
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
