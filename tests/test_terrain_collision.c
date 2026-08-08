#include <henka/terrain_collision.h>
#include <henka/terrain_edit.h>

static int test_physics_patch_revision(void)
{
    henka_terrain_world_desc desc = henka_terrain_world_desc_default();
    henka_terrain_world* world = NULL;
    henka_terrain_collision_patch patch;
    int32_t heights[HENKA_TERRAIN_COLLISION_PATCH_SAMPLES];
    henka_terrain_edit_command command = henka_terrain_edit_command_default();
    int result = 0;
    if (henka_terrain_world_create(&desc, &world) != HENKA_SUCCESS ||
        henka_terrain_world_reserve_region(world, (henka_terrain_region_id){0, 0}) != HENKA_SUCCESS ||
        henka_terrain_world_set_region_residency(
            world, (henka_terrain_region_id){0, 0}, true, false, false) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    command.center_sample_x = 10;
    command.center_sample_z = 10;
    command.radius_samples = 2U;
    command.value_millimeters = 1000;
    if (henka_terrain_world_apply_edit(world, &command, 1U) != HENKA_SUCCESS ||
        henka_terrain_world_build_collision_patch(
            world, (henka_terrain_chunk_id){0, 0}, heights,
            HENKA_TERRAIN_COLLISION_PATCH_SAMPLES, &patch) != HENKA_SUCCESS ||
        patch.revision != 1U || patch.sample_edge != HENKA_TERRAIN_COLLISION_PATCH_EDGE ||
        heights[10U * HENKA_TERRAIN_COLLISION_PATCH_EDGE + 10U] != 1000)
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
    return test_physics_patch_revision() ? 0 : 1;
}
