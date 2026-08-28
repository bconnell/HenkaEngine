#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <henka/audio.h>

#define HENKA_TEST_ASSERT(condition) \
    do \
    { \
        if (!(condition)) \
        { \
            fprintf(stderr, "audio assertion failed: %s\\n", #condition); \
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

static bool test_write_real_wav(const char* path, size_t frame_count)
{
    const size_t data_size = frame_count * sizeof(int16_t);
    const size_t file_size = 44U + data_size;
    unsigned char* bytes = (unsigned char*)calloc(1U, file_size);
    FILE* file;
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

static bool test_write_bytes(const char* path, const unsigned char* bytes, size_t size)
{
    FILE* file;
    bool success;
#if defined(_WIN32)
    if (fopen_s(&file, path, "wb") != 0)
    {
        file = NULL;
    }
#else
    file = fopen(path, "wb");
#endif
    success = file != NULL && fwrite(bytes, 1U, size, file) == size;
    if (file != NULL && fclose(file) != 0)
    {
        success = false;
    }
    return success;
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
    const char* wav_path = "build/test_tmp/audio_foundation.wav";
    const char* malformed_path = "build/test_tmp/audio_malformed.wav";
    const unsigned char malformed[] = {'R', 'I', 'F', 'F', 0U, 0U, 0U, 0U};
    henka_audio_clip* clip = NULL;
    henka_audio_clip* malformed_clip = NULL;
    henka_audio_clip_info clip_info;
    henka_audio_system* system = NULL;
    henka_audio_system_config config = {0U};
    henka_scene* scene = NULL;
    henka_entity entity;
    henka_entity second_entity;
    henka_audio_voice_desc desc = henka_audio_voice_desc_default();
    henka_audio_listener listener = henka_audio_listener_default();
    henka_audio_voice_id voice = HENKA_INVALID_AUDIO_VOICE_ID;
    henka_audio_voice_id stopped_voice = HENKA_INVALID_AUDIO_VOICE_ID;
    henka_audio_voice_id replacement_voice = HENKA_INVALID_AUDIO_VOICE_ID;
    float samples[HENKA_AUDIO_OUTPUT_CHANNELS * 2U];
    float right_position_left;
    float right_position_right;
    float left_position_left;
    float left_position_right;
    float near_sum;
    float far_sum;
    henka_audio_diagnostics diagnostics;
    int result = EXIT_FAILURE;

    HENKA_TEST_ASSERT(test_write_real_wav(wav_path, 128U));
    HENKA_TEST_ASSERT(test_write_bytes(malformed_path, malformed, sizeof(malformed)));
    HENKA_TEST_ASSERT(henka_audio_clip_load_file(
        ".", "../audio_foundation.wav", &clip) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(clip == NULL);
    HENKA_TEST_ASSERT(henka_audio_clip_load_file(
        ".", wav_path, &clip) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_audio_clip_get_info(clip, &clip_info) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(clip_info.sample_rate == HENKA_AUDIO_DEFAULT_SAMPLE_RATE);
    HENKA_TEST_ASSERT(clip_info.channels == 1U);
    HENKA_TEST_ASSERT(clip_info.bits_per_sample == 16U);
    HENKA_TEST_ASSERT(clip_info.frame_count == 128U);
    HENKA_TEST_ASSERT(henka_audio_clip_load_file(
        ".", malformed_path, &malformed_clip) == HENKA_ERROR_ASSET_SOURCE);
    HENKA_TEST_ASSERT(malformed_clip == NULL);
    HENKA_TEST_ASSERT(henka_scene_create(&scene) == HENKA_SUCCESS);
    entity = henka_scene_create_entity_named(scene, "Audio production object");
    HENKA_TEST_ASSERT(entity != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(henka_scene_set_entity_transform(
        scene,
        entity,
        (henka_transform){
            (henka_vec3){1.0f, 0.0f, -4.0f},
            (henka_quat){0.0f, 0.0f, 0.0f, 1.0f},
            (henka_vec3){1.0f, 1.0f, 1.0f}}) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_audio_system_create(&config, &system) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_audio_system_set_listener(system, listener) == HENKA_SUCCESS);
    desc.looping = true;
    HENKA_TEST_ASSERT(henka_audio_voice_play(
        system, scene, entity, clip, &desc, &voice) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_audio_voice_is_valid(system, voice));
    HENKA_TEST_ASSERT(henka_audio_system_mix(system, samples, 1U) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(test_nonzero_mix(samples, 1U));
    right_position_left = samples[0];
    right_position_right = samples[1];
    HENKA_TEST_ASSERT(right_position_right > right_position_left);
    HENKA_TEST_ASSERT(henka_scene_translate_entity(
        scene, entity, (henka_vec3){-2.0f, 0.0f, 0.0f}) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_audio_system_mix(system, samples, 1U) == HENKA_SUCCESS);
    left_position_left = samples[0];
    left_position_right = samples[1];
    HENKA_TEST_ASSERT(left_position_left > left_position_right);
    HENKA_TEST_ASSERT(henka_scene_translate_entity(
        scene, entity, (henka_vec3){1.0f, 0.0f, 0.0f}) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_audio_system_mix(system, samples, 1U) == HENKA_SUCCESS);
    far_sum = fabsf(samples[0]) + fabsf(samples[1]);
    listener.position = (henka_vec3){0.0f, 0.0f, -3.0f};
    HENKA_TEST_ASSERT(henka_audio_system_set_listener(system, listener) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_audio_system_mix(system, samples, 1U) == HENKA_SUCCESS);
    near_sum = fabsf(samples[0]) + fabsf(samples[1]);
    HENKA_TEST_ASSERT(near_sum > far_sum);
    HENKA_TEST_ASSERT(henka_audio_system_set_bus_gain(
        system, HENKA_AUDIO_BUS_SFX, 0.0f) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_audio_system_mix(system, samples, 1U) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(!test_nonzero_mix(samples, 1U));
    HENKA_TEST_ASSERT(henka_audio_system_set_bus_gain(
        system, HENKA_AUDIO_BUS_SFX, 1.0f) == HENKA_SUCCESS);
    henka_scene_destroy_entity(scene, entity);
    HENKA_TEST_ASSERT(henka_audio_system_mix(system, samples, 1U) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(!henka_audio_voice_is_valid(system, voice));
    HENKA_TEST_ASSERT(henka_audio_system_get_active_voice_count(system) == 0U);
    HENKA_TEST_ASSERT(henka_audio_system_get_diagnostics(system, &diagnostics) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(diagnostics.stale_voice_count == 1U);
    second_entity = henka_scene_create_entity_named(scene, "Audio generation object");
    HENKA_TEST_ASSERT(second_entity != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(henka_audio_voice_play(
        system, scene, second_entity, clip, &desc, &stopped_voice) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_audio_voice_stop(system, stopped_voice) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_audio_voice_play(
        system, scene, second_entity, clip, &desc, &replacement_voice) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(replacement_voice != stopped_voice);
    HENKA_TEST_ASSERT(henka_audio_voice_stop(system, stopped_voice) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_audio_voice_stop(system, replacement_voice) == HENKA_SUCCESS);
    result = EXIT_SUCCESS;

    remove(wav_path);
    remove(malformed_path);
    henka_audio_system_destroy(system);
    henka_scene_destroy(scene);
    henka_audio_clip_destroy(clip);
    if (result == EXIT_SUCCESS)
    {
        puts("audio foundation tests passed");
    }
    return result;
}
