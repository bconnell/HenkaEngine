#define MA_NO_DEVICE_IO
#define MA_NO_THREADING
#define MINIAUDIO_IMPLEMENTATION

/* stb_vorbis is included before miniaudio so miniaudio can register its
 * optional Vorbis decoding backend. Both dependencies are pinned by the root
 * CMake project and remain behind this private Henka decoder boundary. */
#include <stb_vorbis.c>
#undef L
#undef C
#undef R
#include <miniaudio.h>

#include "audio_decoder.h"

#include <stdio.h>
#include <string.h>

#include <henka/memory.h>

#define HENKA_AUDIO_DECODER_MIN_SAMPLE_RATE 8000U
#define HENKA_AUDIO_DECODER_MAX_SAMPLE_RATE 192000U
#define HENKA_AUDIO_DECODER_MAX_CHANNELS 2U

struct henka_audio_decoder
{
    ma_decoder decoder;
    henka_audio_decoder_info info;
    bool initialized;
};

static bool henka_audio_decoder_has_extension(
    const char* path,
    const char* extension)
{
    size_t path_length;
    size_t extension_length;
    size_t index;

    if (path == NULL || extension == NULL)
    {
        return false;
    }
    path_length = strlen(path);
    extension_length = strlen(extension);
    if (extension_length == 0U || path_length < extension_length)
    {
        return false;
    }
    index = path_length - extension_length;
    while (index < path_length)
    {
        char left = path[index];
        char right = extension[index - (path_length - extension_length)];
        if (left >= 'A' && left <= 'Z')
        {
            left = (char)(left - 'A' + 'a');
        }
        if (right >= 'A' && right <= 'Z')
        {
            right = (char)(right - 'A' + 'a');
        }
        if (left != right)
        {
            return false;
        }
        ++index;
    }
    return true;
}

bool henka_audio_decoder_is_supported_path(const char* path)
{
    return henka_audio_decoder_has_extension(path, ".ogg") ||
        henka_audio_decoder_has_extension(path, ".oga") ||
        henka_audio_decoder_has_extension(path, ".mp3") ||
        henka_audio_decoder_has_extension(path, ".flac");
}

static bool henka_audio_decoder_get_file_size(
    const char* path,
    size_t max_source_bytes)
{
    FILE* file = NULL;
    long file_size;
    bool success = false;

    if (path == NULL || max_source_bytes == 0U)
    {
        return false;
    }
#if defined(_WIN32)
    if (fopen_s(&file, path, "rb") != 0)
    {
        file = NULL;
    }
#else
    file = fopen(path, "rb");
#endif
    if (file != NULL && fseek(file, 0L, SEEK_END) == 0)
    {
        file_size = ftell(file);
        if (file_size > 0L &&
            (unsigned long long)file_size <= (unsigned long long)max_source_bytes)
        {
            success = true;
        }
    }
    if (file != NULL)
    {
        fclose(file);
    }
    return success;
}

static henka_result henka_audio_decoder_validate(
    henka_audio_decoder* decoder)
{
    ma_uint64 frame_count;

    if (decoder == NULL || !decoder->initialized ||
        decoder->decoder.outputChannels == 0U ||
        decoder->decoder.outputChannels > HENKA_AUDIO_DECODER_MAX_CHANNELS ||
        decoder->decoder.outputSampleRate < HENKA_AUDIO_DECODER_MIN_SAMPLE_RATE ||
        decoder->decoder.outputSampleRate > HENKA_AUDIO_DECODER_MAX_SAMPLE_RATE ||
        decoder->decoder.outputFormat != ma_format_f32 ||
        ma_decoder_get_length_in_pcm_frames(&decoder->decoder, &frame_count) != MA_SUCCESS ||
        frame_count == 0U || frame_count > (ma_uint64)SIZE_MAX)
    {
        return HENKA_ERROR_ASSET_SOURCE;
    }
    decoder->info = (henka_audio_decoder_info){
        decoder->decoder.outputSampleRate,
        (uint16_t)decoder->decoder.outputChannels,
        32U,
        (size_t)frame_count};
    return HENKA_SUCCESS;
}

henka_result henka_audio_decoder_open(
    const char* path,
    size_t max_source_bytes,
    henka_audio_decoder** out_decoder)
{
    henka_audio_decoder* decoder;
    ma_decoder_config config;
    ma_result result;

    if (out_decoder == NULL || path == NULL || path[0] == '\0' ||
        !henka_audio_decoder_is_supported_path(path))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_decoder = NULL;
    if (!henka_audio_decoder_get_file_size(path, max_source_bytes))
    {
        return HENKA_ERROR_ASSET_SOURCE;
    }
    decoder = (henka_audio_decoder*)henka_calloc(1U, sizeof(*decoder));
    if (decoder == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    config = ma_decoder_config_init(ma_format_f32, 0U, 0U);
    result = ma_decoder_init_file(path, &config, &decoder->decoder);
    if (result != MA_SUCCESS)
    {
        henka_free(decoder);
        return HENKA_ERROR_ASSET_SOURCE;
    }
    decoder->initialized = true;
    if (henka_audio_decoder_validate(decoder) != HENKA_SUCCESS)
    {
        henka_audio_decoder_destroy(decoder);
        return HENKA_ERROR_ASSET_SOURCE;
    }
    *out_decoder = decoder;
    return HENKA_SUCCESS;
}

void henka_audio_decoder_destroy(henka_audio_decoder* decoder)
{
    if (decoder == NULL)
    {
        return;
    }
    if (decoder->initialized)
    {
        ma_decoder_uninit(&decoder->decoder);
    }
    henka_free(decoder);
}

henka_result henka_audio_decoder_get_info(
    const henka_audio_decoder* decoder,
    henka_audio_decoder_info* out_info)
{
    if (decoder == NULL || out_info == NULL || !decoder->initialized)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_info = decoder->info;
    return HENKA_SUCCESS;
}

henka_result henka_audio_decoder_read_frames(
    henka_audio_decoder* decoder,
    float* out_samples,
    size_t frame_capacity,
    size_t* out_frames)
{
    ma_uint64 frames_read = 0U;
    ma_result result;

    if (out_frames == NULL || decoder == NULL || !decoder->initialized ||
        (frame_capacity != 0U && out_samples == NULL) ||
        frame_capacity > (size_t)UINT64_MAX)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_frames = 0U;
    if (frame_capacity == 0U)
    {
        return HENKA_SUCCESS;
    }
    result = ma_decoder_read_pcm_frames(
        &decoder->decoder,
        out_samples,
        (ma_uint64)frame_capacity,
        &frames_read);
    if (result != MA_SUCCESS && result != MA_AT_END)
    {
        return HENKA_ERROR_ASSET_SOURCE;
    }
    if (frames_read > (ma_uint64)frame_capacity)
    {
        return HENKA_ERROR_ASSET_SOURCE;
    }
    *out_frames = (size_t)frames_read;
    return HENKA_SUCCESS;
}

henka_result henka_audio_decoder_seek(
    henka_audio_decoder* decoder,
    size_t source_frame)
{
    if (decoder == NULL || !decoder->initialized ||
        source_frame > decoder->info.frame_count ||
        source_frame > (size_t)UINT64_MAX ||
        ma_decoder_seek_to_pcm_frame(
            &decoder->decoder,
            (ma_uint64)source_frame) != MA_SUCCESS)
    {
        return HENKA_ERROR_ASSET_SOURCE;
    }
    return HENKA_SUCCESS;
}
