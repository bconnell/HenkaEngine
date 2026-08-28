#include <henka/audio.h>

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <henka/memory.h>
#include <henka/persistence.h>

#define HENKA_AUDIO_MIN_SAMPLE_RATE 8000U
#define HENKA_AUDIO_MAX_SAMPLE_RATE 192000U
#define HENKA_AUDIO_MAX_GAIN 8.0f
#define HENKA_AUDIO_MIN_PITCH 0.25f
#define HENKA_AUDIO_MAX_PITCH 4.0f
#define HENKA_AUDIO_FILE_HEADER_BYTES 12U
#define HENKA_AUDIO_CHUNK_HEADER_BYTES 8U

typedef struct henka_audio_voice_slot
{
    uint32_t generation;
    bool active;
    henka_scene* scene;
    henka_entity entity;
    const henka_audio_clip* clip;
    henka_audio_voice_desc desc;
    double source_position;
    bool paused;
} henka_audio_voice_slot;

struct henka_audio_clip
{
    char* source_path;
    uint32_t sample_rate;
    uint16_t channels;
    uint16_t bits_per_sample;
    size_t frame_count;
    float* samples;
};

struct henka_audio_system
{
    uint32_t output_sample_rate;
    henka_audio_listener listener;
    float bus_gains[HENKA_AUDIO_BUS_COUNT];
    henka_audio_voice_slot voices[HENKA_AUDIO_MAX_VOICES];
    uint32_t active_voice_count;
    uint32_t stale_voice_count;
    uint64_t mixed_frame_count;
    bool output_clipped;
};

struct henka_audio_emitter
{
    henka_audio_system* system;
    henka_scene* scene;
    henka_entity entity;
    henka_audio_clip* clip;
    bool owns_clip;
    henka_audio_emitter_config config;
    henka_audio_voice_id voice;
};

static bool henka_audio_bus_is_valid(henka_audio_bus bus)
{
    return bus >= HENKA_AUDIO_BUS_MASTER && bus < HENKA_AUDIO_BUS_COUNT;
}

static bool henka_audio_float_is_valid(float value)
{
    return isfinite(value) != 0;
}

static bool henka_audio_string_is_valid(
    const char* value,
    size_t capacity)
{
    size_t index;
    if (value == NULL || capacity == 0U)
    {
        return false;
    }
    for (index = 0U; index < capacity; ++index)
    {
        if (value[index] == '\0')
        {
            return true;
        }
    }
    return false;
}

static bool henka_audio_vec3_is_finite(henka_vec3 value)
{
    return henka_audio_float_is_valid(value.x) &&
        henka_audio_float_is_valid(value.y) &&
        henka_audio_float_is_valid(value.z);
}

static bool henka_audio_size_multiply(size_t left, size_t right, size_t* out_value)
{
    if (out_value == NULL || (right != 0U && left > SIZE_MAX / right))
    {
        return false;
    }
    *out_value = left * right;
    return true;
}

static uint16_t henka_audio_read_u16(const unsigned char* bytes)
{
    return (uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8U);
}

static uint32_t henka_audio_read_u32(const unsigned char* bytes)
{
    return (uint32_t)bytes[0] |
        ((uint32_t)bytes[1] << 8U) |
        ((uint32_t)bytes[2] << 16U) |
        ((uint32_t)bytes[3] << 24U);
}

static int32_t henka_audio_read_s24(const unsigned char* bytes)
{
    int32_t value = (int32_t)bytes[0] |
        ((int32_t)bytes[1] << 8U) |
        ((int32_t)bytes[2] << 16U);
    if ((value & 0x00800000) != 0)
    {
        value |= (int32_t)0xff000000;
    }
    return value;
}

static FILE* henka_audio_open_file(const char* path, const char* mode)
{
    FILE* file = NULL;
#if defined(_WIN32)
    if (path == NULL || mode == NULL || fopen_s(&file, path, mode) != 0)
    {
        return NULL;
    }
#else
    file = fopen(path, mode);
#endif
    return file;
}

static bool henka_audio_read_file(
    const char* path,
    unsigned char** out_bytes,
    size_t* out_size)
{
    FILE* file = NULL;
    long file_size;
    unsigned char* bytes = NULL;
    size_t size;
    bool success = false;

    if (path == NULL || out_bytes == NULL || out_size == NULL ||
        path[0] == '\0')
    {
        return false;
    }
    *out_bytes = NULL;
    *out_size = 0U;
    file = henka_audio_open_file(path, "rb");
    if (file == NULL ||
        fseek(file, 0L, SEEK_END) != 0)
    {
        if (file != NULL)
        {
            fclose(file);
        }
        return false;
    }
    file_size = ftell(file);
    if (file_size <= 0L ||
        (unsigned long long)file_size > HENKA_AUDIO_MAX_CLIP_BYTES ||
        fseek(file, 0L, SEEK_SET) != 0)
    {
        fclose(file);
        return false;
    }
    size = (size_t)file_size;
    bytes = (unsigned char*)henka_malloc(size);
    if (bytes != NULL && fread(bytes, 1U, size, file) == size)
    {
        success = true;
        *out_bytes = bytes;
        *out_size = size;
    }
    else
    {
        henka_free(bytes);
    }
    fclose(file);
    return success;
}

