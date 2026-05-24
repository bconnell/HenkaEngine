#ifndef HENKA_TIME_H
#define HENKA_TIME_H

#include <stdbool.h>
#include <stdint.h>

typedef struct henka_time_state
{
    double delta_seconds;
    double total_seconds;
    double last_tick_seconds;
    uint64_t frame_index;
    bool initialized;
} henka_time_state;

void henka_time_reset(henka_time_state* state);
void henka_time_tick(henka_time_state* state);

#endif
