#include <henka/terrain_render.h>

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "../henka_internal.h"
#include <henka/log.h>
#include <henka/memory.h>
#include <henka/terrain_edit.h>
#include <henka/terrain_render.h>

#define HENKA_TERRAIN_RENDER_DEPENDENCY_COUNT 9U

static void henka_terrain_render_add_bytes(uint64_t* total, uint64_t amount)
{
    if (total == NULL || *total == UINT64_MAX)
    {
        return;
    }
    if (UINT64_MAX - *total < amount)
    {
        *total = UINT64_MAX;
        return;
    }
    *total += amount;
}

typedef struct henka_terrain_render_request
{
    uint32_t slot_index;
    uint32_t serial;
    uint32_t lod_level;
    uint32_t edge_transition_mask;
    uint32_t fallback_skirt_mask;
} henka_terrain_render_request;

typedef struct henka_terrain_render_slot
{
    bool occupied;
    bool resident;
    bool queued;
    bool visible;
    uint32_t serial;
    henka_terrain_chunk_id chunk_id;
    uint32_t requested_lod;
    uint32_t selected_lod;
    uint32_t desired_lod;
    uint32_t requested_edge_transition_mask;
    uint32_t selected_edge_transition_mask;
    uint32_t desired_edge_transition_mask;
    uint32_t requested_fallback_skirt_mask;
    uint32_t selected_fallback_skirt_mask;
    uint32_t desired_fallback_skirt_mask;
    henka_entity entity;
    henka_mesh* mesh;
    henka_terrain_revision revision;
    henka_terrain_generation generation;
    henka_terrain_revision dependency_revisions[HENKA_TERRAIN_RENDER_DEPENDENCY_COUNT];
    henka_terrain_generation dependency_generations[HENKA_TERRAIN_RENDER_DEPENDENCY_COUNT];
} henka_terrain_render_slot;

struct henka_terrain_render_runtime
{
    henka_engine* engine;
    henka_scene* scene;
    const henka_terrain_world* world;
    henka_terrain_render_desc desc;
    char material_name[64];
    henka_terrain_render_slot* slots;
    henka_terrain_render_request* requests;
    uint32_t request_head;
    uint32_t request_count;
    henka_terrain_render_stats stats;
};

static bool henka_terrain_render_chunk_equal(
    henka_terrain_chunk_id left,
    henka_terrain_chunk_id right)
{
    return henka_terrain_chunk_id_equal(left, right);
}

static bool henka_terrain_render_distance_valid(float value)
{
    return isfinite(value) && value > 0.0f;
}

static bool henka_terrain_render_desc_is_valid(const henka_terrain_render_desc* desc)
{
    uint32_t index;

    if (desc == NULL || desc->max_resident_chunks == 0U ||
        desc->max_resident_chunks > HENKA_TERRAIN_MAX_RESIDENT_CHUNKS ||
        desc->max_pending_requests == 0U ||
        desc->max_pending_requests > HENKA_TERRAIN_MAX_PENDING_IO ||
        henka_material_validate(&desc->material) != HENKA_SUCCESS ||
        !isfinite(desc->lod_hysteresis) || desc->lod_hysteresis < 0.0f ||
        desc->lod_hysteresis >= 1.0f)
    {
        return false;
    }
    for (index = 0U; index <= HENKA_TERRAIN_MESH_MAX_LOD_LEVEL; ++index)
    {
        if (!henka_terrain_render_distance_valid(desc->lod_max_distances[index]) ||
            (index > 0U && desc->lod_max_distances[index - 1U] >= desc->lod_max_distances[index]))
        {
            return false;
        }
    }
    return true;
}

henka_terrain_render_desc henka_terrain_render_desc_default(void)
{
    henka_terrain_render_desc desc = {0};
    desc.max_resident_chunks = 256U;
    desc.max_pending_requests = 256U;
    desc.lod_max_distances[0] = 128.0f;
    desc.lod_max_distances[1] = 256.0f;
    desc.lod_max_distances[2] = 512.0f;
    desc.lod_max_distances[3] = 1024.0f;
    desc.lod_hysteresis = 0.15f;
    desc.material = henka_material_terrain_default();
    return desc;
}

static int32_t henka_terrain_render_find_slot(
    const henka_terrain_render_runtime* runtime,
    henka_terrain_chunk_id chunk_id)
{
    uint32_t index;
    for (index = 0U; index < runtime->desc.max_resident_chunks; ++index)
    {
        if (runtime->slots[index].occupied &&
            henka_terrain_render_chunk_equal(runtime->slots[index].chunk_id, chunk_id))
        {
            return (int32_t)index;
        }
    }
    return -1;
}

static int32_t henka_terrain_render_find_free_slot(
    const henka_terrain_render_runtime* runtime)
{
    uint32_t index;
    for (index = 0U; index < runtime->desc.max_resident_chunks; ++index)
    {
        if (!runtime->slots[index].occupied)
        {
            return (int32_t)index;
        }
    }
    return -1;
}

static henka_result henka_terrain_render_queue_slot(
    henka_terrain_render_runtime* runtime,
    uint32_t slot_index,
    uint32_t lod_level,
    uint32_t edge_transition_mask,
    uint32_t fallback_skirt_mask)
{
    uint32_t index;
    uint32_t queue_index;
    henka_terrain_render_slot* slot = &runtime->slots[slot_index];

    for (index = 0U; index < runtime->request_count; ++index)
    {
        queue_index = (runtime->request_head + index) % runtime->desc.max_pending_requests;
        if (runtime->requests[queue_index].slot_index == slot_index &&
            runtime->requests[queue_index].serial == slot->serial)
        {
            runtime->requests[queue_index].lod_level = lod_level;
            runtime->requests[queue_index].edge_transition_mask = edge_transition_mask;
            runtime->requests[queue_index].fallback_skirt_mask = fallback_skirt_mask;
            slot->queued = true;
            runtime->stats.coalesced_requests += 1U;
            return HENKA_SUCCESS;
        }
    }
    if (runtime->request_count >= runtime->desc.max_pending_requests)
    {
        runtime->stats.dropped_requests += 1U;
        return HENKA_ERROR_LIMIT;
    }
    queue_index = (runtime->request_head + runtime->request_count) % runtime->desc.max_pending_requests;
    runtime->requests[queue_index] = (henka_terrain_render_request){
        slot_index, slot->serial, lod_level, edge_transition_mask, fallback_skirt_mask};
    runtime->request_count += 1U;
    if (runtime->request_count > runtime->stats.max_pending_requests)
    {
        runtime->stats.max_pending_requests = runtime->request_count;
    }
    slot->queued = true;
    runtime->stats.queued_requests += 1U;
    return HENKA_SUCCESS;
}

