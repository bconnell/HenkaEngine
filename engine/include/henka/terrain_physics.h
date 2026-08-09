#ifndef HENKA_TERRAIN_PHYSICS_H
#define HENKA_TERRAIN_PHYSICS_H

#include <stdbool.h>
#include <stdint.h>

#include <henka/camera.h>
#include <henka/math.h>
#include <henka/terrain_collision.h>

#define HENKA_TERRAIN_PHYSICS_MAX_PATCHES 64U
#define HENKA_TERRAIN_PHYSICS_MAX_RAY_STEPS 32768U

typedef struct henka_terrain_physics henka_terrain_physics;

typedef struct henka_terrain_physics_desc
{
    uint32_t max_patches;
} henka_terrain_physics_desc;

typedef struct henka_terrain_physics_patch_desc
{
    henka_terrain_collision_patch patch;
    float sample_spacing_meters;
    float origin_x_meters;
    float origin_z_meters;
} henka_terrain_physics_patch_desc;

typedef struct henka_terrain_physics_hit
{
    bool hit;
    henka_terrain_chunk_id chunk_id;
    henka_terrain_revision revision;
    henka_terrain_generation generation;
    float distance;
    henka_vec3 position;
    float height_meters;
    henka_vec3 normal;
} henka_terrain_physics_hit;

typedef struct henka_terrain_physics_stats
{
    uint32_t resident_patch_count;
    uint32_t max_patches;
    uint64_t replacement_count;
    uint64_t rejected_replacement_count;
    uint64_t sample_query_count;
    uint64_t missed_query_count;
} henka_terrain_physics_stats;

henka_terrain_physics_desc henka_terrain_physics_desc_default(void);
henka_result henka_terrain_physics_create(
    const henka_terrain_physics_desc* desc,
    henka_terrain_physics** out_physics);
void henka_terrain_physics_destroy(henka_terrain_physics* physics);
henka_result henka_terrain_physics_replace_patch(
    henka_terrain_physics* physics,
    const henka_terrain_physics_patch_desc* desc);
henka_result henka_terrain_physics_remove_patch(
    henka_terrain_physics* physics,
    henka_terrain_chunk_id chunk_id);
henka_result henka_terrain_physics_sample(
    henka_terrain_physics* physics,
    float world_x_meters,
    float world_z_meters,
    henka_terrain_physics_hit* out_hit);
/* Bounded ray query over currently resident physics patches. */
henka_result henka_terrain_physics_raycast(
    henka_terrain_physics* physics,
    henka_ray ray,
    float max_distance,
    henka_terrain_physics_hit* out_hit);
void henka_terrain_physics_get_stats(
    const henka_terrain_physics* physics,
    henka_terrain_physics_stats* out_stats);

#endif
