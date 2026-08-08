#include <henka/terrain.h>

#include <limits.h>
#include <string.h>

#include <henka/memory.h>

#include "terrain_internal.h"

static bool henka_terrain_size_multiply_fits(size_t left, size_t right)
{
    return right == 0U || left <= SIZE_MAX / right;
}

static bool henka_terrain_region_id_matches(
    henka_terrain_region_id left,
    henka_terrain_region_id right)
{
    return left.x == right.x && left.z == right.z;
}

static bool henka_terrain_chunk_id_matches(
    henka_terrain_chunk_id left,
    henka_terrain_chunk_id right)
{
    return left.x == right.x && left.z == right.z;
}

henka_terrain_world_desc henka_terrain_world_desc_default(void)
{
    henka_terrain_world_desc desc;
    memset(&desc, 0, sizeof(desc));
    desc.format_version = HENKA_TERRAIN_FORMAT_VERSION;
    desc.world_width_meters = HENKA_TERRAIN_DEFAULT_WORLD_EDGE_METERS;
    desc.world_depth_meters = HENKA_TERRAIN_DEFAULT_WORLD_EDGE_METERS;
    desc.region_edge_meters = HENKA_TERRAIN_DEFAULT_REGION_EDGE_METERS;
    desc.chunk_edge_meters = HENKA_TERRAIN_DEFAULT_CHUNK_EDGE_METERS;
    desc.samples_per_chunk = HENKA_TERRAIN_DEFAULT_SAMPLES_PER_CHUNK;
    desc.base_sample_spacing_meters = HENKA_TERRAIN_DEFAULT_SAMPLE_SPACING_METERS;
    desc.chunks_per_region_edge = HENKA_TERRAIN_DEFAULT_CHUNKS_PER_REGION_EDGE;
    desc.regions_across = HENKA_TERRAIN_DEFAULT_REGIONS_ACROSS;
    desc.regions_down = HENKA_TERRAIN_DEFAULT_REGIONS_ACROSS;
    desc.max_resident_regions = 16U;
    desc.max_resident_chunks = 128U;
    desc.max_pending_io = 64U;
    desc.max_stream_observers = 8U;
    return desc;
}

