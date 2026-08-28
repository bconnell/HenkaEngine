#ifndef HENKA_AUDIO_OUTPUT_H
#define HENKA_AUDIO_OUTPUT_H

#include <stdbool.h>
#include <stdint.h>

#include <henka/audio.h>
#include <henka/result.h>
#include <henka/scene.h>

#define HENKA_AUDIO_OUTPUT_DEFAULT_MAX_PUMP_FRAMES 1024U
#define HENKA_AUDIO_OUTPUT_DEFAULT_MAX_QUEUED_FRAMES 4096U
#define HENKA_AUDIO_OUTPUT_MAX_PUMP_FRAMES 16384U
#define HENKA_AUDIO_OUTPUT_MAX_QUEUED_FRAMES 65536U

typedef struct henka_audio_output henka_audio_output;

typedef struct henka_audio_output_config
{
    uint32_t max_pump_frames;
    uint32_t max_queued_frames;
} henka_audio_output_config;

typedef struct henka_audio_output_info
{
    bool device_open;
    uint32_t sample_rate;
    uint32_t queued_frames;
    uint64_t pumped_frames;
    uint64_t rejected_frames;
} henka_audio_output_info;

henka_audio_output_config henka_audio_output_config_default(void);
henka_result henka_audio_output_create(
    henka_audio_system* system,
    henka_scene* scene,
    const henka_audio_output_config* config,
    henka_audio_output** out_output);
void henka_audio_output_destroy(henka_audio_output* output);
/* Reopens the playback device through a replacement stream. The prior stream
 * remains live if reopening fails; a successful recovery clears only queued
 * device data and preserves mixer state and diagnostics counters. */
henka_result henka_audio_output_recover(henka_audio_output* output);
/* Mixes and queues at most the configured frame budget. This is deliberately
 * caller-pumped: the caller owns scene/audio-system synchronization and must
 * invoke it from the same owner thread as other Audio commands. */
henka_result henka_audio_output_pump(
    henka_audio_output* output,
    uint32_t frame_count);
henka_result henka_audio_output_get_info(
    const henka_audio_output* output,
    henka_audio_output_info* out_info);

#endif
