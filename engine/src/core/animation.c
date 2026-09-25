#include <henka/animation.h>

#include <math.h>

static bool henka_animation_vec3_is_finite(henka_vec3 value)
{
    return isfinite(value.x) && isfinite(value.y) && isfinite(value.z);
}

static bool henka_animation_quat_is_finite(henka_quat value)
{
    return isfinite(value.x) && isfinite(value.y) &&
        isfinite(value.z) && isfinite(value.w);
}

static bool henka_animation_transform_is_finite(henka_transform value)
{
    return henka_animation_vec3_is_finite(value.position) &&
        henka_animation_quat_is_finite(value.rotation) &&
        henka_animation_vec3_is_finite(value.scale);
}

static bool henka_animation_track_is_valid(
    const henka_animation_transform_track* track)
{
    size_t index;

    if (track == NULL || track->keys == NULL ||
        track->key_count == 0U ||
        track->key_count > HENKA_ANIMATION_MAX_TRACK_KEYS ||
        !isfinite(track->duration_seconds) ||
        track->duration_seconds <= 0.0)
    {
        return false;
    }

    for (index = 0U; index < track->key_count; ++index)
    {
        const henka_animation_transform_key* key = &track->keys[index];
        const float length_squared =
            key->transform.rotation.x * key->transform.rotation.x +
            key->transform.rotation.y * key->transform.rotation.y +
            key->transform.rotation.z * key->transform.rotation.z +
            key->transform.rotation.w * key->transform.rotation.w;

        if (!isfinite(key->time_seconds) ||
            key->time_seconds < 0.0 ||
            key->time_seconds > track->duration_seconds ||
            (index > 0U &&
                key->time_seconds <= track->keys[index - 1U].time_seconds) ||
            !henka_animation_transform_is_finite(key->transform) ||
            !isfinite(length_squared) ||
            length_squared <= 0.000001f)
        {
            return false;
        }
    }
    return true;
}

static float henka_animation_lerp(float left, float right, float factor)
{
    return left + (right - left) * factor;
}

static henka_vec3 henka_animation_lerp_vec3(
    henka_vec3 left,
    henka_vec3 right,
    float factor)
{
    return (henka_vec3){
        henka_animation_lerp(left.x, right.x, factor),
        henka_animation_lerp(left.y, right.y, factor),
        henka_animation_lerp(left.z, right.z, factor)};
}

static henka_quat henka_animation_nlerp_quat(
    henka_quat left,
    henka_quat right,
    float factor)
{
    const float dot =
        left.x * right.x +
        left.y * right.y +
        left.z * right.z +
        left.w * right.w;
    henka_quat candidate;

    if (dot < 0.0f)
    {
        right.x = -right.x;
        right.y = -right.y;
        right.z = -right.z;
        right.w = -right.w;
    }

    candidate = (henka_quat){
        henka_animation_lerp(left.x, right.x, factor),
        henka_animation_lerp(left.y, right.y, factor),
        henka_animation_lerp(left.z, right.z, factor),
        henka_animation_lerp(left.w, right.w, factor)};
    return henka_quat_normalize(candidate);
}

henka_result henka_animation_sample_transform_track(
    const henka_animation_transform_track* track,
    double time_seconds,
    henka_transform* out_transform)
{
    double sample_time;
    size_t upper_index;
    henka_transform candidate;

    if (!henka_animation_track_is_valid(track) ||
        out_transform == NULL ||
        !isfinite(time_seconds) ||
        time_seconds < 0.0)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    sample_time = time_seconds;
    if (track->looping)
    {
        sample_time = fmod(sample_time, track->duration_seconds);
        if (!isfinite(sample_time))
        {
            return HENKA_ERROR_NUMERIC_RANGE;
        }
    }
    else if (sample_time > track->duration_seconds)
    {
        sample_time = track->duration_seconds;
    }

    if (track->key_count == 1U ||
        sample_time <= track->keys[0].time_seconds)
    {
        candidate = track->keys[0].transform;
    }
    else if (sample_time >= track->keys[track->key_count - 1U].time_seconds)
    {
        candidate = track->keys[track->key_count - 1U].transform;
    }
    else
    {
        float factor;
        const henka_animation_transform_key* lower;
        const henka_animation_transform_key* upper;

        upper_index = 1U;
        while (upper_index < track->key_count &&
            sample_time >= track->keys[upper_index].time_seconds)
        {
            ++upper_index;
        }
        lower = &track->keys[upper_index - 1U];
        upper = &track->keys[upper_index];
        factor = (float)(
            (sample_time - lower->time_seconds) /
            (upper->time_seconds - lower->time_seconds));
        if (!isfinite(factor) || factor < 0.0f || factor > 1.0f)
        {
            return HENKA_ERROR_NUMERIC_RANGE;
        }

        candidate.position = henka_animation_lerp_vec3(
            lower->transform.position,
            upper->transform.position,
            factor);
        candidate.rotation = henka_animation_nlerp_quat(
            lower->transform.rotation,
            upper->transform.rotation,
            factor);
        candidate.scale = henka_animation_lerp_vec3(
            lower->transform.scale,
            upper->transform.scale,
            factor);
    }

    if (!henka_animation_transform_is_finite(candidate))
    {
        return HENKA_ERROR_NUMERIC_RANGE;
    }
    *out_transform = candidate;
    return HENKA_SUCCESS;
}

