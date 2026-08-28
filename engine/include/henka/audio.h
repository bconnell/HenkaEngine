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
typedef struct henka_audio_emitter henka_audio_emitter;
typedef uint64_t henka_audio_voice_id;

#define HENKA_INVALID_AUDIO_VOICE_ID ((henka_audio_voice_id)0)
#define HENKA_AUDIO_MAX_VOICES 64U
#define HENKA_AUDIO_OUTPUT_CHANNELS 2U
#define HENKA_AUDIO_DEFAULT_SAMPLE_RATE 48000U
#define HENKA_AUDIO_MAX_CLIP_BYTES (64U * 1024U * 1024U)
#define HENKA_AUDIO_MAX_CLIP_PATH_BYTES 512U

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

/* Value-only authored configuration for an object-attached emitter. It owns
 * no runtime clip, scene, entity, device, or mixer resources. The clip path is
 * project-relative and is resolved by the owning asset/runtime boundary. */
typedef struct henka_audio_emitter_config
{
    bool enabled;
    bool looping;
    bool spatial;
    henka_audio_bus bus;
    float gain;
    float pitch;
    float min_distance;
    float max_distance;
    char clip_path[HENKA_AUDIO_MAX_CLIP_PATH_BYTES];
} henka_audio_emitter_config;

typedef struct henka_audio_voice_info
{
    henka_audio_voice_id id;
    henka_audio_bus bus;
    henka_scene* scene;
    henka_entity entity;
    const henka_audio_clip* clip;
    size_t source_frame;
    float gain;
    float pitch;
    bool active;
    bool paused;
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
henka_audio_emitter_config henka_audio_emitter_config_default(void);
henka_result henka_audio_emitter_config_validate(
    const henka_audio_emitter_config* config);
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
/* Replaces a decoded clip payload only after the complete replacement file
 * validates. The clip pointer remains stable for manager-owned borrowers, and
 * the prior payload remains live when loading or validation fails. The caller
 * must synchronize this operation with the audio owner before mixing. */
henka_result henka_audio_clip_reload_file(
    henka_audio_clip* clip,
    const char* project_root,
    const char* relative_path);
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
/* Voice controls operate on resident clips. Pausing preserves the current
 * source position; restart clears it and resumes playback. Seeking to the
 * clip frame count is valid and lets the next mix complete a non-looping
 * voice deterministically. */
henka_result henka_audio_voice_pause(
    henka_audio_system* system,
    henka_audio_voice_id voice);
henka_result henka_audio_voice_resume(
    henka_audio_system* system,
    henka_audio_voice_id voice);
henka_result henka_audio_voice_restart(
    henka_audio_system* system,
    henka_audio_voice_id voice);
henka_result henka_audio_voice_seek(
    henka_audio_system* system,
    henka_audio_voice_id voice,
    size_t source_frame);
henka_result henka_audio_voice_set_gain(
    henka_audio_system* system,
    henka_audio_voice_id voice,
    float gain);
henka_result henka_audio_voice_set_pitch(
    henka_audio_system* system,
    henka_audio_voice_id voice,
    float pitch);
bool henka_audio_voice_is_valid(
    const henka_audio_system* system,
    henka_audio_voice_id voice);
bool henka_audio_voice_is_paused(
    const henka_audio_system* system,
    henka_audio_voice_id voice);
henka_result henka_audio_voice_get_info(
    const henka_audio_system* system,
    henka_audio_voice_id voice,
    henka_audio_voice_info* out_info);
size_t henka_audio_system_get_active_voice_count(
    const henka_audio_system* system);
uint32_t henka_audio_system_get_sample_rate(
    const henka_audio_system* system);

/* Creates a real object-attached emitter through the production scene/entity
 * path. The emitter owns its resident clip and stops that voice on destroy;
 * system and scene are borrowed and must outlive the emitter. The entity may
 * be destroyed first; the mixer then rejects the stale binding and the
 * emitter becomes invalid without producing orphaned audio. */
henka_result henka_audio_emitter_create(
    henka_audio_system* system,
    const char* project_root,
    henka_scene* scene,
    henka_entity entity,
    const henka_audio_emitter_config* config,
    henka_audio_emitter** out_emitter);
/* Creates an emitter from a borrowed clip, for use with manager-owned audio
 * assets. The clip and system must outlive the emitter; the emitter does not
 * destroy the borrowed clip. */
henka_result henka_audio_emitter_create_with_clip(
    henka_audio_system* system,
    henka_scene* scene,
    henka_entity entity,
    const henka_audio_clip* clip,
    const henka_audio_emitter_config* config,
    henka_audio_emitter** out_emitter);
void henka_audio_emitter_destroy(henka_audio_emitter* emitter);
bool henka_audio_emitter_is_valid(const henka_audio_emitter* emitter);
henka_result henka_audio_emitter_get_config(
    const henka_audio_emitter* emitter,
    henka_audio_emitter_config* out_config);
henka_result henka_audio_emitter_get_voice_info(
    const henka_audio_emitter* emitter,
    henka_audio_voice_info* out_info);
/* Controls the voice owned by an emitter. Stop is idempotent for an already
 * stopped emitter; restart recreates the generation-checked voice from the
 * retained resident clip and authored configuration. Both operations reject
 * a destroyed scene entity and preserve the borrowed-object lifetime rules. */
henka_result henka_audio_emitter_stop(henka_audio_emitter* emitter);
henka_result henka_audio_emitter_restart(henka_audio_emitter* emitter);
bool henka_audio_emitter_is_playing(const henka_audio_emitter* emitter);

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
