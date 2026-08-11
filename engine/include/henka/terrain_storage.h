#ifndef HENKA_TERRAIN_STORAGE_H
#define HENKA_TERRAIN_STORAGE_H

#include <stddef.h>
#include <stdint.h>

#include <henka/terrain.h>

#define HENKA_TERRAIN_MAX_REGION_RECORD_BYTES (16U * 1024U * 1024U)
#define HENKA_TERRAIN_MANIFEST_VERSION UINT32_C(1)
#define HENKA_TERRAIN_MAX_MANIFEST_BYTES 256U
/* Committed journal history is reclaimed automatically once this bounded
 * threshold is reached. Explicit compaction remains available for callers
 * that want to reclaim it earlier. */
#define HENKA_TERRAIN_STORAGE_AUTO_COMPACT_THRESHOLD_BYTES (8U * 1024U * 1024U)

typedef struct henka_terrain_storage henka_terrain_storage;

henka_result henka_terrain_region_encode(
    const henka_terrain_world_desc* desc,
    henka_terrain_region_id region_id,
    henka_terrain_revision revision,
    henka_terrain_generation generation,
    const henka_terrain_sample* samples,
    size_t sample_count,
    uint8_t* buffer,
    size_t buffer_capacity,
    size_t* out_size);
henka_result henka_terrain_region_decode(
    const henka_terrain_world_desc* desc,
    const uint8_t* buffer,
    size_t buffer_size,
    henka_terrain_region_storage_info* out_info,
    henka_terrain_sample* samples,
    size_t sample_capacity);

henka_result henka_terrain_storage_create(
    const henka_terrain_world_desc* desc,
    const char* root_path,
    henka_terrain_storage** out_storage);
void henka_terrain_storage_destroy(henka_terrain_storage* storage);
henka_result henka_terrain_storage_recover(henka_terrain_storage* storage);
/* Creates the manifest when absent and rejects an incompatible existing one. */
henka_result henka_terrain_storage_ensure_manifest(henka_terrain_storage* storage);
henka_result henka_terrain_storage_load_manifest(
    henka_terrain_storage* storage,
    henka_terrain_world_desc* out_desc);
/* Reclaims committed journal history without changing region snapshots. */
henka_result henka_terrain_storage_compact(henka_terrain_storage* storage);
henka_result henka_terrain_storage_begin(
    henka_terrain_storage* storage,
    uint64_t transaction_id);
henka_result henka_terrain_storage_write_region(
    henka_terrain_storage* storage,
    henka_terrain_region_id region_id,
    henka_terrain_revision revision,
    henka_terrain_generation generation,
    const henka_terrain_sample* samples,
    size_t sample_count);
henka_result henka_terrain_storage_commit(
    henka_terrain_storage* storage,
    uint64_t transaction_id);
/* Atomically persists every currently CPU-resident world region and clears
 * their dirty flags only after the storage transaction commits. The world
 * and storage are borrowed and must describe the same bounded terrain. */
henka_result henka_terrain_storage_save_resident_regions(
    henka_terrain_storage* storage,
    henka_terrain_world* world,
    uint64_t transaction_id,
    uint32_t* out_saved_region_count);
/* Atomically persists only currently CPU-resident dirty regions and clears
 * their dirty flags only after the storage transaction commits. Clean
 * resident regions are not rewritten. */
henka_result henka_terrain_storage_save_dirty_regions(
    henka_terrain_storage* storage,
    henka_terrain_world* world,
    uint64_t transaction_id,
    uint32_t* out_saved_region_count);
henka_result henka_terrain_storage_abort(
    henka_terrain_storage* storage,
    uint64_t transaction_id);
henka_result henka_terrain_storage_load_region(
    henka_terrain_storage* storage,
    henka_terrain_region_id region_id,
    henka_terrain_region_storage_info* out_info,
    henka_terrain_sample* samples,
    size_t sample_capacity);

#endif
