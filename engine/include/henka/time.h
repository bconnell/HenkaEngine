#ifndef HENKA_TIME_H
#define HENKA_TIME_H

#include <stdbool.h>
#include <stdint.h>

#include <henka/result.h>

typedef struct henka_time_state
{
    double delta_seconds;
    double total_seconds;
    double last_tick_seconds;
    uint64_t frame_index;
    bool initialized;
} henka_time_state;

typedef struct henka_frame_time_stats
{
    uint64_t sample_count;
    double total_seconds;
    double minimum_seconds;
    double maximum_seconds;
} henka_frame_time_stats;

void henka_time_reset(henka_time_state* state);
void henka_time_tick(henka_time_state* state);

void henka_frame_time_stats_reset(henka_frame_time_stats* stats);
henka_result henka_frame_time_stats_push(
    henka_frame_time_stats* stats,
    double delta_seconds);
henka_result henka_frame_time_stats_get_average(
    const henka_frame_time_stats* stats,
    double* out_average_seconds);

#endif
