#ifndef HENKA_AUDIO_DECODER_H
#define HENKA_AUDIO_DECODER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <henka/result.h>

typedef struct henka_audio_decoder henka_audio_decoder;

typedef struct henka_audio_decoder_info
{
    uint32_t sample_rate;
    uint16_t channels;
    uint16_t bits_per_sample;
    size_t frame_count;
} henka_audio_decoder_info;

bool henka_audio_decoder_is_supported_path(const char* path);
/* Opens one of Henka's supported compressed formats without decoding the
 * complete source. The caller owns the decoder and must destroy it. */
henka_result henka_audio_decoder_open(
    const char* path,
    size_t max_source_bytes,
    henka_audio_decoder** out_decoder);
void henka_audio_decoder_destroy(henka_audio_decoder* decoder);
henka_result henka_audio_decoder_get_info(
    const henka_audio_decoder* decoder,
    henka_audio_decoder_info* out_info);
henka_result henka_audio_decoder_read_frames(
    henka_audio_decoder* decoder,
    float* out_samples,
    size_t frame_capacity,
    size_t* out_frames);
henka_result henka_audio_decoder_seek(
    henka_audio_decoder* decoder,
    size_t source_frame);

#endif
