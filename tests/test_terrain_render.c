#include <math.h>
#include <henka/engine.h>
#include <henka/memory.h>
#include <henka/scene.h>
#include <henka/terrain_render.h>

static int test_default_descriptor(void)
{
    henka_terrain_render_desc desc = henka_terrain_render_desc_default();
    return desc.max_resident_chunks > 0U &&
        desc.max_pending_requests > 0U &&
        desc.lod_max_distances[0] < desc.lod_max_distances[1] &&
        desc.lod_max_distances[1] < desc.lod_max_distances[2] &&
        desc.lod_max_distances[2] < desc.lod_max_distances[3] &&
        isfinite(desc.lod_hysteresis) && desc.lod_hysteresis >= 0.0f &&
        desc.lod_hysteresis < 1.0f &&
        desc.material.type == HENKA_MATERIAL_TYPE_LIT &&
        desc.material.terrain_layers_enabled &&
        desc.material.terrain_layers[0].texture_scale_meters > 0.0f &&
        desc.material.terrain_layers[1].texture_scale_meters > 0.0f &&
        desc.material.terrain_layers[2].texture_scale_meters > 0.0f &&
        desc.material.terrain_layers[3].texture_scale_meters > 0.0f;
}

static int test_invalid_boundaries(void)
{
    henka_terrain_render_desc desc = henka_terrain_render_desc_default();
    henka_terrain_render_runtime* runtime = NULL;
    desc.lod_max_distances[2] = desc.lod_max_distances[1];
    return henka_terrain_render_runtime_create(NULL, NULL, NULL, &desc, &runtime) == HENKA_ERROR_INVALID_ARGUMENT &&
        runtime == NULL;
}

static int test_edit_request_requires_valid_inputs(void)
{
    henka_terrain_edit_command command = henka_terrain_edit_command_default();
    return henka_terrain_render_runtime_request_edit(NULL, &command) == HENKA_ERROR_INVALID_ARGUMENT &&
        henka_terrain_render_runtime_request_edit(NULL, NULL) == HENKA_ERROR_INVALID_ARGUMENT;
}

static int test_dirty_refresh_requires_valid_runtime(void)
{
    return henka_terrain_render_runtime_refresh_dirty(NULL) == HENKA_ERROR_INVALID_ARGUMENT;
}

