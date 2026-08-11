#ifndef SANDBOX3D_TERRAIN_AUTOSAVE_H
#define SANDBOX3D_TERRAIN_AUTOSAVE_H

#include <stdbool.h>
#include <stdint.h>

#define SANDBOX3D_TERRAIN_AUTOSAVE_INTERVAL_SECONDS 10.0

/* Advances a bounded dirty-only save timer. A due timer resets to zero so the
 * caller can attempt one transaction and retry after the next interval if it
 * fails. No persistence is performed by this policy helper. */
bool sandbox3d_terrain_autosave_is_due(
    double* elapsed_seconds,
    double delta_seconds,
    uint32_t dirty_region_count,
    double interval_seconds);

#endif
