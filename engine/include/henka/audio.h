#ifndef HENKA_AUDIO_H
#define HENKA_AUDIO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <henka/math.h>
#include <henka/result.h>
#include <henka/scene.h>

typedef struct henka_audio_system henka_audio_system;
typedef struct henka_audio_clip henka_audio_clip;
typedef uint64_t henka_audio_voice_id;

#define HENKA_INVALID_AUDIO_VOICE_ID ((henka_audio_voice_id)0)
#define HENKA_AUDIO_MAX_VOICES 64U
#define HENKA_AUDIO_OUTPUT_CHANNELS 2U
#define HENKA_AUDIO_DEFAULT_SAMPLE_RATE 48000U
#define HENKA_AUDIO_MAX_CLIP_BYTES (64U * 1024U * 1024U)

typedef enum henka_audio_bus
{
    HENKA_AUDIO_BUS_MASTER = 0,
    HENKA_AUDIO_BUS_MUSIC,
    HENKA_AUDIO_BUS_SFX,
    HENKA_AUDIO_BUS_DIALOGUE,
    HENKA_AUDIO_BUS_AMBIENCE,
    HENKA_AUDIO_BUS_UI,
    HENKA_AUDIO_BUS_COUNT
} henka_audio_bus;

typedef struct henka_audio_system_config
{
    /* Zero selects the deterministic 48 kHz output rate. The first runtime
     * mixer slice intentionally emits bounded stereo float PCM only. */
    uint32_t output_sample_rate;
} henka_audio_system_config;

typedef struct henka_audio_clip_info
{
    const char* source_path;
    uint32_t sample_rate;
    uint16_t channels;
    uint16_t bits_per_sample;
    size_t frame_count;
    bool resident;
} henka_audio_clip_info;

typedef struct henka_audio_listener
{
    henka_vec3 position;
    henka_vec3 forward;
    henka_vec3 up;
} henka_audio_listener;

typedef struct henka_audio_voice_desc
{
    henka_audio_bus bus;
    float gain;
    float pitch;
    float min_distance;
    float max_distance;
    bool looping;
    bool spatial;
} henka_audio_voice_desc;

typedef struct henka_audio_voice_info
{
    henka_audio_voice_id id;
    henka_audio_bus bus;
    henka_scene* scene;
    henka_entity entity;
    const henka_audio_clip* clip;
    size_t source_frame;
    bool active;
    bool looping;
    bool spatial;
} henka_audio_voice_info;

typedef struct henka_audio_diagnostics
{
    uint32_t output_sample_rate;
    uint32_t active_voice_count;
    uint32_t stale_voice_count;
    uint64_t mixed_frame_count;
    bool output_clipped;
} henka_audio_diagnostics;

henka_audio_voice_desc henka_audio_voice_desc_default(void);
henka_audio_listener henka_audio_listener_default(void);
henka_result henka_audio_system_create(
    const henka_audio_system_config* config,
    henka_audio_system** out_system);
void henka_audio_system_destroy(henka_audio_system* system);
henka_result henka_audio_system_set_listener(
    henka_audio_system* system,
    henka_audio_listener listener);
henka_result henka_audio_system_get_listener(
    const henka_audio_system* system,
    henka_audio_listener* out_listener);
henka_result henka_audio_system_set_bus_gain(
    henka_audio_system* system,
    henka_audio_bus bus,
    float gain);
henka_result henka_audio_system_get_bus_gain(
    const henka_audio_system* system,
    henka_audio_bus bus,
    float* out_gain);
henka_result henka_audio_system_get_diagnostics(
    const henka_audio_system* system,
    henka_audio_diagnostics* out_diagnostics);

/* Loads resident PCM WAV data beneath project_root. Rooted, traversal, URI,
 * and device-like paths are rejected by the same confined path boundary used
 * by other Henka asset loaders. The clip owns its decoded samples. */
henka_result henka_audio_clip_load_file(
    const char* project_root,
    const char* relative_path,
    henka_audio_clip** out_clip);
void henka_audio_clip_destroy(henka_audio_clip* clip);
henka_result henka_audio_clip_get_info(
    const henka_audio_clip* clip,
    henka_audio_clip_info* out_info);

/* The scene, entity, and clip are borrowed production objects. They must
 * outlive every voice that references them; callers must stop voices before
 * destroying their scene or clip. Voice commands and mixing are currently
 * single-owner operations; the future device adapter will own synchronization
 * at that boundary rather than making this deterministic core lock-free by
 * accident. */
henka_result henka_audio_voice_play(
    henka_audio_system* system,
    henka_scene* scene,
    henka_entity entity,
    const henka_audio_clip* clip,
    const henka_audio_voice_desc* desc,
    henka_audio_voice_id* out_voice);
henka_result henka_audio_voice_stop(
    henka_audio_system* system,
    henka_audio_voice_id voice);
bool henka_audio_voice_is_valid(
    const henka_audio_system* system,
    henka_audio_voice_id voice);
henka_result henka_audio_voice_get_info(
    const henka_audio_system* system,
    henka_audio_voice_id voice,
    henka_audio_voice_info* out_info);
size_t henka_audio_system_get_active_voice_count(
    const henka_audio_system* system);

/* Mixes bounded stereo float PCM. output_interleaved must contain at least
 * frame_count * HENKA_AUDIO_OUTPUT_CHANNELS floats. The buffer is cleared
 * before deterministic slot-order mixing. A destroyed/stale entity stops its
 * voice before it can contribute another sample. */
henka_result henka_audio_system_mix(
    henka_audio_system* system,
    float* output_interleaved,
    size_t frame_count);

const char* henka_audio_bus_get_label(henka_audio_bus bus);

#endif