static bool henka_audio_chunk_is(
    const unsigned char* bytes,
    const char* text)
{
    return memcmp(bytes, text, 4U) == 0;
}

static bool henka_audio_parse_wav(
    const unsigned char* bytes,
    size_t byte_count,
    henka_audio_clip* clip)
{
    const unsigned char* format_bytes = NULL;
    const unsigned char* data_bytes = NULL;
    uint32_t data_size = 0U;
    size_t offset = HENKA_AUDIO_FILE_HEADER_BYTES;
    size_t format_size = 0U;
    bool found_format = false;
    bool found_data = false;
    uint16_t format_tag;
    uint16_t block_align;
    size_t sample_count;
    size_t sample_bytes;
    size_t frame_count;
    size_t sample_index;

    if (bytes == NULL || clip == NULL || byte_count < HENKA_AUDIO_FILE_HEADER_BYTES ||
        !henka_audio_chunk_is(bytes, "RIFF") ||
        henka_audio_read_u32(bytes + 4U) < 4U ||
        (size_t)henka_audio_read_u32(bytes + 4U) > byte_count - 8U ||
        !henka_audio_chunk_is(bytes + 8U, "WAVE"))
    {
        return false;
    }
    while (offset <= byte_count && byte_count - offset >= HENKA_AUDIO_CHUNK_HEADER_BYTES)
    {
        const uint32_t chunk_size = henka_audio_read_u32(bytes + offset + 4U);
        const size_t chunk_data_offset = offset + HENKA_AUDIO_CHUNK_HEADER_BYTES;
        size_t next_offset;
        if ((size_t)chunk_size > byte_count - chunk_data_offset)
        {
            return false;
        }
        if (henka_audio_chunk_is(bytes + offset, "fmt "))
        {
            if (found_format || chunk_size < 16U)
            {
                return false;
            }
            format_bytes = bytes + chunk_data_offset;
            format_size = (size_t)chunk_size;
            found_format = true;
        }
        else if (henka_audio_chunk_is(bytes + offset, "data"))
        {
            if (found_data)
            {
                return false;
            }
            data_bytes = bytes + chunk_data_offset;
            data_size = chunk_size;
            found_data = true;
        }
        next_offset = chunk_data_offset + (size_t)chunk_size;
        if ((chunk_size & 1U) != 0U)
        {
            if (next_offset == byte_count)
            {
                return false;
            }
            ++next_offset;
        }
        if (next_offset < offset || next_offset > byte_count)
        {
            return false;
        }
        offset = next_offset;
    }
    if (!found_format || !found_data || format_size < 16U || data_size == 0U)
    {
        return false;
    }

    format_tag = henka_audio_read_u16(format_bytes);
    clip->channels = henka_audio_read_u16(format_bytes + 2U);
    clip->sample_rate = henka_audio_read_u32(format_bytes + 4U);
    block_align = henka_audio_read_u16(format_bytes + 12U);
    clip->bits_per_sample = henka_audio_read_u16(format_bytes + 14U);
    if ((format_tag != 1U && format_tag != 3U) ||
        clip->channels < 1U || clip->channels > HENKA_AUDIO_OUTPUT_CHANNELS ||
        clip->sample_rate < HENKA_AUDIO_MIN_SAMPLE_RATE ||
        clip->sample_rate > HENKA_AUDIO_MAX_SAMPLE_RATE ||
        clip->bits_per_sample == 0U || clip->bits_per_sample > 32U ||
        (format_tag == 3U && clip->bits_per_sample != 32U) ||
        (format_tag == 1U && clip->bits_per_sample != 8U &&
         clip->bits_per_sample != 16U && clip->bits_per_sample != 24U &&
         clip->bits_per_sample != 32U) ||
        block_align != (uint16_t)(clip->channels * (clip->bits_per_sample / 8U)) ||
        block_align == 0U || data_size % block_align != 0U)
    {
        return false;
    }
    frame_count = (size_t)data_size / block_align;
    if (!henka_audio_size_multiply(frame_count, clip->channels, &sample_count) ||
        !henka_audio_size_multiply(sample_count, sizeof(float), &sample_bytes) ||
        sample_bytes > HENKA_AUDIO_MAX_CLIP_BYTES)
    {
        return false;
    }
    clip->samples = (float*)henka_malloc(sample_bytes);
    if (clip->samples == NULL)
    {
        return false;
    }
    for (sample_index = 0U; sample_index < sample_count; ++sample_index)
    {
        const size_t source_offset = sample_index * (clip->bits_per_sample / 8U);
        float sample;
        if (format_tag == 3U)
        {
            uint32_t sample_bits = henka_audio_read_u32(data_bytes + source_offset);
            memcpy(&sample, &sample_bits, sizeof(sample));
        }
        else if (clip->bits_per_sample == 8U)
        {
            sample = ((float)data_bytes[source_offset] - 128.0f) / 128.0f;
        }
        else if (clip->bits_per_sample == 16U)
        {
            sample = (float)(int16_t)henka_audio_read_u16(
                data_bytes + source_offset) / 32768.0f;
        }
        else if (clip->bits_per_sample == 24U)
        {
            sample = (float)henka_audio_read_s24(data_bytes + source_offset) /
                8388608.0f;
        }
        else
        {
            sample = (float)(int32_t)henka_audio_read_u32(
                data_bytes + source_offset) / 2147483648.0f;
        }
        if (!henka_audio_float_is_valid(sample) || sample < -1.0f || sample > 1.0f)
        {
            henka_free(clip->samples);
            clip->samples = NULL;
            return false;
        }
        clip->samples[sample_index] = sample;
    }
    clip->frame_count = frame_count;
    return true;
}

