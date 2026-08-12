#ifndef HENKA_TERRAIN_REPLICA_H
#define HENKA_TERRAIN_REPLICA_H

#include <stdbool.h>
#include <stdint.h>

#include <henka/terrain_network.h>
#include <henka/terrain_storage.h>

typedef struct henka_terrain_replica henka_terrain_replica;

typedef struct henka_terrain_replica_desc
{
    henka_terrain_world* world;
    uint32_t max_snapshot_bytes;
} henka_terrain_replica_desc;

typedef struct henka_terrain_replica_diagnostics
{
    uint64_t applied_delta_count;
    uint64_t duplicate_delta_count;
    uint64_t rejected_delta_count;
    uint64_t accepted_snapshot_fragment_count;
    uint64_t completed_snapshot_count;
    uint64_t rejected_snapshot_count;
    uint64_t stale_snapshot_count;
} henka_terrain_replica_diagnostics;

henka_terrain_replica_desc henka_terrain_replica_desc_default(void);
henka_result henka_terrain_replica_create(
    const henka_terrain_replica_desc* desc,
    henka_terrain_replica** out_replica);
void henka_terrain_replica_destroy(henka_terrain_replica* replica);
henka_result henka_terrain_replica_apply_delta(
    henka_terrain_replica* replica,
    const henka_terrain_edit_delta* delta,
    bool* out_applied);
henka_result henka_terrain_replica_apply_snapshot_fragment(
    henka_terrain_replica* replica,
    const henka_terrain_snapshot_fragment* fragment,
    bool* out_complete);
void henka_terrain_replica_get_diagnostics(
    const henka_terrain_replica* replica,
    henka_terrain_replica_diagnostics* out_diagnostics);

#endif
