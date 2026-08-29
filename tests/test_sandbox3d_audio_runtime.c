#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#if defined(_WIN32)
#include <direct.h>
#else
#include <sys/stat.h>
#endif

#include <henka/henka.h>
#include <henka/audio.h>
#include <henka/memory.h>
#include <henka/scene.h>

#include "../examples/sandbox3d/audio_runtime.h"
#include "../engine/src/henka_internal.h"

static const char test_compressed_ogg_base64[] =
#include "fixtures/audio_fixture_ogg.b64"
;
static const char test_compressed_mp3_base64[] =
#include "fixtures/audio_fixture_mp3.b64"
;
static const char test_compressed_flac_base64[] =
#include "fixtures/audio_fixture_flac.b64"
;

#define TEST_AUDIO_PACKAGE_ROOT "build/test_tmp/audio_runtime_package"
#define TEST_AUDIO_PACKAGE_AUDIO_ROOT \
    TEST_AUDIO_PACKAGE_ROOT "/assets/audio"

static int test_base64_value(char value)
{
    if (value >= 'A' && value <= 'Z') return value - 'A';
    if (value >= 'a' && value <= 'z') return value - 'a' + 26;
    if (value >= '0' && value <= '9') return value - '0' + 52;
    if (value == '+') return 62;
    if (value == '/') return 63;
    return -1;
}

static bool test_make_directory(const char* path)
{
#if defined(_WIN32)
    return _mkdir(path) == 0 || errno == EEXIST;
#else
    return mkdir(path, 0700) == 0 || errno == EEXIST;
#endif
}

static bool test_write_base64_file(const char* path, const char* encoded)
{
    FILE* file = NULL;
    size_t length;
    size_t index;
    bool success = true;

    if (path == NULL || encoded == NULL)
        return false;
    length = strlen(encoded);
    if (length == 0U || length % 4U != 0U)
        return false;
#if defined(_WIN32)
    if (fopen_s(&file, path, "wb") != 0) file = NULL;
#else
    file = fopen(path, "wb");
#endif
    if (file == NULL)
        return false;
    for (index = 0U; index < length && success; index += 4U)
    {
        const int first = test_base64_value(encoded[index]);
        const int second = test_base64_value(encoded[index + 1U]);
        const int third = encoded[index + 2U] == '=' ? 0 :
            test_base64_value(encoded[index + 2U]);
        const int fourth = encoded[index + 3U] == '=' ? 0 :
            test_base64_value(encoded[index + 3U]);
        const unsigned char bytes[3U] = {
            (unsigned char)((first << 2) | (second >> 4)),
            (unsigned char)((second << 4) | (third >> 2)),
            (unsigned char)((third << 6) | fourth)};
        size_t byte_count = 3U;
        if (first < 0 || second < 0 || third < 0 || fourth < 0 ||
            (encoded[index + 2U] == '=' && encoded[index + 3U] != '=') ||
            (index + 4U != length &&
                (encoded[index + 2U] == '=' || encoded[index + 3U] == '=')))
        {
            success = false;
            break;
        }
        if (encoded[index + 3U] == '=') byte_count = 2U;
        if (encoded[index + 2U] == '=') byte_count = 1U;
        success = fwrite(bytes, 1U, byte_count, file) == byte_count;
    }
    if (fclose(file) != 0) success = false;
    return success;
}

static bool test_copy_file(const char* source_path, const char* destination_path)
{
    FILE* source = NULL;
    FILE* destination = NULL;
    unsigned char buffer[4096U];
    size_t bytes_read;
    bool success = true;

#if defined(_WIN32)
    if (fopen_s(&source, source_path, "rb") != 0) source = NULL;
    if (source != NULL && fopen_s(&destination, destination_path, "wb") != 0)
        destination = NULL;
#else
    source = fopen(source_path, "rb");
    if (source != NULL) destination = fopen(destination_path, "wb");
#endif
    if (source == NULL || destination == NULL) success = false;
    while (success && (bytes_read = fread(buffer, 1U, sizeof(buffer), source)) > 0U)
    {
        success = fwrite(buffer, 1U, bytes_read, destination) == bytes_read;
    }
    if (source != NULL && fclose(source) != 0) success = false;
    if (destination != NULL && fclose(destination) != 0) success = false;
    return success;
}

static bool test_prepare_package_audio(void)
{
    static const struct
    {
        const char* name;
        const char* encoded;
    } fixtures[] = {
        {"henka_audio_fixture.ogg", test_compressed_ogg_base64},
        {"henka_audio_fixture.mp3", test_compressed_mp3_base64},
        {"henka_audio_fixture.flac", test_compressed_flac_base64}};
    size_t index;

    if (!test_make_directory("build/test_tmp"))
        return false;
    if (!test_make_directory(TEST_AUDIO_PACKAGE_ROOT))
        return false;
    if (!test_make_directory(TEST_AUDIO_PACKAGE_ROOT "/assets"))
        return false;
    if (!test_make_directory(TEST_AUDIO_PACKAGE_AUDIO_ROOT))
        return false;
    if (!test_copy_file(
            "assets/audio/henka_audio_fixture.wav",
            TEST_AUDIO_PACKAGE_AUDIO_ROOT "/henka_audio_fixture.wav"))
    {
        return false;
    }
    for (index = 0U; index < sizeof(fixtures) / sizeof(fixtures[0]); ++index)
    {
        char path[256U];
        (void)snprintf(
            path,
            sizeof(path),
            "%s/%s",
            TEST_AUDIO_PACKAGE_AUDIO_ROOT,
            fixtures[index].name);
        if (!test_write_base64_file(path, fixtures[index].encoded))
        {
            return false;
        }
    }
    return true;
}

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
    emitter_config.streaming = false;
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
    if (!test_prepare_package_audio())
    {
        fprintf(stderr, "sandbox audio runtime compressed fixture setup failed\n");
        goto cleanup;
    }
    engine.asset_base_path = TEST_AUDIO_PACKAGE_ROOT;
    audio_result = sandbox3d_audio_runtime_validate_stream_fixture(
        runtime,
        scene,
        assets,
        &camera);
    if (audio_result != HENKA_SUCCESS)
    {
        fprintf(stderr, "sandbox audio runtime packaged stream validation failed: %d\n", (int)audio_result);
        goto cleanup;
    }
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
    remove(TEST_AUDIO_PACKAGE_AUDIO_ROOT "/henka_audio_fixture.wav");
    remove(TEST_AUDIO_PACKAGE_AUDIO_ROOT "/henka_audio_fixture.ogg");
    remove(TEST_AUDIO_PACKAGE_AUDIO_ROOT "/henka_audio_fixture.mp3");
    remove(TEST_AUDIO_PACKAGE_AUDIO_ROOT "/henka_audio_fixture.flac");
    remove(TEST_AUDIO_PACKAGE_AUDIO_ROOT);
    remove(TEST_AUDIO_PACKAGE_ROOT "/assets");
    remove(TEST_AUDIO_PACKAGE_ROOT);
    henka_scene_destroy(scene);
    return exit_code;
}
