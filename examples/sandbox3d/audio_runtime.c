#include <math.h>
#include <stdio.h>
#include <string.h>

#include <henka/memory.h>

#include "audio_runtime.h"

#define SANDBOX3D_AUDIO_FIXTURE_PATH "assets/audio/henka_audio_fixture.wav"
#define SANDBOX3D_AUDIO_SMOKE_FRAME_COUNT 32U

struct sandbox3d_audio_runtime
{
    henka_audio_system* system;
    henka_audio_output* output;
    henka_result output_result;
    double frame_accumulator;
    uint32_t max_pump_frames;
    henka_audio_emitter* preview_emitter;
};

static bool sandbox3d_audio_runtime_has_signal(
    const float* samples,
    size_t frame_count)
{
    size_t index;

    if (samples == NULL || frame_count == 0U)
    {
        return false;
    }
    for (index = 0U; index < frame_count * HENKA_AUDIO_OUTPUT_CHANNELS; ++index)
    {
        if (fabsf(samples[index]) > 0.0001f)
        {
            return true;
        }
    }
    return false;
}

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
    henka_audio_emitter_destroy(runtime->preview_emitter);
    runtime->preview_emitter = NULL;
    henka_audio_output_destroy(runtime->output);
    henka_audio_system_destroy(runtime->system);
    henka_free(runtime);
}

