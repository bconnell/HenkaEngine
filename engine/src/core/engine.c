#include "henka_internal.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include <henka/log.h>
#include <henka/memory.h>

static char* henka_duplicate_string(const char* value)
{
    char* copy;
    size_t length;

    if (value == NULL)
    {
        return NULL;
    }

    length = strlen(value);
    copy = henka_malloc(length + 1U);
    if (copy == NULL)
    {
        return NULL;
    }

    memcpy(copy, value, length + 1U);
    return copy;
}

static bool henka_file_exists(const char* path)
{
    FILE* file;

    if (path == NULL || path[0] == '\0')
    {
        return false;
    }

    file = NULL;
    if (fopen_s(&file, path, "r") != 0 || file == NULL)
    {
        return false;
    }

    fclose(file);
    return true;
}

static bool henka_action_name_equals(const char* left, const char* right)
{
    if (left == NULL || right == NULL)
    {
        return false;
    }

    while (*left != '\0' || *right != '\0')
    {
        while (*left == ' ' || *left == '_' || *left == '-')
        {
            ++left;
        }

        while (*right == ' ' || *right == '_' || *right == '-')
        {
            ++right;
        }

        if (tolower((unsigned char)*left) != tolower((unsigned char)*right))
        {
            return false;
        }

        if (*left == '\0' || *right == '\0')
        {
            return *left == *right;
        }

        ++left;
        ++right;
    }

    return true;
}

static char* henka_engine_resolve_base_path(const char* configured_path, const char* default_suffix)
{
    char* resolved_path;

    if (configured_path != NULL && configured_path[0] != '\0')
    {
        return henka_duplicate_string(configured_path);
    }

    resolved_path = henka_platform_get_base_path_copy();
    if (resolved_path == NULL)
    {
        return NULL;
    }

    if (default_suffix == NULL || default_suffix[0] == '\0')
    {
        return resolved_path;
    }

    {
        char* joined_path;
        henka_result result;

        result = henka_path_resolve(resolved_path, default_suffix, &joined_path);
        henka_free(resolved_path);
        if (result != HENKA_SUCCESS)
        {
            return NULL;
        }

        return joined_path;
    }
}

static bool henka_engine_config_is_valid(const henka_engine_config* config)
{
    if (config == NULL)
    {
        return false;
    }

    if (config->application_name == NULL || config->application_name[0] == '\0')
    {
        return false;
    }

    if (config->window_width <= 0 || config->window_height <= 0)
    {
        return false;
    }

    return true;
}

static const char* henka_input_action_default_name(henka_input_action action)
{
    switch (action)
    {
        case HENKA_INPUT_ACTION_MOVE_FORWARD:
            return "Move Forward";
        case HENKA_INPUT_ACTION_MOVE_BACK:
            return "Move Back";
        case HENKA_INPUT_ACTION_MOVE_LEFT:
            return "Move Left";
        case HENKA_INPUT_ACTION_MOVE_RIGHT:
            return "Move Right";
        case HENKA_INPUT_ACTION_MOVE_UP:
            return "Move Up";
        case HENKA_INPUT_ACTION_MOVE_DOWN:
            return "Move Down";
        case HENKA_INPUT_ACTION_INTERACT:
            return "Interact";
        case HENKA_INPUT_ACTION_OPEN_PANELS:
            return "Open Panels";
        case HENKA_INPUT_ACTION_CHANGE_LAYOUT:
            return "Change Layout";
        case HENKA_INPUT_ACTION_TOGGLE_MOUSE_CAPTURE:
            return "Toggle Mouse Capture";
        case HENKA_INPUT_ACTION_UNKNOWN:
        default:
            return "Unknown";
    }
}

