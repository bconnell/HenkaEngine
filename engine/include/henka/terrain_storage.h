#ifndef HENKA_TERRAIN_STORAGE_H
#define HENKA_TERRAIN_STORAGE_H

#include <stddef.h>
#include <stdint.h>

#include <henka/terrain.h>

#define HENKA_TERRAIN_MAX_REGION_RECORD_BYTES (16U * 1024U * 1024U)

typedef struct henka_terrain_storage henka_terrain_storage;

typedef struct henka_terrain_region_storage_info
{
    henka_terrain_region_id id;
    henka_terrain_revision revision;
    henka_terrain_generation generation;
} henka_terrain_region_storage_info;

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
