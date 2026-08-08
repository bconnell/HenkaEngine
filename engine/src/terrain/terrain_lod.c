#include <henka/terrain_lod.h>

#include <math.h>

henka_result henka_terrain_lod_select(
    const henka_terrain_world* world,
    henka_terrain_lod_observer observer,
    henka_terrain_lod_chunk* out_chunks,
    uint32_t* in_out_chunk_count,
    henka_terrain_lod_diagnostics* out_diagnostics)
{
    henka_terrain_world_desc desc;
    henka_terrain_lod_diagnostics diagnostics = {0};
    uint32_t capacity;
    uint32_t required = 0U;
    uint32_t region_z;
    uint32_t region_x;
    if (in_out_chunk_count == NULL || out_chunks == NULL || world == NULL ||
        !isfinite(observer.world_x_meters) || !isfinite(observer.world_z_meters) ||
        !isfinite(observer.view_distance_meters) || observer.view_distance_meters < 0.0F ||
        henka_terrain_world_get_desc(world, &desc) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    capacity = *in_out_chunk_count;
    for (region_z = 0U; region_z < desc.regions_down; ++region_z)
    {
        for (region_x = 0U; region_x < desc.regions_across; ++region_x)
        {
            henka_terrain_region_state region_state;
            uint32_t local_z;
            uint32_t local_x;
            henka_terrain_region_id region_id = {(int32_t)region_x, (int32_t)region_z};
            if (henka_terrain_world_get_region_state(world, region_id, &region_state) != HENKA_SUCCESS ||
                !region_state.render_resident)
            {
                continue;
            }
            ++diagnostics.resident_regions_seen;
            for (local_z = 0U; local_z < desc.chunks_per_region_edge; ++local_z)
            {
                for (local_x = 0U; local_x < desc.chunks_per_region_edge; ++local_x)
                {
                    henka_terrain_chunk_id chunk_id = {
                        (int32_t)(region_x * desc.chunks_per_region_edge + local_x),
                        (int32_t)(region_z * desc.chunks_per_region_edge + local_z)};
                    float center_x = ((float)chunk_id.x + 0.5F) * (float)desc.chunk_edge_meters;
                    float center_z = ((float)chunk_id.z + 0.5F) * (float)desc.chunk_edge_meters;
                    float dx = center_x - observer.world_x_meters;
                    float dz = center_z - observer.world_z_meters;
                    float distance_squared = dx * dx + dz * dz;
                    uint32_t lod_level;
                    ++diagnostics.considered_chunks;
                    if (distance_squared > observer.view_distance_meters * observer.view_distance_meters)
                    {
                        ++diagnostics.culled_chunks;
                        continue;
                    }
                    if (distance_squared <= (float)(desc.chunk_edge_meters * desc.chunk_edge_meters * 4U))
                    {
                        lod_level = 0U;
                    }
                    else if (distance_squared <= (float)(desc.chunk_edge_meters * desc.chunk_edge_meters * 16U))
                    {
                        lod_level = 1U;
                    }
                    else if (distance_squared <= (float)(desc.chunk_edge_meters * desc.chunk_edge_meters * 64U))
                    {
                        lod_level = 2U;
                    }
                    else
                    {
                        lod_level = HENKA_TERRAIN_MAX_LOD_LEVEL;
                    }
                    if (required >= HENKA_TERRAIN_MAX_LOD_SELECTION)
                    {
                        return HENKA_ERROR_LIMIT;
                    }
                    if (required < capacity)
                    {
                        out_chunks[required] = (henka_terrain_lod_chunk){chunk_id, lod_level, distance_squared};
                    }
                    ++required;
                }
            }
        }
    }
    diagnostics.selected_chunks = required;
    if (capacity < required)
    {
        *in_out_chunk_count = required;
        if (out_diagnostics != NULL)
        {
            *out_diagnostics = diagnostics;
        }
        return HENKA_ERROR_LIMIT;
    }
    *in_out_chunk_count = required;
    if (out_diagnostics != NULL)
    {
        *out_diagnostics = diagnostics;
    }
    return HENKA_SUCCESS;
}
