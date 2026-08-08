#include <henka/terrain_collision.h>

#include "terrain_internal.h"

henka_result henka_terrain_world_build_collision_patch(
    const henka_terrain_world* world,
    henka_terrain_chunk_id chunk_id,
    int32_t* out_heights_millimeters,
    uint32_t height_capacity,
    henka_terrain_collision_patch* out_patch)
{
    henka_terrain_region_id region_id;
    henka_terrain_region_state state;
    const henka_terrain_region_record* region;
    uint32_t local_chunk_x;
    uint32_t local_chunk_z;
    uint32_t origin_x;
    uint32_t origin_z;
    uint32_t x;
    uint32_t z;
    if (world == NULL || out_heights_millimeters == NULL || out_patch == NULL ||
        height_capacity < HENKA_TERRAIN_COLLISION_PATCH_SAMPLES ||
        !henka_terrain_chunk_id_is_valid(&world->desc, chunk_id) ||
        henka_terrain_region_id_from_chunk(&world->desc, chunk_id, &region_id) != HENKA_SUCCESS ||
        henka_terrain_world_get_region_state(world, region_id, &state) != HENKA_SUCCESS ||
        !state.physics_resident)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    region = henka_terrain_find_region_record_const(world, region_id);
    if (region == NULL || region->samples == NULL)
    {
        return HENKA_ERROR_ASSET_SOURCE;
    }
    local_chunk_x = (uint32_t)chunk_id.x % world->desc.chunks_per_region_edge;
    local_chunk_z = (uint32_t)chunk_id.z % world->desc.chunks_per_region_edge;
    origin_x = local_chunk_x * (world->desc.samples_per_chunk - 1U);
    origin_z = local_chunk_z * (world->desc.samples_per_chunk - 1U);
    for (z = 0U; z < HENKA_TERRAIN_COLLISION_PATCH_EDGE; ++z)
    {
        for (x = 0U; x < HENKA_TERRAIN_COLLISION_PATCH_EDGE; ++x)
        {
            out_heights_millimeters[z * HENKA_TERRAIN_COLLISION_PATCH_EDGE + x] =
                region->samples[(origin_z + z) * world->layout.samples_per_region_edge + origin_x + x]
                    .height_millimeters;
        }
    }
    *out_patch = (henka_terrain_collision_patch){
        chunk_id, state.revision, state.generation,
        HENKA_TERRAIN_COLLISION_PATCH_EDGE, out_heights_millimeters};
    return HENKA_SUCCESS;
}
