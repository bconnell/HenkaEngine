#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <henka/audio.h>

static const char test_compressed_ogg_base64[] =
#include "fixtures/audio_fixture_ogg.b64"
;
static const char test_compressed_mp3_base64[] =
#include "fixtures/audio_fixture_mp3.b64"
;
static const char test_compressed_flac_base64[] =
#include "fixtures/audio_fixture_flac.b64"
;

#define HENKA_TEST_ASSERT(condition) \
    do \
    { \
        if (!(condition)) \
        { \
            fprintf(stderr, "audio assertion failed at line %d: %s\\n", __LINE__, #condition); \
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

static int test_base64_value(char value)
{
    if (value >= 'A' && value <= 'Z')
    {
        return value - 'A';
    }
    if (value >= 'a' && value <= 'z')
    {
        return value - 'a' + 26;
    }
    if (value >= '0' && value <= '9')
    {
        return value - '0' + 52;
    }
    if (value == '+')
    {
        return 62;
    }
    if (value == '/')
    {
        return 63;
    }
    return -1;
}

static bool test_write_base64_file(const char* path, const char* encoded)
{
    FILE* file = NULL;
    size_t length;
    size_t index;
    bool success = true;

    if (path == NULL || encoded == NULL)
    {
        return false;
    }
    length = strlen(encoded);
    if (length == 0U || length % 4U != 0U)
    {
        return false;
    }
#if defined(_WIN32)
    if (fopen_s(&file, path, "wb") != 0)
    {
        file = NULL;
    }
#else
    file = fopen(path, "wb");
#endif
    if (file == NULL)
    {
        return false;
    }
    for (index = 0U; index < length && success; index += 4U)
    {
        const char first = encoded[index];
        const char second = encoded[index + 1U];
        const char third = encoded[index + 2U];
        const char fourth = encoded[index + 3U];
        const int first_value = test_base64_value(first);
        const int second_value = test_base64_value(second);
        const int third_value = third == '=' ? 0 : test_base64_value(third);
        const int fourth_value = fourth == '=' ? 0 : test_base64_value(fourth);
        const unsigned char bytes[3U] = {
            (unsigned char)((first_value << 2) | (second_value >> 4)),
            (unsigned char)((second_value << 4) | (third_value >> 2)),
            (unsigned char)((third_value << 6) | fourth_value)};
        size_t byte_count = 3U;

        if (first_value < 0 || second_value < 0 || third_value < 0 ||
            fourth_value < 0 || (third == '=' && fourth != '=') ||
            (index + 4U != length && (third == '=' || fourth == '=')))
        {
            success = false;
            break;
        }
        if (fourth == '=')
        {
            byte_count = 2U;
        }
        if (third == '=')
        {
            byte_count = 1U;
        }
        success = fwrite(bytes, 1U, byte_count, file) == byte_count;
    }
    if (fclose(file) != 0)
    {
        success = false;
    }
    return success;
}

static int test_compressed_format_contract(void)
{
    static const struct
    {
        const char* path;
        const char* base64;
    } cases[] = {
        {"build/test_tmp/audio_fixture.ogg", test_compressed_ogg_base64},
        {"build/test_tmp/audio_fixture.mp3", test_compressed_mp3_base64},
        {"build/test_tmp/audio_fixture.flac", test_compressed_flac_base64}};
    henka_audio_system* system = NULL;
    henka_scene* scene = NULL;
    henka_entity entity;
    henka_audio_voice_desc desc = henka_audio_voice_desc_default();
    float samples[HENKA_AUDIO_OUTPUT_CHANNELS * 4U] = {0.0f};
    const unsigned char malformed[] = {'O', 'g', 'g', 'S', 0U, 0U, 0U, 0U};
    size_t index;

    HENKA_TEST_ASSERT(henka_scene_create(&scene) == HENKA_SUCCESS);
    entity = henka_scene_create_entity_named(scene, "Compressed Audio Object");
    HENKA_TEST_ASSERT(entity != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(henka_audio_system_create(NULL, &system) == HENKA_SUCCESS);
    desc.spatial = false;
    for (index = 0U; index < sizeof(cases) / sizeof(cases[0]); ++index)
    {
        henka_audio_clip* clip = NULL;
        henka_audio_stream* stream = NULL;
        henka_audio_clip_info clip_info;
        henka_audio_stream_info stream_info;
        henka_audio_voice_id voice = HENKA_INVALID_AUDIO_VOICE_ID;
        size_t frames_read = 0U;

        HENKA_TEST_ASSERT(test_write_base64_file(cases[index].path, cases[index].base64));
        HENKA_TEST_ASSERT(henka_audio_clip_load_file(
            ".", cases[index].path, &clip) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(henka_audio_clip_get_info(clip, &clip_info) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(clip_info.resident && clip_info.channels >= 1U &&
            clip_info.channels <= HENKA_AUDIO_OUTPUT_CHANNELS &&
            clip_info.sample_rate >= 8000U && clip_info.sample_rate <= 192000U &&
            clip_info.bits_per_sample == 32U && clip_info.frame_count > 0U);
        HENKA_TEST_ASSERT(henka_audio_voice_play(
            system, scene, entity, clip, &desc, &voice) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(henka_audio_system_mix(system, samples, 1U) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(henka_audio_voice_stop(system, voice) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(henka_audio_stream_load_file(
            ".", cases[index].path, &stream) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(henka_audio_stream_get_info(
            stream, &stream_info) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(!stream_info.resident &&
            stream_info.channels == clip_info.channels &&
            stream_info.sample_rate == clip_info.sample_rate &&
            stream_info.frame_count == clip_info.frame_count);
        HENKA_TEST_ASSERT(henka_audio_stream_read_frames(
            stream,
            0U,
            4U,
            samples,
            4U,
            &frames_read) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(frames_read > 0U);
        HENKA_TEST_ASSERT(henka_audio_voice_play_stream(
            system, scene, entity, stream, &desc, &voice) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(henka_audio_system_mix(system, samples, 1U) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(henka_audio_voice_stop(system, voice) == HENKA_SUCCESS);
        henka_audio_stream_destroy(stream);
        henka_audio_clip_destroy(clip);
    }
    HENKA_TEST_ASSERT(test_write_bytes(
        "build/test_tmp/audio_malformed.ogg",
        malformed,
        sizeof(malformed)));
    {
        henka_audio_clip* malformed_clip = (henka_audio_clip*)1;
        henka_audio_stream* malformed_stream = (henka_audio_stream*)1;
        HENKA_TEST_ASSERT(henka_audio_clip_load_file(
            ".",
            "build/test_tmp/audio_malformed.ogg",
            &malformed_clip) == HENKA_ERROR_ASSET_SOURCE);
        HENKA_TEST_ASSERT(malformed_clip == NULL);
        HENKA_TEST_ASSERT(henka_audio_stream_load_file(
            ".",
            "build/test_tmp/audio_malformed.ogg",
            &malformed_stream) == HENKA_ERROR_ASSET_SOURCE);
        HENKA_TEST_ASSERT(malformed_stream == NULL);
    }
    henka_audio_system_destroy(system);
    henka_scene_destroy(scene);
    return EXIT_SUCCESS;
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

static int test_streaming_wav_contract(const char* wav_path)
{
    henka_audio_stream* stream = NULL;
    henka_audio_stream_info info;
    henka_audio_system* system = NULL;
    henka_audio_voice_desc desc = henka_audio_voice_desc_default();
    henka_audio_voice_info voice_info;
    henka_audio_voice_id voice = HENKA_INVALID_AUDIO_VOICE_ID;
    henka_audio_emitter_config emitter_config = henka_audio_emitter_config_default();
    henka_audio_emitter* emitter = NULL;
    henka_audio_emitter* owned_emitter = NULL;
    henka_scene* scene = NULL;
    henka_entity entity;
    float samples[8U] = {0.0f};
    float long_mix[HENKA_AUDIO_OUTPUT_CHANNELS * 2050U] = {0.0f};
    size_t frames_read = 0U;

    HENKA_TEST_ASSERT(henka_audio_stream_load_file(
        ".", wav_path, &stream) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(stream != NULL);
    HENKA_TEST_ASSERT(henka_audio_stream_get_info(
        stream, &info) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(!info.resident);
    HENKA_TEST_ASSERT(info.frame_count == 8192U);
    HENKA_TEST_ASSERT(henka_audio_stream_read_frames(
        stream, 0U, 4U, samples, 4U, &frames_read) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(frames_read == 4U);
    HENKA_TEST_ASSERT(samples[0] > 0.2f && samples[0] < 0.3f);
    HENKA_TEST_ASSERT(henka_audio_stream_read_frames(
        stream, info.frame_count, 1U, samples, 4U, &frames_read) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(frames_read == 0U);
    HENKA_TEST_ASSERT(henka_audio_stream_read_frames(
        stream, info.frame_count + 1U, 1U, samples, 4U, &frames_read) ==
        HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_audio_stream_read_frames(
        stream, 0U, 5U, samples, 4U, &frames_read) ==
        HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_scene_create(&scene) == HENKA_SUCCESS);
    entity = henka_scene_create_entity_named(scene, "Stream Audio Object");
    HENKA_TEST_ASSERT(entity != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(henka_audio_system_create(NULL, &system) == HENKA_SUCCESS);
    desc.looping = true;
    HENKA_TEST_ASSERT(henka_audio_voice_play_stream(
        system, scene, entity, stream, &desc, &voice) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_audio_system_mix(system, samples, 1U) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(test_nonzero_mix(samples, 1U));
    HENKA_TEST_ASSERT(henka_audio_system_mix(system, long_mix, 2050U) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(test_nonzero_mix(long_mix, 2050U));
    HENKA_TEST_ASSERT(henka_audio_voice_get_info(
        system, voice, &voice_info) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(voice_info.stream == stream && voice_info.clip == NULL);
    HENKA_TEST_ASSERT(henka_audio_voice_seek(system, voice, info.frame_count) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_audio_system_mix(system, samples, 1U) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_audio_voice_is_valid(system, voice));
    HENKA_TEST_ASSERT(henka_audio_voice_stop(system, voice) == HENKA_SUCCESS);
    emitter_config.enabled = true;
    (void)snprintf(
        emitter_config.clip_path,
        sizeof(emitter_config.clip_path),
        "%s",
        wav_path);
    HENKA_TEST_ASSERT(henka_audio_emitter_create_with_stream(
        system,
        scene,
        entity,
        stream,
        &emitter_config,
        &emitter) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_audio_emitter_is_playing(emitter));
    henka_audio_emitter_destroy(emitter);
    HENKA_TEST_ASSERT(henka_audio_emitter_create_stream(
        system,
        ".",
        scene,
        entity,
        &emitter_config,
        &owned_emitter) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_audio_emitter_is_playing(owned_emitter));
    henka_audio_emitter_destroy(owned_emitter);
    henka_audio_stream_destroy(stream);
    henka_audio_system_destroy(system);
    henka_scene_destroy(scene);
    return EXIT_SUCCESS;
}

int main(void)
{
    const char* wav_path = "build/test_tmp/audio_foundation.wav";
    const char* stream_path = "build/test_tmp/audio_stream.wav";
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
    henka_audio_emitter_config emitter_config = henka_audio_emitter_config_default();
    henka_audio_emitter* emitter = NULL;
    henka_audio_listener listener = henka_audio_listener_default();
    henka_audio_voice_id voice = HENKA_INVALID_AUDIO_VOICE_ID;
    henka_audio_voice_id stopped_voice = HENKA_INVALID_AUDIO_VOICE_ID;
    henka_audio_voice_id replacement_voice = HENKA_INVALID_AUDIO_VOICE_ID;
    henka_audio_voice_id non_spatial_voice = HENKA_INVALID_AUDIO_VOICE_ID;
    float samples[HENKA_AUDIO_OUTPUT_CHANNELS * 2U];
    float right_position_left;
    float right_position_right;
    float left_position_left;
    float left_position_right;
    float near_sum;
    float far_sum;
    size_t paused_source_frame;
    henka_audio_voice_info emitter_info;
    henka_audio_diagnostics diagnostics;
    int result = EXIT_FAILURE;

    HENKA_TEST_ASSERT(test_write_real_wav(wav_path, 128U));
    HENKA_TEST_ASSERT(test_write_real_wav(stream_path, 8192U));
    HENKA_TEST_ASSERT(test_compressed_format_contract() == EXIT_SUCCESS);
    HENKA_TEST_ASSERT(test_streaming_wav_contract(stream_path) == EXIT_SUCCESS);
    HENKA_TEST_ASSERT(test_write_bytes(malformed_path, malformed, sizeof(malformed)));
    {
        henka_audio_stream* malformed_stream = (henka_audio_stream*)1;
        HENKA_TEST_ASSERT(henka_audio_stream_load_file(
            ".", malformed_path, &malformed_stream) == HENKA_ERROR_ASSET_SOURCE);
        HENKA_TEST_ASSERT(malformed_stream == NULL);
    }
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
    emitter_config.enabled = true;
    emitter_config.looping = true;
    (void)snprintf(
        emitter_config.clip_path,
        sizeof(emitter_config.clip_path),
        "%s",
        wav_path);
    HENKA_TEST_ASSERT(henka_audio_emitter_create(
        system,
        ".",
        scene,
        entity,
        &emitter_config,
        &emitter) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_audio_emitter_is_valid(emitter));
    HENKA_TEST_ASSERT(henka_audio_emitter_is_playing(emitter));
    HENKA_TEST_ASSERT(henka_audio_emitter_get_voice_info(
        emitter,
        &emitter_info) == HENKA_SUCCESS);
    voice = emitter_info.id;
    HENKA_TEST_ASSERT(henka_audio_voice_is_valid(system, voice));
    HENKA_TEST_ASSERT(henka_audio_emitter_seek(emitter, 64U) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_audio_emitter_play(emitter) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_audio_emitter_get_voice_info(
        emitter,
        &emitter_info) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(emitter_info.source_frame == 64U);
    HENKA_TEST_ASSERT(henka_audio_emitter_restart(emitter) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_audio_emitter_get_voice_info(
        emitter,
        &emitter_info) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(emitter_info.source_frame == 0U);
    HENKA_TEST_ASSERT(henka_audio_emitter_stop(emitter) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(!henka_audio_emitter_is_playing(emitter));
    HENKA_TEST_ASSERT(henka_audio_system_get_active_voice_count(system) == 0U);
    HENKA_TEST_ASSERT(henka_audio_emitter_restart(emitter) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_audio_emitter_is_playing(emitter));
    HENKA_TEST_ASSERT(henka_audio_emitter_pause(emitter) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_audio_emitter_resume(emitter) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_audio_emitter_set_gain(emitter, 0.5f) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_audio_emitter_set_pitch(emitter, 1.25f) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_audio_emitter_set_looping(emitter, false) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_audio_emitter_set_spatial(emitter, false) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_audio_emitter_set_bus(
        emitter, HENKA_AUDIO_BUS_DIALOGUE) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_audio_emitter_seek(emitter, 64U) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_audio_emitter_get_voice_info(
        emitter,
        &emitter_info) == HENKA_SUCCESS);
    voice = emitter_info.id;
    HENKA_TEST_ASSERT(emitter_info.gain == 0.5f && emitter_info.pitch == 1.25f &&
        !emitter_info.looping && !emitter_info.spatial &&
        emitter_info.bus == HENKA_AUDIO_BUS_DIALOGUE &&
        emitter_info.source_frame == 64U);
    HENKA_TEST_ASSERT(henka_audio_emitter_set_gain(emitter, 1.0f) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_audio_emitter_set_pitch(emitter, 1.0f) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_audio_emitter_set_looping(emitter, true) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_audio_emitter_set_spatial(emitter, true) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_audio_emitter_set_bus(
        emitter, HENKA_AUDIO_BUS_SFX) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_audio_emitter_stop(emitter) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_audio_emitter_play(emitter) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_audio_emitter_get_voice_info(
        emitter,
        &emitter_info) == HENKA_SUCCESS);
    voice = emitter_info.id;
    HENKA_TEST_ASSERT(emitter_info.gain == 1.0f && emitter_info.pitch == 1.0f &&
        emitter_info.looping && emitter_info.spatial &&
        emitter_info.bus == HENKA_AUDIO_BUS_SFX);
    HENKA_TEST_ASSERT(henka_audio_voice_pause(system, voice) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_audio_voice_get_info(
        system, voice, &emitter_info) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(emitter_info.paused);
    HENKA_TEST_ASSERT(emitter_info.source_frame == 0U);
    HENKA_TEST_ASSERT(henka_audio_system_mix(system, samples, 1U) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(!test_nonzero_mix(samples, 1U));
    HENKA_TEST_ASSERT(henka_audio_voice_resume(system, voice) == HENKA_SUCCESS);
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
    HENKA_TEST_ASSERT(henka_audio_voice_pause(system, voice) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_audio_voice_is_paused(system, voice));
    HENKA_TEST_ASSERT(henka_audio_voice_get_info(
        system, voice, &emitter_info) == HENKA_SUCCESS);
    paused_source_frame = emitter_info.source_frame;
    HENKA_TEST_ASSERT(paused_source_frame > 0U);
    HENKA_TEST_ASSERT(henka_audio_system_mix(system, samples, 1U) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(!test_nonzero_mix(samples, 1U));
    HENKA_TEST_ASSERT(henka_audio_voice_get_info(
        system, voice, &emitter_info) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(emitter_info.source_frame == paused_source_frame);
    HENKA_TEST_ASSERT(henka_audio_voice_resume(system, voice) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(!henka_audio_voice_is_paused(system, voice));
    HENKA_TEST_ASSERT(henka_audio_voice_seek(system, voice, 64U) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_audio_voice_set_gain(system, voice, 0.5f) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_audio_voice_set_pitch(system, voice, 1.25f) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_audio_voice_get_info(
        system, voice, &emitter_info) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(emitter_info.source_frame == 64U);
    HENKA_TEST_ASSERT(emitter_info.gain == 0.5f);
    HENKA_TEST_ASSERT(emitter_info.pitch == 1.25f);
    HENKA_TEST_ASSERT(henka_audio_voice_restart(system, voice) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_audio_voice_get_info(
        system, voice, &emitter_info) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(!emitter_info.paused && emitter_info.source_frame == 0U);
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
    HENKA_TEST_ASSERT(!henka_audio_emitter_is_valid(emitter));
    HENKA_TEST_ASSERT(henka_audio_emitter_stop(emitter) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_audio_emitter_restart(emitter) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_audio_emitter_get_voice_info(
        emitter,
        &emitter_info) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_audio_system_get_active_voice_count(system) == 0U);
    HENKA_TEST_ASSERT(henka_audio_system_get_diagnostics(system, &diagnostics) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(diagnostics.stale_voice_count == 1U);
    second_entity = henka_scene_create_entity_named(scene, "Audio generation object");
    HENKA_TEST_ASSERT(second_entity != HENKA_INVALID_ENTITY);
    desc.spatial = false;
    HENKA_TEST_ASSERT(henka_scene_set_entity_transform(
        scene,
        second_entity,
        (henka_transform){
            (henka_vec3){100.0f, 0.0f, 0.0f},
            (henka_quat){0.0f, 0.0f, 0.0f, 1.0f},
            (henka_vec3){1.0f, 1.0f, 1.0f}}) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_audio_voice_play(
        system, scene, second_entity, clip, &desc, &non_spatial_voice) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_audio_system_mix(system, samples, 1U) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(samples[0] > 0.15f && samples[1] > 0.15f);
    HENKA_TEST_ASSERT(fabsf(samples[0] - samples[1]) < 0.0001f);
    HENKA_TEST_ASSERT(henka_audio_voice_stop(system, non_spatial_voice) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_audio_voice_play(
        system, scene, second_entity, clip, &desc, &stopped_voice) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_audio_voice_stop(system, stopped_voice) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_audio_voice_play(
        system, scene, second_entity, clip, &desc, &replacement_voice) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(replacement_voice != stopped_voice);
    HENKA_TEST_ASSERT(henka_audio_voice_stop(system, stopped_voice) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_audio_voice_pause(
        system, stopped_voice) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_audio_voice_resume(
        system, stopped_voice) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_audio_voice_restart(
        system, stopped_voice) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_audio_voice_seek(
        system, stopped_voice, 0U) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_audio_voice_set_gain(
        system, stopped_voice, 1.0f) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_audio_voice_set_pitch(
        system, stopped_voice, 1.0f) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(!henka_audio_voice_is_paused(system, stopped_voice));
    HENKA_TEST_ASSERT(henka_audio_voice_get_info(
        system, replacement_voice, &emitter_info) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_audio_voice_set_gain(
        system, replacement_voice, -0.1f) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_audio_voice_set_pitch(
        system, replacement_voice, 0.0f) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_audio_voice_seek(
        system, replacement_voice, clip_info.frame_count + 1U) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_audio_voice_get_info(
        system, replacement_voice, &emitter_info) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(emitter_info.gain == desc.gain && emitter_info.pitch == desc.pitch &&
        emitter_info.source_frame == 0U);
    HENKA_TEST_ASSERT(henka_audio_voice_stop(system, replacement_voice) == HENKA_SUCCESS);
    result = EXIT_SUCCESS;

    remove(wav_path);
    remove(stream_path);
    remove(malformed_path);
    henka_audio_emitter_destroy(emitter);
    henka_audio_system_destroy(system);
    henka_scene_destroy(scene);
    henka_audio_clip_destroy(clip);
    if (result == EXIT_SUCCESS)
    {
        puts("audio foundation tests passed");
    }
    return result;
}
