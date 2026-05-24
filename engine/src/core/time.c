#include <henka/time.h>

#include <time.h>

static double henka_time_now_seconds_internal(void)
{
    struct timespec value;

    timespec_get(&value, TIME_UTC);
    return (double)value.tv_sec + ((double)value.tv_nsec / 1000000000.0);
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
    state->delta_seconds = now_seconds - state->last_tick_seconds;
    if (state->delta_seconds < 0.0)
    {
        state->delta_seconds = 0.0;
    }

    state->total_seconds += state->delta_seconds;
    state->last_tick_seconds = now_seconds;
    state->frame_index += 1U;
}