static void henka_terrain_render_cancel_slot_requests(
    henka_terrain_render_runtime* runtime,
    uint32_t slot_index,
    uint32_t serial)
{
    uint32_t index;
    uint32_t kept = 0U;
    uint32_t original_count = runtime->request_count;
    for (index = 0U; index < original_count; ++index)
    {
        uint32_t source_index = (runtime->request_head + index) % runtime->desc.max_pending_requests;
        henka_terrain_render_request request = runtime->requests[source_index];
        if (request.slot_index == slot_index && request.serial == serial)
        {
            continue;
        }
        runtime->requests[(runtime->request_head + kept) % runtime->desc.max_pending_requests] = request;
        ++kept;
    }
    runtime->request_count = kept;
}

static henka_vec3 henka_terrain_render_chunk_center(
    const henka_terrain_render_runtime* runtime,
    henka_terrain_chunk_id chunk_id)
{
    henka_terrain_world_desc desc;
    (void)henka_terrain_world_get_desc(runtime->world, &desc);
    return (henka_vec3){
        (float)chunk_id.x * (float)desc.chunk_edge_meters + (float)desc.chunk_edge_meters * 0.5f,
        0.0f,
        (float)chunk_id.z * (float)desc.chunk_edge_meters + (float)desc.chunk_edge_meters * 0.5f};
}

static void henka_terrain_render_get_dependency_identity(
    const henka_terrain_render_runtime* runtime,
    henka_terrain_chunk_id chunk_id,
    henka_terrain_revision out_revisions[HENKA_TERRAIN_RENDER_DEPENDENCY_COUNT],
    henka_terrain_generation out_generations[HENKA_TERRAIN_RENDER_DEPENDENCY_COUNT])
{
    static const int32_t offsets[HENKA_TERRAIN_RENDER_DEPENDENCY_COUNT][2] = {
        {-1, -1}, {0, -1}, {1, -1},
        {-1, 0}, {0, 0}, {1, 0},
        {-1, 1}, {0, 1}, {1, 1}};
    henka_terrain_world_desc desc;
    henka_terrain_region_id center_region;
    uint32_t index;

    memset(out_revisions, 0, sizeof(henka_terrain_revision) * HENKA_TERRAIN_RENDER_DEPENDENCY_COUNT);
    memset(out_generations, 0, sizeof(henka_terrain_generation) * HENKA_TERRAIN_RENDER_DEPENDENCY_COUNT);
    if (runtime == NULL ||
        henka_terrain_world_get_desc(runtime->world, &desc) != HENKA_SUCCESS ||
        henka_terrain_region_id_from_chunk(&desc, chunk_id, &center_region) != HENKA_SUCCESS)
    {
        return;
    }
    for (index = 0U; index < HENKA_TERRAIN_RENDER_DEPENDENCY_COUNT; ++index)
    {
        henka_terrain_region_state state;
        henka_terrain_region_id region_id = {
            center_region.x + offsets[index][0],
            center_region.z + offsets[index][1]};
        if (!henka_terrain_region_id_is_valid(&desc, region_id) ||
            henka_terrain_world_get_region_state(runtime->world, region_id, &state) != HENKA_SUCCESS ||
            !state.render_resident)
        {
            continue;
        }
        out_revisions[index] = state.revision;
        out_generations[index] = state.generation;
    }
}

static bool henka_terrain_render_dependencies_match(
    const henka_terrain_render_runtime* runtime,
    const henka_terrain_render_slot* slot)
{
    henka_terrain_revision revisions[HENKA_TERRAIN_RENDER_DEPENDENCY_COUNT];
    henka_terrain_generation generations[HENKA_TERRAIN_RENDER_DEPENDENCY_COUNT];
    uint32_t index;
    henka_terrain_render_get_dependency_identity(runtime, slot->chunk_id, revisions, generations);
    for (index = 0U; index < HENKA_TERRAIN_RENDER_DEPENDENCY_COUNT; ++index)
    {
        if (revisions[index] != slot->dependency_revisions[index] ||
            generations[index] != slot->dependency_generations[index])
        {
            return false;
        }
    }
    return true;
}

static void henka_terrain_render_capture_dependencies(
    const henka_terrain_render_runtime* runtime,
    henka_terrain_render_slot* slot)
{
    henka_terrain_render_get_dependency_identity(
        runtime,
        slot->chunk_id,
        slot->dependency_revisions,
        slot->dependency_generations);
}

static bool henka_terrain_render_get_chunk_bounds(
    const henka_terrain_render_runtime* runtime,
    henka_terrain_chunk_id chunk_id,
    uint32_t fallback_skirt_mask,
    henka_bounds* out_bounds)
{
    henka_terrain_world_desc desc;
    henka_terrain_layout layout;
    henka_terrain_region_id region_id;
    const henka_terrain_sample* samples;
    size_t sample_count;
    uint32_t chunk_span;
    uint32_t region_span;
    uint32_t local_chunk_x;
    uint32_t local_chunk_z;
    uint32_t base_x;
    uint32_t base_z;
    uint32_t local_z;
    int32_t min_height = INT32_MAX;
    int32_t max_height = INT32_MIN;
    float skirt_depth = 0.0f;

    if (runtime == NULL || out_bounds == NULL ||
        henka_terrain_world_get_desc(runtime->world, &desc) != HENKA_SUCCESS ||
        henka_terrain_world_desc_get_layout(&desc, &layout) != HENKA_SUCCESS ||
        henka_terrain_region_id_from_chunk(&desc, chunk_id, &region_id) != HENKA_SUCCESS ||
        henka_terrain_world_get_region_samples(
            runtime->world, region_id, &samples, &sample_count) != HENKA_SUCCESS ||
        sample_count != layout.samples_per_region)
    {
        return false;
    }
    chunk_span = desc.samples_per_chunk - 1U;
    region_span = layout.samples_per_region_edge - 1U;
    local_chunk_x = (uint32_t)(chunk_id.x - region_id.x * (int32_t)desc.chunks_per_region_edge);
    local_chunk_z = (uint32_t)(chunk_id.z - region_id.z * (int32_t)desc.chunks_per_region_edge);
    base_x = local_chunk_x * chunk_span;
    base_z = local_chunk_z * chunk_span;
    if (base_x > region_span || base_z > region_span ||
        chunk_span > region_span - base_x || chunk_span > region_span - base_z)
    {
        return false;
    }
    for (local_z = 0U; local_z <= chunk_span; ++local_z)
    {
        uint32_t local_x;
        for (local_x = 0U; local_x <= chunk_span; ++local_x)
        {
            const henka_terrain_sample* sample = &samples[
                (base_z + local_z) * layout.samples_per_region_edge + base_x + local_x];
            if (sample->height_millimeters < min_height) min_height = sample->height_millimeters;
            if (sample->height_millimeters > max_height) max_height = sample->height_millimeters;
        }
    }
    if (min_height == INT32_MAX || max_height == INT32_MIN)
    {
        return false;
    }
    if (fallback_skirt_mask != 0U)
    {
        skirt_depth = fmaxf(8.0f, (float)desc.chunk_edge_meters * 0.25f);
    }
    out_bounds->center = (henka_vec3){
        (float)chunk_id.x * (float)desc.chunk_edge_meters + (float)desc.chunk_edge_meters * 0.5f,
        ((float)min_height + (float)max_height) * 0.0005f - skirt_depth * 0.5f,
        (float)chunk_id.z * (float)desc.chunk_edge_meters + (float)desc.chunk_edge_meters * 0.5f};
    out_bounds->extents = (henka_vec3){
        (float)desc.chunk_edge_meters * 0.5f,
        ((float)max_height - (float)min_height) * 0.0005f + skirt_depth * 0.5f + 0.01f,
        (float)desc.chunk_edge_meters * 0.5f};
    return isfinite(out_bounds->center.x) && isfinite(out_bounds->center.y) &&
        isfinite(out_bounds->center.z) && isfinite(out_bounds->extents.x) &&
        isfinite(out_bounds->extents.y) && isfinite(out_bounds->extents.z);
}

