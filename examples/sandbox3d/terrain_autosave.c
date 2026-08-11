#include "terrain_autosave.h"

#include <math.h>
#include <stddef.h>

bool sandbox3d_terrain_autosave_is_due(
    double* elapsed_seconds,
    double delta_seconds,
    uint32_t dirty_region_count,
    double interval_seconds)
{
    if (elapsed_seconds == NULL)
    {
        return false;
    }
    if (!isfinite(interval_seconds) || interval_seconds <= 0.0 ||
        !isfinite(delta_seconds) || delta_seconds < 0.0)
    {
        *elapsed_seconds = 0.0;
        return false;
    }
    if (dirty_region_count == 0U)
    {
        *elapsed_seconds = 0.0;
        return false;
    }
    if (!isfinite(*elapsed_seconds) || *elapsed_seconds < 0.0)
    {
        *elapsed_seconds = 0.0;
    }
    if (*elapsed_seconds >= interval_seconds ||
        delta_seconds >= interval_seconds - *elapsed_seconds)
    {
        *elapsed_seconds = 0.0;
        return true;
    }
    *elapsed_seconds += delta_seconds;
    return false;
}