static henka_audio_voice_id henka_audio_make_voice_id(
    size_t slot_index,
    uint32_t generation)
{
    return ((henka_audio_voice_id)generation << 32U) |
        (henka_audio_voice_id)(slot_index + 1U);
}

static henka_audio_voice_slot* henka_audio_find_voice(
    henka_audio_system* system,
    henka_audio_voice_id voice)
{
    const uint32_t slot_value = (uint32_t)(voice & UINT32_MAX);
    const uint32_t generation = (uint32_t)(voice >> 32U);
    henka_audio_voice_slot* slot;
    if (system == NULL || slot_value == 0U ||
        slot_value > HENKA_AUDIO_MAX_VOICES || generation == 0U)
    {
        return NULL;
    }
    slot = &system->voices[slot_value - 1U];
    return slot->active && slot->generation == generation ? slot : NULL;
}

static const henka_audio_voice_slot* henka_audio_find_voice_const(
    const henka_audio_system* system,
    henka_audio_voice_id voice)
{
    return henka_audio_find_voice((henka_audio_system*)system, voice);
}

static void henka_audio_release_voice(
    henka_audio_system* system,
    henka_audio_voice_slot* slot)
{
    if (system == NULL || slot == NULL || !slot->active)
    {
        return;
    }
    slot->active = false;
    slot->scene = NULL;
    slot->entity = HENKA_INVALID_ENTITY;
    slot->clip = NULL;
    slot->source_position = 0.0;
    slot->paused = false;
    if (system->active_voice_count > 0U)
    {
        --system->active_voice_count;
    }
}

static bool henka_audio_validate_listener(henka_audio_listener listener)
{
    const float forward_length = henka_vec3_length(listener.forward);
    const float up_length = henka_vec3_length(listener.up);
    const float right_length = henka_vec3_length(henka_vec3_cross(
        listener.forward,
        listener.up));
    return henka_audio_vec3_is_finite(listener.position) &&
        henka_audio_vec3_is_finite(listener.forward) &&
        henka_audio_vec3_is_finite(listener.up) &&
        isfinite(forward_length) != 0 && forward_length > 0.0f &&
        isfinite(up_length) != 0 && up_length > 0.0f &&
        isfinite(right_length) != 0 && right_length > 0.0001f;
}

static bool henka_audio_validate_voice_desc(const henka_audio_voice_desc* desc)
{
    return desc != NULL && desc->bus > HENKA_AUDIO_BUS_MASTER &&
        henka_audio_bus_is_valid(desc->bus) &&
        henka_audio_float_is_valid(desc->gain) && desc->gain >= 0.0f &&
        desc->gain <= HENKA_AUDIO_MAX_GAIN &&
        henka_audio_float_is_valid(desc->pitch) &&
        desc->pitch >= HENKA_AUDIO_MIN_PITCH && desc->pitch <= HENKA_AUDIO_MAX_PITCH &&
        henka_audio_float_is_valid(desc->min_distance) && desc->min_distance >= 0.0f &&
        henka_audio_float_is_valid(desc->max_distance) &&
        desc->max_distance > desc->min_distance;
}