static uint32_t henka_terrain_render_select_lod(
    const henka_terrain_render_runtime* runtime,
    henka_terrain_render_slot* slot,
    henka_vec3 observer_position)
{
    henka_vec3 center;
    float dx;
    float dz;
    float distance;
    uint32_t level;

    center = henka_terrain_render_chunk_center(runtime, slot->chunk_id);
    dx = observer_position.x - center.x;
    dz = observer_position.z - center.z;
    distance = sqrtf(dx * dx + dz * dz);
    if (!isfinite(distance))
    {
        return slot->selected_lod;
    }
    level = 0U;
    while (level < HENKA_TERRAIN_MESH_MAX_LOD_LEVEL &&
           distance > runtime->desc.lod_max_distances[level])
    {
        level += 1U;
    }
    if (slot->resident)
    {
        const float previous_boundary = slot->selected_lod == 0U ? 0.0f :
            runtime->desc.lod_max_distances[slot->selected_lod - 1U];
        const float next_boundary = runtime->desc.lod_max_distances[slot->selected_lod];
        const float hysteresis = runtime->desc.lod_hysteresis;
        if (level > slot->selected_lod && distance <= next_boundary * (1.0f + hysteresis))
        {
            level = slot->selected_lod;
        }
        else if (level < slot->selected_lod && distance >= previous_boundary * (1.0f - hysteresis))
        {
            level = slot->selected_lod;
        }
    }
    return level;
}

static bool henka_terrain_render_are_neighbors(
    henka_terrain_chunk_id left,
    henka_terrain_chunk_id right)
{
    int32_t dx = left.x - right.x;
    int32_t dz = left.z - right.z;
    return (dx == 0 && (dz == 1 || dz == -1)) ||
        (dz == 0 && (dx == 1 || dx == -1));
}

static void henka_terrain_render_topology_masks(
    const henka_terrain_render_runtime* runtime,
    const henka_terrain_render_slot* slot,
    uint32_t* out_transition_mask,
    uint32_t* out_fallback_mask)
{
    static const int32_t offsets[4][2] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};
    static const uint32_t edge_masks[4] = {
        HENKA_TERRAIN_MESH_EDGE_NORTH,
        HENKA_TERRAIN_MESH_EDGE_EAST,
        HENKA_TERRAIN_MESH_EDGE_SOUTH,
        HENKA_TERRAIN_MESH_EDGE_WEST};
    henka_terrain_world_desc desc;
    uint32_t index;
    uint32_t transition_mask = 0U;
    uint32_t fallback_mask = 0U;

    if (out_transition_mask != NULL) *out_transition_mask = 0U;
    if (out_fallback_mask != NULL) *out_fallback_mask = 0U;
    if (runtime == NULL || slot == NULL ||
        henka_terrain_world_get_desc(runtime->world, &desc) != HENKA_SUCCESS)
    {
        return;
    }
    for (index = 0U; index < 4U; ++index)
    {
        henka_terrain_chunk_id neighbor_id = {
            slot->chunk_id.x + offsets[index][0],
            slot->chunk_id.z + offsets[index][1]};
        int32_t neighbor_slot_index;
        const henka_terrain_render_slot* neighbor;
        int32_t lod_difference;
        if (!henka_terrain_chunk_id_is_valid(&desc, neighbor_id))
        {
            continue;
        }
        neighbor_slot_index = henka_terrain_render_find_slot(runtime, neighbor_id);
        if (neighbor_slot_index < 0 || !runtime->slots[neighbor_slot_index].resident)
        {
            fallback_mask |= edge_masks[index];
            continue;
        }
        neighbor = &runtime->slots[neighbor_slot_index];
        lod_difference = (int32_t)neighbor->desired_lod - (int32_t)slot->desired_lod;
        if (lod_difference == 1)
        {
            transition_mask |= edge_masks[index];
        }
        else if (lod_difference > 1 || lod_difference < -1)
        {
            fallback_mask |= edge_masks[index];
        }
    }
    if (out_transition_mask != NULL) *out_transition_mask = transition_mask;
    if (out_fallback_mask != NULL) *out_fallback_mask = fallback_mask;
}

static henka_result henka_terrain_render_request_chunk_internal(
    henka_terrain_render_runtime* runtime,
    henka_terrain_chunk_id chunk_id,
    uint32_t lod_level,
    uint32_t edge_transition_mask,
    uint32_t fallback_skirt_mask);

