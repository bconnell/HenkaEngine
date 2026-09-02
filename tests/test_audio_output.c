#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL.h>

#include <henka/audio.h>
#include <henka/audio_output.h>
#include <henka/scene.h>

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

static bool test_write_audio_fixture(const char* path)
{
    const size_t frame_count = 256U;
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
        test_write_u16(bytes, 44U + frame_index * 2U, 8192U);
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

#define HENKA_TEST_ASSERT(condition) \
    do \
    { \
        if (!(condition)) \
        { \
            fprintf(stderr, "audio output assertion failed: %s\n", #condition); \
            exit_code = EXIT_FAILURE; \
            goto cleanup; \
        } \
    } while (0)

int main(void)
{
    const char* audio_path = "build/test_tmp/audio_output.wav";
    henka_scene* scene = NULL;
    henka_audio_system* system = NULL;
    henka_audio_emitter* emitter = NULL;
    henka_audio_output* output = NULL;
    henka_entity entity = HENKA_INVALID_ENTITY;
    henka_audio_emitter_config emitter_config = henka_audio_emitter_config_default();
    henka_audio_output_info info;
    henka_audio_output_config output_config = henka_audio_output_config_default();
    henka_audio_output_config invalid_config;
    int exit_code = EXIT_FAILURE;

    HENKA_TEST_ASSERT(test_write_audio_fixture(audio_path));
#if defined(_WIN32)
    HENKA_TEST_ASSERT(_putenv_s("SDL_AUDIODRIVER", "dummy") == 0);
#endif
    HENKA_TEST_ASSERT(henka_scene_create(&scene) == HENKA_SUCCESS);
    entity = henka_scene_create_entity_named(scene, "Audio output object");
    HENKA_TEST_ASSERT(entity != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(henka_audio_system_create(
        &(henka_audio_system_config){0U}, &system) == HENKA_SUCCESS);
    emitter_config.enabled = true;
    emitter_config.looping = true;
    (void)snprintf(
        emitter_config.clip_path,
        sizeof(emitter_config.clip_path),
        "%s",
        audio_path);
    HENKA_TEST_ASSERT(henka_audio_emitter_create(
        system, ".", scene, entity, &emitter_config, &emitter) == HENKA_SUCCESS);
    invalid_config = output_config;
    invalid_config.max_pump_frames = HENKA_AUDIO_OUTPUT_MAX_PUMP_FRAMES + 1U;
    HENKA_TEST_ASSERT(henka_audio_output_create(
        system, scene, &invalid_config, &output) == HENKA_ERROR_LIMIT);
    HENKA_TEST_ASSERT(output == NULL);
    output_config.max_pump_frames = 128U;
    output_config.max_queued_frames = 256U;
    HENKA_TEST_ASSERT(henka_audio_output_create(
        system, scene, &output_config, &output) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_audio_output_pump(output, 64U) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_audio_output_get_info(output, &info) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(info.device_open);
    HENKA_TEST_ASSERT(info.sample_rate == HENKA_AUDIO_DEFAULT_SAMPLE_RATE);
    HENKA_TEST_ASSERT(!info.recovery_pending && info.recovery_attempts == 0U &&
        !info.recovery_exhausted);
    HENKA_TEST_ASSERT(info.pumped_frames == 64U);
    HENKA_TEST_ASSERT(info.queued_frames > 0U);
    HENKA_TEST_ASSERT(henka_audio_output_recover(output) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_audio_output_get_info(output, &info) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(info.device_open);
    HENKA_TEST_ASSERT(info.queued_frames == 0U);
    HENKA_TEST_ASSERT(info.pumped_frames == 64U);
    {
        const uint32_t device_event_types[] = {
            SDL_EVENT_AUDIO_DEVICE_ADDED,
            SDL_EVENT_AUDIO_DEVICE_REMOVED,
            SDL_EVENT_AUDIO_DEVICE_FORMAT_CHANGED};
        size_t event_index;
        for (event_index = 0U;
            event_index < sizeof(device_event_types) / sizeof(device_event_types[0]);
            ++event_index)
        {
            SDL_Event device_event = {0};
            device_event.type = device_event_types[event_index];
            device_event.adevice.recording = false;
            HENKA_TEST_ASSERT(SDL_PushEvent(&device_event) == 1);
            HENKA_TEST_ASSERT(henka_audio_output_get_info(output, &info) == HENKA_SUCCESS);
            HENKA_TEST_ASSERT(info.recovery_pending);
            HENKA_TEST_ASSERT(henka_audio_output_pump(output, 32U) == HENKA_SUCCESS);
            HENKA_TEST_ASSERT(henka_audio_output_get_info(output, &info) == HENKA_SUCCESS);
            HENKA_TEST_ASSERT(info.device_open && !info.recovery_pending &&
                info.recovery_attempts == 0U && !info.recovery_exhausted);
        }
        {
            SDL_Event recording_event = {0};
            recording_event.type = SDL_EVENT_AUDIO_DEVICE_REMOVED;
            recording_event.adevice.recording = true;
            HENKA_TEST_ASSERT(SDL_PushEvent(&recording_event) == 1);
            HENKA_TEST_ASSERT(henka_audio_output_get_info(output, &info) == HENKA_SUCCESS);
            HENKA_TEST_ASSERT(!info.recovery_pending);
        }
    }
    HENKA_TEST_ASSERT(henka_audio_output_get_info(output, &info) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(info.pumped_frames == 160U);
    HENKA_TEST_ASSERT(info.queued_frames > 0U);
    HENKA_TEST_ASSERT(henka_audio_output_pump(output, 129U) == HENKA_ERROR_LIMIT);
    HENKA_TEST_ASSERT(henka_audio_output_get_info(output, &info) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(info.rejected_frames == 129U);
    exit_code = EXIT_SUCCESS;

cleanup:
    henka_audio_output_destroy(output);
    henka_audio_emitter_destroy(emitter);
    henka_audio_system_destroy(system);
    henka_scene_destroy(scene);
    remove(audio_path);
    if (exit_code == EXIT_SUCCESS)
    {
        puts("audio output tests passed");
    }
    return exit_code;
}