henka_audio_voice_desc henka_audio_voice_desc_default(void)
{
    return (henka_audio_voice_desc){
        HENKA_AUDIO_BUS_SFX,
        1.0f,
        1.0f,
        1.0f,
        32.0f,
        false,
        true};
}

henka_audio_listener henka_audio_listener_default(void)
{
    return (henka_audio_listener){
        (henka_vec3){0.0f, 0.0f, 0.0f},
        (henka_vec3){0.0f, 0.0f, -1.0f},
        (henka_vec3){0.0f, 1.0f, 0.0f}};
}

henka_audio_emitter_config henka_audio_emitter_config_default(void)
{
    henka_audio_emitter_config config;
    memset(&config, 0, sizeof(config));
    config.bus = HENKA_AUDIO_BUS_SFX;
    config.gain = 1.0f;
    config.pitch = 1.0f;
    config.min_distance = 1.0f;
    config.max_distance = 32.0f;
    config.spatial = true;
    return config;
}

henka_result henka_audio_emitter_config_validate(
    const henka_audio_emitter_config* config)
{
    if (config == NULL ||
        !henka_audio_string_is_valid(
            config->clip_path,
            HENKA_AUDIO_MAX_CLIP_PATH_BYTES) ||
        config->bus <= HENKA_AUDIO_BUS_MASTER ||
        !henka_audio_bus_is_valid(config->bus) ||
        !henka_audio_float_is_valid(config->gain) ||
        config->gain < 0.0f || config->gain > HENKA_AUDIO_MAX_GAIN ||
        !henka_audio_float_is_valid(config->pitch) ||
        config->pitch < HENKA_AUDIO_MIN_PITCH ||
        config->pitch > HENKA_AUDIO_MAX_PITCH ||
        !henka_audio_float_is_valid(config->min_distance) ||
        config->min_distance < 0.0f ||
        !henka_audio_float_is_valid(config->max_distance) ||
        config->max_distance <= config->min_distance ||
        (config->enabled && config->clip_path[0] == '\0'))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return HENKA_SUCCESS;
}

