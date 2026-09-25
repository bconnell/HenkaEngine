#ifndef HENKA_ANIMATION_H
#define HENKA_ANIMATION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <henka/math.h>
#include <henka/result.h>

#define HENKA_ANIMATION_MAX_TRACK_KEYS 4096U

typedef struct henka_animation_transform_key
{
    double time_seconds;
    henka_transform transform;
} henka_animation_transform_key;

typedef struct henka_animation_transform_track
{
    const henka_animation_transform_key* keys;
    size_t key_count;
    double duration_seconds;
    bool looping;
} henka_animation_transform_track;

typedef struct henka_animation_event
{
    double time_seconds;
    uint32_t id;
} henka_animation_event;

typedef struct henka_animation_event_track
{
    const henka_animation_event* events;
    size_t event_count;
    double duration_seconds;
    bool looping;
} henka_animation_event_track;

/* Samples caller-owned transform keys without allocating or mutating source
 * data. Key times must be strictly increasing and lie in [0,duration].
 * Non-looping tracks clamp at their ends; looping tracks wrap by duration.
 * A failed request leaves out_transform unchanged. */
henka_result henka_animation_sample_transform_track(
    const henka_animation_transform_track* track,
    double time_seconds,
    henka_transform* out_transform);

/* Collects event IDs in playback order for the half-open/closed interval
 * (start_seconds,end_seconds]. Looping tracks may cross at most one wrap
 * boundary per call. out_count always reports the required number of IDs when
 * the request is valid; insufficient capacity returns HENKA_ERROR_LIMIT
 * without writing partial output. */
henka_result henka_animation_collect_events(
    const henka_animation_event_track* track,
    double start_seconds,
    double end_seconds,
    uint32_t* out_ids,
    size_t capacity,
    size_t* out_count);

#endif
