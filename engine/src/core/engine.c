#include "henka_internal.h"

#include <string.h>

#include <henka/log.h>
#include <henka/memory.h>

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

    input->close_requested = false;
}

henka_result henka_engine_create(const henka_engine_config* config, henka_engine** out_engine)
{
    henka_engine* engine;
    henka_platform_desc platform_desc;
    henka_result result;

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

    result = henka_renderer_create(engine->platform, engine->config.enable_vsync, &engine->renderer);
    if (result != HENKA_SUCCESS)
    {
        HENKA_LOG_ERROR("renderer initialization failed: %s", henka_result_to_string(result));
        henka_platform_destroy(engine->platform);
        henka_free(engine);
        return result;
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

    henka_renderer_destroy(engine->renderer);
    henka_platform_destroy(engine->platform);
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

    HENKA_LOG_INFO("entering engine run loop");

    while (!engine->exit_requested)
    {
        henka_input_reset_frame_state(&engine->input);

        result = henka_platform_poll_events(engine->platform, &engine->input, &frame_state);
        if (result != HENKA_SUCCESS)
        {
            HENKA_LOG_ERROR("platform event polling failed: %s", henka_result_to_string(result));
            return result;
        }

        if (frame_state.close_requested || henka_input_was_key_pressed(engine, HENKA_KEY_ESCAPE))
        {
            henka_engine_request_exit(engine);
        }

        if (frame_state.resized)
        {
            henka_renderer_resize_viewport(engine->renderer, frame_state.framebuffer_width, frame_state.framebuffer_height);
        }

        result = henka_renderer_begin_frame(engine->renderer);
        if (result != HENKA_SUCCESS)
        {
            HENKA_LOG_ERROR("renderer begin frame failed: %s", henka_result_to_string(result));
            return result;
        }

        henka_renderer_clear_frame(engine->renderer);

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
