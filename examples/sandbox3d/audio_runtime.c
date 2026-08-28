#include <math.h>

#include <henka/memory.h>

#include "audio_runtime.h"

struct sandbox3d_audio_runtime
{
    henka_audio_system* system;
    henka_audio_output* output;
    henka_result output_result;
    double frame_accumulator;
    uint32_t max_pump_frames;
};

henka_result sandbox3d_audio_runtime_create(
    henka_scene* scene,
    sandbox3d_audio_runtime** out_runtime)
{
    sandbox3d_audio_runtime* runtime;
    henka_audio_output_config output_config;
    henka_result result;

    if (out_runtime == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_runtime = NULL;
    if (scene == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    runtime = (sandbox3d_audio_runtime*)henka_calloc(1U, sizeof(*runtime));
    if (runtime == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    result = henka_audio_system_create(&(henka_audio_system_config){0U}, &runtime->system);
    if (result != HENKA_SUCCESS)
    {
        henka_free(runtime);
        return result;
    }
    output_config = henka_audio_output_config_default();
    runtime->max_pump_frames = output_config.max_pump_frames;
    runtime->output_result = henka_audio_output_create(
        runtime->system,
        scene,
        &output_config,
        &runtime->output);
    if (runtime->output_result != HENKA_SUCCESS &&
        runtime->output_result != HENKA_ERROR_PLATFORM)
    {
        result = runtime->output_result;
        henka_audio_system_destroy(runtime->system);
        henka_free(runtime);
        return result;
    }
    *out_runtime = runtime;
    return HENKA_SUCCESS;
}

void sandbox3d_audio_runtime_destroy(
    sandbox3d_audio_runtime* runtime)
{
    if (runtime == NULL)
    {
        return;
    }
    henka_audio_output_destroy(runtime->output);
    henka_audio_system_destroy(runtime->system);
    henka_free(runtime);
}

bool sandbox3d_audio_runtime_is_output_available(
    const sandbox3d_audio_runtime* runtime)
{
    return runtime != NULL && runtime->output != NULL;
}

henka_audio_system* sandbox3d_audio_runtime_get_system_if_available(
    const sandbox3d_audio_runtime* runtime)
{
    return sandbox3d_audio_runtime_is_output_available(runtime)
        ? runtime->system
        : NULL;
}

henka_result sandbox3d_audio_runtime_update_listener(
    sandbox3d_audio_runtime* runtime,
    const henka_camera* camera)
{
    henka_audio_listener listener;
    if (runtime == NULL || camera == NULL ||
        !sandbox3d_audio_runtime_is_output_available(runtime) ||
        !henka_camera_is_valid(camera))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    listener = henka_audio_listener_default();
    listener.position = camera->position;
    listener.forward = henka_camera_get_forward(camera);
    listener.up = henka_camera_get_up(camera);
    return henka_audio_system_set_listener(runtime->system, listener);
}

henka_result sandbox3d_audio_runtime_pump(
    sandbox3d_audio_runtime* runtime,
    double delta_seconds)
{
    double available_frames;
    uint32_t frame_count;

    if (runtime == NULL || !sandbox3d_audio_runtime_is_output_available(runtime) ||
        !isfinite(delta_seconds) || delta_seconds < 0.0)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (delta_seconds == 0.0)
    {
        return HENKA_SUCCESS;
    }
    available_frames = runtime->frame_accumulator +
        delta_seconds * (double)henka_audio_system_get_sample_rate(runtime->system);
    if (!isfinite(available_frames))
    {
        return HENKA_ERROR_NUMERIC_RANGE;
    }
    if (available_frames >= (double)runtime->max_pump_frames)
    {
        frame_count = runtime->max_pump_frames;
        runtime->frame_accumulator = 0.0;
    }
    else
    {
        frame_count = (uint32_t)floor(available_frames);
        runtime->frame_accumulator = available_frames - (double)frame_count;
    }
    return frame_count == 0U
        ? HENKA_SUCCESS
        : henka_audio_output_pump(runtime->output, frame_count);
}

henka_result sandbox3d_audio_runtime_get_output_info(
    const sandbox3d_audio_runtime* runtime,
    henka_audio_output_info* out_info)
{
    if (runtime == NULL || out_info == NULL || runtime->output == NULL)
    {
        return runtime == NULL || out_info == NULL
            ? HENKA_ERROR_INVALID_ARGUMENT
            : runtime->output_result;
    }
    return henka_audio_output_get_info(runtime->output, out_info);
}
