#include <limits.h>
#include <stdint.h>

#include <SDL3/SDL.h>

#include <henka/audio_output.h>
#include <henka/memory.h>

#define HENKA_AUDIO_OUTPUT_FRAME_BYTES \
    (HENKA_AUDIO_OUTPUT_CHANNELS * sizeof(float))

struct henka_audio_output
{
    henka_audio_system* system;
    henka_scene* scene;
    SDL_AudioStream* stream;
    float* mix_buffer;
    uint32_t max_pump_frames;
    uint32_t max_queued_frames;
    uint32_t sample_rate;
    uint64_t pumped_frames;
    uint64_t rejected_frames;
};

static bool henka_audio_output_size_multiply(
    size_t left,
    size_t right,
    size_t* out_product)
{
    if (out_product == NULL || (right != 0U && left > SIZE_MAX / right))
    {
        return false;
    }
    *out_product = left * right;
    return true;
}

static void henka_audio_output_count_rejection(
    henka_audio_output* output,
    uint32_t frame_count)
{
    if (UINT64_MAX - output->rejected_frames < (uint64_t)frame_count)
    {
        output->rejected_frames = UINT64_MAX;
    }
    else
    {
        output->rejected_frames += (uint64_t)frame_count;
    }
}

static henka_result henka_audio_output_get_queued_frames(
    const henka_audio_output* output,
    uint32_t* out_frames)
{
    int queued_bytes;
    uint64_t queued_frames;
    if (output == NULL || out_frames == NULL || output->stream == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    queued_bytes = SDL_GetAudioStreamQueued(output->stream);
    if (queued_bytes < 0)
    {
        return HENKA_ERROR_PLATFORM;
    }
    queued_frames = (uint64_t)queued_bytes / HENKA_AUDIO_OUTPUT_FRAME_BYTES;
    *out_frames = queued_frames > UINT32_MAX
        ? UINT32_MAX
        : (uint32_t)queued_frames;
    return HENKA_SUCCESS;
}

henka_audio_output_config henka_audio_output_config_default(void)
{
    henka_audio_output_config config;
    config.max_pump_frames = HENKA_AUDIO_OUTPUT_DEFAULT_MAX_PUMP_FRAMES;
    config.max_queued_frames = HENKA_AUDIO_OUTPUT_DEFAULT_MAX_QUEUED_FRAMES;
    return config;
}

henka_result henka_audio_output_create(
    henka_audio_system* system,
    henka_scene* scene,
    const henka_audio_output_config* config,
    henka_audio_output** out_output)
{
    henka_audio_output_config effective_config;
    henka_audio_output* output;
    uint32_t sample_rate;
    size_t mix_sample_count;
    size_t mix_buffer_bytes;
    SDL_AudioSpec spec = {0};

    if (out_output == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_output = NULL;
    if (system == NULL || scene == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    effective_config = config == NULL
        ? henka_audio_output_config_default()
        : *config;
    if (effective_config.max_pump_frames == 0U ||
        effective_config.max_pump_frames > HENKA_AUDIO_OUTPUT_MAX_PUMP_FRAMES ||
        effective_config.max_queued_frames == 0U ||
        effective_config.max_queued_frames > HENKA_AUDIO_OUTPUT_MAX_QUEUED_FRAMES)
    {
        return HENKA_ERROR_LIMIT;
    }
    sample_rate = henka_audio_system_get_sample_rate(system);
    if (sample_rate == 0U || sample_rate > (uint32_t)INT_MAX ||
        !henka_audio_output_size_multiply(
            (size_t)effective_config.max_pump_frames,
            HENKA_AUDIO_OUTPUT_CHANNELS,
            &mix_sample_count) ||
        !henka_audio_output_size_multiply(
            mix_sample_count, sizeof(float), &mix_buffer_bytes))
    {
        return HENKA_ERROR_NUMERIC_RANGE;
    }
    output = (henka_audio_output*)henka_calloc(1U, sizeof(*output));
    if (output == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    output->mix_buffer = (float*)henka_calloc(1U, mix_buffer_bytes);
    if (output->mix_buffer == NULL)
    {
        henka_free(output);
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    output->system = system;
    output->scene = scene;
    output->max_pump_frames = effective_config.max_pump_frames;
    output->max_queued_frames = effective_config.max_queued_frames;
    output->sample_rate = sample_rate;

    if (!SDL_InitSubSystem(SDL_INIT_AUDIO))
    {
        henka_free(output->mix_buffer);
        henka_free(output);
        return HENKA_ERROR_PLATFORM;
    }
    spec.format = SDL_AUDIO_F32;
    spec.channels = (int)HENKA_AUDIO_OUTPUT_CHANNELS;
    spec.freq = (int)sample_rate;
    output->stream = SDL_OpenAudioDeviceStream(
        SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
    if (output->stream == NULL)
    {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        henka_free(output->mix_buffer);
        henka_free(output);
        return HENKA_ERROR_PLATFORM;
    }
    if (!SDL_ResumeAudioStreamDevice(output->stream))
    {
        SDL_DestroyAudioStream(output->stream);
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        henka_free(output->mix_buffer);
        henka_free(output);
        return HENKA_ERROR_PLATFORM;
    }
    *out_output = output;
    return HENKA_SUCCESS;
}

void henka_audio_output_destroy(henka_audio_output* output)
{
    if (output == NULL)
    {
        return;
    }
    if (output->stream != NULL)
    {
        SDL_DestroyAudioStream(output->stream);
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }
    henka_free(output->mix_buffer);
    henka_free(output);
}

henka_result henka_audio_output_pump(
    henka_audio_output* output,
    uint32_t frame_count)
{
    henka_result mix_result;
    uint32_t queued_frames;
    size_t sample_count;
    size_t byte_count;
    if (output == NULL || output->stream == NULL || frame_count == 0U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (frame_count > output->max_pump_frames ||
        henka_audio_output_get_queued_frames(output, &queued_frames) != HENKA_SUCCESS)
    {
        if (frame_count > output->max_pump_frames)
        {
            henka_audio_output_count_rejection(output, frame_count);
            return HENKA_ERROR_LIMIT;
        }
        return HENKA_ERROR_PLATFORM;
    }
    if (queued_frames > output->max_queued_frames ||
        frame_count > output->max_queued_frames - queued_frames)
    {
        henka_audio_output_count_rejection(output, frame_count);
        return HENKA_ERROR_LIMIT;
    }
    if (!henka_audio_output_size_multiply(
            (size_t)frame_count, HENKA_AUDIO_OUTPUT_CHANNELS, &sample_count) ||
        !henka_audio_output_size_multiply(
            sample_count, sizeof(float), &byte_count) ||
        byte_count > (size_t)INT_MAX)
    {
        return HENKA_ERROR_NUMERIC_RANGE;
    }
    mix_result = henka_audio_system_mix(
        output->system, output->mix_buffer, (size_t)frame_count);
    if (mix_result != HENKA_SUCCESS)
    {
        return mix_result;
    }
    if (!SDL_PutAudioStreamData(output->stream, output->mix_buffer, (int)byte_count))
    {
        return HENKA_ERROR_PLATFORM;
    }
    if (UINT64_MAX - output->pumped_frames < (uint64_t)frame_count)
    {
        output->pumped_frames = UINT64_MAX;
    }
    else
    {
        output->pumped_frames += (uint64_t)frame_count;
    }
    return HENKA_SUCCESS;
}

henka_result henka_audio_output_get_info(
    const henka_audio_output* output,
    henka_audio_output_info* out_info)
{
    uint32_t queued_frames;
    henka_result result;
    if (output == NULL || out_info == NULL || output->stream == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_audio_output_get_queued_frames(output, &queued_frames);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    out_info->device_open = true;
    out_info->sample_rate = output->sample_rate;
    out_info->queued_frames = queued_frames;
    out_info->pumped_frames = output->pumped_frames;
    out_info->rejected_frames = output->rejected_frames;
    return HENKA_SUCCESS;
}
