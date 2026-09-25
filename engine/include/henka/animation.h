#ifndef HENKA_ANIMATION_H
#define HENKA_ANIMATION_H

#include <stdbool.h>
#include <stddef.h>

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

/* Samples caller-owned transform keys without allocating or mutating source
 * data. Key times must be strictly increasing and lie in [0,duration].
 * Non-looping tracks clamp at their ends; looping tracks wrap by duration.
 * A failed request leaves out_transform unchanged. */
henka_result henka_animation_sample_transform_track(
    const henka_animation_transform_track* track,
    double time_seconds,
    henka_transform* out_transform);

#endif
