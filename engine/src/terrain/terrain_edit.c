#include <henka/terrain_edit.h>

#include <limits.h>
#include <string.h>

#include <henka/memory.h>

#include "terrain_internal.h"

#define HENKA_TERRAIN_EDIT_MAX_AFFECTED_REGIONS 16U
#define HENKA_TERRAIN_EDIT_WEIGHT_MAX 65535U

typedef struct henka_terrain_edit_candidate
{
    henka_terrain_region_record* record;
    henka_terrain_sample* samples;
    henka_terrain_region_id id;
} henka_terrain_edit_candidate;

static int64_t henka_terrain_edit_clamp_height(int64_t value)
{
    if (value < INT32_MIN)
    {
        return INT32_MIN;
    }
    if (value > INT32_MAX)
    {
        return INT32_MAX;
    }
    return value;
}

static bool henka_terrain_edit_operation_is_valid(henka_terrain_edit_operation operation)
{
    return operation >= HENKA_TERRAIN_EDIT_RAISE && operation <= HENKA_TERRAIN_EDIT_PAINT;
}

static bool henka_terrain_edit_falloff_is_valid(henka_terrain_edit_falloff falloff)
{
    return falloff >= HENKA_TERRAIN_EDIT_FALLOFF_LINEAR &&
        falloff <= HENKA_TERRAIN_EDIT_FALLOFF_SMOOTH;
}

static uint32_t henka_terrain_edit_weight(
    const henka_terrain_edit_command* command,
    int32_t sample_x,
    int32_t sample_z)
{
    int64_t dx = (int64_t)sample_x - command->center_sample_x;
    int64_t dz = (int64_t)sample_z - command->center_sample_z;
    uint64_t distance_squared = (uint64_t)(dx * dx + dz * dz);
    uint64_t radius_squared = (uint64_t)command->radius_samples * command->radius_samples;
    uint64_t linear;

    if (distance_squared >= radius_squared)
    {
        return 0U;
    }
    linear = ((radius_squared - distance_squared) * HENKA_TERRAIN_EDIT_WEIGHT_MAX) /
        radius_squared;
    if (command->falloff == HENKA_TERRAIN_EDIT_FALLOFF_LINEAR)
    {
        return (uint32_t)linear;
    }
    return (uint32_t)((linear * linear *
        (3U * HENKA_TERRAIN_EDIT_WEIGHT_MAX - 2U * linear)) /
        ((uint64_t)HENKA_TERRAIN_EDIT_WEIGHT_MAX * HENKA_TERRAIN_EDIT_WEIGHT_MAX));
}

static bool henka_terrain_edit_region_for_sample(
    const henka_terrain_world* world,
    int32_t sample_x,
    int32_t sample_z,
    henka_terrain_region_id* out_region,
    uint32_t* out_local_x,
    uint32_t* out_local_z)
{
    uint32_t region_sample_span = world->desc.region_edge_meters /
        world->desc.base_sample_spacing_meters;
    int32_t region_x = sample_x / (int32_t)region_sample_span;
    int32_t region_z = sample_z / (int32_t)region_sample_span;
    if (out_region == NULL || out_local_x == NULL || out_local_z == NULL ||
        !henka_terrain_region_id_is_valid(&world->desc, (henka_terrain_region_id){region_x, region_z}))
    {
        return false;
    }
    *out_region = (henka_terrain_region_id){region_x, region_z};
    *out_local_x = (uint32_t)(sample_x - region_x * (int32_t)region_sample_span);
    *out_local_z = (uint32_t)(sample_z - region_z * (int32_t)region_sample_span);
    return *out_local_x < world->layout.samples_per_region_edge &&
        *out_local_z < world->layout.samples_per_region_edge;
}

