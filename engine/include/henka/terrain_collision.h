#ifndef HENKA_TERRAIN_COLLISION_H
#define HENKA_TERRAIN_COLLISION_H

#include <stdint.h>

#include <henka/terrain.h>

#define HENKA_TERRAIN_COLLISION_PATCH_EDGE 65U
#define HENKA_TERRAIN_COLLISION_PATCH_SAMPLES \
    (HENKA_TERRAIN_COLLISION_PATCH_EDGE * HENKA_TERRAIN_COLLISION_PATCH_EDGE)

typedef struct henka_terrain_collision_patch
{
    henka_terrain_chunk_id chunk_id;
    henka_terrain_revision revision;
    henka_terrain_generation generation;
    uint32_t sample_edge;
    int32_t* heights_millimeters;
} henka_terrain_collision_patch;

henka_result henka_terrain_world_build_collision_patch(
    const henka_terrain_world* world,
    henka_terrain_chunk_id chunk_id,
    int32_t* out_heights_millimeters,
    uint32_t height_capacity,
    henka_terrain_collision_patch* out_patch);

#endif
