#include <henka/terrain_collision_runtime.h>

#include <string.h>

#include <henka/memory.h>

typedef struct henka_terrain_collision_runtime_request
{
    bool active;
    henka_terrain_chunk_id chunk_id;
} henka_terrain_collision_runtime_request;

struct henka_terrain_collision_runtime
{
    henka_terrain_world* world;
    henka_terrain_physics* physics;
    henka_terrain_collision_runtime_desc desc;
    henka_terrain_collision_runtime_request* requests;
    henka_terrain_collision_runtime_stats stats;
};

static uint32_t henka_terrain_collision_runtime_find_request(
    const henka_terrain_collision_runtime* runtime,
    henka_terrain_chunk_id chunk_id)
{
    uint32_t index;
    for (index = 0U; index < runtime->desc.max_pending_chunks; ++index)
    {
        if (runtime->requests[index].active &&
            henka_terrain_chunk_id_equal(runtime->requests[index].chunk_id, chunk_id))
        {
            return index;
        }
    }
    return runtime->desc.max_pending_chunks;
}

henka_terrain_collision_runtime_desc henka_terrain_collision_runtime_desc_default(void)
{
    return (henka_terrain_collision_runtime_desc){64U};
}

henka_result henka_terrain_collision_runtime_create(
    henka_terrain_world* world,
    henka_terrain_physics* physics,
    const henka_terrain_collision_runtime_desc* desc,
    henka_terrain_collision_runtime** out_runtime)
{
    henka_terrain_collision_runtime_desc defaults;
    henka_terrain_collision_runtime* runtime;

    if (out_runtime == NULL || world == NULL || physics == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_runtime = NULL;
    defaults = henka_terrain_collision_runtime_desc_default();
    if (desc == NULL)
    {
        desc = &defaults;
    }
    if (desc->max_pending_chunks == 0U ||
        desc->max_pending_chunks > HENKA_TERRAIN_COLLISION_RUNTIME_MAX_PENDING)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    runtime = henka_calloc(1U, sizeof(*runtime));
    if (runtime == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    runtime->requests = henka_calloc(
        desc->max_pending_chunks,
        sizeof(*runtime->requests));
    if (runtime->requests == NULL)
    {
        henka_free(runtime);
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    runtime->world = world;
    runtime->physics = physics;
    runtime->desc = *desc;
    *out_runtime = runtime;
    return HENKA_SUCCESS;
}

void henka_terrain_collision_runtime_destroy(henka_terrain_collision_runtime* runtime)
{
    if (runtime == NULL)
    {
        return;
    }
    henka_free(runtime->requests);
    henka_free(runtime);
}

henka_result henka_terrain_collision_runtime_request_chunk(
    henka_terrain_collision_runtime* runtime,
    henka_terrain_chunk_id chunk_id)
{
    uint32_t index;
    henka_terrain_world_desc world_desc;

    if (runtime == NULL ||
        henka_terrain_world_get_desc(runtime->world, &world_desc) != HENKA_SUCCESS ||
        !henka_terrain_chunk_id_is_valid(&world_desc, chunk_id))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (henka_terrain_collision_runtime_find_request(runtime, chunk_id) <
        runtime->desc.max_pending_chunks)
    {
        ++runtime->stats.coalesced_count;
        return HENKA_SUCCESS;
    }
    for (index = 0U; index < runtime->desc.max_pending_chunks; ++index)
    {
        if (!runtime->requests[index].active)
        {
            runtime->requests[index].active = true;
            runtime->requests[index].chunk_id = chunk_id;
            ++runtime->stats.pending_chunk_count;
            ++runtime->stats.queued_count;
            return HENKA_SUCCESS;
        }
    }
    ++runtime->stats.dropped_count;
    return HENKA_ERROR_LIMIT;
}

henka_result henka_terrain_collision_runtime_request_edit(
    henka_terrain_collision_runtime* runtime,
    const henka_terrain_edit_command* command)
{
    henka_terrain_world_desc world_desc;
    uint32_t chunk_sample_span;
    uint32_t samples_across;
    uint32_t samples_down;
    int64_t min_sample_x;
    int64_t max_sample_x;
    int64_t min_sample_z;
    int64_t max_sample_z;
    int32_t min_chunk_x;
    int32_t max_chunk_x;
    int32_t min_chunk_z;
    int32_t max_chunk_z;
    int32_t chunk_z;
    int32_t chunk_x;

    if (runtime == NULL || command == NULL ||
        henka_terrain_world_get_desc(runtime->world, &world_desc) != HENKA_SUCCESS ||
        henka_terrain_edit_command_validate(runtime->world, command) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    chunk_sample_span = world_desc.chunk_edge_meters / world_desc.base_sample_spacing_meters;
    samples_across = world_desc.world_width_meters / world_desc.base_sample_spacing_meters + 1U;
    samples_down = world_desc.world_depth_meters / world_desc.base_sample_spacing_meters + 1U;
    if (chunk_sample_span == 0U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    min_sample_x = (int64_t)command->center_sample_x - command->radius_samples - chunk_sample_span;
    max_sample_x = (int64_t)command->center_sample_x + command->radius_samples + chunk_sample_span;
    min_sample_z = (int64_t)command->center_sample_z - command->radius_samples - chunk_sample_span;
    max_sample_z = (int64_t)command->center_sample_z + command->radius_samples + chunk_sample_span;
    if (min_sample_x < 0) { min_sample_x = 0; }
    if (min_sample_z < 0) { min_sample_z = 0; }
    if (max_sample_x >= samples_across) { max_sample_x = samples_across - 1U; }
    if (max_sample_z >= samples_down) { max_sample_z = samples_down - 1U; }
    min_chunk_x = (int32_t)(min_sample_x / chunk_sample_span);
    max_chunk_x = (int32_t)(max_sample_x / chunk_sample_span);
    min_chunk_z = (int32_t)(min_sample_z / chunk_sample_span);
    max_chunk_z = (int32_t)(max_sample_z / chunk_sample_span);
    for (chunk_z = min_chunk_z; chunk_z <= max_chunk_z; ++chunk_z)
    {
        for (chunk_x = min_chunk_x; chunk_x <= max_chunk_x; ++chunk_x)
        {
            henka_terrain_region_id region_id;
            henka_terrain_region_state state;
            henka_result result;
            if (henka_terrain_region_id_from_chunk(
                    &world_desc, (henka_terrain_chunk_id){chunk_x, chunk_z}, &region_id) != HENKA_SUCCESS ||
                henka_terrain_world_get_region_state(runtime->world, region_id, &state) != HENKA_SUCCESS ||
                !state.physics_resident)
            {
                continue;
            }
            result = henka_terrain_collision_runtime_request_chunk(
                runtime, (henka_terrain_chunk_id){chunk_x, chunk_z});
            if (result != HENKA_SUCCESS)
            {
                return result;
            }
        }
    }
    return HENKA_SUCCESS;
}

henka_result henka_terrain_collision_runtime_remove_chunk(
    henka_terrain_collision_runtime* runtime,
    henka_terrain_chunk_id chunk_id)
{
    uint32_t index;
    if (runtime == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    index = henka_terrain_collision_runtime_find_request(runtime, chunk_id);
    if (index < runtime->desc.max_pending_chunks)
    {
        runtime->requests[index].active = false;
        if (runtime->stats.pending_chunk_count > 0U)
        {
            --runtime->stats.pending_chunk_count;
        }
    }
    return henka_terrain_physics_remove_patch(runtime->physics, chunk_id);
}

henka_result henka_terrain_collision_runtime_pump(
    henka_terrain_collision_runtime* runtime,
    uint32_t max_rebuilds)
{
    uint32_t index;
    uint32_t rebuilt = 0U;
    henka_terrain_world_desc world_desc;
    int32_t heights[HENKA_TERRAIN_COLLISION_PATCH_SAMPLES];

    if (runtime == NULL || max_rebuilds == 0U ||
        henka_terrain_world_get_desc(runtime->world, &world_desc) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    for (index = 0U; index < runtime->desc.max_pending_chunks && rebuilt < max_rebuilds; ++index)
    {
        henka_terrain_chunk_id chunk_id;
        henka_terrain_collision_patch patch;
        henka_terrain_physics_patch_desc physics_patch;
        henka_result result;
        if (!runtime->requests[index].active)
        {
            continue;
        }
        chunk_id = runtime->requests[index].chunk_id;
        runtime->requests[index].active = false;
        if (runtime->stats.pending_chunk_count > 0U)
        {
            --runtime->stats.pending_chunk_count;
        }
        result = henka_terrain_world_build_collision_patch(
            runtime->world,
            chunk_id,
            heights,
            HENKA_TERRAIN_COLLISION_PATCH_SAMPLES,
            &patch);
        if (result == HENKA_SUCCESS)
        {
            physics_patch.patch = patch;
            physics_patch.sample_spacing_meters = (float)world_desc.base_sample_spacing_meters;
            physics_patch.origin_x_meters = (float)chunk_id.x * (float)world_desc.chunk_edge_meters;
            physics_patch.origin_z_meters = (float)chunk_id.z * (float)world_desc.chunk_edge_meters;
            result = henka_terrain_physics_replace_patch(runtime->physics, &physics_patch);
        }
        if (result == HENKA_SUCCESS)
        {
            ++runtime->stats.rebuilt_count;
        }
        else
        {
            ++runtime->stats.failed_count;
        }
        ++rebuilt;
    }
    return HENKA_SUCCESS;
}

void henka_terrain_collision_runtime_get_stats(
    const henka_terrain_collision_runtime* runtime,
    henka_terrain_collision_runtime_stats* out_stats)
{
    if (out_stats == NULL)
    {
        return;
    }
    *out_stats = runtime == NULL ?
        (henka_terrain_collision_runtime_stats){0} : runtime->stats;
}
