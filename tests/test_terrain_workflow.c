#include <stdint.h>
#include <string.h>

#include <henka/memory.h>
#include <henka/terrain_collision.h>
#include <henka/terrain_edit.h>
#include <henka/terrain_physics.h>
#include <henka/terrain_storage.h>

static int test_runtime_edit_collision_and_restart(void)
{
    const henka_terrain_region_id region = {0, 0};
    const henka_terrain_chunk_id chunk = {0, 0};
    henka_terrain_world_desc desc = henka_terrain_world_desc_default();
    henka_terrain_layout layout;
    henka_terrain_world* world = NULL;
    henka_terrain_world* restarted_world = NULL;
    henka_terrain_storage* storage = NULL;
    henka_terrain_storage* recovered_storage = NULL;
    henka_terrain_physics* physics = NULL;
    henka_terrain_physics* restarted_physics = NULL;
    henka_terrain_sample* samples = NULL;
    henka_terrain_sample* recovered_samples = NULL;
    int32_t* collision_heights = NULL;
    const henka_terrain_sample* world_samples = NULL;
    henka_terrain_region_storage_info info;
    henka_terrain_region_storage_info committed_info;
    henka_terrain_region_state state;
    henka_terrain_physics_hit hit;
    henka_terrain_collision_patch patch;
    size_t sample_count;
    int32_t committed_height;
    uint8_t committed_layer_weight;
    int result = 0;
    uint32_t index;

    desc.max_resident_regions = 1U;
    desc.max_resident_chunks = 1U;
    if (henka_terrain_world_desc_get_layout(&desc, &layout) != HENKA_SUCCESS ||
        henka_terrain_world_create(&desc, &world) != HENKA_SUCCESS ||
        henka_terrain_world_create(&desc, &restarted_world) != HENKA_SUCCESS ||
        henka_terrain_storage_create(&desc, "build/test_tmp/terrain_workflow", &storage) != HENKA_SUCCESS ||
        henka_terrain_storage_ensure_manifest(storage) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    samples = henka_calloc(layout.samples_per_region, sizeof(*samples));
    recovered_samples = henka_calloc(layout.samples_per_region, sizeof(*recovered_samples));
    collision_heights = henka_calloc(HENKA_TERRAIN_COLLISION_PATCH_SAMPLES, sizeof(*collision_heights));
    if (samples == NULL || recovered_samples == NULL || collision_heights == NULL)
    {
        goto cleanup;
    }
    for (index = 0U; index < (uint32_t)layout.samples_per_region; ++index)
    {
        samples[index].height_millimeters = 1000;
        samples[index].material_weights[0] = 220U;
        samples[index].material_weights[1] = 20U;
        samples[index].material_weights[2] = 10U;
        samples[index].material_weights[3] = 5U;
        (void)henka_terrain_normalize_weights(samples[index].material_weights);
    }
    if (henka_terrain_world_apply_region_snapshot(
            world, (henka_terrain_region_storage_info){region, 1U, 1U},
            samples, layout.samples_per_region) != HENKA_SUCCESS ||
        henka_terrain_world_set_region_residency(world, region, true, true, false) != HENKA_SUCCESS ||
        henka_terrain_physics_create(
            &(henka_terrain_physics_desc){1U}, &physics) != HENKA_SUCCESS ||
        henka_terrain_world_build_collision_patch(
            world, chunk, collision_heights,
            HENKA_TERRAIN_COLLISION_PATCH_SAMPLES, &patch) != HENKA_SUCCESS ||
        henka_terrain_physics_replace_patch(
            physics, &(henka_terrain_physics_patch_desc){patch, 1.0F, 0.0F, 0.0F}) != HENKA_SUCCESS ||
        henka_terrain_physics_sample(physics, 32.0F, 32.0F, &hit) != HENKA_SUCCESS ||
        !hit.hit || hit.height_meters < 0.99F || hit.height_meters > 1.01F)
    {
        goto cleanup;
    }

    if (henka_terrain_world_apply_edit(
            world,
            &(henka_terrain_edit_command){
                1U, HENKA_TERRAIN_EDIT_ALGORITHM_VERSION,
                HENKA_TERRAIN_EDIT_RAISE, 32, 32, 4U,
                HENKA_TERRAIN_EDIT_FALLOFF_SMOOTH, 500, 0U, 0U},
            10U) != HENKA_SUCCESS ||
        henka_terrain_world_get_region_samples(world, region, &world_samples, &sample_count) != HENKA_SUCCESS ||
        world_samples[32U * layout.samples_per_region_edge + 32U].height_millimeters <= 1000 ||
        henka_terrain_world_build_collision_patch(
            world, chunk, collision_heights,
            HENKA_TERRAIN_COLLISION_PATCH_SAMPLES, &patch) != HENKA_SUCCESS ||
        henka_terrain_physics_replace_patch(
            physics, &(henka_terrain_physics_patch_desc){patch, 1.0F, 0.0F, 0.0F}) != HENKA_SUCCESS ||
        henka_terrain_physics_sample(physics, 32.0F, 32.0F, &hit) != HENKA_SUCCESS ||
        !hit.hit || hit.height_meters <= 1.0F)
    {
        goto cleanup;
    }

    committed_height = world_samples[32U * layout.samples_per_region_edge + 32U].height_millimeters;
    committed_layer_weight = world_samples[32U * layout.samples_per_region_edge + 32U].material_weights[2];
    if (henka_terrain_world_apply_edit(
            world,
            &(henka_terrain_edit_command){
                2U, HENKA_TERRAIN_EDIT_ALGORITHM_VERSION,
                HENKA_TERRAIN_EDIT_PAINT, 32, 32, 4U,
                HENKA_TERRAIN_EDIT_FALLOFF_LINEAR, 0, 2U, 255U},
            11U) != HENKA_SUCCESS ||
        world_samples[32U * layout.samples_per_region_edge + 32U].material_weights[2] <= committed_layer_weight)
    {
        goto cleanup;
    }
    if (henka_terrain_world_get_region_state(world, region, &state) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    committed_info = (henka_terrain_region_storage_info){region, state.revision, state.generation};
    committed_height = world_samples[32U * layout.samples_per_region_edge + 32U].height_millimeters;
    committed_layer_weight = world_samples[32U * layout.samples_per_region_edge + 32U].material_weights[2];
    if (henka_terrain_storage_begin(storage, 100U) != HENKA_SUCCESS ||
        henka_terrain_storage_write_region(
            storage, region, committed_info.revision, committed_info.generation,
            world_samples, sample_count) != HENKA_SUCCESS ||
        henka_terrain_storage_commit(storage, 100U) != HENKA_SUCCESS ||
        henka_terrain_storage_load_region(
            storage, region, &info, recovered_samples, layout.samples_per_region) != HENKA_SUCCESS ||
        info.revision != committed_info.revision ||
        recovered_samples[32U * layout.samples_per_region_edge + 32U].height_millimeters != committed_height ||
        recovered_samples[32U * layout.samples_per_region_edge + 32U].material_weights[2] != committed_layer_weight)
    {
        goto cleanup;
    }
    henka_terrain_storage_destroy(storage);
    storage = NULL;
    if (henka_terrain_storage_create(
            &desc, "build/test_tmp/terrain_workflow", &recovered_storage) != HENKA_SUCCESS ||
        henka_terrain_storage_recover(recovered_storage) != HENKA_SUCCESS ||
        henka_terrain_storage_load_region(
            recovered_storage, region, &info, recovered_samples, layout.samples_per_region) != HENKA_SUCCESS ||
        info.revision != committed_info.revision)
    {
        goto cleanup;
    }
    if (henka_terrain_world_apply_region_snapshot(
            restarted_world, info, recovered_samples, layout.samples_per_region) != HENKA_SUCCESS ||
        henka_terrain_world_set_region_residency(restarted_world, region, true, true, false) != HENKA_SUCCESS ||
        henka_terrain_physics_create(
            &(henka_terrain_physics_desc){1U}, &restarted_physics) != HENKA_SUCCESS ||
        henka_terrain_world_build_collision_patch(
            restarted_world, chunk, collision_heights,
            HENKA_TERRAIN_COLLISION_PATCH_SAMPLES, &patch) != HENKA_SUCCESS ||
        henka_terrain_physics_replace_patch(
            restarted_physics, &(henka_terrain_physics_patch_desc){patch, 1.0F, 0.0F, 0.0F}) != HENKA_SUCCESS ||
        henka_terrain_physics_sample(restarted_physics, 32.0F, 32.0F, &hit) != HENKA_SUCCESS ||
        !hit.hit || hit.height_meters <= 1.0F)
    {
        goto cleanup;
    }
    if (henka_terrain_storage_begin(recovered_storage, 101U) != HENKA_SUCCESS ||
        henka_terrain_storage_write_region(
            recovered_storage, region, committed_info.revision + 1U,
            committed_info.generation + 1U, world_samples, sample_count) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    henka_terrain_storage_destroy(recovered_storage);
    recovered_storage = NULL;
    if (henka_terrain_storage_create(
            &desc, "build/test_tmp/terrain_workflow", &storage) != HENKA_SUCCESS ||
        henka_terrain_storage_recover(storage) != HENKA_SUCCESS ||
        henka_terrain_storage_load_region(
            storage, region, &info, recovered_samples, layout.samples_per_region) != HENKA_SUCCESS ||
        info.revision != committed_info.revision ||
        recovered_samples[32U * layout.samples_per_region_edge + 32U].height_millimeters != committed_height)
    {
        goto cleanup;
    }
    result = 1;

cleanup:
    henka_terrain_physics_destroy(restarted_physics);
    henka_terrain_physics_destroy(physics);
    henka_terrain_storage_destroy(recovered_storage);
    henka_terrain_storage_destroy(storage);
    henka_terrain_world_destroy(restarted_world);
    henka_terrain_world_destroy(world);
    henka_free(recovered_samples);
    henka_free(samples);
    henka_free(collision_heights);
    return result;
}

int main(void)
{
    return test_runtime_edit_collision_and_restart() ? 0 : 1;
}
