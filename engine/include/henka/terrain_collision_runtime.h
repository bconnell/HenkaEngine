#ifndef HENKA_TERRAIN_COLLISION_RUNTIME_H
#define HENKA_TERRAIN_COLLISION_RUNTIME_H

#include <stdint.h>

#include <henka/terrain_edit.h>
#include <henka/terrain_physics.h>

#define HENKA_TERRAIN_COLLISION_RUNTIME_MAX_PENDING 256U

typedef struct henka_terrain_collision_runtime henka_terrain_collision_runtime;

typedef struct henka_terrain_collision_runtime_desc
{
    uint32_t max_pending_chunks;
} henka_terrain_collision_runtime_desc;

typedef struct henka_terrain_collision_runtime_stats
{
    uint32_t pending_chunk_count;
    uint32_t max_pending_chunk_count;
    uint64_t queued_count;
    uint64_t coalesced_count;
    uint64_t rebuilt_count;
    uint64_t failed_count;
    uint64_t dropped_count;
} henka_terrain_collision_runtime_stats;

henka_terrain_collision_runtime_desc henka_terrain_collision_runtime_desc_default(void);
henka_result henka_terrain_collision_runtime_create(
    henka_terrain_world* world,
    henka_terrain_physics* physics,
    const henka_terrain_collision_runtime_desc* desc,
    henka_terrain_collision_runtime** out_runtime);
void henka_terrain_collision_runtime_destroy(henka_terrain_collision_runtime* runtime);
henka_result henka_terrain_collision_runtime_request_chunk(
    henka_terrain_collision_runtime* runtime,
    henka_terrain_chunk_id chunk_id);
/* Gives bounded collision residency a deterministic camera/interaction focus. */
henka_result henka_terrain_collision_runtime_set_focus(
    henka_terrain_collision_runtime* runtime,
    henka_terrain_region_id region_id);
/* Synchronizes bounded physics patches with currently physics-resident regions. */
henka_result henka_terrain_collision_runtime_sync_residency(
    henka_terrain_collision_runtime* runtime);
/* Queues height-edit coverage; paint-only edits return without collision work. */
henka_result henka_terrain_collision_runtime_request_edit(
    henka_terrain_collision_runtime* runtime,
    const henka_terrain_edit_command* command);
henka_result henka_terrain_collision_runtime_remove_chunk(
    henka_terrain_collision_runtime* runtime,
    henka_terrain_chunk_id chunk_id);
henka_result henka_terrain_collision_runtime_pump(
    henka_terrain_collision_runtime* runtime,
    uint32_t max_rebuilds);
void henka_terrain_collision_runtime_get_stats(
    const henka_terrain_collision_runtime* runtime,
    henka_terrain_collision_runtime_stats* out_stats);

#endif
