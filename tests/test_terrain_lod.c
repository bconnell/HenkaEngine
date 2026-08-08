#include <string.h>

#include <henka/terrain_lod.h>

static int test_deterministic_resident_selection(void)
{
    henka_terrain_world_desc desc = henka_terrain_world_desc_default();
    henka_terrain_world* world = NULL;
    henka_terrain_lod_chunk chunks[128];
    henka_terrain_lod_chunk repeat[128];
    henka_terrain_lod_diagnostics diagnostics;
    henka_terrain_lod_observer observer = {64.0F, 64.0F, 300.0F};
    uint32_t count = 128U;
    uint32_t repeat_count = 128U;
    int result = 0;
    if (henka_terrain_world_create(&desc, &world) != HENKA_SUCCESS ||
        henka_terrain_world_reserve_region(world, (henka_terrain_region_id){0, 0}) != HENKA_SUCCESS ||
        henka_terrain_world_set_region_residency(
            world, (henka_terrain_region_id){0, 0}, false, true, false) != HENKA_SUCCESS ||
        henka_terrain_lod_select(world, observer, chunks, &count, &diagnostics) != HENKA_SUCCESS ||
        count == 0U || diagnostics.selected_chunks != count ||
        henka_terrain_lod_select(world, observer, repeat, &repeat_count, NULL) != HENKA_SUCCESS ||
        repeat_count != count || memcmp(chunks, repeat, (size_t)count * sizeof(chunks[0])) != 0)
    {
        goto cleanup;
    }
    result = 1;
cleanup:
    henka_terrain_world_destroy(world);
    return result;
}

int main(void)
{
    return test_deterministic_resident_selection() ? 0 : 1;
}