static int test_observer_sync_refreshes_replacement_and_bounds(void)
{
    henka_engine_config engine_config = {0};
    henka_engine* engine = NULL;
    henka_scene* scene = NULL;
    henka_terrain_world_desc world_desc = henka_terrain_world_desc_default();
    henka_terrain_layout layout;
    henka_terrain_world* world = NULL;
    henka_terrain_sample* samples = NULL;
    henka_terrain_render_desc render_desc = henka_terrain_render_desc_default();
    henka_terrain_render_runtime* runtime = NULL;
    henka_terrain_render_chunk_info chunk_info;
    henka_bounds initial_bounds;
    henka_bounds replacement_bounds;
    henka_bounds failed_bounds;
    henka_result step_result;
    henka_result result = HENKA_ERROR_UNKNOWN;
    size_t index;

    engine_config.application_name = "Terrain Render Replacement Test";
    engine_config.window_width = 320;
    engine_config.window_height = 240;
    engine_config.enable_vsync = false;
    engine_config.asset_base_path = ".";
    world_desc.world_width_meters = 128U;
    world_desc.world_depth_meters = 128U;
    world_desc.region_edge_meters = 128U;
    world_desc.chunk_edge_meters = 64U;
    world_desc.samples_per_chunk = 65U;
    world_desc.base_sample_spacing_meters = 1U;
    world_desc.chunks_per_region_edge = 2U;
    world_desc.regions_across = 1U;
    world_desc.regions_down = 1U;
    world_desc.max_resident_regions = 1U;
    world_desc.max_resident_chunks = 4U;
    world_desc.max_pending_io = 4U;
    world_desc.max_stream_observers = 1U;
    render_desc.max_resident_chunks = 2U;
    render_desc.max_pending_requests = 1U;

    if (henka_engine_create(&engine_config, &engine) != HENKA_SUCCESS ||
        henka_scene_create(&scene) != HENKA_SUCCESS ||
        henka_terrain_world_desc_get_layout(&world_desc, &layout) != HENKA_SUCCESS ||
        henka_terrain_world_create(&world_desc, &world) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    samples = henka_calloc(layout.samples_per_region, sizeof(*samples));
    if (samples == NULL)
    {
        goto cleanup;
    }
    for (index = 0U; index < layout.samples_per_region; ++index)
    {
        samples[index].material_weights[0] = 255U;
    }
    if (henka_terrain_world_apply_region_snapshot(
            world,
            (henka_terrain_region_storage_info){{0, 0}, 1U, 1U},
            samples,
            layout.samples_per_region) != HENKA_SUCCESS ||
        henka_terrain_world_set_region_residency(
            world, (henka_terrain_region_id){0, 0}, true, true, false) != HENKA_SUCCESS ||
        henka_terrain_render_runtime_create(
            engine, scene, world, &render_desc, &runtime) != HENKA_SUCCESS ||
        henka_terrain_render_runtime_request_chunk(
            runtime, (henka_terrain_chunk_id){0, 0}, 0U) != HENKA_SUCCESS ||
        henka_terrain_render_runtime_pump(runtime, 1U) != HENKA_SUCCESS ||
        henka_terrain_render_runtime_request_chunk(
            runtime, (henka_terrain_chunk_id){1, 0}, 0U) != HENKA_SUCCESS ||
        henka_terrain_render_runtime_pump(runtime, 1U) != HENKA_SUCCESS ||
        henka_terrain_render_runtime_get_chunk(
            runtime, (henka_terrain_chunk_id){0, 0}, &chunk_info) != HENKA_SUCCESS ||
        chunk_info.revision != 1U ||
        henka_scene_get_entity_local_bounds(
            scene, chunk_info.entity, &initial_bounds) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    samples[0].height_millimeters = 1000;

    step_result = henka_terrain_world_apply_region_snapshot(
        world, (henka_terrain_region_storage_info){{0, 0}, 2U, 2U},
        samples, layout.samples_per_region);
    if (step_result != HENKA_SUCCESS)
        goto cleanup;
    step_result = henka_terrain_render_runtime_update_observer(
        runtime, (henka_vec3){32.0f, 0.0f, 32.0f});
    if (step_result != HENKA_ERROR_LIMIT)
        goto cleanup;
    step_result = henka_terrain_render_runtime_pump(runtime, 1U);
    if (step_result != HENKA_SUCCESS)
        goto cleanup;
    step_result = henka_terrain_render_runtime_update_observer(
        runtime, (henka_vec3){32.0f, 0.0f, 32.0f});
    if (step_result != HENKA_ERROR_LIMIT)
        goto cleanup;
    step_result = henka_terrain_render_runtime_pump(runtime, 1U);
    if (step_result != HENKA_SUCCESS)
        goto cleanup;
    step_result = henka_terrain_render_runtime_update_observer(
        runtime, (henka_vec3){32.0f, 0.0f, 32.0f});
    if (step_result != HENKA_SUCCESS)
        goto cleanup;
    step_result = henka_terrain_render_runtime_pump(runtime, 1U);
    if (step_result != HENKA_SUCCESS ||
        henka_terrain_render_runtime_get_chunk(
            runtime, (henka_terrain_chunk_id){0, 0}, &chunk_info) != HENKA_SUCCESS ||
        chunk_info.revision != 2U ||
        henka_scene_get_entity_local_bounds(
            scene, chunk_info.entity, &replacement_bounds) != HENKA_SUCCESS ||
        replacement_bounds.center.y <= initial_bounds.center.y ||
        replacement_bounds.extents.y <= initial_bounds.extents.y)
    {
        goto cleanup;
    }
    if (henka_terrain_world_set_region_residency(
            world, (henka_terrain_region_id){0, 0}, true, false, false) != HENKA_SUCCESS ||
        henka_terrain_render_runtime_request_chunk(
            runtime, (henka_terrain_chunk_id){0, 0}, 0U) != HENKA_SUCCESS ||
        henka_terrain_render_runtime_pump(runtime, 1U) != HENKA_SUCCESS ||
        henka_scene_get_entity_local_bounds(
            scene, chunk_info.entity, &failed_bounds) != HENKA_SUCCESS ||
        failed_bounds.center.x != replacement_bounds.center.x ||
        failed_bounds.center.y != replacement_bounds.center.y ||
        failed_bounds.center.z != replacement_bounds.center.z ||
        failed_bounds.extents.x != replacement_bounds.extents.x ||
        failed_bounds.extents.y != replacement_bounds.extents.y ||
        failed_bounds.extents.z != replacement_bounds.extents.z ||
        henka_terrain_world_set_region_residency(
            world, (henka_terrain_region_id){0, 0}, true, true, false) != HENKA_SUCCESS ||
        henka_terrain_render_runtime_request_chunk(
            runtime, (henka_terrain_chunk_id){0, 0}, 0U) != HENKA_SUCCESS ||
        henka_terrain_render_runtime_pump(runtime, 1U) != HENKA_SUCCESS ||
        henka_terrain_render_runtime_get_chunk(
            runtime, (henka_terrain_chunk_id){1, 0}, &chunk_info) != HENKA_SUCCESS ||
        chunk_info.revision != 2U)
    {
        goto cleanup;
    }
    if (henka_terrain_render_runtime_update_observer(
            runtime, (henka_vec3){500.0f, 0.0f, 500.0f}) != HENKA_ERROR_LIMIT ||
        henka_terrain_render_runtime_pump(runtime, 1U) != HENKA_SUCCESS ||
        henka_terrain_render_runtime_update_observer(
            runtime, (henka_vec3){500.0f, 0.0f, 500.0f}) != HENKA_SUCCESS ||
        henka_terrain_render_runtime_pump(runtime, 1U) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    result = HENKA_SUCCESS;

cleanup:
    henka_terrain_render_runtime_destroy(runtime);
    henka_terrain_world_destroy(world);
    henka_free(samples);
    henka_scene_destroy(scene);
    henka_engine_destroy(engine);
    return result == HENKA_SUCCESS ? 1 : 0;
}

int main(void)
{
    return test_default_descriptor() && test_invalid_boundaries() &&
        test_edit_request_requires_valid_inputs() &&
        test_dirty_refresh_requires_valid_runtime() &&
        test_observer_sync_refreshes_replacement_and_bounds() ? 0 : 1;
}