static void henka_engine_initialize_action_bindings(henka_engine* engine)
{
    size_t index;

    for (index = 0; index < HENKA_INPUT_ACTION_COUNT; ++index)
    {
        engine->action_key_bindings[index] = HENKA_KEY_UNKNOWN;
        engine->action_mouse_bindings[index] = HENKA_MOUSE_BUTTON_UNKNOWN;
    }

    engine->action_key_bindings[HENKA_INPUT_ACTION_MOVE_FORWARD] = HENKA_KEY_W;
    engine->action_key_bindings[HENKA_INPUT_ACTION_MOVE_BACK] = HENKA_KEY_S;
    engine->action_key_bindings[HENKA_INPUT_ACTION_MOVE_LEFT] = HENKA_KEY_A;
    engine->action_key_bindings[HENKA_INPUT_ACTION_MOVE_RIGHT] = HENKA_KEY_D;
    engine->action_key_bindings[HENKA_INPUT_ACTION_MOVE_UP] = HENKA_KEY_E;
    engine->action_key_bindings[HENKA_INPUT_ACTION_MOVE_DOWN] = HENKA_KEY_Q;
    engine->action_key_bindings[HENKA_INPUT_ACTION_INTERACT] = HENKA_KEY_UNKNOWN;
    engine->action_key_bindings[HENKA_INPUT_ACTION_OPEN_PANELS] = HENKA_KEY_F4;
    engine->action_key_bindings[HENKA_INPUT_ACTION_CHANGE_LAYOUT] = HENKA_KEY_F5;
    engine->action_key_bindings[HENKA_INPUT_ACTION_TOGGLE_MOUSE_CAPTURE] = HENKA_KEY_TAB;
    engine->action_mouse_bindings[HENKA_INPUT_ACTION_TOGGLE_MOUSE_CAPTURE] = HENKA_MOUSE_BUTTON_RIGHT;
}

static henka_package_mode henka_engine_resolve_package_mode(const henka_engine* engine)
{
    char* marker_path;
    henka_package_mode package_mode;

    if (engine == NULL)
    {
        return HENKA_PACKAGE_MODE_DEVELOPMENT;
    }

    if (engine->config.package_mode != HENKA_PACKAGE_MODE_AUTO)
    {
        return engine->config.package_mode;
    }

    marker_path = NULL;
    package_mode = HENKA_PACKAGE_MODE_DEVELOPMENT;
    if (henka_path_resolve(engine->asset_base_path, "PACKAGE_INFO.txt", &marker_path) == HENKA_SUCCESS)
    {
        if (henka_file_exists(marker_path))
        {
            package_mode = HENKA_PACKAGE_MODE_PACKAGED;
        }

        henka_free(marker_path);
    }

    return package_mode;
}

static void henka_input_reset_frame_state(henka_input_state* input)
{
    size_t index;

    for (index = 0; index < HENKA_KEY_COUNT; ++index)
    {
        input->keys_pressed[index] = false;
        input->keys_released[index] = false;
    }

    for (index = 0; index < HENKA_MOUSE_BUTTON_COUNT; ++index)
    {
        input->mouse_buttons_pressed[index] = false;
        input->mouse_buttons_released[index] = false;
    }

    input->mouse_delta.x = 0.0f;
    input->mouse_delta.y = 0.0f;
    input->mouse_wheel_delta.x = 0.0f;
    input->mouse_wheel_delta.y = 0.0f;
    input->close_requested = false;
}

static void henka_engine_handle_resize(henka_engine* engine, const henka_platform_frame_state* frame_state)
{
    if (engine == NULL || frame_state == NULL || !frame_state->resized)
    {
        return;
    }

    henka_renderer_resize_viewport(engine->renderer, frame_state->framebuffer_width, frame_state->framebuffer_height);

    if (engine->active_scene != NULL && engine->active_scene->has_camera && frame_state->framebuffer_height > 0)
    {
        henka_viewport viewport;

        viewport = henka_renderer_get_scene_viewport(engine->renderer);
        henka_camera_set_aspect_ratio(
            &engine->active_scene->camera,
            henka_viewport_get_aspect_ratio(viewport));
    }
}

static henka_tool_window_slot* henka_engine_find_tool_window_slot(henka_engine* engine, henka_window_id window_id)
{
    size_t index;

    if (engine == NULL || window_id == HENKA_INVALID_WINDOW_ID)
    {
        return NULL;
    }

    for (index = 0U; index < HENKA_MAX_TOOL_WINDOWS; ++index)
    {
        if (engine->tool_windows[index].id == window_id)
        {
            return &engine->tool_windows[index];
        }
    }

    return NULL;
}

