#include <stdint.h>
#include <string.h>

#include <henka/terrain.h>

static int test_default_layout(void)
{
    henka_terrain_world_desc desc = henka_terrain_world_desc_default();
    henka_terrain_layout layout;

    if (henka_terrain_world_desc_validate(&desc) != HENKA_SUCCESS ||
        desc.world_width_meters != 8192U ||
        desc.world_depth_meters != 8192U ||
        desc.region_edge_meters != 512U ||
        desc.chunk_edge_meters != 64U ||
        desc.samples_per_chunk != 65U ||
        desc.base_sample_spacing_meters != 1U ||
        desc.chunks_per_region_edge != 8U ||
        desc.regions_across != 16U ||
        henka_terrain_world_desc_get_layout(&desc, &layout) != HENKA_SUCCESS ||
        layout.samples_per_region_edge != 513U ||
        layout.samples_per_region != 513U * 513U ||
        layout.chunks_per_world != 128U * 128U)
    {
        return 0;
    }
    desc.samples_per_chunk = 64U;
    return henka_terrain_world_desc_validate(&desc) != HENKA_SUCCESS;
}

static int test_deterministic_weights(void)
{
    uint8_t weights[HENKA_TERRAIN_ACTIVE_MATERIAL_COUNT] = {1U, 2U, 3U, 4U};
    uint8_t expected[HENKA_TERRAIN_ACTIVE_MATERIAL_COUNT] = {26U, 51U, 76U, 102U};
    uint8_t zero_weights[HENKA_TERRAIN_ACTIVE_MATERIAL_COUNT] = {0U, 0U, 0U, 0U};
    uint32_t total = 0U;
    uint32_t index;

    if (henka_terrain_normalize_weights(weights) != HENKA_SUCCESS ||
        memcmp(weights, expected, sizeof(weights)) != 0)
    {
        return 0;
    }
    for (index = 0U; index < HENKA_TERRAIN_ACTIVE_MATERIAL_COUNT; ++index)
    {
        total += weights[index];
    }
    if (total != 255U ||
        henka_terrain_normalize_weights(zero_weights) != HENKA_SUCCESS ||
        zero_weights[0] != 255U || zero_weights[1] != 0U ||
        zero_weights[2] != 0U || zero_weights[3] != 0U)
    {
        return 0;
    }
    return 1;
}

static int test_bounded_residency(void)
{
    henka_terrain_world_desc desc = henka_terrain_world_desc_default();
    henka_terrain_world* world = NULL;
    henka_terrain_region_id region = {0, 0};
    henka_terrain_chunk_id chunk = {0, 0};
    henka_terrain_region_state region_state;
    henka_terrain_world_stats stats;

    desc.max_resident_regions = 1U;
    desc.max_resident_chunks = 1U;
    if (henka_terrain_world_create(&desc, &world) != HENKA_SUCCESS ||
        henka_terrain_world_reserve_region(world, region) != HENKA_SUCCESS ||
        henka_terrain_world_reserve_chunk(world, chunk) != HENKA_SUCCESS ||
        henka_terrain_world_reserve_chunk(world, (henka_terrain_chunk_id){1, 0}) == HENKA_SUCCESS ||
        henka_terrain_world_get_resident_region_at(world, 0U, &region_state) != HENKA_SUCCESS ||
        !henka_terrain_region_id_equal(region_state.id, region) ||
        henka_terrain_world_get_resident_region_at(world, 1U, &region_state) == HENKA_SUCCESS ||
        henka_terrain_world_get_region_state(world, region, &region_state) != HENKA_SUCCESS ||
        !region_state.cpu_resident || region_state.revision != 0U ||
        henka_terrain_world_get_stats(world, &stats) != HENKA_SUCCESS ||
        stats.resident_region_count != 1U || stats.resident_chunk_count != 1U ||
        stats.max_resident_regions != 1U || stats.max_resident_chunks != 1U)
    {
        henka_terrain_world_destroy(world);
        return 0;
    }
    henka_terrain_world_release_chunk(world, chunk);
    henka_terrain_world_release_region(world, region);
    henka_terrain_world_destroy(world);
    return 1;
}

int main(void)
{
    return test_default_layout() && test_deterministic_weights() && test_bounded_residency() ? 0 : 1;
}