static henka_result henka_terrain_render_refresh_dirty_internal(
    henka_terrain_render_runtime* runtime)
{
    henka_result first_error = HENKA_SUCCESS;
    uint32_t index;

    if (runtime == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    for (index = 0U; index < runtime->desc.max_resident_chunks; ++index)
    {
        henka_terrain_render_slot* slot = &runtime->slots[index];
        henka_terrain_world_desc slot_desc;
        henka_terrain_region_id region_id;
        henka_terrain_region_state region_state;
        henka_result result;

        if (!slot->occupied || !slot->resident ||
            henka_terrain_world_get_desc(runtime->world, &slot_desc) != HENKA_SUCCESS ||
            henka_terrain_region_id_from_chunk(
                &slot_desc, slot->chunk_id, &region_id) != HENKA_SUCCESS ||
            henka_terrain_world_get_region_state(
                runtime->world, region_id, &region_state) != HENKA_SUCCESS ||
            (slot->revision == region_state.revision &&
             slot->generation == region_state.generation &&
             henka_terrain_render_dependencies_match(runtime, slot)))
        {
            continue;
        }
        result = henka_terrain_render_request_chunk_internal(
            runtime,
            slot->chunk_id,
            slot->requested_lod,
            slot->selected_edge_transition_mask,
            slot->selected_fallback_skirt_mask);
        if (result == HENKA_SUCCESS)
        {
            if (runtime->stats.dirty_refresh_requests < UINT64_MAX)
            {
                ++runtime->stats.dirty_refresh_requests;
            }
        }
        else if (first_error == HENKA_SUCCESS)
        {
            first_error = result;
        }
    }
    return first_error;
}

static float henka_terrain_render_distance_squared(
    const henka_terrain_render_runtime* runtime,
    henka_terrain_chunk_id chunk_id,
    henka_vec3 observer_position)
{
    henka_vec3 center = henka_terrain_render_chunk_center(runtime, chunk_id);
    float dx = observer_position.x - center.x;
    float dz = observer_position.z - center.z;
    return dx * dx + dz * dz;
}

static bool henka_terrain_render_chunk_is_render_resident(
    const henka_terrain_render_runtime* runtime,
    henka_terrain_chunk_id chunk_id)
{
    henka_terrain_world_desc desc;
    henka_terrain_region_id region_id;
    henka_terrain_region_state state;
    return henka_terrain_world_get_desc(runtime->world, &desc) == HENKA_SUCCESS &&
        henka_terrain_region_id_from_chunk(&desc, chunk_id, &region_id) == HENKA_SUCCESS &&
        henka_terrain_world_get_region_state(runtime->world, region_id, &state) == HENKA_SUCCESS &&
        state.render_resident;
}

static int32_t henka_terrain_render_find_farthest_slot(
    const henka_terrain_render_runtime* runtime,
    henka_vec3 observer_position,
    float* out_distance_squared)
{
    int32_t farthest = -1;
    uint32_t index;
    float distance_squared = -1.0f;
    for (index = 0U; index < runtime->desc.max_resident_chunks; ++index)
    {
        const henka_terrain_render_slot* slot = &runtime->slots[index];
        float candidate_distance;
        if (!slot->occupied)
        {
            continue;
        }
        candidate_distance = henka_terrain_render_distance_squared(
            runtime, slot->chunk_id, observer_position);
        if (farthest < 0 || candidate_distance > distance_squared ||
            (candidate_distance == distance_squared && (uint32_t)farthest < index))
        {
            farthest = (int32_t)index;
            distance_squared = candidate_distance;
        }
    }
    if (out_distance_squared != NULL)
    {
        *out_distance_squared = distance_squared;
    }
    return farthest;
}

static henka_result henka_terrain_render_schedule_resident_chunks(
    henka_terrain_render_runtime* runtime,
    henka_vec3 observer_position)
{
    henka_terrain_world_desc desc;
    henka_terrain_world_stats stats;
    uint32_t index;
    float far_distance_squared;
    henka_result first_error = HENKA_SUCCESS;

    if (henka_terrain_world_get_desc(runtime->world, &desc) != HENKA_SUCCESS ||
        henka_terrain_world_get_stats(runtime->world, &stats) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    far_distance_squared = runtime->desc.lod_max_distances[HENKA_TERRAIN_MESH_MAX_LOD_LEVEL];
    far_distance_squared *= far_distance_squared;

    /* Drop presentation slots whose source region is no longer render resident
     * or whose chunk is outside the bounded outer LOD band. */
    for (index = 0U; index < runtime->desc.max_resident_chunks; ++index)
    {
        henka_terrain_render_slot* slot = &runtime->slots[index];
        if (slot->occupied &&
            (!henka_terrain_render_chunk_is_render_resident(runtime, slot->chunk_id) ||
             henka_terrain_render_distance_squared(runtime, slot->chunk_id, observer_position) >
                 far_distance_squared))
        {
            (void)henka_terrain_render_runtime_remove_chunk(runtime, slot->chunk_id);
        }
    }

    /* A committed world edit changes the source identity without changing the
     * chunk's residency. Queue a transactional replacement for stale uploads;
     * the existing bounded queue coalesces repeated observations. */
    {
        henka_result refresh_result = henka_terrain_render_refresh_dirty_internal(runtime);
        if (refresh_result != HENKA_SUCCESS)
        {
            first_error = refresh_result;
        }
    }

    /* The world may expose more render-resident chunks than the graphical
     * owner can retain. Scan stable region/chunk order and replace only a
     * farther slot, producing a deterministic nearest bounded working set. */
    for (index = 0U; index < stats.resident_region_count; ++index)
    {
        henka_terrain_region_state region_state;
        uint32_t local_z;
        uint32_t local_x;
        if (henka_terrain_world_get_resident_region_at(
                runtime->world, index, &region_state) != HENKA_SUCCESS ||
            !region_state.render_resident)
        {
            continue;
        }
        for (local_z = 0U; local_z < desc.chunks_per_region_edge; ++local_z)
        {
            for (local_x = 0U; local_x < desc.chunks_per_region_edge; ++local_x)
            {
                henka_terrain_chunk_id chunk_id = {
                    region_state.id.x * (int32_t)desc.chunks_per_region_edge + (int32_t)local_x,
                    region_state.id.z * (int32_t)desc.chunks_per_region_edge + (int32_t)local_z};
                int32_t farthest_slot;
                int32_t existing_slot;
                float candidate_distance_squared;
                float farthest_distance_squared;

                if (!henka_terrain_chunk_id_is_valid(&desc, chunk_id))
                {
                    continue;
                }
                candidate_distance_squared = henka_terrain_render_distance_squared(
                    runtime, chunk_id, observer_position);
                if (!isfinite(candidate_distance_squared) ||
                    candidate_distance_squared > far_distance_squared)
                {
                    continue;
                }
                existing_slot = henka_terrain_render_find_slot(runtime, chunk_id);
                if (existing_slot >= 0)
                {
                    continue;
                }
                if (henka_terrain_render_find_free_slot(runtime) >= 0)
                {
                    if (runtime->request_count >= runtime->desc.max_pending_requests)
                    {
                        continue;
                    }
                    farthest_slot = -1;
                }
                else
                {
                    farthest_slot = henka_terrain_render_find_farthest_slot(
                        runtime, observer_position, &farthest_distance_squared);
                    if (farthest_slot >= 0 && candidate_distance_squared >= farthest_distance_squared)
                    {
                        continue;
                    }
                    if (runtime->request_count >= runtime->desc.max_pending_requests &&
                        !runtime->slots[farthest_slot].queued)
                    {
                        continue;
                    }
                }
                if (farthest_slot >= 0)
                {
                    (void)henka_terrain_render_runtime_remove_chunk(
                        runtime, runtime->slots[farthest_slot].chunk_id);
                }
                {
                    henka_result request_result = henka_terrain_render_runtime_request_chunk(
                        runtime, chunk_id, 0U);
                    if (request_result != HENKA_SUCCESS && first_error == HENKA_SUCCESS)
                    {
                        first_error = request_result;
                    }
                }
            }
        }
    }
    return first_error;
}

static void henka_terrain_render_set_visibility(
    henka_terrain_render_runtime* runtime,
    henka_terrain_render_slot* slot,
    bool visible)
{
    if (slot->resident && slot->visible != visible)
    {
        (void)henka_scene_set_entity_visible(runtime->scene, slot->entity, visible);
        slot->visible = visible;
    }
}

henka_result henka_terrain_render_runtime_create(
    henka_engine* engine,
    henka_scene* scene,
    const henka_terrain_world* world,
    const henka_terrain_render_desc* desc,
    henka_terrain_render_runtime** out_runtime)
{
    henka_terrain_render_runtime* runtime;
    henka_terrain_render_desc resolved;
    henka_terrain_world_desc world_desc;
    henka_asset_manager* assets;
    henka_shader* terrain_shader = NULL;

    if (out_runtime == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_runtime = NULL;
    resolved = desc != NULL ? *desc : henka_terrain_render_desc_default();
    if (engine == NULL || scene == NULL || world == NULL || engine->renderer == NULL ||
        henka_terrain_world_get_desc(world, &world_desc) != HENKA_SUCCESS ||
        henka_terrain_world_desc_validate(&world_desc) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (resolved.material.shader == NULL)
    {
        assets = henka_engine_get_asset_manager(engine);
        if (assets == NULL || henka_assets_load_shader(
                assets,
                "assets/shaders/basic_lit.vert",
                "assets/shaders/basic_lit.frag",
                &terrain_shader) != HENKA_SUCCESS || terrain_shader == NULL)
        {
            return HENKA_ERROR_ASSET_SOURCE;
        }
        resolved.material.shader = terrain_shader;
    }
    if (!henka_terrain_render_desc_is_valid(&resolved))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    runtime = henka_calloc(1U, sizeof(*runtime));
    if (runtime == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    runtime->slots = henka_calloc(resolved.max_resident_chunks, sizeof(*runtime->slots));
    runtime->requests = henka_calloc(resolved.max_pending_requests, sizeof(*runtime->requests));
    if (runtime->slots == NULL || runtime->requests == NULL)
    {
        henka_free(runtime->requests);
        henka_free(runtime->slots);
        henka_free(runtime);
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    runtime->engine = engine;
    runtime->scene = scene;
    runtime->world = world;
    runtime->desc = resolved;
    {
        uint32_t index;
        for (index = 0U; index < resolved.max_resident_chunks; ++index)
        {
            runtime->slots[index].entity = HENKA_INVALID_ENTITY;
        }
    }
    if (resolved.material.name != NULL && resolved.material.name[0] != '\0')
    {
        if (strlen(resolved.material.name) >= sizeof(runtime->material_name))
        {
            henka_free(runtime->requests);
            henka_free(runtime->slots);
            henka_free(runtime);
            return HENKA_ERROR_LIMIT;
        }
        (void)memcpy(runtime->material_name, resolved.material.name, strlen(resolved.material.name) + 1U);
    }
    else
    {
        (void)memcpy(runtime->material_name, "Terrain", sizeof("Terrain"));
    }
    runtime->desc.material.name = runtime->material_name;
    runtime->stats = (henka_terrain_render_stats){0};
    *out_runtime = runtime;
    return HENKA_SUCCESS;
}

void henka_terrain_render_runtime_destroy(henka_terrain_render_runtime* runtime)
{
    uint32_t index;
    if (runtime == NULL)
    {
        return;
    }
    for (index = 0U; index < runtime->desc.max_resident_chunks; ++index)
    {
        henka_terrain_render_slot* slot = &runtime->slots[index];
        if (slot->occupied)
        {
            if (slot->entity != HENKA_INVALID_ENTITY &&
                henka_scene_is_entity_valid(runtime->scene, slot->entity))
            {
                henka_scene_destroy_entity(runtime->scene, slot->entity);
            }
            henka_mesh_destroy(slot->mesh);
        }
    }
    henka_free(runtime->requests);
    henka_free(runtime->slots);
    henka_free(runtime);
}

static henka_result henka_terrain_render_request_chunk_internal(
    henka_terrain_render_runtime* runtime,
    henka_terrain_chunk_id chunk_id,
    uint32_t lod_level,
    uint32_t edge_transition_mask,
    uint32_t fallback_skirt_mask)
{
    henka_terrain_world_desc desc;
    int32_t slot_index;
    henka_terrain_render_slot* slot;
    henka_result result;

    if (runtime == NULL || lod_level > HENKA_TERRAIN_MESH_MAX_LOD_LEVEL ||
        (edge_transition_mask & ~HENKA_TERRAIN_MESH_EDGE_ALL) != 0U ||
        (fallback_skirt_mask & ~HENKA_TERRAIN_MESH_EDGE_ALL) != 0U ||
        (edge_transition_mask & fallback_skirt_mask) != 0U ||
        henka_terrain_world_get_desc(runtime->world, &desc) != HENKA_SUCCESS ||
        !henka_terrain_chunk_id_is_valid(&desc, chunk_id))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    slot_index = henka_terrain_render_find_slot(runtime, chunk_id);
    if (slot_index < 0)
    {
        slot_index = henka_terrain_render_find_free_slot(runtime);
        if (slot_index < 0)
        {
            runtime->stats.dropped_requests += 1U;
            return HENKA_ERROR_LIMIT;
        }
        slot = &runtime->slots[slot_index];
        {
            uint32_t next_serial = slot->serial + 1U;
            if (next_serial == 0U)
            {
                next_serial = 1U;
            }
            *slot = (henka_terrain_render_slot){0};
            slot->serial = next_serial;
        }
        slot->occupied = true;
        slot->entity = HENKA_INVALID_ENTITY;
        slot->chunk_id = chunk_id;
        slot->selected_lod = lod_level;
    }
    slot = &runtime->slots[slot_index];
    result = henka_terrain_render_queue_slot(
        runtime,
        (uint32_t)slot_index,
        lod_level,
        edge_transition_mask,
        fallback_skirt_mask);
    if (result == HENKA_SUCCESS)
    {
        slot->requested_lod = lod_level;
        slot->requested_edge_transition_mask = edge_transition_mask;
        slot->requested_fallback_skirt_mask = fallback_skirt_mask;
    }
    if (result != HENKA_SUCCESS && !slot->resident)
    {
        slot->occupied = false;
    }
    return result;
}

henka_result henka_terrain_render_runtime_request_chunk(
    henka_terrain_render_runtime* runtime,
    henka_terrain_chunk_id chunk_id,
    uint32_t lod_level)
{
    return henka_terrain_render_request_chunk_internal(
        runtime, chunk_id, lod_level, 0U, HENKA_TERRAIN_MESH_EDGE_ALL);
}

henka_result henka_terrain_render_runtime_request_edit(
    henka_terrain_render_runtime* runtime,
    const henka_terrain_edit_command* command)
{
    henka_terrain_world_desc desc;
    henka_terrain_layout layout;
    uint32_t samples_across;
    uint32_t samples_down;
    uint32_t chunk_sample_span;
    uint32_t min_sample_x;
    uint32_t max_sample_x;
    uint32_t min_sample_z;
    uint32_t max_sample_z;
    int32_t min_chunk_x;
    int32_t max_chunk_x;
    int32_t min_chunk_z;
    int32_t max_chunk_z;
    int32_t chunk_z;
    henka_result first_error = HENKA_SUCCESS;

    if (runtime == NULL || command == NULL || runtime->world == NULL ||
        henka_terrain_world_get_desc(runtime->world, &desc) != HENKA_SUCCESS ||
        henka_terrain_world_desc_get_layout(&desc, &layout) != HENKA_SUCCESS ||
        henka_terrain_edit_command_validate(runtime->world, command) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    samples_across = desc.world_width_meters / desc.base_sample_spacing_meters + 1U;
    samples_down = desc.world_depth_meters / desc.base_sample_spacing_meters + 1U;
    chunk_sample_span = desc.samples_per_chunk - 1U;
    min_sample_x = command->center_sample_x > (int32_t)command->radius_samples
        ? (uint32_t)(command->center_sample_x - (int32_t)command->radius_samples) : 0U;
    min_sample_z = command->center_sample_z > (int32_t)command->radius_samples
        ? (uint32_t)(command->center_sample_z - (int32_t)command->radius_samples) : 0U;
    max_sample_x = (uint32_t)command->center_sample_x + command->radius_samples;
    max_sample_z = (uint32_t)command->center_sample_z + command->radius_samples;
    if (max_sample_x >= samples_across)
    {
        max_sample_x = samples_across - 1U;
    }
    if (max_sample_z >= samples_down)
    {
        max_sample_z = samples_down - 1U;
    }

    min_chunk_x = (int32_t)(min_sample_x / chunk_sample_span) - 1;
    max_chunk_x = (int32_t)(max_sample_x / chunk_sample_span) + 1;
    min_chunk_z = (int32_t)(min_sample_z / chunk_sample_span) - 1;
    max_chunk_z = (int32_t)(max_sample_z / chunk_sample_span) + 1;
    if (min_chunk_x < 0) { min_chunk_x = 0; }
    if (min_chunk_z < 0) { min_chunk_z = 0; }
    if (max_chunk_x >= (int32_t)layout.chunks_across)
    {
        max_chunk_x = (int32_t)layout.chunks_across - 1;
    }
    if (max_chunk_z >= (int32_t)layout.chunks_down)
    {
        max_chunk_z = (int32_t)layout.chunks_down - 1;
    }

    for (chunk_z = min_chunk_z; chunk_z <= max_chunk_z; ++chunk_z)
    {
        int32_t chunk_x;
        for (chunk_x = min_chunk_x; chunk_x <= max_chunk_x; ++chunk_x)
        {
            henka_terrain_chunk_id chunk_id = {chunk_x, chunk_z};
            int32_t slot_index = henka_terrain_render_find_slot(runtime, chunk_id);
            henka_terrain_render_slot* slot;
            henka_result result;
            if (slot_index < 0)
            {
                continue;
            }
            slot = &runtime->slots[slot_index];
            if (!slot->occupied)
            {
                continue;
            }
            result = henka_terrain_render_request_chunk_internal(
                runtime,
                chunk_id,
                slot->resident ? slot->selected_lod : slot->desired_lod,
                slot->resident ? slot->selected_edge_transition_mask : slot->desired_edge_transition_mask,
                slot->resident ? slot->selected_fallback_skirt_mask : slot->desired_fallback_skirt_mask);
            if (result != HENKA_SUCCESS && first_error == HENKA_SUCCESS)
            {
                first_error = result;
            }
        }
    }
    return first_error;
}

henka_result henka_terrain_render_runtime_refresh_dirty(
    henka_terrain_render_runtime* runtime)
{
    return henka_terrain_render_refresh_dirty_internal(runtime);
}

henka_result henka_terrain_render_runtime_remove_chunk(
    henka_terrain_render_runtime* runtime,
    henka_terrain_chunk_id chunk_id)
{
    int32_t slot_index;
    henka_terrain_render_slot* slot;

    if (runtime == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    slot_index = henka_terrain_render_find_slot(runtime, chunk_id);
    if (slot_index < 0)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    slot = &runtime->slots[slot_index];
    henka_terrain_render_cancel_slot_requests(
        runtime, (uint32_t)slot_index, slot->serial);
    if (slot->entity != HENKA_INVALID_ENTITY && henka_scene_is_entity_valid(runtime->scene, slot->entity))
    {
        henka_scene_destroy_entity(runtime->scene, slot->entity);
    }
    henka_mesh_destroy(slot->mesh);
    {
        uint32_t next_serial = slot->serial + 1U;
        if (next_serial == 0U)
        {
            next_serial = 1U;
        }
        *slot = (henka_terrain_render_slot){0};
        slot->serial = next_serial;
        slot->entity = HENKA_INVALID_ENTITY;
    }
    return HENKA_SUCCESS;
}

henka_result henka_terrain_render_runtime_update_observer(
    henka_terrain_render_runtime* runtime,
    henka_vec3 observer_position)
{
    uint32_t first;
    uint32_t second;
    if (runtime == NULL || !isfinite(observer_position.x) ||
        !isfinite(observer_position.y) || !isfinite(observer_position.z))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    {
        henka_result schedule_result = henka_terrain_render_schedule_resident_chunks(
            runtime, observer_position);
        if (schedule_result != HENKA_SUCCESS)
        {
            return schedule_result;
        }
    }
    for (first = 0U; first < runtime->desc.max_resident_chunks; ++first)
    {
        henka_terrain_render_slot* slot = &runtime->slots[first];
        if (!slot->occupied)
        {
            continue;
        }
        slot->desired_lod = henka_terrain_render_select_lod(runtime, slot, observer_position);
    }
    /* Clamp adjacent resident chunks to one LOD step in slot-index order. */
    for (first = 0U; first < runtime->desc.max_resident_chunks; ++first)
    {
        henka_terrain_render_slot* slot = &runtime->slots[first];
        if (!slot->occupied)
        {
            continue;
        }
        for (second = first + 1U; second < runtime->desc.max_resident_chunks; ++second)
        {
            henka_terrain_render_slot* neighbor = &runtime->slots[second];
            if (!neighbor->occupied || !henka_terrain_render_are_neighbors(slot->chunk_id, neighbor->chunk_id))
            {
                continue;
            }
            if (slot->desired_lod > neighbor->desired_lod + 1U)
            {
                slot->desired_lod = neighbor->desired_lod + 1U;
            }
            else if (neighbor->desired_lod > slot->desired_lod + 1U)
            {
                neighbor->desired_lod = slot->desired_lod + 1U;
            }
        }
    }
    for (first = 0U; first < runtime->desc.max_resident_chunks; ++first)
    {
        henka_terrain_render_slot* slot = &runtime->slots[first];
        if (!slot->occupied)
        {
            continue;
        }
        henka_terrain_render_topology_masks(
            runtime,
            slot,
            &slot->desired_edge_transition_mask,
            &slot->desired_fallback_skirt_mask);
    }
    for (first = 0U; first < runtime->desc.max_resident_chunks; ++first)
    {
        henka_terrain_render_slot* slot = &runtime->slots[first];
        float distance;
        henka_vec3 center;
        float dx;
        float dz;
        if (!slot->occupied)
        {
            continue;
        }
        center = henka_terrain_render_chunk_center(runtime, slot->chunk_id);
        dx = observer_position.x - center.x;
        dz = observer_position.z - center.z;
        distance = sqrtf(dx * dx + dz * dz);
        henka_terrain_render_set_visibility(
            runtime,
            slot,
            isfinite(distance) && distance <= runtime->desc.lod_max_distances[HENKA_TERRAIN_MESH_MAX_LOD_LEVEL]);
        if (slot->desired_lod != slot->requested_lod ||
            slot->desired_edge_transition_mask != slot->requested_edge_transition_mask ||
            slot->desired_fallback_skirt_mask != slot->requested_fallback_skirt_mask)
        {
            (void)henka_terrain_render_request_chunk_internal(
                runtime,
                slot->chunk_id,
                slot->desired_lod,
                slot->desired_edge_transition_mask,
                slot->desired_fallback_skirt_mask);
        }
    }
    return HENKA_SUCCESS;
}

henka_result henka_terrain_render_runtime_pump(
    henka_terrain_render_runtime* runtime,
    uint32_t max_rebuilds)
{
    uint32_t rebuilt = 0U;
    if (runtime == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    while (runtime->request_count > 0U && rebuilt < max_rebuilds)
    {
        henka_terrain_render_request request = runtime->requests[runtime->request_head];
        henka_terrain_render_slot* slot;
        henka_mesh* candidate = NULL;
        henka_terrain_revision revision = 0U;
        henka_terrain_generation generation = 0U;
        henka_result result;
        char name[64];
        henka_bounds bounds;
        henka_bounds old_bounds;

        runtime->request_head = (runtime->request_head + 1U) % runtime->desc.max_pending_requests;
        runtime->request_count -= 1U;
        if (request.slot_index >= runtime->desc.max_resident_chunks)
        {
            continue;
        }
        slot = &runtime->slots[request.slot_index];
        if (!slot->occupied || slot->serial != request.serial)
        {
            continue;
        }
        slot->queued = false;
        result = henka_mesh_create_from_terrain_chunk_with_edge_mask(
            runtime->engine,
            runtime->world,
            slot->chunk_id,
            request.lod_level,
            request.edge_transition_mask,
            request.fallback_skirt_mask,
            &candidate,
            &revision,
            &generation);
        if (result != HENKA_SUCCESS)
        {
            runtime->stats.failed_rebuilds += 1U;
            HENKA_LOG_WARN(
                "Terrain chunk (%d,%d) LOD %u rebuild failed: %s",
                slot->chunk_id.x,
                slot->chunk_id.z,
                request.lod_level,
                henka_result_to_string(result));
            continue;
        }
        if (!henka_terrain_render_get_chunk_bounds(
                runtime, slot->chunk_id, request.fallback_skirt_mask, &bounds))
        {
            henka_mesh_destroy(candidate);
            runtime->stats.failed_rebuilds += 1U;
            continue;
        }
        if (!slot->resident)
        {
            (void)snprintf(name, sizeof(name), "TerrainChunk_%d_%d", slot->chunk_id.x, slot->chunk_id.z);
            slot->entity = henka_scene_create_entity_named(runtime->scene, name);
            if (slot->entity == HENKA_INVALID_ENTITY ||
                henka_scene_set_entity_material(runtime->scene, slot->entity, runtime->desc.material) != HENKA_SUCCESS ||
                henka_scene_set_entity_mesh(runtime->scene, slot->entity, candidate) != HENKA_SUCCESS ||
                henka_scene_set_entity_transform(runtime->scene, slot->entity, henka_transform_identity()) != HENKA_SUCCESS ||
                henka_scene_set_entity_local_bounds(runtime->scene, slot->entity, bounds) != HENKA_SUCCESS)
            {
                if (slot->entity != HENKA_INVALID_ENTITY)
                {
                    henka_scene_destroy_entity(runtime->scene, slot->entity);
                }
                slot->entity = HENKA_INVALID_ENTITY;
                henka_mesh_destroy(candidate);
                runtime->stats.failed_rebuilds += 1U;
                continue;
            }
            slot->visible = true;
        }
        else if (henka_scene_get_entity_local_bounds(runtime->scene, slot->entity, &old_bounds) != HENKA_SUCCESS ||
            henka_scene_set_entity_mesh(runtime->scene, slot->entity, candidate) != HENKA_SUCCESS)
        {
            henka_mesh_destroy(candidate);
            runtime->stats.failed_rebuilds += 1U;
            continue;
        }
        else if (henka_scene_set_entity_local_bounds(runtime->scene, slot->entity, bounds) != HENKA_SUCCESS)
        {
            (void)henka_scene_set_entity_mesh(runtime->scene, slot->entity, slot->mesh);
            (void)henka_scene_set_entity_local_bounds(runtime->scene, slot->entity, old_bounds);
            henka_mesh_destroy(candidate);
            runtime->stats.failed_rebuilds += 1U;
            continue;
        }
        henka_mesh_destroy(slot->mesh);
        slot->mesh = candidate;
        slot->resident = true;
        slot->selected_lod = request.lod_level;
        slot->requested_lod = request.lod_level;
        slot->selected_edge_transition_mask = request.edge_transition_mask;
        slot->requested_edge_transition_mask = request.edge_transition_mask;
        slot->selected_fallback_skirt_mask = request.fallback_skirt_mask;
        slot->requested_fallback_skirt_mask = request.fallback_skirt_mask;
        slot->revision = revision;
        slot->generation = generation;
        henka_terrain_render_capture_dependencies(runtime, slot);
        runtime->stats.rebuilt_chunks += 1U;
        if (request.edge_transition_mask != 0U)
        {
            runtime->stats.transition_rebuilds += 1U;
        }
        if (request.fallback_skirt_mask != 0U)
        {
            runtime->stats.fallback_skirt_chunks += 1U;
        }
        rebuilt += 1U;
    }
    return HENKA_SUCCESS;
}

henka_result henka_terrain_render_runtime_get_chunk(
    const henka_terrain_render_runtime* runtime,
    henka_terrain_chunk_id chunk_id,
    henka_terrain_render_chunk_info* out_info)
{
    int32_t slot_index;
    const henka_terrain_render_slot* slot;
    if (runtime == NULL || out_info == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    slot_index = henka_terrain_render_find_slot(runtime, chunk_id);
    if (slot_index < 0)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    slot = &runtime->slots[slot_index];
    *out_info = (henka_terrain_render_chunk_info){
        slot->chunk_id, slot->entity, slot->mesh, slot->revision, slot->generation,
        slot->selected_lod,
        slot->selected_edge_transition_mask,
        slot->selected_fallback_skirt_mask,
        slot->resident, slot->visible, slot->queued};
    return HENKA_SUCCESS;
}

henka_result henka_terrain_render_runtime_get_stats(
    const henka_terrain_render_runtime* runtime,
    henka_terrain_render_stats* out_stats)
{
    uint32_t index;
    const henka_texture* material_textures[HENKA_MATERIAL_TERRAIN_LAYER_COUNT * 3U] = {0};
    uint32_t material_texture_count = 0U;
    henka_terrain_render_stats* mutable_stats;
    if (runtime == NULL || out_stats == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_stats = runtime->stats;
    out_stats->pending_requests = runtime->request_count;
    out_stats->resident_chunks = 0U;
    out_stats->visible_chunks = 0U;
    out_stats->runtime_cpu_bytes =
        (uint64_t)sizeof(*runtime) +
        (uint64_t)runtime->desc.max_resident_chunks * sizeof(*runtime->slots) +
        (uint64_t)runtime->desc.max_pending_requests * sizeof(*runtime->requests);
    out_stats->gpu_vertex_bytes = 0U;
    out_stats->gpu_index_bytes = 0U;
    out_stats->material_texture_count = 0U;
    out_stats->material_gpu_bytes = 0U;
    memset(out_stats->lod_counts, 0, sizeof(out_stats->lod_counts));
    for (index = 0U; index < HENKA_MATERIAL_TERRAIN_LAYER_COUNT; ++index)
    {
        const henka_material_layer* layer = &runtime->desc.material.terrain_layers[index];
        const henka_texture* textures[3] = {
            layer->base_color_texture,
            layer->normal_texture,
            layer->metallic_roughness_texture};
        uint32_t texture_index;
        for (texture_index = 0U; texture_index < 3U; ++texture_index)
        {
            henka_texture_info texture_info;
            uint32_t prior_index;
            bool duplicate = false;
            if (textures[texture_index] == NULL)
            {
                continue;
            }
            for (prior_index = 0U; prior_index < material_texture_count; ++prior_index)
            {
                if (material_textures[prior_index] == textures[texture_index])
                {
                    duplicate = true;
                    break;
                }
            }
            if (duplicate || material_texture_count >= sizeof(material_textures) / sizeof(material_textures[0]))
            {
                continue;
            }
            material_textures[material_texture_count++] = textures[texture_index];
            if (henka_texture_get_info(textures[texture_index], &texture_info) == HENKA_SUCCESS)
            {
                henka_terrain_render_add_bytes(&out_stats->material_gpu_bytes, texture_info.resident_gpu_bytes);
            }
        }
    }
    out_stats->material_texture_count = material_texture_count;
    for (index = 0U; index < runtime->desc.max_resident_chunks; ++index)
    {
        const henka_terrain_render_slot* slot = &runtime->slots[index];
        if (slot->occupied && slot->resident)
        {
            out_stats->resident_chunks += 1U;
            if (slot->visible)
            {
                out_stats->visible_chunks += 1U;
            }
            out_stats->lod_counts[slot->selected_lod] += 1U;
            if (slot->mesh != NULL)
            {
                henka_terrain_render_add_bytes(
                    &out_stats->gpu_vertex_bytes,
                    (uint64_t)slot->mesh->vertex_count * sizeof(henka_vertex));
                henka_terrain_render_add_bytes(
                    &out_stats->gpu_index_bytes,
                    (uint64_t)slot->mesh->index_count * sizeof(unsigned int));
            }
        }
    }
    mutable_stats = (henka_terrain_render_stats*)&runtime->stats;
    if (out_stats->resident_chunks > out_stats->max_resident_chunks)
    {
        out_stats->max_resident_chunks = out_stats->resident_chunks;
    }
    if (out_stats->visible_chunks > out_stats->max_visible_chunks)
    {
        out_stats->max_visible_chunks = out_stats->visible_chunks;
    }
    mutable_stats->max_resident_chunks = out_stats->max_resident_chunks;
    mutable_stats->max_visible_chunks = out_stats->max_visible_chunks;
    return HENKA_SUCCESS;
}
