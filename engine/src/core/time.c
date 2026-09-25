#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include <henka/time.h>

#include <math.h>
#include <time.h>

#if defined(_WIN32)
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

static double henka_time_now_seconds_internal(void)
{
#if defined(_WIN32)
    LARGE_INTEGER counter;
    LARGE_INTEGER frequency;

    if (QueryPerformanceFrequency(&frequency) != 0 &&
        frequency.QuadPart > 0 &&
        QueryPerformanceCounter(&counter) != 0)
    {
        return (double)counter.QuadPart / (double)frequency.QuadPart;
    }

    return (double)GetTickCount64() / 1000.0;
#else
    struct timespec value;

#if defined(CLOCK_MONOTONIC)
    if (clock_gettime(CLOCK_MONOTONIC, &value) == 0)
    {
        return (double)value.tv_sec + ((double)value.tv_nsec / 1000000000.0);
    }
#endif

    if (timespec_get(&value, TIME_UTC) == TIME_UTC)
    {
        return (double)value.tv_sec + ((double)value.tv_nsec / 1000000000.0);
    }

    return 0.0;
#endif
}

void henka_time_reset(henka_time_state* state)
{
    if (state == NULL)
    {
        return;
    }

    state->delta_seconds = 0.0;
    state->total_seconds = 0.0;
    state->last_tick_seconds = henka_time_now_seconds_internal();
    state->frame_index = 0U;
    state->initialized = true;
}

void henka_time_tick(henka_time_state* state)
{
    double now_seconds;

    if (state == NULL)
    {
        return;
    }

    if (!state->initialized)
    {
        henka_time_reset(state);
        return;
    }

    now_seconds = henka_time_now_seconds_internal();
    if (!isfinite(now_seconds))
    {
        state->delta_seconds = 0.0;
        state->frame_index += 1U;
        return;
    }

    state->delta_seconds = now_seconds - state->last_tick_seconds;
    if (!isfinite(state->delta_seconds) || state->delta_seconds < 0.0)
    {
        state->delta_seconds = 0.0;
    }

    state->total_seconds += state->delta_seconds;
    state->last_tick_seconds = now_seconds;
    state->frame_index += 1U;
}

void henka_frame_time_stats_reset(henka_frame_time_stats* stats)
{
    if (stats == NULL)
    {
        return;
    }
    stats->sample_count = 0U;
    stats->total_seconds = 0.0;
    stats->minimum_seconds = 0.0;
    stats->maximum_seconds = 0.0;
}

henka_result henka_frame_time_stats_push(
    henka_frame_time_stats* stats,
    double delta_seconds)
{
    if (stats == NULL ||
        !isfinite(delta_seconds) ||
        delta_seconds < 0.0 ||
        stats->sample_count == UINT64_MAX ||
        !isfinite(stats->total_seconds + delta_seconds))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    if (stats->sample_count == 0U)
    {
        stats->minimum_seconds = delta_seconds;
        stats->maximum_seconds = delta_seconds;
    }
    else
    {
        if (delta_seconds < stats->minimum_seconds)
        {
            stats->minimum_seconds = delta_seconds;
        }
        if (delta_seconds > stats->maximum_seconds)
        {
            stats->maximum_seconds = delta_seconds;
        }
    }
    stats->total_seconds += delta_seconds;
    ++stats->sample_count;
    return HENKA_SUCCESS;
}

henka_result henka_frame_time_stats_get_average(
    const henka_frame_time_stats* stats,
    double* out_average_seconds)
{
    double average;

    if (stats == NULL ||
        out_average_seconds == NULL ||
        stats->sample_count == 0U ||
        !isfinite(stats->total_seconds) ||
        stats->total_seconds < 0.0)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    average = stats->total_seconds / (double)stats->sample_count;
    if (!isfinite(average))
    {
        return HENKA_ERROR_NUMERIC_RANGE;
    }
    *out_average_seconds = average;
    return HENKA_SUCCESS;
}
