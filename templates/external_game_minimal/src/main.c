#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include <henka/henka.h>

static bool external_terrain_workflow(void)
{
    const henka_terrain_region_id region_id = {0, 0};
    const henka_terrain_chunk_id chunk_id = {0, 0};
    henka_terrain_world_desc world_desc = henka_terrain_world_desc_default();
    henka_terrain_layout layout = {0};
    henka_terrain_world* world = NULL;
    henka_terrain_world* restarted_world = NULL;
    henka_terrain_storage* storage = NULL;
    henka_terrain_storage* restarted_storage = NULL;
    henka_terrain_physics* physics = NULL;
    henka_terrain_sample* samples = NULL;
    henka_terrain_sample* loaded_samples = NULL;
    int32_t* collision_heights = NULL;
    henka_terrain_mesh_vertex* vertices = NULL;
    uint32_t* indices = NULL;
    henka_terrain_mesh_data mesh = {0};
    henka_terrain_collision_patch collision_patch = {0};
    henka_terrain_physics_hit hit = {0};
    henka_terrain_region_storage_info region_info = {0};
    henka_terrain_region_state region_state = {0};
    const henka_terrain_sample* world_samples = NULL;
    size_t sample_count = 0U;
    size_t index;
    uint32_t center_index;
    int32_t saved_height = 0;
    uint8_t saved_rock_weight = 0U;
    bool success = false;

    world_desc.max_resident_regions = 1U;
    world_desc.max_resident_chunks = 1U;
    world_desc.max_pending_io = 4U;
    if (henka_terrain_world_desc_get_layout(&world_desc, &layout) != HENKA_SUCCESS ||
        henka_terrain_world_create(&world_desc, &world) != HENKA_SUCCESS ||
        henka_terrain_world_create(&world_desc, &restarted_world) != HENKA_SUCCESS ||
        henka_terrain_storage_create(&world_desc, "external_terrain_workflow", &storage) != HENKA_SUCCESS ||
        henka_terrain_storage_ensure_manifest(storage) != HENKA_SUCCESS)
    {
        goto cleanup;
    }

    samples = henka_calloc(layout.samples_per_region, sizeof(*samples));
    loaded_samples = henka_calloc(layout.samples_per_region, sizeof(*loaded_samples));
    collision_heights = henka_calloc(HENKA_TERRAIN_COLLISION_PATCH_SAMPLES, sizeof(*collision_heights));
    vertices = henka_calloc(HENKA_TERRAIN_MESH_MAX_VERTICES, sizeof(*vertices));
    indices = henka_calloc(HENKA_TERRAIN_MESH_MAX_INDICES, sizeof(*indices));
    if (samples == NULL || loaded_samples == NULL || collision_heights == NULL ||
        vertices == NULL || indices == NULL)
    {
        goto cleanup;
    }

    center_index = 32U * layout.samples_per_region_edge + 32U;
    for (index = 0U; index < layout.samples_per_region; ++index)
    {
        samples[index].height_millimeters = 1000;
        samples[index].material_weights[0] = 220U;
        samples[index].material_weights[1] = 20U;
        samples[index].material_weights[2] = 10U;
        samples[index].material_weights[3] = 5U;
        (void)henka_terrain_normalize_weights(samples[index].material_weights);
    }
    if (henka_terrain_world_apply_region_snapshot(
            world, (henka_terrain_region_storage_info){region_id, 1U, 1U},
            samples, layout.samples_per_region) != HENKA_SUCCESS ||
        henka_terrain_world_set_region_residency(world, region_id, true, true, false) != HENKA_SUCCESS)
    {
        goto cleanup;
    }

    {
        henka_material material = henka_material_terrain_default();
        if (!material.terrain_layers_enabled ||
            material.terrain_layers[0].texture_scale_meters <= 0.0f ||
            material.terrain_layers[1].roughness < 0.045f ||
            material.terrain_layers[2].base_color.x <= 0.0f)
        {
            goto cleanup;
        }
    }

    if (henka_terrain_world_apply_edit(
            world,
            &(henka_terrain_edit_command){
                1U, HENKA_TERRAIN_EDIT_ALGORITHM_VERSION,
                HENKA_TERRAIN_EDIT_RAISE, 32, 32, 4U,
                HENKA_TERRAIN_EDIT_FALLOFF_SMOOTH, 500, 0U, 0U},
            2U) != HENKA_SUCCESS ||
        henka_terrain_world_apply_edit(
            world,
            &(henka_terrain_edit_command){
                2U, HENKA_TERRAIN_EDIT_ALGORITHM_VERSION,
                HENKA_TERRAIN_EDIT_PAINT, 32, 32, 4U,
                HENKA_TERRAIN_EDIT_FALLOFF_LINEAR, 0, 2U, 255U},
            3U) != HENKA_SUCCESS ||
        henka_terrain_world_get_region_samples(world, region_id, &world_samples, &sample_count) != HENKA_SUCCESS ||
        sample_count != layout.samples_per_region ||
        world_samples[center_index].height_millimeters <= 1000 ||
        world_samples[center_index].material_weights[2] <= 10U)
    {
        goto cleanup;
    }
    saved_height = world_samples[center_index].height_millimeters;
    saved_rock_weight = world_samples[center_index].material_weights[2];

    if (henka_terrain_physics_create(&(henka_terrain_physics_desc){1U}, &physics) != HENKA_SUCCESS ||
        henka_terrain_world_build_collision_patch(
            world, chunk_id, collision_heights,
            HENKA_TERRAIN_COLLISION_PATCH_SAMPLES, &collision_patch) != HENKA_SUCCESS ||
        henka_terrain_physics_replace_patch(
            physics, &(henka_terrain_physics_patch_desc){collision_patch, 1.0F, 0.0F, 0.0F}) != HENKA_SUCCESS ||
        henka_terrain_physics_raycast(
            physics,
            (henka_ray){{32.0F, 20.0F, 32.0F}, {0.0F, -1.0F, 0.0F}},
            40.0F, &hit) != HENKA_SUCCESS ||
        !hit.hit || hit.height_meters <= 1.0F)
    {
        goto cleanup;
    }

    mesh = (henka_terrain_mesh_data){
        chunk_id, 0U, 0U, 0U,
        vertices, HENKA_TERRAIN_MESH_MAX_VERTICES, 0U,
        indices, HENKA_TERRAIN_MESH_MAX_INDICES, 0U};
    if (henka_terrain_mesh_build_chunk(world, chunk_id, 0U, &mesh) != HENKA_SUCCESS ||
        mesh.revision != 3U || mesh.vertex_count == 0U || mesh.index_count == 0U)
    {
        goto cleanup;
    }

    if (henka_terrain_world_get_region_state(world, region_id, &region_state) != HENKA_SUCCESS ||
        henka_terrain_storage_begin(storage, 1U) != HENKA_SUCCESS ||
        henka_terrain_storage_write_region(
            storage, region_id, region_state.revision, region_state.generation,
            world_samples, sample_count) != HENKA_SUCCESS ||
        henka_terrain_storage_commit(storage, 1U) != HENKA_SUCCESS ||
        henka_terrain_storage_load_region(
            storage, region_id, &region_info, loaded_samples, layout.samples_per_region) != HENKA_SUCCESS ||
        region_info.revision != region_state.revision ||
        loaded_samples[center_index].height_millimeters != saved_height ||
        loaded_samples[center_index].material_weights[2] != saved_rock_weight)
    {
        goto cleanup;
    }

    henka_terrain_storage_destroy(storage);
    storage = NULL;
    if (henka_terrain_storage_create(
            &world_desc, "external_terrain_workflow", &restarted_storage) != HENKA_SUCCESS ||
        henka_terrain_storage_recover(restarted_storage) != HENKA_SUCCESS ||
        henka_terrain_storage_load_region(
            restarted_storage, region_id, &region_info, loaded_samples,
            layout.samples_per_region) != HENKA_SUCCESS ||
        henka_terrain_world_apply_region_snapshot(
            restarted_world, region_info, loaded_samples, layout.samples_per_region) != HENKA_SUCCESS ||
        henka_terrain_world_set_region_residency(
            restarted_world, region_id, true, true, false) != HENKA_SUCCESS ||
        henka_terrain_world_get_region_samples(
            restarted_world, region_id, &world_samples, &sample_count) != HENKA_SUCCESS ||
        world_samples[center_index].height_millimeters != saved_height ||
        world_samples[center_index].material_weights[2] != saved_rock_weight)
    {
        goto cleanup;
    }

    printf("External game template initialized.\n");
    printf("External Terrain material, edit, collision, render-data, save, and restart workflow passed.\n");
    success = true;

cleanup:
    henka_terrain_physics_destroy(physics);
    henka_terrain_storage_destroy(restarted_storage);
    henka_terrain_storage_destroy(storage);
    henka_terrain_world_destroy(restarted_world);
    henka_terrain_world_destroy(world);
    henka_free(indices);
    henka_free(vertices);
    henka_free(collision_heights);
    henka_free(loaded_samples);
    henka_free(samples);
    return success;
}

int main(void)
{
    return external_terrain_workflow() ? 0 : 1;
}