static henka_terrain_sample* henka_terrain_edit_candidate_sample(
    henka_terrain_edit_candidate* candidates,
    uint32_t candidate_count,
    henka_terrain_region_id region_id,
    uint32_t local_x,
    uint32_t local_z,
    uint32_t samples_per_region_edge)
{
    uint32_t index;
    for (index = 0U; index < candidate_count; ++index)
    {
        if (henka_terrain_region_id_equal(candidates[index].id, region_id))
        {
            return &candidates[index].samples[
                (size_t)local_z * samples_per_region_edge + local_x];
        }
    }
    return NULL;
}

static const henka_terrain_sample* henka_terrain_edit_source_sample(
    const henka_terrain_world* world,
    int32_t sample_x,
    int32_t sample_z)
{
    henka_terrain_region_id region_id;
    uint32_t local_x;
    uint32_t local_z;
    const henka_terrain_region_record* record;
    if (!henka_terrain_edit_region_for_sample(world, sample_x, sample_z, &region_id, &local_x, &local_z))
    {
        return NULL;
    }
    record = henka_terrain_find_region_record_const(world, region_id);
    return record == NULL || record->samples == NULL
        ? NULL
        : &record->samples[(size_t)local_z * world->layout.samples_per_region_edge + local_x];
}

henka_terrain_edit_command henka_terrain_edit_command_default(void)
{
    return (henka_terrain_edit_command){
        0U,
        HENKA_TERRAIN_EDIT_ALGORITHM_VERSION,
        HENKA_TERRAIN_EDIT_RAISE,
        0,
        0,
        1U,
        HENKA_TERRAIN_EDIT_FALLOFF_LINEAR,
        100,
        0U,
        255U};
}

henka_result henka_terrain_edit_command_validate(
    const henka_terrain_world* world,
    const henka_terrain_edit_command* command)
{
    uint32_t samples_across;
    uint32_t samples_down;
    uint32_t max_radius_samples;
    if (world == NULL || command == NULL ||
        command->algorithm_version != HENKA_TERRAIN_EDIT_ALGORITHM_VERSION ||
        !henka_terrain_edit_operation_is_valid(command->operation) ||
        !henka_terrain_edit_falloff_is_valid(command->falloff) ||
        command->radius_samples == 0U || command->value_millimeters == INT32_MIN)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    max_radius_samples = HENKA_TERRAIN_EDIT_MAX_RADIUS_METERS /
        world->desc.base_sample_spacing_meters;
    samples_across = world->desc.world_width_meters / world->desc.base_sample_spacing_meters + 1U;
    samples_down = world->desc.world_depth_meters / world->desc.base_sample_spacing_meters + 1U;
    if (command->radius_samples > max_radius_samples ||
        command->center_sample_x < 0 || command->center_sample_z < 0 ||
        (uint32_t)command->center_sample_x >= samples_across ||
        (uint32_t)command->center_sample_z >= samples_down ||
        (command->operation == HENKA_TERRAIN_EDIT_PAINT &&
            command->paint_layer >= HENKA_TERRAIN_ACTIVE_MATERIAL_COUNT))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return HENKA_SUCCESS;
}

