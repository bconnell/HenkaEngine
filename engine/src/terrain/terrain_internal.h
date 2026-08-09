#ifndef HENKA_TERRAIN_INTERNAL_H
#define HENKA_TERRAIN_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <henka/terrain.h>
#include <henka/terrain_storage.h>

typedef struct henka_terrain_region_record
{
    bool active;
    henka_terrain_region_state state;
    henka_terrain_sample* samples;
    size_t sample_count;
} henka_terrain_region_record;

typedef struct henka_terrain_chunk_record
{
    bool active;
    henka_terrain_chunk_id id;
} henka_terrain_chunk_record;

struct henka_terrain_world
{
    henka_terrain_world_desc desc;
    henka_terrain_layout layout;
    henka_terrain_region_record* regions;
    henka_terrain_chunk_record* chunks;
    uint32_t resident_region_count;
    uint32_t resident_chunk_count;
    uint32_t pending_io_count;
};

henka_terrain_region_record* henka_terrain_find_region_record(
    henka_terrain_world* world,
    henka_terrain_region_id id);
const henka_terrain_region_record* henka_terrain_find_region_record_const(
    const henka_terrain_world* world,
    henka_terrain_region_id id);
henka_terrain_chunk_record* henka_terrain_find_chunk_record(
    henka_terrain_world* world,
    henka_terrain_chunk_id id);
#endif
