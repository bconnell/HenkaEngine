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

static int test_runtime_queue_saturation_recovers(void)
{
    henka_terrain_world_desc world_desc = henka_terrain_world_desc_default();
    henka_terrain_world* world = NULL;
    henka_terrain_physics* physics = NULL;
    henka_terrain_collision_runtime* runtime = NULL;
    henka_terrain_physics_desc physics_desc = {4U};
    henka_terrain_collision_runtime_desc runtime_desc = {3U};
    henka_terrain_collision_runtime_stats stats;
    int result = 1;

    world_desc.max_resident_regions = 1U;
    if (henka_terrain_world_create(&world_desc, &world) != HENKA_SUCCESS ||
        henka_terrain_world_reserve_region(world, (henka_terrain_region_id){0, 0}) != HENKA_SUCCESS ||
        henka_terrain_world_set_region_residency(
            world, (henka_terrain_region_id){0, 0}, true, true, false) != HENKA_SUCCESS ||
        henka_terrain_physics_create(&physics_desc, &physics) != HENKA_SUCCESS ||
        henka_terrain_collision_runtime_create(
            world, physics, &runtime_desc, &runtime) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    if (henka_terrain_collision_runtime_request_chunk(
            runtime, (henka_terrain_chunk_id){0, 0}) != HENKA_SUCCESS ||
        henka_terrain_collision_runtime_request_chunk(
            runtime, (henka_terrain_chunk_id){1, 0}) != HENKA_SUCCESS ||
        henka_terrain_collision_runtime_request_chunk(
            runtime, (henka_terrain_chunk_id){2, 0}) != HENKA_SUCCESS ||
        henka_terrain_collision_runtime_request_chunk(
            runtime, (henka_terrain_chunk_id){3, 0}) != HENKA_ERROR_LIMIT)
    {
        goto cleanup;
    }
    henka_terrain_collision_runtime_get_stats(runtime, &stats);
    if (stats.pending_chunk_count != 3U || stats.max_pending_chunk_count != 3U ||
        stats.queued_count != 3U || stats.dropped_count != 1U)
    {
        goto cleanup;
    }
    if (henka_terrain_collision_runtime_pump(runtime, 1U) != HENKA_SUCCESS ||
        henka_terrain_collision_runtime_remove_chunk(
            runtime, (henka_terrain_chunk_id){0, 0}) != HENKA_SUCCESS ||
        henka_terrain_collision_runtime_request_chunk(
            runtime, (henka_terrain_chunk_id){3, 0}) != HENKA_SUCCESS ||
        henka_terrain_collision_runtime_pump(runtime, 3U) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    henka_terrain_collision_runtime_get_stats(runtime, &stats);
    if (stats.pending_chunk_count != 0U || stats.queued_count != 4U ||
        stats.rebuilt_count != 4U || stats.dropped_count != 1U)
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

static int test_runtime_failed_rebuild_retains_previous_patch(void)
{
    henka_terrain_world_desc world_desc = henka_terrain_world_desc_default();
    henka_terrain_world* world = NULL;
    henka_terrain_physics* physics = NULL;
    henka_terrain_collision_runtime* runtime = NULL;
    henka_terrain_physics_desc physics_desc = henka_terrain_physics_desc_default();
    henka_terrain_collision_runtime_stats stats;
    henka_terrain_physics_hit hit;
    int result = 1;

    world_desc.max_resident_regions = 1U;
    if (henka_terrain_world_create(&world_desc, &world) != HENKA_SUCCESS ||
        henka_terrain_world_reserve_region(world, (henka_terrain_region_id){0, 0}) != HENKA_SUCCESS ||
        henka_terrain_world_set_region_residency(
            world, (henka_terrain_region_id){0, 0}, true, true, false) != HENKA_SUCCESS ||
        henka_terrain_physics_create(&physics_desc, &physics) != HENKA_SUCCESS ||
        henka_terrain_collision_runtime_create(
            world, physics, NULL, &runtime) != HENKA_SUCCESS ||
        henka_terrain_collision_runtime_request_chunk(
            runtime, (henka_terrain_chunk_id){0, 0}) != HENKA_SUCCESS ||
        henka_terrain_collision_runtime_pump(runtime, 1U) != HENKA_SUCCESS ||
        henka_terrain_physics_sample(physics, 1.0f, 1.0f, &hit) != HENKA_SUCCESS ||
        !hit.hit || hit.height_meters != 0.0f)
    {
        goto cleanup;
    }
    if (henka_terrain_world_set_region_residency(
            world, (henka_terrain_region_id){0, 0}, false, false, false) != HENKA_SUCCESS ||
        henka_terrain_collision_runtime_request_chunk(
            runtime, (henka_terrain_chunk_id){0, 0}) != HENKA_SUCCESS ||
        henka_terrain_collision_runtime_pump(runtime, 1U) != HENKA_SUCCESS ||
        henka_terrain_physics_sample(physics, 1.0f, 1.0f, &hit) != HENKA_SUCCESS ||
        !hit.hit || hit.height_meters != 0.0f)
    {
        goto cleanup;
    }
    henka_terrain_collision_runtime_get_stats(runtime, &stats);
    if (stats.failed_count != 1U || stats.rebuilt_count != 1U ||
        stats.pending_chunk_count != 0U)
    {
        goto cleanup;
    }
    if (henka_terrain_world_set_region_residency(
            world, (henka_terrain_region_id){0, 0}, true, true, false) != HENKA_SUCCESS ||
        henka_terrain_collision_runtime_request_chunk(
            runtime, (henka_terrain_chunk_id){0, 0}) != HENKA_SUCCESS ||
        henka_terrain_collision_runtime_pump(runtime, 1U) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    henka_terrain_collision_runtime_get_stats(runtime, &stats);
    if (stats.failed_count != 1U || stats.rebuilt_count != 2U)
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

static int test_runtime_distributes_bounded_patches_across_regions(void)
{
    henka_terrain_world_desc world_desc = henka_terrain_world_desc_default();
    henka_terrain_world* world = NULL;
    henka_terrain_physics* physics = NULL;
    henka_terrain_collision_runtime* runtime = NULL;
    const henka_terrain_region_id regions[] = {{0, 0}, {1, 0}, {0, 1}, {1, 1}};
    const henka_terrain_physics_desc physics_desc = {4U};
    const henka_terrain_collision_runtime_desc runtime_desc = {4U};
    size_t region_index;
    int result = 1;

    world_desc.max_resident_regions = 5U;
    if (henka_terrain_world_create(&world_desc, &world) != HENKA_SUCCESS ||
        henka_terrain_physics_create(&physics_desc, &physics) != HENKA_SUCCESS ||
        henka_terrain_collision_runtime_create(
            world, physics, &runtime_desc, &runtime) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    for (region_index = 0U; region_index < sizeof(regions) / sizeof(regions[0]); ++region_index)
    {
        if (henka_terrain_world_reserve_region(world, regions[region_index]) != HENKA_SUCCESS ||
            henka_terrain_world_set_region_residency(
                world, regions[region_index], true, true, false) != HENKA_SUCCESS)
        {
            goto cleanup;
        }
    }
    if (henka_terrain_collision_runtime_sync_residency(runtime) != HENKA_SUCCESS ||
        henka_terrain_collision_runtime_pump(runtime, 4U) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    for (region_index = 0U; region_index < sizeof(regions) / sizeof(regions[0]); ++region_index)
    {
        henka_terrain_physics_hit hit = {0};
        const float x = (float)regions[region_index].x * 512.0f + 32.0f;
        const float z = (float)regions[region_index].z * 512.0f + 32.0f;
        if (henka_terrain_physics_sample(physics, x, z, &hit) != HENKA_SUCCESS || !hit.hit)
        {
            goto cleanup;
        }
    }
    if (henka_terrain_world_reserve_region(world, (henka_terrain_region_id){2, 0}) != HENKA_SUCCESS ||
        henka_terrain_world_set_region_residency(
            world, (henka_terrain_region_id){2, 0}, true, true, false) != HENKA_SUCCESS ||
        henka_terrain_collision_runtime_set_focus(
            runtime, (henka_terrain_region_id){2, 0}) != HENKA_SUCCESS ||
        henka_terrain_collision_runtime_sync_residency(runtime) != HENKA_SUCCESS ||
        henka_terrain_collision_runtime_pump(runtime, 1U) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    {
        henka_terrain_physics_hit focus_hit = {0};
        if (henka_terrain_physics_sample(physics, 2.0f * 512.0f + 32.0f, 32.0f, &focus_hit) != HENKA_SUCCESS ||
            !focus_hit.hit)
        {
            goto cleanup;
        }
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
        test_runtime_syncs_physics_residency() ||
        test_runtime_queue_saturation_recovers() ||
        test_runtime_failed_rebuild_retains_previous_patch() ||
        test_runtime_distributes_bounded_patches_across_regions();
}