henka_result henka_terrain_edit_get_affected_regions(
    const henka_terrain_world* world,
    const henka_terrain_edit_command* command,
    henka_terrain_region_id* out_regions,
    uint32_t* in_out_region_count)
{
    uint32_t region_sample_span;
    uint32_t min_x;
    uint32_t max_x;
    uint32_t min_z;
    uint32_t max_z;
    int32_t min_region_x;
    int32_t max_region_x;
    int32_t min_region_z;
    int32_t max_region_z;
    uint32_t required = 0U;
    int32_t region_z;
    int32_t region_x;
    henka_result result;

    if (in_out_region_count == NULL || out_regions == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_terrain_edit_command_validate(world, command);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    region_sample_span = world->desc.region_edge_meters / world->desc.base_sample_spacing_meters;
    min_x = command->center_sample_x > (int32_t)command->radius_samples
        ? (uint32_t)(command->center_sample_x - (int32_t)command->radius_samples) : 0U;
    min_z = command->center_sample_z > (int32_t)command->radius_samples
        ? (uint32_t)(command->center_sample_z - (int32_t)command->radius_samples) : 0U;
    max_x = (uint32_t)command->center_sample_x + command->radius_samples;
    max_z = (uint32_t)command->center_sample_z + command->radius_samples;
    {
        uint32_t samples_across = world->desc.world_width_meters / world->desc.base_sample_spacing_meters + 1U;
        uint32_t samples_down = world->desc.world_depth_meters / world->desc.base_sample_spacing_meters + 1U;
        if (max_x >= samples_across) { max_x = samples_across - 1U; }
        if (max_z >= samples_down) { max_z = samples_down - 1U; }
    }
    min_region_x = (int32_t)(min_x / region_sample_span);
    max_region_x = (int32_t)(max_x / region_sample_span);
    min_region_z = (int32_t)(min_z / region_sample_span);
    max_region_z = (int32_t)(max_z / region_sample_span);
    for (region_z = min_region_z; region_z <= max_region_z; ++region_z)
    {
        for (region_x = min_region_x; region_x <= max_region_x; ++region_x)
        {
            if (required >= HENKA_TERRAIN_EDIT_MAX_AFFECTED_REGIONS)
            {
                return HENKA_ERROR_LIMIT;
            }
            if (required < *in_out_region_count)
            {
                out_regions[required] = (henka_terrain_region_id){region_x, region_z};
            }
            ++required;
        }
    }
    if (*in_out_region_count < required)
    {
        *in_out_region_count = required;
        return HENKA_ERROR_LIMIT;
    }
    *in_out_region_count = required;
    return HENKA_SUCCESS;
}

static void henka_terrain_edit_apply_paint(
    henka_terrain_sample* sample,
    const henka_terrain_edit_command* command,
    uint32_t weight)
{
    uint32_t old_target = sample->material_weights[command->paint_layer];
    uint32_t old_remaining = 255U - old_target;
    uint32_t desired_target = old_target +
        ((255U - old_target) * command->paint_strength * weight) /
        (255U * HENKA_TERRAIN_EDIT_WEIGHT_MAX);
    uint32_t new_remaining = 255U - desired_target;
    uint32_t layer;
    sample->material_weights[command->paint_layer] = (uint8_t)desired_target;
    for (layer = 0U; layer < HENKA_TERRAIN_ACTIVE_MATERIAL_COUNT; ++layer)
    {
        if (layer != command->paint_layer)
        {
            sample->material_weights[layer] = old_remaining == 0U
                ? 0U
                : (uint8_t)(((uint32_t)sample->material_weights[layer] * new_remaining) / old_remaining);
        }
    }
    (void)henka_terrain_normalize_weights(sample->material_weights);
}

henka_result henka_terrain_world_apply_edit(
    henka_terrain_world* world,
    const henka_terrain_edit_command* command,
    henka_terrain_revision transaction_id)
{
    henka_terrain_edit_candidate candidates[HENKA_TERRAIN_EDIT_MAX_AFFECTED_REGIONS] = {0};
    uint32_t candidate_count = 0U;
    uint32_t region_sample_span;
    uint32_t min_x;
    uint32_t max_x;
    uint32_t min_z;
    uint32_t max_z;
    int32_t min_region_x;
    int32_t max_region_x;
    int32_t min_region_z;
    int32_t max_region_z;
    int32_t region_z;
    int32_t region_x;
    uint32_t x;
    uint32_t z;
    uint32_t index;
    henka_result result;

    (void)transaction_id;
    result = henka_terrain_edit_command_validate(world, command);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    region_sample_span = world->desc.region_edge_meters / world->desc.base_sample_spacing_meters;
    min_x = command->center_sample_x > (int32_t)command->radius_samples
        ? (uint32_t)(command->center_sample_x - (int32_t)command->radius_samples) : 0U;
    min_z = command->center_sample_z > (int32_t)command->radius_samples
        ? (uint32_t)(command->center_sample_z - (int32_t)command->radius_samples) : 0U;
    max_x = (uint32_t)command->center_sample_x + command->radius_samples;
    max_z = (uint32_t)command->center_sample_z + command->radius_samples;
    {
        uint32_t samples_across = world->desc.world_width_meters / world->desc.base_sample_spacing_meters + 1U;
        uint32_t samples_down = world->desc.world_depth_meters / world->desc.base_sample_spacing_meters + 1U;
        if (max_x >= samples_across) { max_x = samples_across - 1U; }
        if (max_z >= samples_down) { max_z = samples_down - 1U; }
    }
    min_region_x = (int32_t)(min_x / region_sample_span);
    max_region_x = (int32_t)(max_x / region_sample_span);
    min_region_z = (int32_t)(min_z / region_sample_span);
    max_region_z = (int32_t)(max_z / region_sample_span);
    for (region_z = min_region_z; region_z <= max_region_z; ++region_z)
    {
        for (region_x = min_region_x; region_x <= max_region_x; ++region_x)
        {
            henka_terrain_region_id id = {region_x, region_z};
            henka_terrain_region_record* record = henka_terrain_find_region_record(world, id);
            if (record == NULL || candidate_count >= HENKA_TERRAIN_EDIT_MAX_AFFECTED_REGIONS)
            {
                return HENKA_ERROR_ASSET_SOURCE;
            }
            candidates[candidate_count].record = record;
            candidates[candidate_count].id = id;
            candidates[candidate_count].samples = henka_malloc(
                world->layout.samples_per_region * sizeof(*record->samples));
            if (candidates[candidate_count].samples == NULL)
            {
                result = HENKA_ERROR_OUT_OF_MEMORY;
                goto cleanup;
            }
            memcpy(candidates[candidate_count].samples, record->samples,
                world->layout.samples_per_region * sizeof(*record->samples));
            ++candidate_count;
        }
    }
    for (z = min_z; z <= max_z; ++z)
    {
        for (x = min_x; x <= max_x; ++x)
        {
            henka_terrain_region_id id;
            uint32_t local_x;
            uint32_t local_z;
            henka_terrain_sample* sample;
            uint32_t weight = henka_terrain_edit_weight(command, (int32_t)x, (int32_t)z);
            if (weight == 0U || !henka_terrain_edit_region_for_sample(
                    world, (int32_t)x, (int32_t)z, &id, &local_x, &local_z))
            {
                continue;
            }
            sample = henka_terrain_edit_candidate_sample(
                candidates, candidate_count, id, local_x, local_z,
                world->layout.samples_per_region_edge);
            if (sample == NULL)
            {
                result = HENKA_ERROR_INVALID_ARGUMENT;
                goto cleanup;
            }
            if (command->operation == HENKA_TERRAIN_EDIT_RAISE ||
                command->operation == HENKA_TERRAIN_EDIT_LOWER)
            {
                int64_t delta = ((int64_t)command->value_millimeters * weight) /
                    HENKA_TERRAIN_EDIT_WEIGHT_MAX;
                sample->height_millimeters = (int32_t)henka_terrain_edit_clamp_height(
                    (int64_t)sample->height_millimeters +
                    (command->operation == HENKA_TERRAIN_EDIT_LOWER ? -delta : delta));
            }
            else if (command->operation == HENKA_TERRAIN_EDIT_FLATTEN)
            {
                int64_t difference = (int64_t)command->value_millimeters - sample->height_millimeters;
                sample->height_millimeters = (int32_t)henka_terrain_edit_clamp_height(
                    (int64_t)sample->height_millimeters +
                    (difference * weight) / HENKA_TERRAIN_EDIT_WEIGHT_MAX);
            }
            else if (command->operation == HENKA_TERRAIN_EDIT_SMOOTH)
            {
                int64_t sum = 0;
                uint32_t count = 0U;
                const henka_terrain_sample* neighbor;
                const int32_t offsets[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
                uint32_t offset_index;
                for (offset_index = 0U; offset_index < 4U; ++offset_index)
                {
                    neighbor = henka_terrain_edit_source_sample(
                        world, (int32_t)x + offsets[offset_index][0],
                        (int32_t)z + offsets[offset_index][1]);
                    if (neighbor != NULL)
                    {
                        sum += neighbor->height_millimeters;
                        ++count;
                    }
                }
                if (count > 0U)
                {
                    int64_t target = sum / (int64_t)count;
                    sample->height_millimeters = (int32_t)henka_terrain_edit_clamp_height(
                        (int64_t)sample->height_millimeters +
                        ((target - sample->height_millimeters) * weight) / HENKA_TERRAIN_EDIT_WEIGHT_MAX);
                }
            }
            else
            {
                henka_terrain_edit_apply_paint(sample, command, weight);
            }
        }
    }
    for (index = 0U; index < candidate_count; ++index)
    {
        if (candidates[index].record->state.revision == UINT64_MAX)
        {
            result = HENKA_ERROR_NUMERIC_RANGE;
            goto cleanup;
        }
    }
    for (index = 0U; index < candidate_count; ++index)
    {
        memcpy(candidates[index].record->samples, candidates[index].samples,
            world->layout.samples_per_region * sizeof(*candidates[index].samples));
        ++candidates[index].record->state.revision;
        candidates[index].record->state.dirty = true;
    }
    result = HENKA_SUCCESS;

cleanup:
    for (index = 0U; index < candidate_count; ++index)
    {
        henka_free(candidates[index].samples);
    }
    return result;
}

henka_result henka_terrain_world_get_region_samples(
    const henka_terrain_world* world,
    henka_terrain_region_id region_id,
    const henka_terrain_sample** out_samples,
    size_t* out_sample_count)
{
    const henka_terrain_region_record* record;
    if (world == NULL || out_samples == NULL || out_sample_count == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    record = henka_terrain_find_region_record_const(world, region_id);
    if (record == NULL || record->samples == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_samples = record->samples;
    *out_sample_count = record->sample_count;
    return HENKA_SUCCESS;
}

henka_result henka_terrain_world_copy_region_samples(
    const henka_terrain_world* source,
    henka_terrain_world* destination,
    henka_terrain_region_id region_id)
{
    const henka_terrain_region_record* source_record;
    henka_terrain_region_record* destination_record;
    henka_terrain_world_desc source_desc;
    henka_terrain_world_desc destination_desc;

    source_record = henka_terrain_find_region_record_const(source, region_id);
    if (source_record == NULL || source_record->samples == NULL ||
        !source_record->state.cpu_resident ||
        henka_terrain_world_get_desc(source, &source_desc) != HENKA_SUCCESS ||
        henka_terrain_world_get_desc(destination, &destination_desc) != HENKA_SUCCESS ||
        source_desc.world_identity != destination_desc.world_identity ||
        source_desc.base_asset_identity != destination_desc.base_asset_identity ||
        source_desc.format_version != destination_desc.format_version)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (henka_terrain_world_reserve_region(destination, region_id) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_LIMIT;
    }
    destination_record = henka_terrain_find_region_record(destination, region_id);
    if (destination_record == NULL || destination_record->samples == NULL ||
        destination_record->sample_count != source_record->sample_count)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    memcpy(
        destination_record->samples,
        source_record->samples,
        source_record->sample_count * sizeof(*source_record->samples));
    destination_record->state.revision = source_record->state.revision;
    destination_record->state.generation = source_record->state.generation;
    destination_record->state.cpu_resident = true;
    destination_record->state.physics_resident = false;
    destination_record->state.render_resident = false;
    destination_record->state.pending_io = false;
    destination_record->state.dirty = false;
    destination_record->state.resident_chunk_count = 0U;
    return HENKA_SUCCESS;
}
