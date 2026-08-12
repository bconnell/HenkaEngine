#include <math.h>
#include <henka/assets.h>
#include <henka/engine.h>
#include <henka/memory.h>
#include <henka/scene.h>
#include <henka/terrain_render.h>

#include "../engine/src/core/memory_internal.h"

typedef struct terrain_pass_test_context
{
    henka_scene* scene;
    henka_terrain_world* world;
    henka_terrain_render_runtime* runtime;
    henka_terrain_sample* samples;
    size_t sample_count;
    uint32_t update_count;
    henka_material_asset* terrain_material_asset;
    int passed;
} terrain_pass_test_context;

static henka_result terrain_pass_test_initialize(
    henka_engine* engine,
    void* user_data)
{
    terrain_pass_test_context* context = user_data;
    henka_terrain_world_desc world_desc = henka_terrain_world_desc_default();
    henka_terrain_layout layout;
    henka_camera camera;
    henka_terrain_render_desc render_desc = henka_terrain_render_desc_default();
    henka_asset_manager* assets;
    henka_result result;
    size_t index;

    if (context == NULL ||
        henka_scene_create(&context->scene) != HENKA_SUCCESS ||
        henka_terrain_world_desc_get_layout(&world_desc, &layout) != HENKA_SUCCESS ||
        henka_terrain_world_create(&world_desc, &context->world) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    context->sample_count = layout.samples_per_region;
    context->samples = henka_calloc(context->sample_count, sizeof(*context->samples));
    if (context->samples == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    for (index = 0U; index < context->sample_count; ++index)
    {
        context->samples[index].material_weights[0] = 255U;
        context->samples[index].height_millimeters =
            (int32_t)((index % layout.samples_per_region_edge) * 2U);
    }
    result = henka_terrain_world_apply_region_snapshot(
        context->world,
        (henka_terrain_region_storage_info){{0, 0}, 1U, 1U},
        context->samples,
        context->sample_count);
    if (result != HENKA_SUCCESS ||
        henka_terrain_world_set_region_residency(
            context->world, (henka_terrain_region_id){0, 0}, true, true, false) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_ASSET_SOURCE;
    }
    if (henka_scene_set_fog(
            context->scene,
            (henka_scene_fog_desc){
                true, HENKA_SCENE_FOG_LINEAR, {0.16f, 0.19f, 0.24f},
                8.0f, 80.0f, 0.04f}) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    camera = henka_camera_create_perspective(
        60.0f * HENKA_DEG_TO_RAD, 4.0f / 3.0f, 0.1f, 200.0f);
    camera.position = (henka_vec3){32.0f, 24.0f, 56.0f};
    if (!henka_camera_look_at(&camera, (henka_vec3){32.0f, 0.0f, 32.0f}) ||
        henka_scene_set_camera(context->scene, &camera) != HENKA_SUCCESS ||
        henka_engine_set_scene(engine, context->scene) != HENKA_SUCCESS ||
        henka_engine_set_viewport_shading_mode(
            engine, HENKA_VIEWPORT_SHADING_RENDERED) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_RENDERER;
    }
    assets = henka_engine_get_asset_manager(engine);
    if (assets == NULL || henka_assets_load_shader(
            assets,
            "assets/shaders/basic_lit.vert",
            "assets/shaders/basic_lit.frag",
            &render_desc.material.shader) != HENKA_SUCCESS ||
        henka_assets_adopt_runtime_material(
            assets,
            "runtime/terrain/test-material",
            &render_desc.material,
            &context->terrain_material_asset) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_ASSET_SOURCE;
    }
    render_desc.material_asset = context->terrain_material_asset;
    if (henka_terrain_render_runtime_create(
            engine, context->scene, context->world, &render_desc, &context->runtime) != HENKA_SUCCESS ||
        henka_terrain_render_runtime_request_chunk(
            context->runtime, (henka_terrain_chunk_id){0, 0}, 0U) != HENKA_SUCCESS ||
        henka_terrain_render_runtime_pump(context->runtime, 1U) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_RENDERER;
    }
    {
        henka_terrain_render_chunk_info chunk_info;
        const henka_material_asset* material_asset = NULL;
        henka_material terrain_material;
        if (henka_terrain_render_runtime_get_chunk(
            context->runtime, (henka_terrain_chunk_id){0, 0}, &chunk_info) != HENKA_SUCCESS ||
            henka_scene_get_entity_material_asset(
                context->scene, chunk_info.entity, &material_asset) != HENKA_SUCCESS ||
            material_asset != context->terrain_material_asset ||
            henka_scene_get_entity_material(
                context->scene, chunk_info.entity, &terrain_material) != HENKA_SUCCESS ||
            !terrain_material.cast_shadows || !terrain_material.receive_shadows)
        {
            return HENKA_ERROR_RENDERER;
        }
    }
    return HENKA_SUCCESS;
}

static void terrain_pass_test_update(
    henka_engine* engine,
    double delta_seconds,
    void* user_data)
{
    terrain_pass_test_context* context = user_data;
    henka_engine_diagnostics diagnostics;
    const uint32_t required_flags =
        HENKA_RENDERED_TERRAIN_PASS_COLOR |
        HENKA_RENDERED_TERRAIN_PASS_SHADOW |
        HENKA_RENDERED_TERRAIN_PASS_DEPTH |
        HENKA_RENDERED_TERRAIN_PASS_AO |
        HENKA_RENDERED_TERRAIN_PASS_SSGI |
        HENKA_RENDERED_TERRAIN_PASS_FOG |
        HENKA_RENDERED_TERRAIN_PASS_HDR;
    (void)delta_seconds;
    if (context == NULL || ++context->update_count < 2U)
    {
        return;
    }
    context->passed =
        henka_engine_get_diagnostics(engine, &diagnostics) == HENKA_SUCCESS &&
        diagnostics.rendered_scene_terrain_draw_calls > 0U &&
        diagnostics.rendered_scene_terrain_shadow_draw_calls > 0U &&
        (diagnostics.rendered_scene_terrain_pass_flags & required_flags) == required_flags;
    henka_engine_request_exit(engine);
}

static void terrain_pass_test_shutdown(
    henka_engine* engine,
    void* user_data)
{
    terrain_pass_test_context* context = user_data;
    (void)engine;
    if (context == NULL)
    {
        return;
    }
    henka_terrain_render_runtime_destroy(context->runtime);
    henka_scene_destroy(context->scene);
    henka_terrain_world_destroy(context->world);
    henka_free(context->samples);
    context->runtime = NULL;
    context->scene = NULL;
    context->world = NULL;
    context->samples = NULL;
}

static int test_rendered_pass_participation(void)
{
    terrain_pass_test_context context = {0};
    henka_engine_config config = {0};
    henka_engine* engine = NULL;
    henka_result result;

    config.application_name = "Terrain Rendered Pass Test";
    config.window_width = 320;
    config.window_height = 240;
    config.enable_vsync = false;
    config.asset_base_path = ".";
    config.on_initialize = terrain_pass_test_initialize;
    config.on_update = terrain_pass_test_update;
    config.on_shutdown = terrain_pass_test_shutdown;
    config.user_data = &context;
    result = henka_engine_create(&config, &engine);
    if (result == HENKA_SUCCESS)
    {
        result = henka_engine_run(engine);
        henka_engine_destroy(engine);
    }
    if (result != HENKA_SUCCESS)
    {
        terrain_pass_test_shutdown(NULL, &context);
    }
    return result == HENKA_SUCCESS && context.passed;
}

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

static int test_paint_updates_weights_without_rebuilding_geometry(void)
{
    henka_engine_config engine_config = {0};
    henka_engine* engine = NULL;
    henka_scene* scene = NULL;
    henka_terrain_world_desc world_desc = henka_terrain_world_desc_default();
    henka_terrain_world* world = NULL;
    henka_terrain_render_desc render_desc = henka_terrain_render_desc_default();
    henka_terrain_render_runtime* runtime = NULL;
    henka_terrain_edit_command command = henka_terrain_edit_command_default();
    henka_terrain_render_stats before;
    henka_terrain_render_stats after;
    henka_terrain_render_chunk_info before_chunk;
    henka_terrain_render_chunk_info after_chunk;
    henka_terrain_region_id region_id = {0, 0};
    henka_terrain_region_storage_info storage_info = {region_id, 1U, 1U};
    henka_terrain_layout layout;
    henka_terrain_sample* samples = NULL;
    size_t index;
    int passed = 0;

    engine_config.application_name = "Terrain Paint Weight Stream Test";
    engine_config.window_width = 320;
    engine_config.window_height = 240;
    engine_config.enable_vsync = false;
    engine_config.asset_base_path = ".";
    world_desc.world_width_meters = 64U;
    world_desc.world_depth_meters = 64U;
    world_desc.region_edge_meters = 64U;
    world_desc.chunk_edge_meters = 64U;
    world_desc.samples_per_chunk = 65U;
    world_desc.base_sample_spacing_meters = 1U;
    world_desc.chunks_per_region_edge = 1U;
    world_desc.regions_across = 1U;
    world_desc.regions_down = 1U;
    world_desc.max_resident_regions = 1U;
    world_desc.max_resident_chunks = 1U;
    world_desc.max_pending_io = 2U;
    world_desc.max_stream_observers = 1U;
    render_desc.max_resident_chunks = 1U;
    render_desc.max_pending_requests = 2U;

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
            world, storage_info, samples, layout.samples_per_region) != HENKA_SUCCESS ||
        henka_terrain_world_set_region_residency(world, region_id, true, true, false) != HENKA_SUCCESS ||
        henka_terrain_render_runtime_create(engine, scene, world, &render_desc, &runtime) != HENKA_SUCCESS ||
        henka_terrain_render_runtime_request_chunk(runtime, (henka_terrain_chunk_id){0, 0}, 0U) != HENKA_SUCCESS ||
        henka_terrain_render_runtime_pump(runtime, 1U) != HENKA_SUCCESS ||
        henka_terrain_render_runtime_get_stats(runtime, &before) != HENKA_SUCCESS ||
        henka_terrain_render_runtime_get_chunk(
            runtime, (henka_terrain_chunk_id){0, 0}, &before_chunk) != HENKA_SUCCESS)
    {
        goto cleanup;
    }

    command.operation = HENKA_TERRAIN_EDIT_PAINT;
    command.center_sample_x = 32;
    command.center_sample_z = 32;
    command.radius_samples = 4U;
    command.paint_layer = 1U;
    command.paint_strength = 255U;
    if (henka_terrain_world_apply_edit(world, &command, 2U) != HENKA_SUCCESS ||
        henka_terrain_render_runtime_request_edit(runtime, &command) != HENKA_SUCCESS ||
        henka_terrain_render_runtime_pump(runtime, 1U) != HENKA_SUCCESS ||
        henka_terrain_render_runtime_get_stats(runtime, &after) != HENKA_SUCCESS ||
        henka_terrain_render_runtime_get_chunk(
            runtime, (henka_terrain_chunk_id){0, 0}, &after_chunk) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    passed = after.rebuilt_chunks == before.rebuilt_chunks &&
        after.weight_updates == before.weight_updates + 1U &&
        after.failed_weight_updates == before.failed_weight_updates &&
        after.gpu_weight_bytes > 0U &&
        after.gpu_weight_bytes == before.gpu_weight_bytes &&
        after_chunk.mesh == before_chunk.mesh &&
        after_chunk.revision > before_chunk.revision &&
        after_chunk.generation == before_chunk.generation;

cleanup:
    henka_terrain_render_runtime_destroy(runtime);
    henka_terrain_world_destroy(world);
    henka_free(samples);
    henka_scene_destroy(scene);
    henka_engine_destroy(engine);
    return passed;
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

static int test_candidate_allocation_failure_retains_resident_mesh(void)
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
    henka_terrain_render_chunk_info before;
    henka_terrain_render_chunk_info after_failure;
    henka_terrain_render_chunk_info after_retry;
    henka_terrain_render_stats before_stats;
    henka_terrain_render_stats after_stats;
    henka_bounds before_bounds;
    henka_bounds after_failure_bounds;
    henka_result result = HENKA_ERROR_UNKNOWN;
    size_t index;

    engine_config.application_name = "Terrain Render Allocation Failure Test";
    engine_config.window_width = 320;
    engine_config.window_height = 240;
    engine_config.enable_vsync = false;
    engine_config.asset_base_path = ".";
    world_desc.world_width_meters = 64U;
    world_desc.world_depth_meters = 64U;
    world_desc.region_edge_meters = 64U;
    world_desc.chunk_edge_meters = 64U;
    world_desc.samples_per_chunk = 65U;
    world_desc.base_sample_spacing_meters = 1U;
    world_desc.chunks_per_region_edge = 1U;
    world_desc.regions_across = 1U;
    world_desc.regions_down = 1U;
    world_desc.max_resident_regions = 1U;
    world_desc.max_resident_chunks = 1U;
    world_desc.max_pending_io = 2U;
    world_desc.max_stream_observers = 1U;
    render_desc.max_resident_chunks = 1U;
    render_desc.max_pending_requests = 2U;

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
        henka_terrain_render_runtime_get_chunk(
            runtime, (henka_terrain_chunk_id){0, 0}, &before) != HENKA_SUCCESS ||
        henka_scene_get_entity_local_bounds(
            scene, before.entity, &before_bounds) != HENKA_SUCCESS ||
        henka_terrain_render_runtime_get_stats(runtime, &before_stats) != HENKA_SUCCESS)
    {
        goto cleanup;
    }

    samples[0].height_millimeters = 1500;
    if (henka_terrain_world_apply_region_snapshot(
            world,
            (henka_terrain_region_storage_info){{0, 0}, 2U, 2U},
            samples,
            layout.samples_per_region) != HENKA_SUCCESS ||
        henka_terrain_render_runtime_refresh_dirty(runtime) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    henka_memory_test_fail_after(0U);
    if (henka_terrain_render_runtime_pump(runtime, 1U) != HENKA_SUCCESS)
    {
        henka_memory_test_disable_failures();
        goto cleanup;
    }
    henka_memory_test_disable_failures();
    if (henka_terrain_render_runtime_get_chunk(
            runtime, (henka_terrain_chunk_id){0, 0}, &after_failure) != HENKA_SUCCESS ||
        henka_scene_get_entity_local_bounds(
            scene, after_failure.entity, &after_failure_bounds) != HENKA_SUCCESS ||
        henka_terrain_render_runtime_get_stats(runtime, &after_stats) != HENKA_SUCCESS ||
        after_failure.mesh != before.mesh ||
        after_failure.revision != before.revision ||
        after_failure_bounds.center.x != before_bounds.center.x ||
        after_failure_bounds.center.y != before_bounds.center.y ||
        after_failure_bounds.center.z != before_bounds.center.z ||
        after_failure_bounds.extents.x != before_bounds.extents.x ||
        after_failure_bounds.extents.y != before_bounds.extents.y ||
        after_failure_bounds.extents.z != before_bounds.extents.z ||
        after_stats.failed_rebuilds <= before_stats.failed_rebuilds)
    {
        goto cleanup;
    }
    if (henka_terrain_render_runtime_refresh_dirty(runtime) != HENKA_SUCCESS ||
        henka_terrain_render_runtime_pump(runtime, 1U) != HENKA_SUCCESS ||
        henka_terrain_render_runtime_get_chunk(
            runtime, (henka_terrain_chunk_id){0, 0}, &after_retry) != HENKA_SUCCESS ||
        after_retry.mesh == before.mesh ||
        after_retry.revision != 2U)
    {
        goto cleanup;
    }
    result = HENKA_SUCCESS;

cleanup:
    henka_memory_test_disable_failures();
    henka_terrain_render_runtime_destroy(runtime);
    henka_terrain_world_destroy(world);
    henka_free(samples);
    henka_scene_destroy(scene);
    henka_engine_destroy(engine);
    return result == HENKA_SUCCESS ? 1 : 0;
}

static int test_observer_working_set_and_distance_culling(void)
{
    henka_engine_config engine_config = {0};
    henka_engine* engine = NULL;
    henka_scene* scene = NULL;
    henka_terrain_world_desc world_desc = henka_terrain_world_desc_default();
    henka_terrain_layout layout;
    henka_terrain_world* world = NULL;
    henka_terrain_render_desc render_desc = henka_terrain_render_desc_default();
    henka_terrain_render_runtime* runtime = NULL;
    henka_terrain_sample* samples = NULL;
    henka_terrain_render_stats stats;
    henka_terrain_render_chunk_info chunk_info;
    henka_entity picked_entity = HENKA_INVALID_ENTITY;
    float picked_distance = 0.0f;
    size_t index;
    int passed = 0;

    engine_config.application_name = "Terrain Observer Working Set Test";
    engine_config.window_width = 320;
    engine_config.window_height = 240;
    engine_config.enable_vsync = false;
    engine_config.asset_base_path = ".";
    world_desc.world_width_meters = 1024U;
    world_desc.world_depth_meters = 512U;
    world_desc.regions_across = 2U;
    world_desc.regions_down = 1U;
    world_desc.max_resident_regions = 2U;
    world_desc.max_resident_chunks = 128U;
    world_desc.max_pending_io = 4U;
    world_desc.max_stream_observers = 1U;
    render_desc.max_resident_chunks = 2U;
    render_desc.max_pending_requests = 2U;

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
        samples[index].height_millimeters = (int32_t)(index % layout.samples_per_region_edge);
    }
    if (henka_terrain_world_apply_region_snapshot(
            world, (henka_terrain_region_storage_info){{0, 0}, 1U, 1U},
            samples, layout.samples_per_region) != HENKA_SUCCESS ||
        henka_terrain_world_apply_region_snapshot(
            world, (henka_terrain_region_storage_info){{1, 0}, 1U, 1U},
            samples, layout.samples_per_region) != HENKA_SUCCESS ||
        henka_terrain_world_set_region_residency(
            world, (henka_terrain_region_id){0, 0}, true, true, false) != HENKA_SUCCESS ||
        henka_terrain_world_set_region_residency(
            world, (henka_terrain_region_id){1, 0}, true, true, false) != HENKA_SUCCESS ||
        henka_terrain_render_runtime_create(
            engine, scene, world, &render_desc, &runtime) != HENKA_SUCCESS ||
        henka_terrain_render_runtime_update_observer(
            runtime, (henka_vec3){32.0f, 0.0f, 32.0f}) != HENKA_SUCCESS ||
        henka_terrain_render_runtime_pump(runtime, 2U) != HENKA_SUCCESS ||
        henka_terrain_render_runtime_get_stats(runtime, &stats) != HENKA_SUCCESS ||
        stats.resident_chunks != 2U || stats.visible_chunks != 2U ||
        henka_terrain_render_runtime_get_chunk(
            runtime, (henka_terrain_chunk_id){0, 0}, &chunk_info) != HENKA_SUCCESS ||
        !chunk_info.resident || !chunk_info.visible ||
        !henka_scene_is_entity_valid(scene, chunk_info.entity) ||
        henka_scene_pick_entity(
            scene,
            (henka_ray){{32.0f, 100.0f, 32.0f}, {0.0f, -1.0f, 0.0f}},
            &picked_entity,
            &picked_distance) != HENKA_SUCCESS ||
        picked_entity != chunk_info.entity ||
        !isfinite(picked_distance) || picked_distance < 0.0f)
    {
        goto cleanup;
    }
    if (henka_terrain_render_runtime_update_observer(
            runtime, (henka_vec3){544.0f, 0.0f, 32.0f}) != HENKA_SUCCESS ||
        henka_terrain_render_runtime_pump(runtime, 2U) != HENKA_SUCCESS ||
        henka_terrain_render_runtime_get_stats(runtime, &stats) != HENKA_SUCCESS ||
        stats.resident_chunks != 2U || stats.visible_chunks != 2U ||
        henka_terrain_render_runtime_get_chunk(
            runtime, (henka_terrain_chunk_id){8, 0}, &chunk_info) != HENKA_SUCCESS ||
        !chunk_info.resident || !chunk_info.visible)
    {
        goto cleanup;
    }
    if (henka_terrain_render_runtime_update_observer(
            runtime, (henka_vec3){5000.0f, 0.0f, 5000.0f}) != HENKA_SUCCESS ||
        henka_terrain_render_runtime_get_stats(runtime, &stats) != HENKA_SUCCESS ||
        stats.resident_chunks != 0U || stats.visible_chunks != 0U ||
        stats.max_resident_chunks < 2U || stats.max_visible_chunks < 2U ||
        henka_scene_get_entity_count(scene) != 0U ||
        henka_scene_is_entity_valid(scene, chunk_info.entity))
    {
        goto cleanup;
    }
    passed = 1;

cleanup:
    henka_terrain_render_runtime_destroy(runtime);
    henka_terrain_world_destroy(world);
    henka_free(samples);
    henka_scene_destroy(scene);
    henka_engine_destroy(engine);
    return passed;
}

int main(void)
{
    return test_default_descriptor() && test_invalid_boundaries() &&
        test_edit_request_requires_valid_inputs() &&
        test_dirty_refresh_requires_valid_runtime() &&
        test_paint_updates_weights_without_rebuilding_geometry() &&
        test_observer_sync_refreshes_replacement_and_bounds() &&
        test_candidate_allocation_failure_retains_resident_mesh() &&
        test_observer_working_set_and_distance_culling() &&
        test_rendered_pass_participation() ? 0 : 1;
}
