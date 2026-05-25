#include "henka_internal.h"

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

static void henka_input_reset_frame_state(henka_input_state* input)
{
    size_t index;

    for (index = 0; index < HENKA_KEY_COUNT; ++index)
    {
        input->keys_pressed[index] = false;
    }

    for (index = 0; index < HENKA_MOUSE_BUTTON_COUNT; ++index)
    {
        input->mouse_buttons_pressed[index] = false;
        input->mouse_buttons_released[index] = false;
    }

    input->mouse_delta.x = 0.0f;
    input->mouse_delta.y = 0.0f;
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
        henka_camera_set_aspect_ratio(
            &engine->active_scene->camera,
            (float)frame_state->framebuffer_width / (float)frame_state->framebuffer_height);
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
    }

    HENKA_LOG_INFO("engine startup complete");

    *out_engine = engine;
    return HENKA_SUCCESS;
}

void henka_engine_destroy(henka_engine* engine)
{
    if (engine == NULL)
    {
        return;
    }

    HENKA_LOG_INFO("shutting down engine");

    if (engine->initialized_callback_ran && engine->config.on_shutdown != NULL)
    {
        engine->config.on_shutdown(engine, engine->config.user_data);
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

const char* henka_engine_get_asset_base_path(const henka_engine* engine)
{
    if (engine == NULL || engine->asset_base_path == NULL)
    {
        return "";
    }

    return engine->asset_base_path;
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
