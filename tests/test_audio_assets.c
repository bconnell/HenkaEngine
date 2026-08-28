#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <henka/assets.h>
#include <henka/audio.h>
#include <henka/memory.h>

#include "../engine/src/henka_internal.h"

#define HENKA_TEST_ASSERT(condition) \
    do \
    { \
        if (!(condition)) \
        { \
            fprintf(stderr, "audio asset assertion failed: %s\\n", #condition); \
            return EXIT_FAILURE; \
        } \
    } while (0)

static void test_write_u16(unsigned char* bytes, size_t offset, uint16_t value)
{
    bytes[offset] = (unsigned char)(value & 0xffU);
    bytes[offset + 1U] = (unsigned char)((value >> 8U) & 0xffU);
}

static void test_write_u32(unsigned char* bytes, size_t offset, uint32_t value)
{
    bytes[offset] = (unsigned char)(value & 0xffU);
    bytes[offset + 1U] = (unsigned char)((value >> 8U) & 0xffU);
    bytes[offset + 2U] = (unsigned char)((value >> 16U) & 0xffU);
    bytes[offset + 3U] = (unsigned char)((value >> 24U) & 0xffU);
}

static bool test_write_wav_with_sample(
    const char* path,
    size_t frame_count,
    int16_t sample_value)
{
    const size_t data_size = frame_count * sizeof(int16_t);
    const size_t file_size = 44U + data_size;
    unsigned char* bytes = (unsigned char*)calloc(1U, file_size);
    FILE* file = NULL;
    size_t frame_index;
    bool success;

    if (bytes == NULL)
    {
        return false;
    }
    memcpy(bytes, "RIFF", 4U);
    test_write_u32(bytes, 4U, (uint32_t)(file_size - 8U));
    memcpy(bytes + 8U, "WAVEfmt ", 8U);
    test_write_u32(bytes, 16U, 16U);
    test_write_u16(bytes, 20U, 1U);
    test_write_u16(bytes, 22U, 1U);
    test_write_u32(bytes, 24U, HENKA_AUDIO_DEFAULT_SAMPLE_RATE);
    test_write_u32(bytes, 28U, HENKA_AUDIO_DEFAULT_SAMPLE_RATE * 2U);
    test_write_u16(bytes, 32U, 2U);
    test_write_u16(bytes, 34U, 16U);
    memcpy(bytes + 36U, "data", 4U);
    test_write_u32(bytes, 40U, (uint32_t)data_size);
    for (frame_index = 0U; frame_index < frame_count; ++frame_index)
    {
        test_write_u16(
            bytes,
            44U + frame_index * 2U,
            (uint16_t)sample_value);
    }
#if defined(_WIN32)
    if (fopen_s(&file, path, "wb") != 0)
    {
        file = NULL;
    }
#else
    file = fopen(path, "wb");
#endif
    success = file != NULL && fwrite(bytes, 1U, file_size, file) == file_size;
    if (file != NULL && fclose(file) != 0)
    {
        success = false;
    }
    free(bytes);
    return success;
}

static bool test_write_wav(const char* path)
{
    return test_write_wav_with_sample(path, 64U, 8192);
}

static bool test_nonzero_mix(const float* samples, size_t frame_count)
{
    size_t index;
    for (index = 0U; index < frame_count * HENKA_AUDIO_OUTPUT_CHANNELS; ++index)
    {
        if (fabsf(samples[index]) > 0.0001f)
        {
            return true;
        }
    }
    return false;
}

int main(void)
{
    const char* wav_path = "build/test_tmp/audio_manager.wav";
    henka_engine engine;
    henka_asset_manager* manager;
    henka_audio_clip* clip = NULL;
    henka_audio_clip* same_clip = NULL;
    henka_asset_metadata metadata;
    henka_audio_system* system = NULL;
    henka_scene* scene = NULL;
    henka_entity entity;
    henka_audio_emitter_config emitter_config =
        henka_audio_emitter_config_default();
    henka_audio_emitter* emitter = NULL;
    henka_audio_clip_info clip_info;
    henka_audio_clip_info reloaded_info;
    float samples[HENKA_AUDIO_OUTPUT_CHANNELS];
    int result = EXIT_FAILURE;

    memset(&engine, 0, sizeof(engine));
    engine.asset_base_path = ".";
    manager = (henka_asset_manager*)henka_malloc(sizeof(*manager));
    HENKA_TEST_ASSERT(manager != NULL);
    memset(manager, 0, sizeof(*manager));
    manager->engine = &engine;
    HENKA_TEST_ASSERT(test_write_wav(wav_path));

    HENKA_TEST_ASSERT(henka_assets_load_audio_clip(
        manager, "../audio_manager.wav", &clip) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(clip == NULL);
    HENKA_TEST_ASSERT(henka_assets_load_audio_clip(
        manager, wav_path, &clip) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(clip != NULL);
    HENKA_TEST_ASSERT(henka_assets_load_audio_clip(
        manager, "build\\test_tmp\\.\\audio_manager.wav", &same_clip) ==
        HENKA_SUCCESS);
    HENKA_TEST_ASSERT(same_clip == clip);
    HENKA_TEST_ASSERT(henka_assets_get_metadata_count(manager) == 1U);
    HENKA_TEST_ASSERT(henka_assets_get_audio_metadata(
        manager, clip, &metadata) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(metadata.type == HENKA_ASSET_TYPE_AUDIO);
    HENKA_TEST_ASSERT(metadata.loaded && !metadata.fallback);
    HENKA_TEST_ASSERT(metadata.reload_supported);
    HENKA_TEST_ASSERT(strcmp(metadata.source_path, wav_path) == 0);
    HENKA_TEST_ASSERT(henka_assets_get_audio_metadata_for_path(
        manager, "build\\test_tmp\\audio_manager.wav", &metadata) ==
        HENKA_SUCCESS);
    HENKA_TEST_ASSERT(metadata.type == HENKA_ASSET_TYPE_AUDIO);
    HENKA_TEST_ASSERT(henka_assets_get_metadata_at_index(
        manager, 0U, &metadata) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(metadata.type == HENKA_ASSET_TYPE_AUDIO);
    HENKA_TEST_ASSERT(henka_audio_clip_get_info(clip, &clip_info) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(clip_info.frame_count == 64U);

    HENKA_TEST_ASSERT(test_write_wav_with_sample(wav_path, 96U, 16384));
    same_clip = NULL;
    HENKA_TEST_ASSERT(henka_assets_reload_audio_clip(
        manager,
        "build\\test_tmp\\.\\audio_manager.wav",
        &same_clip) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(same_clip == clip);
    HENKA_TEST_ASSERT(henka_audio_clip_get_info(clip, &reloaded_info) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(reloaded_info.frame_count == 96U);
    HENKA_TEST_ASSERT(reloaded_info.sample_rate == clip_info.sample_rate);

    {
        FILE* file = NULL;
        const unsigned char malformed[] = {'R', 'I', 'F', 'F', 0U, 0U, 0U, 0U};
#if defined(_WIN32)
        HENKA_TEST_ASSERT(fopen_s(&file, wav_path, "wb") == 0 && file != NULL);
#else
        file = fopen(wav_path, "wb");
#endif
        HENKA_TEST_ASSERT(file != NULL);
        HENKA_TEST_ASSERT(fwrite(malformed, 1U, sizeof(malformed), file) == sizeof(malformed));
        HENKA_TEST_ASSERT(fclose(file) == 0);
    }
    same_clip = (henka_audio_clip*)1;
    HENKA_TEST_ASSERT(henka_assets_reload_audio_clip(
        manager,
        wav_path,
        &same_clip) == HENKA_ERROR_ASSET_SOURCE);
    HENKA_TEST_ASSERT(same_clip == NULL);
    HENKA_TEST_ASSERT(henka_audio_clip_get_info(clip, &clip_info) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(clip_info.frame_count == 96U);
    HENKA_TEST_ASSERT(henka_assets_get_audio_metadata(
        manager, clip, &metadata) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(metadata.loaded && !metadata.fallback);
    HENKA_TEST_ASSERT(metadata.reload_supported);

    HENKA_TEST_ASSERT(henka_scene_create(&scene) == HENKA_SUCCESS);
    entity = henka_scene_create_entity_named(scene, "Manager Audio Object");
    HENKA_TEST_ASSERT(entity != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(henka_audio_system_create(NULL, &system) == HENKA_SUCCESS);
    emitter_config.enabled = true;
    emitter_config.looping = true;
    (void)snprintf(
        emitter_config.clip_path,
        sizeof(emitter_config.clip_path),
        "%s",
        wav_path);
    HENKA_TEST_ASSERT(henka_audio_emitter_create_with_clip(
        system,
        scene,
        entity,
        clip,
        &emitter_config,
        &emitter) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_audio_system_mix(system, samples, 1U) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(test_nonzero_mix(samples, 1U));

    henka_scene_destroy_entity(scene, entity);
    HENKA_TEST_ASSERT(henka_audio_system_mix(system, samples, 1U) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_audio_system_get_active_voice_count(system) == 0U);
    result = EXIT_SUCCESS;

    remove(wav_path);
    henka_audio_emitter_destroy(emitter);
    henka_audio_system_destroy(system);
    henka_scene_destroy(scene);
    henka_asset_manager_destroy(manager);
    return result;
}
