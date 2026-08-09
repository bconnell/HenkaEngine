#include <henka/terrain_collision_runtime.h>
#include <henka/terrain_edit.h>

static int test_runtime_edit_discovers_neighbor_chunks(void)
{
    henka_terrain_world_desc world_desc = henka_terrain_world_desc_default();
    henka_terrain_world* world = NULL;
    henka_terrain_physics* physics = NULL;
    henka_terrain_collision_runtime* runtime = NULL;
    henka_terrain_physics_desc physics_desc = henka_terrain_physics_desc_default();
    henka_terrain_collision_runtime_stats stats;
    henka_terrain_edit_command command = henka_terrain_edit_command_default();
    int result = 1;

    if (henka_terrain_world_create(&world_desc, &world) != HENKA_SUCCESS ||
        henka_terrain_world_reserve_region(world, (henka_terrain_region_id){0, 0}) != HENKA_SUCCESS ||
        henka_terrain_world_set_region_residency(
            world, (henka_terrain_region_id){0, 0}, true, false, false) != HENKA_SUCCESS ||
        henka_terrain_physics_create(&physics_desc, &physics) != HENKA_SUCCESS ||
        henka_terrain_collision_runtime_create(
            world, physics, NULL, &runtime) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    command.center_sample_x = 63;
    command.center_sample_z = 63;
    command.radius_samples = 1U;
    if (henka_terrain_world_apply_edit(world, &command, 1U) != HENKA_SUCCESS ||
        henka_terrain_collision_runtime_request_edit(runtime, &command) != HENKA_SUCCESS ||
        henka_terrain_collision_runtime_pump(runtime, 16U) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    henka_terrain_collision_runtime_get_stats(runtime, &stats);
    if (stats.queued_count != 9U || stats.rebuilt_count != 9U ||
        stats.max_pending_chunk_count < 9U || stats.pending_chunk_count != 0U)
    {
        goto cleanup;
    }
    result = 0;

cleanup:
    henka_terrain_collision_runtime_destroy(runtime);
    henka_terrain_physics_destroy(physics);
    henka_terrain_world_destroy(world);
    return result;
}

static int test_runtime_syncs_physics_residency(void)
{
    henka_terrain_world_desc world_desc = henka_terrain_world_desc_default();
    henka_terrain_world* world = NULL;
    henka_terrain_physics* physics = NULL;
    henka_terrain_collision_runtime* runtime = NULL;
    henka_terrain_physics_desc physics_desc = {4U};
    henka_terrain_collision_runtime_desc runtime_desc = {4U};
    henka_terrain_physics_stats physics_stats;
    int result = 1;

    world_desc.max_resident_regions = 1U;
    if (henka_terrain_world_create(&world_desc, &world) != HENKA_SUCCESS ||
        henka_terrain_world_reserve_region(world, (henka_terrain_region_id){0, 0}) != HENKA_SUCCESS ||
        henka_terrain_world_set_region_residency(
            world, (henka_terrain_region_id){0, 0}, true, false, false) != HENKA_SUCCESS ||
        henka_terrain_physics_create(&physics_desc, &physics) != HENKA_SUCCESS ||
        henka_terrain_collision_runtime_create(
            world, physics, &runtime_desc, &runtime) != HENKA_SUCCESS ||
        henka_terrain_collision_runtime_sync_residency(runtime) != HENKA_SUCCESS ||
        henka_terrain_collision_runtime_pump(runtime, 4U) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    henka_terrain_physics_get_stats(physics, &physics_stats);
    if (physics_stats.resident_patch_count != 4U)
    {
        goto cleanup;
    }
    if (henka_terrain_world_set_region_residency(
            world, (henka_terrain_region_id){0, 0}, false, false, false) != HENKA_SUCCESS ||
        henka_terrain_collision_runtime_sync_residency(runtime) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    henka_terrain_physics_get_stats(physics, &physics_stats);
    if (physics_stats.resident_patch_count != 0U)
    {
        goto cleanup;
    }
    result = 0;

cleanup:
    henka_terrain_collision_runtime_destroy(runtime);
    henka_terrain_physics_destroy(physics);
    henka_terrain_world_destroy(world);
    return result;
}

int main(void)
{
    henka_terrain_world_desc world_desc = henka_terrain_world_desc_default();
    henka_terrain_world* world = NULL;
    henka_terrain_physics* physics = NULL;
    henka_terrain_collision_runtime* runtime = NULL;
    henka_terrain_physics_desc physics_desc = henka_terrain_physics_desc_default();
    henka_terrain_collision_runtime_stats stats;
    henka_terrain_physics_hit hit;
    henka_terrain_edit_command command = henka_terrain_edit_command_default();
    int result = 1;

    world_desc.max_resident_regions = 2U;
    if (henka_terrain_world_create(&world_desc, &world) != HENKA_SUCCESS ||
        henka_terrain_world_reserve_region(world, (henka_terrain_region_id){0, 0}) != HENKA_SUCCESS ||
        henka_terrain_world_reserve_chunk(world, (henka_terrain_chunk_id){0, 0}) != HENKA_SUCCESS ||
        henka_terrain_world_set_region_residency(
            world, (henka_terrain_region_id){0, 0}, true, false, false) != HENKA_SUCCESS ||
        henka_terrain_physics_create(&physics_desc, &physics) != HENKA_SUCCESS ||
        henka_terrain_collision_runtime_create(
            world, physics, NULL, &runtime) != HENKA_SUCCESS ||
        henka_terrain_collision_runtime_request_chunk(runtime, (henka_terrain_chunk_id){0, 0}) != HENKA_SUCCESS ||
        henka_terrain_collision_runtime_request_chunk(runtime, (henka_terrain_chunk_id){0, 0}) != HENKA_SUCCESS ||
        henka_terrain_collision_runtime_pump(runtime, 1U) != HENKA_SUCCESS ||
        henka_terrain_physics_sample(physics, 1.0f, 1.0f, &hit) != HENKA_SUCCESS ||
        !hit.hit || hit.height_meters != 0.0f)
    {
        goto cleanup;
    }
    command.operation = HENKA_TERRAIN_EDIT_RAISE;
    command.center_sample_x = 1;
    command.center_sample_z = 1;
    command.radius_samples = 1U;
    command.value_millimeters = 1000;
    if (henka_terrain_world_apply_edit(world, &command, 1U) != HENKA_SUCCESS ||
        henka_terrain_collision_runtime_request_chunk(runtime, (henka_terrain_chunk_id){0, 0}) != HENKA_SUCCESS ||
        henka_terrain_collision_runtime_pump(runtime, 1U) != HENKA_SUCCESS ||
        henka_terrain_physics_sample(physics, 1.0f, 1.0f, &hit) != HENKA_SUCCESS ||
        !hit.hit || hit.height_meters <= 0.0f)
    {
        goto cleanup;
    }
    henka_terrain_collision_runtime_get_stats(runtime, &stats);
    if (stats.rebuilt_count != 2U || stats.coalesced_count != 1U || stats.pending_chunk_count != 0U)
    {
        goto cleanup;
    }
    result = 0;

cleanup:
    henka_terrain_collision_runtime_destroy(runtime);
    henka_terrain_physics_destroy(physics);
    henka_terrain_world_destroy(world);
    return result || test_runtime_edit_discovers_neighbor_chunks() ||
        test_runtime_syncs_physics_residency();
}