henka_result henka_terrain_world_desc_get_layout(
    const henka_terrain_world_desc* desc,
    henka_terrain_layout* out_layout)
{
    uint64_t samples_per_region_edge;
    uint64_t samples_per_region;
    uint64_t chunks_across;
    uint64_t chunks_down;
    uint64_t chunks_per_world;

    if (desc == NULL || out_layout == NULL ||
        henka_terrain_world_desc_validate(desc) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    samples_per_region_edge = (uint64_t)desc->chunks_per_region_edge *
        (uint64_t)(desc->samples_per_chunk - 1U) + 1U;
    samples_per_region = samples_per_region_edge * samples_per_region_edge;
    chunks_across = (uint64_t)desc->world_width_meters / desc->chunk_edge_meters;
    chunks_down = (uint64_t)desc->world_depth_meters / desc->chunk_edge_meters;
    chunks_per_world = chunks_across * chunks_down;
    if (samples_per_region_edge > UINT32_MAX ||
        samples_per_region > SIZE_MAX || chunks_across > UINT32_MAX ||
        chunks_down > UINT32_MAX || chunks_per_world > SIZE_MAX)
    {
        return HENKA_ERROR_NUMERIC_RANGE;
    }
    out_layout->samples_per_region_edge = (uint32_t)samples_per_region_edge;
    out_layout->samples_per_region = (size_t)samples_per_region;
    out_layout->chunks_across = (uint32_t)chunks_across;
    out_layout->chunks_down = (uint32_t)chunks_down;
    out_layout->chunks_per_world = (size_t)chunks_per_world;
    return HENKA_SUCCESS;
}

henka_result henka_terrain_world_desc_validate(const henka_terrain_world_desc* desc)
{
    size_t samples_per_region_edge;
    size_t samples_per_region;
    size_t chunks_per_world;

    if (desc == NULL || desc->format_version != HENKA_TERRAIN_FORMAT_VERSION ||
        desc->world_width_meters == 0U || desc->world_depth_meters == 0U ||
        desc->world_width_meters > HENKA_TERRAIN_MAX_WORLD_EDGE_METERS ||
        desc->world_depth_meters > HENKA_TERRAIN_MAX_WORLD_EDGE_METERS ||
        desc->region_edge_meters == 0U || desc->chunk_edge_meters == 0U ||
        desc->samples_per_chunk < 2U || desc->base_sample_spacing_meters == 0U ||
        desc->chunks_per_region_edge == 0U || desc->regions_across == 0U ||
        desc->regions_down == 0U || desc->max_resident_regions == 0U ||
        desc->max_resident_regions > HENKA_TERRAIN_MAX_RESIDENT_REGIONS ||
        desc->max_resident_chunks == 0U ||
        desc->max_resident_chunks > HENKA_TERRAIN_MAX_RESIDENT_CHUNKS ||
        desc->max_pending_io == 0U || desc->max_pending_io > HENKA_TERRAIN_MAX_PENDING_IO ||
        desc->max_stream_observers == 0U ||
        desc->max_stream_observers > HENKA_TERRAIN_MAX_STREAM_OBSERVERS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (desc->world_width_meters % desc->region_edge_meters != 0U ||
        desc->world_depth_meters % desc->region_edge_meters != 0U ||
        desc->region_edge_meters % desc->chunk_edge_meters != 0U ||
        desc->chunk_edge_meters % desc->base_sample_spacing_meters != 0U ||
        desc->samples_per_chunk !=
            desc->chunk_edge_meters / desc->base_sample_spacing_meters + 1U ||
        desc->chunks_per_region_edge != desc->region_edge_meters / desc->chunk_edge_meters ||
        desc->regions_across != desc->world_width_meters / desc->region_edge_meters ||
        desc->regions_down != desc->world_depth_meters / desc->region_edge_meters)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    samples_per_region_edge = (size_t)desc->chunks_per_region_edge *
        (size_t)(desc->samples_per_chunk - 1U) + 1U;
    if (!henka_terrain_size_multiply_fits(samples_per_region_edge, samples_per_region_edge))
    {
        return HENKA_ERROR_NUMERIC_RANGE;
    }
    samples_per_region = samples_per_region_edge * samples_per_region_edge;
    if (samples_per_region > SIZE_MAX / sizeof(henka_terrain_sample))
    {
        return HENKA_ERROR_NUMERIC_RANGE;
    }
    {
        size_t chunks_across = (size_t)(desc->world_width_meters / desc->chunk_edge_meters);
        size_t chunks_down = (size_t)(desc->world_depth_meters / desc->chunk_edge_meters);
        if (!henka_terrain_size_multiply_fits(chunks_across, chunks_down))
        {
            return HENKA_ERROR_NUMERIC_RANGE;
        }
        chunks_per_world = chunks_across * chunks_down;
    }
    if (chunks_per_world > SIZE_MAX / sizeof(henka_terrain_chunk_record))
    {
        return HENKA_ERROR_NUMERIC_RANGE;
    }
    return HENKA_SUCCESS;
}

bool henka_terrain_region_id_equal(henka_terrain_region_id left, henka_terrain_region_id right)
{
    return henka_terrain_region_id_matches(left, right);
}

bool henka_terrain_chunk_id_equal(henka_terrain_chunk_id left, henka_terrain_chunk_id right)
{
    return henka_terrain_chunk_id_matches(left, right);
}

bool henka_terrain_region_id_is_valid(
    const henka_terrain_world_desc* desc,
    henka_terrain_region_id id)
{
    return desc != NULL && henka_terrain_world_desc_validate(desc) == HENKA_SUCCESS &&
        id.x >= 0 && id.z >= 0 && (uint32_t)id.x < desc->regions_across &&
        (uint32_t)id.z < desc->regions_down;
}

bool henka_terrain_chunk_id_is_valid(
    const henka_terrain_world_desc* desc,
    henka_terrain_chunk_id id)
{
    uint32_t chunks_across;
    uint32_t chunks_down;
    if (desc == NULL || henka_terrain_world_desc_validate(desc) != HENKA_SUCCESS)
    {
        return false;
    }
    chunks_across = desc->world_width_meters / desc->chunk_edge_meters;
    chunks_down = desc->world_depth_meters / desc->chunk_edge_meters;
    return id.x >= 0 && id.z >= 0 && (uint32_t)id.x < chunks_across &&
        (uint32_t)id.z < chunks_down;
}

henka_result henka_terrain_region_id_from_chunk(
    const henka_terrain_world_desc* desc,
    henka_terrain_chunk_id chunk_id,
    henka_terrain_region_id* out_region_id)
{
    if (out_region_id == NULL || !henka_terrain_chunk_id_is_valid(desc, chunk_id))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    out_region_id->x = chunk_id.x / (int32_t)desc->chunks_per_region_edge;
    out_region_id->z = chunk_id.z / (int32_t)desc->chunks_per_region_edge;
    return HENKA_SUCCESS;
}

henka_result henka_terrain_normalize_weights(
    uint8_t weights[HENKA_TERRAIN_ACTIVE_MATERIAL_COUNT])
{
    uint32_t sum = 0U;
    uint32_t total = 0U;
    uint32_t remainders[HENKA_TERRAIN_ACTIVE_MATERIAL_COUNT];
    uint32_t index;

    if (weights == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    for (index = 0U; index < HENKA_TERRAIN_ACTIVE_MATERIAL_COUNT; ++index)
    {
        sum += weights[index];
    }
    if (sum == 0U)
    {
        weights[0] = 255U;
        for (index = 1U; index < HENKA_TERRAIN_ACTIVE_MATERIAL_COUNT; ++index)
        {
            weights[index] = 0U;
        }
        return HENKA_SUCCESS;
    }
    for (index = 0U; index < HENKA_TERRAIN_ACTIVE_MATERIAL_COUNT; ++index)
    {
        uint32_t product = (uint32_t)weights[index] * 255U;
        weights[index] = (uint8_t)(product / sum);
        remainders[index] = product % sum;
        total += weights[index];
    }
    while (total < 255U)
    {
        uint32_t selected = 0U;
        for (index = 1U; index < HENKA_TERRAIN_ACTIVE_MATERIAL_COUNT; ++index)
        {
            if (remainders[index] > remainders[selected])
            {
                selected = index;
            }
        }
        ++weights[selected];
        remainders[selected] = 0U;
        ++total;
    }
    return HENKA_SUCCESS;
}

henka_terrain_region_record* henka_terrain_find_region_record(
    henka_terrain_world* world,
    henka_terrain_region_id id)
{
    uint32_t index;
    if (world == NULL)
    {
        return NULL;
    }
    for (index = 0U; index < world->desc.max_resident_regions; ++index)
    {
        if (world->regions[index].active &&
            henka_terrain_region_id_matches(world->regions[index].state.id, id))
        {
            return &world->regions[index];
        }
    }
    return NULL;
}

const henka_terrain_region_record* henka_terrain_find_region_record_const(
    const henka_terrain_world* world,
    henka_terrain_region_id id)
{
    return henka_terrain_find_region_record((henka_terrain_world*)world, id);
}

henka_terrain_chunk_record* henka_terrain_find_chunk_record(
    henka_terrain_world* world,
    henka_terrain_chunk_id id)
{
    uint32_t index;
    if (world == NULL)
    {
        return NULL;
    }
    for (index = 0U; index < world->desc.max_resident_chunks; ++index)
    {
        if (world->chunks[index].active &&
            henka_terrain_chunk_id_matches(world->chunks[index].id, id))
        {
            return &world->chunks[index];
        }
    }
    return NULL;
}

henka_terrain_world* henka_terrain_world_allocate(
    const henka_terrain_world_desc* desc,
    const henka_terrain_layout* layout)
{
    henka_terrain_world* world;
    world = henka_calloc(1U, sizeof(*world));
    if (world == NULL)
    {
        return NULL;
    }
    world->desc = *desc;
    world->layout = *layout;
    world->regions = henka_calloc(desc->max_resident_regions, sizeof(*world->regions));
    world->chunks = henka_calloc(desc->max_resident_chunks, sizeof(*world->chunks));
    if (world->regions == NULL || world->chunks == NULL)
    {
        henka_free(world->chunks);
        henka_free(world->regions);
        henka_free(world);
        return NULL;
    }
    return world;
}

henka_result henka_terrain_world_create(
    const henka_terrain_world_desc* desc,
    henka_terrain_world** out_world)
{
    henka_terrain_layout layout;
    henka_terrain_world* world;
    if (out_world == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_world = NULL;
    if (desc == NULL || henka_terrain_world_desc_get_layout(desc, &layout) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    world = henka_terrain_world_allocate(desc, &layout);
    if (world == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    *out_world = world;
    return HENKA_SUCCESS;
}

void henka_terrain_world_destroy(henka_terrain_world* world)
{
    uint32_t index;
    if (world == NULL)
    {
        return;
    }
    for (index = 0U; index < world->desc.max_resident_regions; ++index)
    {
        henka_free(world->regions[index].samples);
    }
    henka_free(world->chunks);
    henka_free(world->regions);
    henka_free(world);
}

henka_result henka_terrain_world_get_desc(
    const henka_terrain_world* world,
    henka_terrain_world_desc* out_desc)
{
    if (world == NULL || out_desc == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_desc = world->desc;
    return HENKA_SUCCESS;
}

henka_result henka_terrain_world_reserve_region(
    henka_terrain_world* world,
    henka_terrain_region_id region_id)
{
    uint32_t index;
    henka_terrain_region_record* record;
    if (world == NULL || !henka_terrain_region_id_is_valid(&world->desc, region_id))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (henka_terrain_find_region_record(world, region_id) != NULL)
    {
        return HENKA_SUCCESS;
    }
    for (index = 0U; index < world->desc.max_resident_regions; ++index)
    {
        if (!world->regions[index].active)
        {
            record = &world->regions[index];
            memset(record, 0, sizeof(*record));
            record->samples = henka_calloc(world->layout.samples_per_region, sizeof(*record->samples));
            if (record->samples == NULL)
            {
                return HENKA_ERROR_OUT_OF_MEMORY;
            }
            record->sample_count = world->layout.samples_per_region;
            for (uint32_t sample_index = 0U; sample_index < record->sample_count; ++sample_index)
            {
                record->samples[sample_index].material_weights[0] = 255U;
            }
            record->active = true;
            record->state.id = region_id;
            record->state.cpu_resident = true;
            ++world->resident_region_count;
            return HENKA_SUCCESS;
        }
    }
    return HENKA_ERROR_LIMIT;
}

henka_result henka_terrain_world_release_region(
    henka_terrain_world* world,
    henka_terrain_region_id region_id)
{
    henka_terrain_region_record* record = henka_terrain_find_region_record(world, region_id);
    if (record == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (record->state.resident_chunk_count != 0U || record->state.pending_io)
    {
        return HENKA_ERROR_LIMIT;
    }
    henka_free(record->samples);
    memset(record, 0, sizeof(*record));
    if (world->resident_region_count > 0U)
    {
        --world->resident_region_count;
    }
    return HENKA_SUCCESS;
}

henka_result henka_terrain_world_reserve_chunk(
    henka_terrain_world* world,
    henka_terrain_chunk_id chunk_id)
{
    uint32_t index;
    henka_terrain_region_id region_id;
    henka_terrain_region_record* region;
    if (world == NULL || !henka_terrain_chunk_id_is_valid(&world->desc, chunk_id) ||
        henka_terrain_region_id_from_chunk(&world->desc, chunk_id, &region_id) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (henka_terrain_find_chunk_record(world, chunk_id) != NULL)
    {
        return HENKA_SUCCESS;
    }
    region = henka_terrain_find_region_record(world, region_id);
    if (region == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    for (index = 0U; index < world->desc.max_resident_chunks; ++index)
    {
        if (!world->chunks[index].active)
        {
            world->chunks[index].active = true;
            world->chunks[index].id = chunk_id;
            ++region->state.resident_chunk_count;
            ++world->resident_chunk_count;
            return HENKA_SUCCESS;
        }
    }
    return HENKA_ERROR_LIMIT;
}

henka_result henka_terrain_world_release_chunk(
    henka_terrain_world* world,
    henka_terrain_chunk_id chunk_id)
{
    henka_terrain_chunk_record* chunk = henka_terrain_find_chunk_record(world, chunk_id);
    henka_terrain_region_id region_id;
    henka_terrain_region_record* region;
    if (chunk == NULL || henka_terrain_region_id_from_chunk(&world->desc, chunk_id, &region_id) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    region = henka_terrain_find_region_record(world, region_id);
    if (region == NULL || region->state.resident_chunk_count == 0U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    memset(chunk, 0, sizeof(*chunk));
    --region->state.resident_chunk_count;
    if (world->resident_chunk_count > 0U)
    {
        --world->resident_chunk_count;
    }
    return HENKA_SUCCESS;
}

henka_result henka_terrain_world_set_region_residency(
    henka_terrain_world* world,
    henka_terrain_region_id region_id,
    bool physics_resident,
    bool render_resident,
    bool pending_io)
{
    henka_terrain_region_record* record = henka_terrain_find_region_record(world, region_id);
    if (record == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (pending_io && !record->state.pending_io)
    {
        if (world->pending_io_count >= world->desc.max_pending_io)
        {
            return HENKA_ERROR_LIMIT;
        }
        ++world->pending_io_count;
    }
    else if (!pending_io && record->state.pending_io && world->pending_io_count > 0U)
    {
        --world->pending_io_count;
    }
    record->state.physics_resident = physics_resident;
    record->state.render_resident = render_resident;
    record->state.pending_io = pending_io;
    return HENKA_SUCCESS;
}

henka_result henka_terrain_world_set_region_revision(
    henka_terrain_world* world,
    henka_terrain_region_id region_id,
    henka_terrain_revision revision,
    henka_terrain_generation generation,
    bool dirty)
{
    henka_terrain_region_record* record = henka_terrain_find_region_record(world, region_id);
    if (record == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    record->state.revision = revision;
    record->state.generation = generation;
    record->state.dirty = dirty;
    return HENKA_SUCCESS;
}

henka_result henka_terrain_world_get_region_state(
    const henka_terrain_world* world,
    henka_terrain_region_id region_id,
    henka_terrain_region_state* out_state)
{
    const henka_terrain_region_record* record =
        henka_terrain_find_region_record_const(world, region_id);
    if (record == NULL || out_state == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_state = record->state;
    return HENKA_SUCCESS;
}

henka_result henka_terrain_world_get_stats(
    const henka_terrain_world* world,
    henka_terrain_world_stats* out_stats)
{
    if (world == NULL || out_stats == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_stats = (henka_terrain_world_stats){
        world->resident_region_count,
        world->resident_chunk_count,
        world->pending_io_count,
        world->desc.max_resident_regions,
        world->desc.max_resident_chunks,
        world->desc.max_pending_io};
    return HENKA_SUCCESS;
}

henka_result henka_terrain_world_apply_region_snapshot(
    henka_terrain_world* world,
    henka_terrain_region_storage_info info,
    const henka_terrain_sample* samples,
    size_t sample_count)
{
    henka_terrain_region_record* record;
    if (world == NULL || samples == NULL || sample_count != world->layout.samples_per_region ||
        !henka_terrain_region_id_is_valid(&world->desc, info.id))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    record = henka_terrain_find_region_record(world, info.id);
    if (record == NULL)
    {
        if (henka_terrain_world_reserve_region(world, info.id) != HENKA_SUCCESS)
        {
            return HENKA_ERROR_LIMIT;
        }
        record = henka_terrain_find_region_record(world, info.id);
    }
    if (record == NULL || record->samples == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    memcpy(record->samples, samples, sample_count * sizeof(*samples));
    record->state.revision = info.revision;
    record->state.generation = info.generation;
    record->state.dirty = false;
    record->state.pending_io = false;
    record->state.cpu_resident = true;
    return HENKA_SUCCESS;
}
