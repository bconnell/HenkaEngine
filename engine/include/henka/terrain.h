#ifndef HENKA_TERRAIN_H
#define HENKA_TERRAIN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <henka/result.h>

#define HENKA_TERRAIN_FORMAT_VERSION UINT32_C(1)
#define HENKA_TERRAIN_ACTIVE_MATERIAL_COUNT 4U
#define HENKA_TERRAIN_DEFAULT_WORLD_EDGE_METERS 8192U
#define HENKA_TERRAIN_DEFAULT_REGION_EDGE_METERS 512U
#define HENKA_TERRAIN_DEFAULT_CHUNK_EDGE_METERS 64U
#define HENKA_TERRAIN_DEFAULT_SAMPLES_PER_CHUNK 65U
#define HENKA_TERRAIN_DEFAULT_SAMPLE_SPACING_METERS 1U
#define HENKA_TERRAIN_DEFAULT_CHUNKS_PER_REGION_EDGE 8U
#define HENKA_TERRAIN_DEFAULT_REGIONS_ACROSS 16U
#define HENKA_TERRAIN_MAX_WORLD_EDGE_METERS 8192U
#define HENKA_TERRAIN_MAX_RESIDENT_REGIONS 256U
#define HENKA_TERRAIN_MAX_RESIDENT_CHUNKS 4096U
#define HENKA_TERRAIN_MAX_PENDING_IO 4096U
#define HENKA_TERRAIN_MAX_STREAM_OBSERVERS 64U

typedef struct henka_terrain_world henka_terrain_world;

typedef uint64_t henka_terrain_world_identity;
typedef uint64_t henka_terrain_base_asset_identity;
typedef uint64_t henka_terrain_revision;
typedef uint64_t henka_terrain_generation;

typedef struct henka_terrain_region_id
{
    int32_t x;
    int32_t z;
} henka_terrain_region_id;

typedef struct henka_terrain_chunk_id
{
    int32_t x;
    int32_t z;
} henka_terrain_chunk_id;

typedef struct henka_terrain_sample
{
    int32_t height_millimeters;
    uint8_t material_weights[HENKA_TERRAIN_ACTIVE_MATERIAL_COUNT];
} henka_terrain_sample;

typedef struct henka_terrain_world_desc
{
    uint32_t format_version;
    henka_terrain_world_identity world_identity;
    henka_terrain_base_asset_identity base_asset_identity;
    uint32_t world_width_meters;
    uint32_t world_depth_meters;
    uint32_t region_edge_meters;
    uint32_t chunk_edge_meters;
    uint32_t samples_per_chunk;
    uint32_t base_sample_spacing_meters;
    uint32_t chunks_per_region_edge;
    uint32_t regions_across;
    uint32_t regions_down;
    uint32_t max_resident_regions;
    uint32_t max_resident_chunks;
    uint32_t max_pending_io;
    uint32_t max_stream_observers;
} henka_terrain_world_desc;

typedef struct henka_terrain_layout
{
    uint32_t samples_per_region_edge;
    size_t samples_per_region;
    uint32_t chunks_across;
    uint32_t chunks_down;
    size_t chunks_per_world;
} henka_terrain_layout;

typedef struct henka_terrain_region_state
{
    henka_terrain_region_id id;
    henka_terrain_revision revision;
    henka_terrain_generation generation;
    uint32_t resident_chunk_count;
    bool cpu_resident;
    bool physics_resident;
    bool render_resident;
    bool pending_io;
    bool dirty;
} henka_terrain_region_state;

typedef struct henka_terrain_world_stats
{
    uint32_t resident_region_count;
    uint32_t resident_chunk_count;
    uint32_t pending_io_count;
    uint32_t max_resident_regions;
    uint32_t max_resident_chunks;
    uint32_t max_pending_io;
} henka_terrain_world_stats;

henka_terrain_world_desc henka_terrain_world_desc_default(void);
henka_result henka_terrain_world_desc_validate(const henka_terrain_world_desc* desc);
henka_result henka_terrain_world_desc_get_layout(
    const henka_terrain_world_desc* desc,
    henka_terrain_layout* out_layout);

bool henka_terrain_region_id_equal(henka_terrain_region_id left, henka_terrain_region_id right);
bool henka_terrain_chunk_id_equal(henka_terrain_chunk_id left, henka_terrain_chunk_id right);
bool henka_terrain_region_id_is_valid(
    const henka_terrain_world_desc* desc,
    henka_terrain_region_id id);
bool henka_terrain_chunk_id_is_valid(
    const henka_terrain_world_desc* desc,
    henka_terrain_chunk_id id);
henka_result henka_terrain_region_id_from_chunk(
    const henka_terrain_world_desc* desc,
    henka_terrain_chunk_id chunk_id,
    henka_terrain_region_id* out_region_id);

henka_result henka_terrain_normalize_weights(
    uint8_t weights[HENKA_TERRAIN_ACTIVE_MATERIAL_COUNT]);

henka_result henka_terrain_world_create(
    const henka_terrain_world_desc* desc,
    henka_terrain_world** out_world);
void henka_terrain_world_destroy(henka_terrain_world* world);
henka_result henka_terrain_world_get_desc(
    const henka_terrain_world* world,
    henka_terrain_world_desc* out_desc);
henka_result henka_terrain_world_reserve_region(
    henka_terrain_world* world,
    henka_terrain_region_id region_id);
henka_result henka_terrain_world_release_region(
    henka_terrain_world* world,
    henka_terrain_region_id region_id);
henka_result henka_terrain_world_reserve_chunk(
    henka_terrain_world* world,
    henka_terrain_chunk_id chunk_id);
henka_result henka_terrain_world_release_chunk(
    henka_terrain_world* world,
    henka_terrain_chunk_id chunk_id);
henka_result henka_terrain_world_set_region_residency(
    henka_terrain_world* world,
    henka_terrain_region_id region_id,
    bool physics_resident,
    bool render_resident,
    bool pending_io);
henka_result henka_terrain_world_set_region_revision(
    henka_terrain_world* world,
    henka_terrain_region_id region_id,
    henka_terrain_revision revision,
    henka_terrain_generation generation,
    bool dirty);
henka_result henka_terrain_world_get_region_state(
    const henka_terrain_world* world,
    henka_terrain_region_id region_id,
    henka_terrain_region_state* out_state);
henka_result henka_terrain_world_get_stats(
    const henka_terrain_world* world,
    henka_terrain_world_stats* out_stats);

#endif