static void henka_engine_close_requested_tool_windows(henka_engine* engine)
{
    size_t index;

    if (engine == NULL)
    {
        return;
    }

    for (index = 0U; index < HENKA_MAX_TOOL_WINDOWS; ++index)
    {
        henka_tool_window_state state;

        if (engine->tool_windows[index].id == HENKA_INVALID_WINDOW_ID ||
            !henka_platform_get_tool_window_state(engine->platform, engine->tool_windows[index].id, &state) ||
            !state.close_requested)
        {
            continue;
        }

        henka_engine_close_tool_window(engine, engine->tool_windows[index].id);
    }
}

henka_result henka_engine_create(const henka_engine_config* config, henka_engine** out_engine)
{
    henka_engine* engine;
    henka_platform_desc platform_desc;
    henka_result result;
    int framebuffer_height;
    int framebuffer_width;

    if (!henka_engine_config_is_valid(config) || out_engine == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    *out_engine = NULL;

    engine = henka_calloc(1U, sizeof(*engine));
    if (engine == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }

    engine->config = *config;
    henka_engine_initialize_action_bindings(engine);

    HENKA_LOG_INFO("creating engine for '%s' (%dx%d, vsync=%s)",
        engine->config.application_name,
        engine->config.window_width,
        engine->config.window_height,
        engine->config.enable_vsync ? "on" : "off");

    platform_desc.application_name = engine->config.application_name;
    platform_desc.window_width = engine->config.window_width;
    platform_desc.window_height = engine->config.window_height;
    platform_desc.enable_vsync = engine->config.enable_vsync;

    result = henka_platform_create(&platform_desc, &engine->platform);
    if (result != HENKA_SUCCESS)
    {
        HENKA_LOG_ERROR("platform initialization failed: %s", henka_result_to_string(result));
        henka_free(engine);
        return result;
    }

    engine->asset_base_path = henka_engine_resolve_base_path(engine->config.asset_base_path, NULL);

    if (engine->asset_base_path == NULL)
    {
        HENKA_LOG_ERROR("asset base path initialization failed");
        henka_platform_destroy(engine->platform);
        henka_free(engine);
        return HENKA_ERROR_OUT_OF_MEMORY;
    }

    engine->user_data_base_path = henka_engine_resolve_base_path(engine->config.user_data_base_path, "user");
    if (engine->user_data_base_path == NULL)
    {
        HENKA_LOG_ERROR("user data path initialization failed");
        henka_free(engine->asset_base_path);
        henka_platform_destroy(engine->platform);
        henka_free(engine);
        return HENKA_ERROR_OUT_OF_MEMORY;
    }

    result = henka_renderer_create(engine->platform, engine->config.enable_vsync, &engine->renderer);
    if (result != HENKA_SUCCESS)
    {
        HENKA_LOG_ERROR("renderer initialization failed: %s", henka_result_to_string(result));
        henka_free(engine->user_data_base_path);
        henka_free(engine->asset_base_path);
        henka_platform_destroy(engine->platform);
        henka_free(engine);
        return result;
    }

    result = henka_asset_manager_create(engine, &engine->asset_manager);
    if (result != HENKA_SUCCESS)
    {
        HENKA_LOG_ERROR("asset manager initialization failed: %s", henka_result_to_string(result));
        henka_renderer_destroy(engine->renderer);
        henka_free(engine->user_data_base_path);
        henka_free(engine->asset_base_path);
        henka_platform_destroy(engine->platform);
        henka_free(engine);
        return result;
    }

    henka_time_reset(&engine->time);

    if (henka_platform_get_framebuffer_size(engine->platform, &framebuffer_width, &framebuffer_height))
    {
        engine->renderer->framebuffer_width = framebuffer_width;
        engine->renderer->framebuffer_height = framebuffer_height;
        engine->renderer->scene_viewport = (henka_viewport){0, 0, framebuffer_width, framebuffer_height};
    }

    engine->package_mode = henka_engine_resolve_package_mode(engine);

    HENKA_LOG_INFO("engine startup complete");

    *out_engine = engine;
    return HENKA_SUCCESS;
}

void henka_engine_destroy(henka_engine* engine)
{
    size_t index;

    if (engine == NULL)
    {
        return;
    }

    HENKA_LOG_INFO("shutting down engine");

    if (engine->initialized_callback_ran && engine->config.on_shutdown != NULL)
    {
        engine->config.on_shutdown(engine, engine->config.user_data);
    }

    for (index = 0U; index < HENKA_MAX_TOOL_WINDOWS; ++index)
    {
        if (engine->tool_windows[index].id != HENKA_INVALID_WINDOW_ID)
        {
            henka_engine_close_tool_window(engine, engine->tool_windows[index].id);
        }
    }

    henka_asset_manager_destroy(engine->asset_manager);
    henka_renderer_destroy(engine->renderer);
    henka_platform_destroy(engine->platform);
    henka_free(engine->user_data_base_path);
    henka_free(engine->asset_base_path);
    henka_free(engine);
    henka_memory_report_leaks();
}

henka_result henka_engine_run(henka_engine* engine)
{
    henka_platform_frame_state frame_state;
    henka_result result;

    if (engine == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    if (engine->config.on_initialize != NULL)
    {
        result = engine->config.on_initialize(engine, engine->config.user_data);
        if (result != HENKA_SUCCESS)
        {
            HENKA_LOG_ERROR("engine initialize callback failed: %s", henka_result_to_string(result));
            return result;
        }

        engine->initialized_callback_ran = true;
    }

    HENKA_LOG_INFO("entering engine run loop");

    while (!engine->exit_requested)
    {
        henka_time_tick(&engine->time);
        henka_input_reset_frame_state(&engine->input);

        result = henka_platform_poll_events(engine->platform, &engine->input, &frame_state);
        if (result != HENKA_SUCCESS)
        {
            HENKA_LOG_ERROR("platform event polling failed: %s", henka_result_to_string(result));
            return result;
        }

        if (frame_state.close_requested)
        {
            henka_engine_request_exit(engine);
        }

        henka_engine_close_requested_tool_windows(engine);

        if (henka_input_was_key_pressed(engine, HENKA_KEY_ESCAPE))
        {
            if (engine->active_ui != NULL && henka_ui_is_visible(engine->active_ui))
            {
                henka_ui_set_visible(engine->active_ui, false);
                henka_engine_set_mouse_capture(engine, false);
            }
            else if (henka_engine_is_mouse_captured(engine))
            {
                henka_engine_set_mouse_capture(engine, false);
            }
            else
            {
                henka_engine_request_exit(engine);
            }
        }

        henka_engine_handle_resize(engine, &frame_state);

        if (engine->config.on_update != NULL)
        {
            engine->config.on_update(engine, engine->time.delta_seconds, engine->config.user_data);
        }

        result = henka_renderer_begin_frame(engine->renderer);
        if (result != HENKA_SUCCESS)
        {
            HENKA_LOG_ERROR("renderer begin frame failed: %s", henka_result_to_string(result));
            return result;
        }

        henka_renderer_clear_frame(engine->renderer);

        if (engine->active_scene != NULL)
        {
            result = henka_renderer_draw_scene(engine->renderer, engine->active_scene);
            if (result != HENKA_SUCCESS)
            {
                HENKA_LOG_ERROR("renderer draw scene failed: %s", henka_result_to_string(result));
                return result;
            }
        }

        if (engine->active_ui != NULL)
        {
            result = henka_renderer_draw_ui(engine->renderer, engine->active_ui);
            if (result != HENKA_SUCCESS)
            {
                HENKA_LOG_ERROR("renderer draw ui failed: %s", henka_result_to_string(result));
                return result;
            }
        }

        result = henka_renderer_end_frame(engine->renderer);
        if (result != HENKA_SUCCESS)
        {
            HENKA_LOG_ERROR("renderer end frame failed: %s", henka_result_to_string(result));
            return result;
        }

        {
            size_t index;
            for (index = 0U; index < HENKA_MAX_TOOL_WINDOWS; ++index)
            {
                if (engine->tool_windows[index].id == HENKA_INVALID_WINDOW_ID ||
                    engine->tool_windows[index].ui_context == NULL)
                {
                    continue;
                }

                result = henka_renderer_draw_tool_window_ui(
                    engine->renderer,
                    engine->tool_windows[index].id,
                    engine->tool_windows[index].ui_context);
                if (result != HENKA_SUCCESS)
                {
                    HENKA_LOG_ERROR("renderer draw tool window ui failed: %s", henka_result_to_string(result));
                    return result;
                }
            }
        }
    }

    HENKA_LOG_INFO("leaving engine run loop");
    return HENKA_SUCCESS;
}

void henka_engine_request_exit(henka_engine* engine)
{
    if (engine != NULL)
    {
        engine->exit_requested = true;
    }
}

henka_result henka_engine_set_scene(henka_engine* engine, henka_scene* scene)
{
    if (engine == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    engine->active_scene = scene;
    return HENKA_SUCCESS;
}

henka_result henka_engine_set_ui_context(henka_engine* engine, henka_ui_context* ui_context)
{
    if (engine == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    engine->active_ui = ui_context;
    return HENKA_SUCCESS;
}

henka_result henka_engine_open_tool_window(
    henka_engine* engine,
    const henka_tool_window_desc* desc,
    henka_ui_context* ui_context,
    henka_window_id* out_window_id)
{
    henka_window_id window_id;
    size_t index;
    henka_result result;

    if (engine == NULL || desc == NULL || ui_context == NULL || out_window_id == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    *out_window_id = HENKA_INVALID_WINDOW_ID;
    for (index = 0U; index < HENKA_MAX_TOOL_WINDOWS; ++index)
    {
        if (engine->tool_windows[index].id == HENKA_INVALID_WINDOW_ID)
        {
            break;
        }
    }
    if (index >= HENKA_MAX_TOOL_WINDOWS)
    {
        return HENKA_ERROR_PLATFORM;
    }

    result = henka_platform_create_tool_window(engine->platform, desc, &window_id);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    result = henka_renderer_create_tool_window_target(engine->renderer, window_id);
    if (result != HENKA_SUCCESS)
    {
        henka_platform_destroy_tool_window(engine->platform, window_id);
        return result;
    }

    engine->tool_windows[index].id = window_id;
    engine->tool_windows[index].ui_context = ui_context;
    *out_window_id = window_id;
    return HENKA_SUCCESS;
}

henka_result henka_engine_close_tool_window(henka_engine* engine, henka_window_id window_id)
{
    henka_tool_window_slot* slot;

    if (engine == NULL || window_id == HENKA_INVALID_WINDOW_ID)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    slot = henka_engine_find_tool_window_slot(engine, window_id);
    if (slot == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    henka_renderer_destroy_tool_window_target(engine->renderer, window_id);
    henka_platform_destroy_tool_window(engine->platform, window_id);
    slot->id = HENKA_INVALID_WINDOW_ID;
    slot->ui_context = NULL;
    return HENKA_SUCCESS;
}

henka_result henka_engine_get_tool_window_state(
    const henka_engine* engine,
    henka_window_id window_id,
    henka_tool_window_state* out_state)
{
    if (engine == NULL || out_state == NULL || window_id == HENKA_INVALID_WINDOW_ID)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    if (!henka_platform_get_tool_window_state(engine->platform, window_id, out_state))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    return HENKA_SUCCESS;
}

const char* henka_window_event_route_to_string(henka_window_event_route route)
{
    switch (route)
    {
        case HENKA_WINDOW_EVENT_ROUTE_MAIN:
            return "Main";
        case HENKA_WINDOW_EVENT_ROUTE_TOOL:
            return "Tool";
        case HENKA_WINDOW_EVENT_ROUTE_UNKNOWN:
            return "Unknown";
        case HENKA_WINDOW_EVENT_ROUTE_NONE:
        default:
            return "None";
    }
}

henka_result henka_engine_set_vsync(henka_engine* engine, bool enabled)
{
    if (engine == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    return henka_renderer_set_vsync(engine->renderer, enabled);
}

bool henka_engine_is_vsync_enabled(const henka_engine* engine)
{
    if (engine == NULL || engine->renderer == NULL)
    {
        return false;
    }

    return engine->renderer->vsync_enabled;
}

henka_result henka_engine_set_wireframe(henka_engine* engine, bool enabled)
{
    if (engine == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    return henka_renderer_set_wireframe(engine->renderer, enabled);
}

bool henka_engine_is_wireframe_enabled(const henka_engine* engine)
{
    if (engine == NULL || engine->renderer == NULL)
    {
        return false;
    }

    return engine->renderer->wireframe_enabled;
}

henka_result henka_engine_set_mouse_capture(henka_engine* engine, bool enabled)
{
    henka_result result;

    if (engine == NULL || engine->renderer == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    result = henka_platform_set_mouse_capture(engine->platform, enabled);
    if (result == HENKA_SUCCESS)
    {
        engine->renderer->mouse_captured = enabled;
    }

    return result;
}

bool henka_engine_is_mouse_captured(const henka_engine* engine)
{
    if (engine == NULL || engine->renderer == NULL)
    {
        return false;
    }

    return engine->renderer->mouse_captured;
}

double henka_engine_get_delta_time(const henka_engine* engine)
{
    if (engine == NULL)
    {
        return 0.0;
    }

    return engine->time.delta_seconds;
}

double henka_engine_get_total_time(const henka_engine* engine)
{
    if (engine == NULL)
    {
        return 0.0;
    }

    return engine->time.total_seconds;
}

uint64_t henka_engine_get_frame_index(const henka_engine* engine)
{
    if (engine == NULL)
    {
        return 0U;
    }

    return engine->time.frame_index;
}

henka_result henka_engine_get_framebuffer_size(const henka_engine* engine, int* out_width, int* out_height)
{
    if (engine == NULL || out_width == NULL || out_height == NULL || engine->renderer == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    *out_width = engine->renderer->framebuffer_width;
    *out_height = engine->renderer->framebuffer_height;
    return HENKA_SUCCESS;
}

henka_result henka_engine_get_window_size(const henka_engine* engine, int* out_width, int* out_height)
{
    if (engine == NULL || engine->platform == NULL || out_width == NULL || out_height == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    if (!henka_platform_get_window_size(engine->platform, out_width, out_height))
    {
        return HENKA_ERROR_PLATFORM;
    }

    return HENKA_SUCCESS;
}

henka_result henka_engine_set_scene_viewport(henka_engine* engine, henka_viewport viewport)
{
    if (engine == NULL || engine->renderer == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    henka_renderer_set_scene_viewport(engine->renderer, viewport);
    if (engine->active_scene != NULL && engine->active_scene->has_camera)
    {
        henka_camera_set_aspect_ratio(&engine->active_scene->camera, henka_viewport_get_aspect_ratio(henka_renderer_get_scene_viewport(engine->renderer)));
    }

    return HENKA_SUCCESS;
}

henka_result henka_engine_get_scene_viewport(const henka_engine* engine, henka_viewport* out_viewport)
{
    if (engine == NULL || engine->renderer == NULL || out_viewport == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    *out_viewport = henka_renderer_get_scene_viewport(engine->renderer);
    return HENKA_SUCCESS;
}

const char* henka_engine_get_asset_base_path(const henka_engine* engine)
{
    if (engine == NULL || engine->asset_base_path == NULL)
    {
        return "";
    }

    return engine->asset_base_path;
}

henka_package_mode henka_engine_get_package_mode(const henka_engine* engine)
{
    if (engine == NULL)
    {
        return HENKA_PACKAGE_MODE_DEVELOPMENT;
    }

    return engine->package_mode;
}

const char* henka_engine_get_package_mode_label(henka_package_mode package_mode)
{
    switch (package_mode)
    {
        case HENKA_PACKAGE_MODE_PACKAGED:
            return "Packaged";
        case HENKA_PACKAGE_MODE_DEVELOPMENT:
            return "Development";
        case HENKA_PACKAGE_MODE_AUTO:
        default:
            return "Auto";
    }
}

henka_result henka_engine_get_diagnostics(const henka_engine* engine, henka_engine_diagnostics* out_diagnostics)
{
    henka_platform_diagnostics platform_diagnostics;

    if (engine == NULL || out_diagnostics == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    out_diagnostics->delta_seconds = engine->time.delta_seconds;
    out_diagnostics->frame_time_milliseconds = engine->time.delta_seconds * 1000.0;
    out_diagnostics->frames_per_second = engine->time.delta_seconds > 0.0 ? (1.0 / engine->time.delta_seconds) : 0.0;
    out_diagnostics->frame_index = engine->time.frame_index;
    out_diagnostics->framebuffer_width = engine->renderer != NULL ? engine->renderer->framebuffer_width : 0;
    out_diagnostics->framebuffer_height = engine->renderer != NULL ? engine->renderer->framebuffer_height : 0;
    out_diagnostics->wireframe_enabled = henka_engine_is_wireframe_enabled(engine);
    out_diagnostics->mouse_captured = henka_engine_is_mouse_captured(engine);
    out_diagnostics->ui_visible = engine->active_ui != NULL && henka_ui_is_visible(engine->active_ui);
    out_diagnostics->package_mode = engine->package_mode;
    memset(&platform_diagnostics, 0, sizeof(platform_diagnostics));
    henka_platform_get_diagnostics(engine->platform, &platform_diagnostics);
    out_diagnostics->multi_window_available = true;
    out_diagnostics->main_window_focused = platform_diagnostics.main_window_focused;
    out_diagnostics->open_tool_window_count = platform_diagnostics.open_tool_window_count;
    out_diagnostics->last_window_event_route = platform_diagnostics.last_event_route;
    out_diagnostics->last_tool_window_id = platform_diagnostics.last_tool_window_id;
    out_diagnostics->last_tool_window_close_requested = platform_diagnostics.last_tool_window_close_requested;
    out_diagnostics->last_tool_window_resized = platform_diagnostics.last_tool_window_resized;
    return HENKA_SUCCESS;
}

const char* henka_engine_get_user_data_base_path(const henka_engine* engine)
{
    if (engine == NULL || engine->user_data_base_path == NULL)
    {
        return "";
    }

    return engine->user_data_base_path;
}

henka_asset_manager* henka_engine_get_asset_manager(henka_engine* engine)
{
    if (engine == NULL)
    {
        return NULL;
    }

    return engine->asset_manager;
}

const henka_asset_manager* henka_engine_get_asset_manager_const(const henka_engine* engine)
{
    if (engine == NULL)
    {
        return NULL;
    }

    return engine->asset_manager;
}

bool henka_input_is_key_down(const henka_engine* engine, henka_key key)
{
    if (engine == NULL || key <= HENKA_KEY_UNKNOWN || key >= HENKA_KEY_COUNT)
    {
        return false;
    }

    return engine->input.keys_down[key];
}

bool henka_input_was_key_pressed(const henka_engine* engine, henka_key key)
{
    if (engine == NULL || key <= HENKA_KEY_UNKNOWN || key >= HENKA_KEY_COUNT)
    {
        return false;
    }

    return engine->input.keys_pressed[key];
}

bool henka_input_was_key_released(const henka_engine* engine, henka_key key)
{
    if (engine == NULL || key <= HENKA_KEY_UNKNOWN || key >= HENKA_KEY_COUNT)
    {
        return false;
    }

    return engine->input.keys_released[key];
}

bool henka_input_is_mouse_button_down(const henka_engine* engine, henka_mouse_button button)
{
    if (engine == NULL || button <= HENKA_MOUSE_BUTTON_UNKNOWN || button >= HENKA_MOUSE_BUTTON_COUNT)
    {
        return false;
    }

    return engine->input.mouse_buttons_down[button];
}

bool henka_input_was_mouse_button_pressed(const henka_engine* engine, henka_mouse_button button)
{
    if (engine == NULL || button <= HENKA_MOUSE_BUTTON_UNKNOWN || button >= HENKA_MOUSE_BUTTON_COUNT)
    {
        return false;
    }

    return engine->input.mouse_buttons_pressed[button];
}

bool henka_input_was_mouse_button_released(const henka_engine* engine, henka_mouse_button button)
{
    if (engine == NULL || button <= HENKA_MOUSE_BUTTON_UNKNOWN || button >= HENKA_MOUSE_BUTTON_COUNT)
    {
        return false;
    }

    return engine->input.mouse_buttons_released[button];
}

henka_vec2 henka_input_get_mouse_position(const henka_engine* engine)
{
    if (engine == NULL)
    {
        return (henka_vec2){0.0f, 0.0f};
    }

    return engine->input.mouse_position;
}

henka_vec2 henka_input_get_mouse_delta(const henka_engine* engine)
{
    if (engine == NULL)
    {
        return (henka_vec2){0.0f, 0.0f};
    }

    return engine->input.mouse_delta;
}

henka_vec2 henka_input_get_mouse_wheel_delta(const henka_engine* engine)
{
    if (engine == NULL)
    {
        return (henka_vec2){0.0f, 0.0f};
    }

    return engine->input.mouse_wheel_delta;
}

const char* henka_input_action_get_name(henka_input_action action)
{
    return henka_input_action_default_name(action);
}

henka_input_action henka_input_action_find_by_name(const char* name)
{
    henka_input_action action;

    if (name == NULL || name[0] == '\0')
    {
        return HENKA_INPUT_ACTION_UNKNOWN;
    }

    for (action = HENKA_INPUT_ACTION_MOVE_FORWARD; action < HENKA_INPUT_ACTION_COUNT; ++action)
    {
        if (henka_action_name_equals(henka_input_action_default_name(action), name))
        {
            return action;
        }
    }

    return HENKA_INPUT_ACTION_UNKNOWN;
}

henka_result henka_input_bind_action_key(struct henka_engine* engine, henka_input_action action, henka_key key)
{
    if (engine == NULL || action <= HENKA_INPUT_ACTION_UNKNOWN || action >= HENKA_INPUT_ACTION_COUNT)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    if (key < HENKA_KEY_UNKNOWN || key >= HENKA_KEY_COUNT)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    engine->action_key_bindings[action] = key;
    return HENKA_SUCCESS;
}

henka_result henka_input_bind_action_mouse_button(struct henka_engine* engine, henka_input_action action, henka_mouse_button button)
{
    if (engine == NULL || action <= HENKA_INPUT_ACTION_UNKNOWN || action >= HENKA_INPUT_ACTION_COUNT)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    if (button < HENKA_MOUSE_BUTTON_UNKNOWN || button >= HENKA_MOUSE_BUTTON_COUNT)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    engine->action_mouse_bindings[action] = button;
    return HENKA_SUCCESS;
}

static bool henka_input_action_query_key(
    const henka_engine* engine,
    henka_input_action action,
    bool (*query_key)(const henka_engine*, henka_key),
    bool (*query_mouse)(const henka_engine*, henka_mouse_button))
{
    henka_key key;
    henka_mouse_button button;

    if (engine == NULL || action <= HENKA_INPUT_ACTION_UNKNOWN || action >= HENKA_INPUT_ACTION_COUNT)
    {
        return false;
    }

    key = engine->action_key_bindings[action];
    button = engine->action_mouse_bindings[action];

    return ((key > HENKA_KEY_UNKNOWN) ? query_key(engine, key) : false) ||
        ((button > HENKA_MOUSE_BUTTON_UNKNOWN) ? query_mouse(engine, button) : false);
}

bool henka_input_action_is_down(const struct henka_engine* engine, henka_input_action action)
{
    return henka_input_action_query_key(engine, action, henka_input_is_key_down, henka_input_is_mouse_button_down);
}

bool henka_input_action_was_pressed(const struct henka_engine* engine, henka_input_action action)
{
    return henka_input_action_query_key(engine, action, henka_input_was_key_pressed, henka_input_was_mouse_button_pressed);
}

bool henka_input_action_was_released(const struct henka_engine* engine, henka_input_action action)
{
    return henka_input_action_query_key(engine, action, henka_input_was_key_released, henka_input_was_mouse_button_released);
}
