#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <henka/henka.h>
#include <henka/audio.h>
#include <henka/memory.h>
#include <henka/scene.h>

#include "../examples/sandbox3d/audio_runtime.h"
#include "../engine/src/henka_internal.h"

int main(void)
{
    const char* audio_asset_path = "assets/audio/henka_audio_fixture.wav";
    henka_scene* scene = NULL;
    sandbox3d_audio_runtime* runtime = NULL;
    henka_audio_clip* clip = NULL;
    henka_audio_emitter* emitter = NULL;
    henka_audio_emitter_config emitter_config =
        henka_audio_emitter_config_default();
    henka_audio_system* audio_system = NULL;
    henka_asset_manager* assets = NULL;
    henka_engine engine;
    henka_entity entity = HENKA_INVALID_ENTITY;
    henka_audio_output_info output_info;
    float mixed_samples[16U * HENKA_AUDIO_OUTPUT_CHANNELS];
    henka_camera camera;
    henka_result audio_result;
    int exit_code = 1;

#if defined(_WIN32)
    (void)_putenv_s("SDL_AUDIODRIVER", "dummy");
#endif

    camera = henka_camera_create_perspective(
        60.0f * HENKA_DEG_TO_RAD,
        16.0f / 9.0f,
        0.1f,
        100.0f);
    camera.position = (henka_vec3){2.0f, 3.0f, 4.0f};
    camera.yaw_radians = 0.35f;
    camera.pitch_radians = -0.2f;
    if (henka_scene_create(&scene) != HENKA_SUCCESS ||
        sandbox3d_audio_runtime_create(scene, &runtime) != HENKA_SUCCESS ||
        !sandbox3d_audio_runtime_is_output_available(runtime) ||
        (audio_system = sandbox3d_audio_runtime_get_system_if_available(runtime)) == NULL ||
        henka_audio_clip_load_file(".", audio_asset_path, &clip) != HENKA_SUCCESS ||
        (entity = henka_scene_create_entity_named(scene, "Packaged Audio Object")) == HENKA_INVALID_ENTITY)
    {
        fprintf(stderr, "sandbox audio runtime test failed during setup\n");
        goto cleanup;
    }
    emitter_config.enabled = true;
    emitter_config.looping = true;
    emitter_config.spatial = true;
    (void)snprintf(
        emitter_config.clip_path,
        sizeof(emitter_config.clip_path),
        "%s",
        audio_asset_path);
    audio_result = henka_audio_emitter_create_with_clip(
        audio_system,
        scene,
        entity,
        clip,
        &emitter_config,
        &emitter);
    if (audio_result != HENKA_SUCCESS)
    {
        fprintf(stderr, "sandbox audio runtime emitter create failed: %d\n", (int)audio_result);
        goto cleanup;
    }
    audio_result = henka_audio_system_mix(audio_system, mixed_samples, 16U);
    if (audio_result != HENKA_SUCCESS || fabsf(mixed_samples[4]) <= 0.0001f)
    {
        fprintf(stderr, "sandbox audio runtime mix failed: result=%d sample=%f\n", (int)audio_result, mixed_samples[4]);
        goto cleanup;
    }
    if (!henka_audio_emitter_is_playing(emitter))
    {
        fprintf(stderr, "sandbox audio runtime emitter is not playing\n");
        goto cleanup;
    }
    audio_result = sandbox3d_audio_runtime_update_listener(runtime, &camera);
    if (audio_result != HENKA_SUCCESS)
    {
        fprintf(stderr, "sandbox audio runtime listener update failed: %d\n", (int)audio_result);
        goto cleanup;
    }
    audio_result = sandbox3d_audio_runtime_pump(runtime, 1.0 / 60.0);
    if (audio_result != HENKA_SUCCESS)
    {
        fprintf(stderr, "sandbox audio runtime pump failed: %d\n", (int)audio_result);
        goto cleanup;
    }
    audio_result = sandbox3d_audio_runtime_get_output_info(runtime, &output_info);
    if (audio_result != HENKA_SUCCESS || !output_info.device_open ||
        output_info.sample_rate == 0U || output_info.pumped_frames == 0U)
    {
        fprintf(stderr, "sandbox audio runtime output failed: result=%d open=%d rate=%u pumped=%llu\n", (int)audio_result, output_info.device_open, (unsigned int)output_info.sample_rate, (unsigned long long)output_info.pumped_frames);
        goto cleanup;
    }

    henka_audio_emitter_destroy(emitter);
    emitter = NULL;
    memset(&engine, 0, sizeof(engine));
    engine.asset_base_path = ".";
    assets = (henka_asset_manager*)henka_malloc(sizeof(*assets));
    if (assets == NULL)
    {
        fprintf(stderr, "sandbox audio runtime preview manager allocation failed\n");
        goto cleanup;
    }
    memset(assets, 0, sizeof(*assets));
    assets->engine = &engine;
    audio_result = sandbox3d_audio_runtime_start_preview(
        runtime,
        scene,
        assets,
        entity,
        &emitter_config);
    if (audio_result != HENKA_SUCCESS ||
        !sandbox3d_audio_runtime_is_preview_playing(runtime))
    {
        fprintf(stderr, "sandbox audio runtime preview start failed: %d\n", (int)audio_result);
        goto cleanup;
    }
    (void)snprintf(
        emitter_config.clip_path,
        sizeof(emitter_config.clip_path),
        "%s",
        "assets/audio/missing-preview.wav");
    audio_result = sandbox3d_audio_runtime_start_preview(
        runtime,
        scene,
        assets,
        entity,
        &emitter_config);
    if (audio_result == HENKA_SUCCESS ||
        !sandbox3d_audio_runtime_is_preview_playing(runtime))
    {
        fprintf(stderr, "sandbox audio runtime preview replacement was not transactional: %d\n", (int)audio_result);
        goto cleanup;
    }
    sandbox3d_audio_runtime_stop_preview(runtime);
    if (sandbox3d_audio_runtime_is_preview_playing(runtime))
    {
        fprintf(stderr, "sandbox audio runtime preview stop failed\n");
        goto cleanup;
    }
    exit_code = 0;

cleanup:
    sandbox3d_audio_runtime_stop_preview(runtime);
    henka_audio_emitter_destroy(emitter);
    henka_audio_clip_destroy(clip);
    henka_asset_manager_destroy(assets);
    sandbox3d_audio_runtime_destroy(runtime);
    if (scene != NULL && entity != HENKA_INVALID_ENTITY)
    {
        henka_scene_destroy_entity(scene, entity);
    }
    henka_scene_destroy(scene);
    return exit_code;
}