henka_result henka_audio_system_create(
    const henka_audio_system_config* config,
    henka_audio_system** out_system)
{
    henka_audio_system* system;
    uint32_t sample_rate = HENKA_AUDIO_DEFAULT_SAMPLE_RATE;
    size_t index;
    if (out_system == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_system = NULL;
    if (config != NULL && config->output_sample_rate != 0U)
    {
        sample_rate = config->output_sample_rate;
    }
    if (sample_rate < HENKA_AUDIO_MIN_SAMPLE_RATE ||
        sample_rate > HENKA_AUDIO_MAX_SAMPLE_RATE)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    system = (henka_audio_system*)henka_calloc(1U, sizeof(*system));
    if (system == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    system->output_sample_rate = sample_rate;
    system->listener = henka_audio_listener_default();
    for (index = 0U; index < HENKA_AUDIO_BUS_COUNT; ++index)
    {
        system->bus_gains[index] = 1.0f;
    }
    *out_system = system;
    return HENKA_SUCCESS;
}

void henka_audio_system_destroy(henka_audio_system* system)
{
    if (system != NULL)
    {
        henka_free(system);
    }
}

henka_result henka_audio_system_set_listener(
    henka_audio_system* system,
    henka_audio_listener listener)
{
    if (system == NULL || !henka_audio_validate_listener(listener))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    listener.forward = henka_vec3_normalize(listener.forward);
    listener.up = henka_vec3_normalize(listener.up);
    system->listener = listener;
    return HENKA_SUCCESS;
}

henka_result henka_audio_system_get_listener(
    const henka_audio_system* system,
    henka_audio_listener* out_listener)
{
    if (system == NULL || out_listener == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_listener = system->listener;
    return HENKA_SUCCESS;
}

henka_result henka_audio_system_set_bus_gain(
    henka_audio_system* system,
    henka_audio_bus bus,
    float gain)
{
    if (system == NULL || !henka_audio_bus_is_valid(bus) ||
        !henka_audio_float_is_valid(gain) || gain < 0.0f || gain > HENKA_AUDIO_MAX_GAIN)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    system->bus_gains[bus] = gain;
    return HENKA_SUCCESS;
}

henka_result henka_audio_system_get_bus_gain(
    const henka_audio_system* system,
    henka_audio_bus bus,
    float* out_gain)
{
    if (system == NULL || out_gain == NULL || !henka_audio_bus_is_valid(bus))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_gain = system->bus_gains[bus];
    return HENKA_SUCCESS;
}

henka_result henka_audio_system_get_diagnostics(
    const henka_audio_system* system,
    henka_audio_diagnostics* out_diagnostics)
{
    if (system == NULL || out_diagnostics == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_diagnostics = (henka_audio_diagnostics){
        system->output_sample_rate,
        system->active_voice_count,
        system->stale_voice_count,
        system->mixed_frame_count,
        system->output_clipped};
    return HENKA_SUCCESS;
}

henka_result henka_audio_clip_load_file(
    const char* project_root,
    const char* relative_path,
    henka_audio_clip** out_clip)
{
    char* resolved_path = NULL;
    unsigned char* bytes = NULL;
    size_t byte_count = 0U;
    henka_audio_clip* clip = NULL;
    henka_result result;
    if (out_clip == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_clip = NULL;
    if (project_root == NULL || project_root[0] == '\0' ||
        relative_path == NULL || relative_path[0] == '\0')
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_path_resolve_confined(project_root, relative_path, &resolved_path);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    if (!henka_audio_read_file(resolved_path, &bytes, &byte_count))
    {
        henka_free(resolved_path);
        return HENKA_ERROR_ASSET_SOURCE;
    }
    clip = (henka_audio_clip*)henka_calloc(1U, sizeof(*clip));
    if (clip == NULL)
    {
        henka_free(bytes);
        henka_free(resolved_path);
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    clip->source_path = resolved_path;
    if (!henka_audio_parse_wav(bytes, byte_count, clip))
    {
        henka_free(bytes);
        henka_audio_clip_destroy(clip);
        return HENKA_ERROR_ASSET_SOURCE;
    }
    henka_free(bytes);
    *out_clip = clip;
    return HENKA_SUCCESS;
}

void henka_audio_clip_destroy(henka_audio_clip* clip)
{
    if (clip != NULL)
    {
        henka_free(clip->samples);
        henka_free(clip->source_path);
        henka_free(clip);
    }
}

henka_result henka_audio_clip_get_info(
    const henka_audio_clip* clip,
    henka_audio_clip_info* out_info)
{
    if (clip == NULL || out_info == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_info = (henka_audio_clip_info){
        clip->source_path,
        clip->sample_rate,
        clip->channels,
        clip->bits_per_sample,
        clip->frame_count,
        true};
    return HENKA_SUCCESS;
}

henka_result henka_audio_voice_play(
    henka_audio_system* system,
    henka_scene* scene,
    henka_entity entity,
    const henka_audio_clip* clip,
    const henka_audio_voice_desc* desc,
    henka_audio_voice_id* out_voice)
{
    henka_audio_voice_desc effective_desc;
    size_t index;
    if (out_voice == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_voice = HENKA_INVALID_AUDIO_VOICE_ID;
    effective_desc = desc == NULL ? henka_audio_voice_desc_default() : *desc;
    if (system == NULL || scene == NULL || clip == NULL || clip->samples == NULL ||
        clip->frame_count == 0U || !henka_scene_is_entity_valid(scene, entity) ||
        !henka_audio_validate_voice_desc(&effective_desc))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    for (index = 0U; index < HENKA_AUDIO_MAX_VOICES; ++index)
    {
        henka_audio_voice_slot* slot = &system->voices[index];
        if (slot->active)
        {
            continue;
        }
        slot->generation = slot->generation == UINT32_MAX ? 1U : slot->generation + 1U;
        slot->active = true;
        slot->scene = scene;
        slot->entity = entity;
        slot->clip = clip;
        slot->desc = effective_desc;
        slot->source_position = 0.0;
        slot->paused = false;
        ++system->active_voice_count;
        *out_voice = henka_audio_make_voice_id(index, slot->generation);
        return HENKA_SUCCESS;
    }
    return HENKA_ERROR_LIMIT;
}

henka_result henka_audio_voice_stop(
    henka_audio_system* system,
    henka_audio_voice_id voice)
{
    henka_audio_voice_slot* slot = henka_audio_find_voice(system, voice);
    if (slot == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    henka_audio_release_voice(system, slot);
    return HENKA_SUCCESS;
}

henka_result henka_audio_voice_pause(
    henka_audio_system* system,
    henka_audio_voice_id voice)
{
    henka_audio_voice_slot* slot = henka_audio_find_voice(system, voice);
    if (slot == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    slot->paused = true;
    return HENKA_SUCCESS;
}

henka_result henka_audio_voice_resume(
    henka_audio_system* system,
    henka_audio_voice_id voice)
{
    henka_audio_voice_slot* slot = henka_audio_find_voice(system, voice);
    if (slot == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    slot->paused = false;
    return HENKA_SUCCESS;
}

henka_result henka_audio_voice_restart(
    henka_audio_system* system,
    henka_audio_voice_id voice)
{
    henka_audio_voice_slot* slot = henka_audio_find_voice(system, voice);
    if (slot == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    slot->source_position = 0.0;
    slot->paused = false;
    return HENKA_SUCCESS;
}

henka_result henka_audio_voice_seek(
    henka_audio_system* system,
    henka_audio_voice_id voice,
    size_t source_frame)
{
    henka_audio_voice_slot* slot = henka_audio_find_voice(system, voice);
    if (slot == NULL || slot->clip == NULL || source_frame > slot->clip->frame_count)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    slot->source_position = (double)source_frame;
    return HENKA_SUCCESS;
}

henka_result henka_audio_voice_set_gain(
    henka_audio_system* system,
    henka_audio_voice_id voice,
    float gain)
{
    henka_audio_voice_slot* slot = henka_audio_find_voice(system, voice);
    if (slot == NULL || !henka_audio_float_is_valid(gain) ||
        gain < 0.0f || gain > HENKA_AUDIO_MAX_GAIN)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    slot->desc.gain = gain;
    return HENKA_SUCCESS;
}

henka_result henka_audio_voice_set_pitch(
    henka_audio_system* system,
    henka_audio_voice_id voice,
    float pitch)
{
    henka_audio_voice_slot* slot = henka_audio_find_voice(system, voice);
    if (slot == NULL || !henka_audio_float_is_valid(pitch) ||
        pitch < HENKA_AUDIO_MIN_PITCH || pitch > HENKA_AUDIO_MAX_PITCH)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    slot->desc.pitch = pitch;
    return HENKA_SUCCESS;
}

bool henka_audio_voice_is_valid(
    const henka_audio_system* system,
    henka_audio_voice_id voice)
{
    return henka_audio_find_voice_const(system, voice) != NULL;
}

bool henka_audio_voice_is_paused(
    const henka_audio_system* system,
    henka_audio_voice_id voice)
{
    const henka_audio_voice_slot* slot = henka_audio_find_voice_const(system, voice);
    return slot != NULL && slot->paused;
}

henka_result henka_audio_voice_get_info(
    const henka_audio_system* system,
    henka_audio_voice_id voice,
    henka_audio_voice_info* out_info)
{
    const henka_audio_voice_slot* slot = henka_audio_find_voice_const(system, voice);
    if (slot == NULL || out_info == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_info = (henka_audio_voice_info){
        voice,
        slot->desc.bus,
        slot->scene,
        slot->entity,
        slot->clip,
        slot->source_position < 0.0 ? 0U : (size_t)slot->source_position,
        slot->desc.gain,
        slot->desc.pitch,
        slot->active,
        slot->paused,
        slot->desc.looping,
        slot->desc.spatial};
    return HENKA_SUCCESS;
}

size_t henka_audio_system_get_active_voice_count(
    const henka_audio_system* system)
{
    return system == NULL ? 0U : system->active_voice_count;
}

uint32_t henka_audio_system_get_sample_rate(
    const henka_audio_system* system)
{
    return system == NULL ? 0U : system->output_sample_rate;
}

henka_result henka_audio_emitter_create_with_clip(
    henka_audio_system* system,
    henka_scene* scene,
    henka_entity entity,
    const henka_audio_clip* clip,
    const henka_audio_emitter_config* config,
    henka_audio_emitter** out_emitter)
{
    henka_audio_emitter* emitter;
    henka_audio_voice_desc voice_desc;
    henka_result result;

    if (out_emitter == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_emitter = NULL;
    if (system == NULL || scene == NULL || clip == NULL ||
        !henka_scene_is_entity_valid(scene, entity) || config == NULL ||
        !config->enabled ||
        henka_audio_emitter_config_validate(config) != HENKA_SUCCESS ||
        clip->frame_count == 0U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    emitter = (henka_audio_emitter*)henka_calloc(1U, sizeof(*emitter));
    if (emitter == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    emitter->system = system;
    emitter->scene = scene;
    emitter->entity = entity;
    emitter->clip = (henka_audio_clip*)clip;
    emitter->owns_clip = false;
    emitter->config = *config;
    voice_desc = (henka_audio_voice_desc){
        config->bus,
        config->gain,
        config->pitch,
        config->min_distance,
        config->max_distance,
        config->looping,
        config->spatial};
    result = henka_audio_voice_play(
        system,
        scene,
        entity,
        emitter->clip,
        &voice_desc,
        &emitter->voice);
    if (result != HENKA_SUCCESS)
    {
        henka_free(emitter);
        return result;
    }
    *out_emitter = emitter;
    return HENKA_SUCCESS;
}

henka_result henka_audio_emitter_create(
    henka_audio_system* system,
    const char* project_root,
    henka_scene* scene,
    henka_entity entity,
    const henka_audio_emitter_config* config,
    henka_audio_emitter** out_emitter)
{
    henka_audio_clip* clip = NULL;
    henka_audio_emitter* emitter = NULL;
    henka_result result;

    if (out_emitter == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_emitter = NULL;
    if (project_root == NULL || config == NULL || system == NULL ||
        scene == NULL || !henka_scene_is_entity_valid(scene, entity) ||
        !config->enabled ||
        henka_audio_emitter_config_validate(config) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_audio_clip_load_file(project_root, config->clip_path, &clip);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    result = henka_audio_emitter_create_with_clip(
        system, scene, entity, clip, config, &emitter);
    if (result != HENKA_SUCCESS)
    {
        henka_audio_clip_destroy(clip);
        return result;
    }
    emitter->owns_clip = true;
    *out_emitter = emitter;
    return HENKA_SUCCESS;
}

void henka_audio_emitter_destroy(henka_audio_emitter* emitter)
{
    if (emitter == NULL)
    {
        return;
    }
    if (emitter->system != NULL &&
        emitter->voice != HENKA_INVALID_AUDIO_VOICE_ID)
    {
        (void)henka_audio_voice_stop(emitter->system, emitter->voice);
    }
    if (emitter->owns_clip)
    {
        henka_audio_clip_destroy(emitter->clip);
    }
    henka_free(emitter);
}

bool henka_audio_emitter_is_valid(const henka_audio_emitter* emitter)
{
    return emitter != NULL && emitter->system != NULL && emitter->scene != NULL &&
        henka_scene_is_entity_valid(emitter->scene, emitter->entity) &&
        henka_audio_voice_is_valid(emitter->system, emitter->voice);
}

henka_result henka_audio_emitter_get_config(
    const henka_audio_emitter* emitter,
    henka_audio_emitter_config* out_config)
{
    if (emitter == NULL || out_config == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_config = emitter->config;
    return HENKA_SUCCESS;
}

henka_result henka_audio_emitter_get_voice_info(
    const henka_audio_emitter* emitter,
    henka_audio_voice_info* out_info)
{
    if (!henka_audio_emitter_is_valid(emitter) || out_info == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return henka_audio_voice_get_info(emitter->system, emitter->voice, out_info);
}

static float henka_audio_clamp(float value, float minimum, float maximum)
{
    return value < minimum ? minimum : value > maximum ? maximum : value;
}

static void henka_audio_get_spatial_gains(
    const henka_audio_system* system,
    const henka_audio_voice_slot* slot,
    henka_vec3 position,
    float* out_left,
    float* out_right)
{
    float attenuation = 1.0f;
    float pan = 0.0f;
    const henka_vec3 relative = henka_vec3_subtract(position, system->listener.position);
    const float distance = henka_vec3_length(relative);
    if (!henka_audio_vec3_is_finite(position) ||
        !henka_audio_float_is_valid(distance))
    {
        *out_left = 0.0f;
        *out_right = 0.0f;
        return;
    }
    if (slot->desc.spatial)
    {
        if (distance >= slot->desc.max_distance)
        {
            attenuation = 0.0f;
        }
        else if (distance > slot->desc.min_distance)
        {
            const float normalized_distance =
                (distance - slot->desc.min_distance) /
                (slot->desc.max_distance - slot->desc.min_distance);
            attenuation = 1.0f - normalized_distance;
            attenuation *= attenuation;
        }
        if (distance > 0.0001f)
        {
            const henka_vec3 direction = henka_vec3_scale(relative, 1.0f / distance);
            const henka_vec3 right = henka_vec3_normalize(henka_vec3_cross(
                system->listener.forward,
                system->listener.up));
            pan = henka_audio_clamp(henka_vec3_dot(direction, right), -1.0f, 1.0f);
        }
    }
    if (!slot->desc.spatial)
    {
        *out_left = 0.70710678f;
        *out_right = 0.70710678f;
    }
    else
    {
        const float angle = (pan + 1.0f) * 0.78539816f;
        *out_left = cosf(angle) * attenuation;
        *out_right = sinf(angle) * attenuation;
    }
}

static float henka_audio_clip_sample(
    const henka_audio_clip* clip,
    size_t frame,
    size_t channel)
{
    return clip->samples[frame * clip->channels + channel];
}

henka_result henka_audio_system_mix(
    henka_audio_system* system,
    float* output_interleaved,
    size_t frame_count)
{
    size_t output_sample_count;
    size_t voice_index;
    if (system == NULL ||
        (frame_count != 0U && output_interleaved == NULL) ||
        !henka_audio_size_multiply(frame_count, HENKA_AUDIO_OUTPUT_CHANNELS,
            &output_sample_count))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (frame_count == 0U)
    {
        return HENKA_SUCCESS;
    }
    memset(output_interleaved, 0, output_sample_count * sizeof(float));
    for (voice_index = 0U; voice_index < HENKA_AUDIO_MAX_VOICES; ++voice_index)
    {
        henka_audio_voice_slot* slot = &system->voices[voice_index];
        henka_transform transform;
        float left_gain;
        float right_gain;
        float bus_gain;
        float step;
        size_t frame_index;
        if (!slot->active)
        {
            continue;
        }
        if (slot->paused)
        {
            continue;
        }
        if (slot->scene == NULL ||
            !henka_scene_is_entity_valid(slot->scene, slot->entity) ||
            henka_scene_get_entity_transform(slot->scene, slot->entity, &transform) != HENKA_SUCCESS)
        {
            henka_audio_release_voice(system, slot);
            ++system->stale_voice_count;
            continue;
        }
        henka_audio_get_spatial_gains(
            system, slot, transform.position, &left_gain, &right_gain);
        bus_gain = system->bus_gains[HENKA_AUDIO_BUS_MASTER] *
            system->bus_gains[slot->desc.bus] * slot->desc.gain;
        step = ((float)slot->clip->sample_rate /
            (float)system->output_sample_rate) * slot->desc.pitch;
        if (!henka_audio_float_is_valid(bus_gain) ||
            !henka_audio_float_is_valid(step) || step <= 0.0f)
        {
            henka_audio_release_voice(system, slot);
            ++system->stale_voice_count;
            continue;
        }
        for (frame_index = 0U; frame_index < frame_count && slot->active; ++frame_index)
        {
            size_t source_frame;
            size_t next_frame;
            float fraction;
            float sample_left;
            float sample_right;
            float mixed_left;
            float mixed_right;
            if (slot->source_position >= (double)slot->clip->frame_count)
            {
                if (!slot->desc.looping)
                {
                    henka_audio_release_voice(system, slot);
                    break;
                }
                slot->source_position = fmod(
                    slot->source_position,
                    (double)slot->clip->frame_count);
            }
            source_frame = (size_t)slot->source_position;
            fraction = (float)(slot->source_position - (double)source_frame);
            next_frame = source_frame + 1U < slot->clip->frame_count
                ? source_frame + 1U
                : slot->desc.looping ? 0U : source_frame;
            sample_left = henka_audio_clip_sample(slot->clip, source_frame, 0U);
            sample_right = slot->clip->channels == 1U
                ? sample_left
                : henka_audio_clip_sample(slot->clip, source_frame, 1U);
            sample_left += (
                henka_audio_clip_sample(slot->clip, next_frame, 0U) - sample_left) * fraction;
            sample_right += (
                (slot->clip->channels == 1U
                    ? henka_audio_clip_sample(slot->clip, next_frame, 0U)
                    : henka_audio_clip_sample(slot->clip, next_frame, 1U)) - sample_right) * fraction;
            mixed_left = output_interleaved[frame_index * HENKA_AUDIO_OUTPUT_CHANNELS] +
                sample_left * left_gain * bus_gain;
            mixed_right = output_interleaved[frame_index * HENKA_AUDIO_OUTPUT_CHANNELS + 1U] +
                sample_right * right_gain * bus_gain;
            if (mixed_left < -1.0f || mixed_left > 1.0f ||
                mixed_right < -1.0f || mixed_right > 1.0f)
            {
                system->output_clipped = true;
            }
            output_interleaved[frame_index * HENKA_AUDIO_OUTPUT_CHANNELS] =
                henka_audio_clamp(mixed_left, -1.0f, 1.0f);
            output_interleaved[frame_index * HENKA_AUDIO_OUTPUT_CHANNELS + 1U] =
                henka_audio_clamp(mixed_right, -1.0f, 1.0f);
            slot->source_position += (double)step;
            if (!slot->desc.looping &&
                slot->source_position >= (double)slot->clip->frame_count)
            {
                henka_audio_release_voice(system, slot);
            }
        }
    }
    if (system->mixed_frame_count > UINT64_MAX - (uint64_t)frame_count)
    {
        system->mixed_frame_count = UINT64_MAX;
    }
    else
    {
        system->mixed_frame_count += (uint64_t)frame_count;
    }
    return HENKA_SUCCESS;
}

const char* henka_audio_bus_get_label(henka_audio_bus bus)
{
    static const char* labels[HENKA_AUDIO_BUS_COUNT] = {
        "Master", "Music", "SFX", "Dialogue", "Ambience", "UI"};
    return henka_audio_bus_is_valid(bus) ? labels[bus] : "Unknown";
}
