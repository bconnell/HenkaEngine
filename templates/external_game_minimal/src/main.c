#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include <henka/henka.h>

typedef struct external_graphical_terrain_state
{
    henka_scene* scene;
    henka_terrain_world* world;
    henka_terrain_render_runtime* render;
    henka_terrain_sample* samples;
    uint64_t start_frame;
    bool success;
} external_graphical_terrain_state;

static void external_graphical_terrain_destroy(external_graphical_terrain_state* state)
{
    if (state == NULL)
    {
        return;
    }
    henka_terrain_render_runtime_destroy(state->render);
    henka_scene_destroy(state->scene);
    henka_terrain_world_destroy(state->world);
    henka_free(state->samples);
    state->render = NULL;
    state->scene = NULL;
    state->world = NULL;
    state->samples = NULL;
}

static henka_result external_graphical_terrain_initialize(
    henka_engine* engine,
    void* user_data)
{
    external_graphical_terrain_state* state = (external_graphical_terrain_state*)user_data;
    henka_terrain_world_desc world_desc = henka_terrain_world_desc_default();
    henka_terrain_layout layout = {0};
    henka_camera camera;
    henka_terrain_render_desc render_desc;
    size_t index;
    if (engine == NULL || state == NULL ||
        henka_terrain_world_desc_get_layout(&world_desc, &layout) != HENKA_SUCCESS ||
        henka_scene_create(&state->scene) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    world_desc.max_resident_regions = 1U;
    world_desc.max_resident_chunks = 1U;
    if (henka_terrain_world_create(&world_desc, &state->world) != HENKA_SUCCESS)
    {
        external_graphical_terrain_destroy(state);
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    state->samples = henka_calloc(layout.samples_per_region, sizeof(*state->samples));
    if (state->samples == NULL)
    {
        external_graphical_terrain_destroy(state);
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    for (index = 0U; index < layout.samples_per_region; ++index)
    {
        state->samples[index].height_millimeters = 900 + (int32_t)((index * 17U) % 300U);
        state->samples[index].material_weights[0] = 180U;
        state->samples[index].material_weights[1] = 40U;
        state->samples[index].material_weights[2] = 25U;
        state->samples[index].material_weights[3] = 10U;
        (void)henka_terrain_normalize_weights(state->samples[index].material_weights);
    }
    if (henka_terrain_world_apply_region_snapshot(
            state->world,
            (henka_terrain_region_storage_info){{0, 0}, 1U, 1U},
            state->samples,
            layout.samples_per_region) != HENKA_SUCCESS ||
        henka_terrain_world_set_region_residency(
            state->world, (henka_terrain_region_id){0, 0}, false, true, false) != HENKA_SUCCESS)
    {
        external_graphical_terrain_destroy(state);
        return HENKA_ERROR_UNKNOWN;
    }
    camera = henka_camera_create_perspective(1.0f, 4.0f / 3.0f, 0.1f, 500.0f);
    camera.position = (henka_vec3){32.0f, 28.0f, 72.0f};
    if (!henka_camera_look_at(&camera, (henka_vec3){32.0f, 1.5f, 32.0f}) ||
        henka_scene_set_camera(state->scene, &camera) != HENKA_SUCCESS)
    {
        external_graphical_terrain_destroy(state);
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    henka_scene_set_light_direction(state->scene, (henka_vec3){-0.4f, -1.0f, -0.2f});
    henka_scene_set_light_intensity(state->scene, 3.0f);
    render_desc = henka_terrain_render_desc_default();
    render_desc.max_resident_chunks = 1U;
    render_desc.max_pending_requests = 2U;
    if (henka_terrain_render_runtime_create(
            engine, state->scene, state->world, &render_desc, &state->render) != HENKA_SUCCESS ||
        henka_terrain_render_runtime_update_observer(state->render, camera.position) != HENKA_SUCCESS ||
        henka_terrain_render_runtime_pump(state->render, 1U) != HENKA_SUCCESS ||
        henka_engine_set_scene(engine, state->scene) != HENKA_SUCCESS ||
        henka_engine_set_viewport_shading_mode(engine, HENKA_VIEWPORT_SHADING_RENDERED) != HENKA_SUCCESS)
    {
        external_graphical_terrain_destroy(state);
        return HENKA_ERROR_RENDERER;
    }
    state->start_frame = henka_engine_get_frame_index(engine);
    return HENKA_SUCCESS;
}

static void external_graphical_terrain_update(
    henka_engine* engine,
    double delta_seconds,
    void* user_data)
{
    external_graphical_terrain_state* state = (external_graphical_terrain_state*)user_data;
    henka_terrain_render_stats render_stats;
    henka_engine_diagnostics diagnostics;
    (void)delta_seconds;
    if (engine == NULL || state == NULL || state->render == NULL ||
        henka_terrain_render_runtime_update_observer(
            state->render, (henka_vec3){32.0f, 28.0f, 72.0f}) != HENKA_SUCCESS ||
        henka_terrain_render_runtime_pump(state->render, 1U) != HENKA_SUCCESS)
    {
        henka_engine_request_exit(engine);
        return;
    }
    if (henka_engine_get_frame_index(engine) < state->start_frame + 4U)
    {
        return;
    }
    state->success =
        henka_terrain_render_runtime_get_stats(state->render, &render_stats) == HENKA_SUCCESS &&
        henka_engine_get_diagnostics(engine, &diagnostics) == HENKA_SUCCESS &&
        render_stats.resident_chunks == 1U &&
        render_stats.rebuilt_chunks >= 1U &&
        diagnostics.rendered_hdr_ready &&
        diagnostics.rendered_shadow_ready &&
        diagnostics.rendered_scene_draw_calls > 0U &&
        diagnostics.rendered_scene_visible_entities > 0U;
    henka_engine_request_exit(engine);
}

static void external_graphical_terrain_shutdown(
    henka_engine* engine,
    void* user_data)
{
    (void)engine;
    external_graphical_terrain_destroy((external_graphical_terrain_state*)user_data);
}

static bool external_graphical_terrain_workflow(void)
{
    external_graphical_terrain_state state = {0};
    henka_engine_config config = {0};
    henka_engine* engine = NULL;
    henka_result result;
    config.application_name = "Henka External Terrain Consumer";
    config.window_width = 640;
    config.window_height = 480;
    config.user_data_base_path = "external_graphical_user_data";
    config.package_mode = HENKA_PACKAGE_MODE_DEVELOPMENT;
    config.on_initialize = external_graphical_terrain_initialize;
    config.on_update = external_graphical_terrain_update;
    config.on_shutdown = external_graphical_terrain_shutdown;
    config.user_data = &state;
    result = henka_engine_create(&config, &engine);
    if (result == HENKA_SUCCESS)
    {
        result = henka_engine_run(engine);
    }
    henka_engine_destroy(engine);
    return result == HENKA_SUCCESS && state.success;
}

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
    if (!external_terrain_workflow() || !external_graphical_terrain_workflow())
    {
        return 1;
    }
    printf("External Terrain graphical Rendered path passed.\n");
    return 0;
}