static bool henka_animation_event_track_is_valid(
    const henka_animation_event_track* track)
{
    size_t index;

    if (track == NULL ||
        track->events == NULL ||
        track->event_count == 0U ||
        track->event_count > HENKA_ANIMATION_MAX_TRACK_KEYS ||
        !isfinite(track->duration_seconds) ||
        track->duration_seconds <= 0.0)
    {
        return false;
    }
    for (index = 0U; index < track->event_count; ++index)
    {
        if (!isfinite(track->events[index].time_seconds) ||
            track->events[index].time_seconds < 0.0 ||
            track->events[index].time_seconds > track->duration_seconds ||
            track->events[index].id == 0U ||
            (index > 0U &&
                track->events[index].time_seconds <
                    track->events[index - 1U].time_seconds))
        {
            return false;
        }
    }
    return true;
}

static bool henka_animation_event_in_interval(
    double event_time,
    double start_time,
    double end_time,
    bool wrapped,
    bool full_cycle)
{
    if (full_cycle)
    {
        return true;
    }
    if (!wrapped)
    {
        return event_time > start_time && event_time <= end_time;
    }
    return event_time > start_time || event_time <= end_time;
}

henka_result henka_animation_collect_events(
    const henka_animation_event_track* track,
    double start_seconds,
    double end_seconds,
    uint32_t* out_ids,
    size_t capacity,
    size_t* out_count)
{
    double start_time;
    double end_time;
    double span;
    bool wrapped = false;
    bool full_cycle = false;
    size_t required = 0U;
    size_t index;

    if (out_count == NULL ||
        !henka_animation_event_track_is_valid(track) ||
        !isfinite(start_seconds) ||
        !isfinite(end_seconds) ||
        start_seconds < 0.0 ||
        end_seconds < start_seconds ||
        (capacity > 0U && out_ids == NULL))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    span = end_seconds - start_seconds;
    if (!isfinite(span))
    {
        return HENKA_ERROR_NUMERIC_RANGE;
    }

    if (track->looping)
    {
        if (span > track->duration_seconds)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        start_time = fmod(start_seconds, track->duration_seconds);
        end_time = fmod(end_seconds, track->duration_seconds);
        if (!isfinite(start_time) || !isfinite(end_time))
        {
            return HENKA_ERROR_NUMERIC_RANGE;
        }
        full_cycle = span == track->duration_seconds && span > 0.0;
        wrapped = !full_cycle && end_time < start_time;
    }
    else
    {
        if (end_seconds > track->duration_seconds)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        start_time = start_seconds;
        end_time = end_seconds;
    }

    for (index = 0U; index < track->event_count; ++index)
    {
        if (henka_animation_event_in_interval(
                track->events[index].time_seconds,
                start_time,
                end_time,
                wrapped,
                full_cycle))
        {
            ++required;
        }
    }

    *out_count = required;
    if (required > capacity)
    {
        return HENKA_ERROR_LIMIT;
    }
    if (required == 0U)
    {
        return HENKA_SUCCESS;
    }

    required = 0U;
    if (wrapped)
    {
        for (index = 0U; index < track->event_count; ++index)
        {
            if (track->events[index].time_seconds > start_time)
            {
                out_ids[required++] = track->events[index].id;
            }
        }
        for (index = 0U; index < track->event_count; ++index)
        {
            if (track->events[index].time_seconds <= end_time)
            {
                out_ids[required++] = track->events[index].id;
            }
        }
    }
    else
    {
        for (index = 0U; index < track->event_count; ++index)
        {
            if (henka_animation_event_in_interval(
                    track->events[index].time_seconds,
                    start_time,
                    end_time,
                    false,
                    full_cycle))
            {
                out_ids[required++] = track->events[index].id;
            }
        }
    }

    return HENKA_SUCCESS;
}