henka_result sandbox3d_audio_runtime_start_preview(
    sandbox3d_audio_runtime* runtime,
    henka_scene* scene,
    henka_asset_manager* assets,
    henka_entity entity,
    const henka_audio_emitter_config* config)
{
    henka_audio_clip* clip = NULL;
    henka_audio_stream* stream = NULL;
    henka_audio_emitter* candidate = NULL;
    henka_result result;

    if (runtime == NULL || scene == NULL || assets == NULL ||
        !sandbox3d_audio_runtime_is_output_available(runtime) ||
        !henka_scene_is_entity_valid(scene, entity) || config == NULL ||
        !config->enabled ||
        henka_audio_emitter_config_validate(config) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (config->streaming)
    {
        result = henka_assets_load_audio_stream(assets, config->clip_path, &stream);
        if (result != HENKA_SUCCESS || stream == NULL)
        {
            return result != HENKA_SUCCESS ? result : HENKA_ERROR_ASSET_SOURCE;
        }
        result = henka_audio_emitter_create_with_stream(
            runtime->system,
            scene,
            entity,
            stream,
            config,
            &candidate);
    }
    else
    {
        result = henka_assets_load_audio_clip(assets, config->clip_path, &clip);
        if (result != HENKA_SUCCESS || clip == NULL)
        {
            return result != HENKA_SUCCESS ? result : HENKA_ERROR_ASSET_SOURCE;
        }
        result = henka_audio_emitter_create_with_clip(
            runtime->system,
            scene,
            entity,
            clip,
            config,
            &candidate);
    }
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    henka_audio_emitter_destroy(runtime->preview_emitter);
    runtime->preview_emitter = candidate;
    return HENKA_SUCCESS;
}

void sandbox3d_audio_runtime_stop_preview(
    sandbox3d_audio_runtime* runtime)
{
    if (runtime == NULL)
    {
        return;
    }
    henka_audio_emitter_destroy(runtime->preview_emitter);
    runtime->preview_emitter = NULL;
}

bool sandbox3d_audio_runtime_is_preview_playing(
    const sandbox3d_audio_runtime* runtime)
{
    return runtime != NULL && runtime->preview_emitter != NULL &&
        henka_audio_emitter_is_playing(runtime->preview_emitter);
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

henka_result sandbox3d_audio_runtime_validate_fixture(
    sandbox3d_audio_runtime* runtime,
    henka_scene* scene,
    henka_asset_manager* assets,
    const henka_camera* camera)
{
    henka_audio_clip* clip = NULL;
    henka_audio_emitter* emitter = NULL;
    henka_audio_emitter_config emitter_config;
    henka_asset_metadata metadata;
    henka_audio_output_info output_info;
    henka_transform transform;
    float mixed_samples[SANDBOX3D_AUDIO_SMOKE_FRAME_COUNT * HENKA_AUDIO_OUTPUT_CHANNELS];
    henka_entity entity = HENKA_INVALID_ENTITY;
    henka_audio_system* system;
    size_t baseline_voice_count;
    henka_result result;

    if (runtime == NULL || scene == NULL || assets == NULL || camera == NULL ||
        !henka_camera_is_valid(camera) ||
        !sandbox3d_audio_runtime_is_output_available(runtime))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    system = sandbox3d_audio_runtime_get_system_if_available(runtime);
    if (system == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    baseline_voice_count = henka_audio_system_get_active_voice_count(system);

    result = henka_assets_load_audio_clip(
        assets,
        SANDBOX3D_AUDIO_FIXTURE_PATH,
        &clip);
    if (result != HENKA_SUCCESS || clip == NULL)
    {
        goto cleanup;
    }
    result = henka_assets_get_audio_metadata(assets, clip, &metadata);
    if (result != HENKA_SUCCESS || metadata.type != HENKA_ASSET_TYPE_AUDIO ||
        !metadata.loaded || metadata.fallback ||
        metadata.source_path == NULL ||
        strcmp(metadata.source_path, SANDBOX3D_AUDIO_FIXTURE_PATH) != 0)
    {
        result = HENKA_ERROR_ASSET_SOURCE;
        goto cleanup;
    }
    entity = henka_scene_create_entity_named(scene, "Packaged Audio Fixture");
    if (entity == HENKA_INVALID_ENTITY)
    {
        result = HENKA_ERROR_OUT_OF_MEMORY;
        goto cleanup;
    }
    transform = (henka_transform){
        camera->position,
        (henka_quat){0.0f, 0.0f, 0.0f, 1.0f},
        (henka_vec3){1.0f, 1.0f, 1.0f}};
    result = henka_scene_set_entity_transform(scene, entity, transform);
    if (result != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    emitter_config = henka_audio_emitter_config_default();
    emitter_config.enabled = true;
    emitter_config.looping = true;
    emitter_config.spatial = true;
    (void)snprintf(
        emitter_config.clip_path,
        sizeof(emitter_config.clip_path),
        "%s",
        SANDBOX3D_AUDIO_FIXTURE_PATH);
    result = henka_audio_emitter_create_with_clip(
        system,
        scene,
        entity,
        clip,
        &emitter_config,
        &emitter);
    if (result != HENKA_SUCCESS || emitter == NULL)
    {
        goto cleanup;
    }
    result = sandbox3d_audio_runtime_update_listener(runtime, camera);
    if (result != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    memset(mixed_samples, 0, sizeof(mixed_samples));
    result = henka_audio_system_mix(
        system,
        mixed_samples,
        SANDBOX3D_AUDIO_SMOKE_FRAME_COUNT);
    if (result != HENKA_SUCCESS || !sandbox3d_audio_runtime_has_signal(
            mixed_samples,
            SANDBOX3D_AUDIO_SMOKE_FRAME_COUNT) ||
        !henka_audio_emitter_is_playing(emitter))
    {
        result = HENKA_ERROR_UNKNOWN;
        goto cleanup;
    }
    result = sandbox3d_audio_runtime_pump(runtime, 1.0 / 60.0);
    if (result != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    result = sandbox3d_audio_runtime_get_output_info(runtime, &output_info);
    if (result != HENKA_SUCCESS || !output_info.device_open ||
        output_info.sample_rate == 0U || output_info.pumped_frames == 0U)
    {
        result = HENKA_ERROR_PLATFORM;
        goto cleanup;
    }

    henka_scene_destroy_entity(scene, entity);
    entity = HENKA_INVALID_ENTITY;
    result = henka_audio_system_mix(system, mixed_samples, 1U);
    if (result != HENKA_SUCCESS ||
        henka_audio_system_get_active_voice_count(system) != baseline_voice_count ||
        henka_audio_emitter_is_valid(emitter))
    {
        result = HENKA_ERROR_UNKNOWN;
        goto cleanup;
    }
    result = HENKA_SUCCESS;

cleanup:
    henka_audio_emitter_destroy(emitter);
    if (entity != HENKA_INVALID_ENTITY)
    {
        henka_scene_destroy_entity(scene, entity);
    }
    /* clip is manager-owned and must not be destroyed here. */
    return result;
}
