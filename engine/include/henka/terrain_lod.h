#ifndef HENKA_TERRAIN_LOD_H
#define HENKA_TERRAIN_LOD_H

#include <stdint.h>

#include <henka/terrain.h>

#define HENKA_TERRAIN_MAX_LOD_LEVEL 3U
#define HENKA_TERRAIN_MAX_LOD_SELECTION 4096U

typedef struct henka_terrain_lod_observer
{
    float world_x_meters;
    float world_z_meters;
    float view_distance_meters;
} henka_terrain_lod_observer;

typedef struct henka_terrain_lod_chunk
{
    henka_terrain_chunk_id id;
    uint32_t lod_level;
    float distance_squared_meters;
} henka_terrain_lod_chunk;

typedef struct henka_terrain_lod_diagnostics
{
    uint32_t considered_chunks;
    uint32_t selected_chunks;
    uint32_t culled_chunks;
    uint32_t resident_regions_seen;
} henka_terrain_lod_diagnostics;

henka_result henka_terrain_lod_select(
    const henka_terrain_world* world,
    henka_terrain_lod_observer observer,
    henka_terrain_lod_chunk* out_chunks,
    uint32_t* in_out_chunk_count,
    henka_terrain_lod_diagnostics* out_diagnostics);

#endif
