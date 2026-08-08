#include <henka/terrain_mesh.h>

#include <math.h>

#include <henka/terrain_edit.h>

static uint32_t henka_terrain_mesh_sample_index(
    uint32_t edge,
    uint32_t x,
    uint32_t z)
{
    return z * edge + x;
}

static float henka_terrain_mesh_height_meters(const henka_terrain_sample* sample)
{
    return (float)sample->height_millimeters / 1000.0F;
}

static henka_result henka_terrain_mesh_get_sample(
    const henka_terrain_sample* samples,
    uint32_t edge,
    uint32_t x,
    uint32_t z,
    float* out_height)
{
    if (samples == NULL || out_height == NULL || x >= edge || z >= edge)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_height = henka_terrain_mesh_height_meters(
        &samples[henka_terrain_mesh_sample_index(edge, x, z)]);
    return isfinite(*out_height) ? HENKA_SUCCESS : HENKA_ERROR_NUMERIC_RANGE;
}

static henka_result henka_terrain_mesh_required_counts(
    const henka_terrain_world_desc* desc,
    uint32_t lod_level,
    uint32_t* out_vertices,
    uint32_t* out_indices)
{
    uint32_t samples_per_side;
    uint64_t vertices;
    uint64_t indices;
    if (desc == NULL || out_vertices == NULL || out_indices == NULL ||
        lod_level > HENKA_TERRAIN_MESH_MAX_LOD_LEVEL ||
        desc->samples_per_chunk < 2U ||
        (desc->samples_per_chunk - 1U) % (1U << lod_level) != 0U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    samples_per_side = (desc->samples_per_chunk - 1U) / (1U << lod_level) + 1U;
    vertices = (uint64_t)samples_per_side * samples_per_side;
    indices = (uint64_t)(samples_per_side - 1U) * (samples_per_side - 1U) * 6U;
    if (vertices > UINT32_MAX || indices > UINT32_MAX)
    {
        return HENKA_ERROR_NUMERIC_RANGE;
    }
    *out_vertices = (uint32_t)vertices;
    *out_indices = (uint32_t)indices;
    return HENKA_SUCCESS;
}

henka_result henka_terrain_mesh_build_chunk(
    const henka_terrain_world* world,
    henka_terrain_chunk_id chunk_id,
    uint32_t lod_level,
    henka_terrain_mesh_data* io_mesh)
{
    henka_terrain_world_desc desc;
    henka_terrain_layout layout;
    henka_terrain_region_id region_id;
    henka_terrain_region_state region_state;
    const henka_terrain_sample* samples;
    size_t sample_count;
    uint32_t required_vertices;
    uint32_t required_indices;
    uint32_t region_sample_edge;
    uint32_t chunk_sample_span;
    uint32_t step;
    uint32_t vertex_index = 0U;
    uint32_t index_index = 0U;
    uint32_t local_x;
    uint32_t local_z;

    if (world == NULL || io_mesh == NULL ||
        henka_terrain_world_get_desc(world, &desc) != HENKA_SUCCESS ||
        !henka_terrain_chunk_id_is_valid(&desc, chunk_id) ||
        henka_terrain_world_desc_get_layout(&desc, &layout) != HENKA_SUCCESS ||
        henka_terrain_region_id_from_chunk(&desc, chunk_id, &region_id) != HENKA_SUCCESS ||
        henka_terrain_world_get_region_state(world, region_id, &region_state) != HENKA_SUCCESS ||
        !region_state.render_resident ||
        henka_terrain_world_get_region_samples(world, region_id, &samples, &sample_count) != HENKA_SUCCESS ||
        sample_count != layout.samples_per_region ||
        henka_terrain_mesh_required_counts(
            &desc, lod_level, &required_vertices, &required_indices) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    io_mesh->chunk_id = chunk_id;
    io_mesh->lod_level = lod_level;
    io_mesh->revision = region_state.revision;
    io_mesh->generation = region_state.generation;
    io_mesh->vertex_count = required_vertices;
    io_mesh->index_count = required_indices;
    if ((required_vertices > 0U && io_mesh->vertices == NULL) ||
        (required_indices > 0U && io_mesh->indices == NULL) ||
        io_mesh->vertex_capacity < required_vertices || io_mesh->index_capacity < required_indices)
    {
        return HENKA_ERROR_LIMIT;
    }

    region_sample_edge = layout.samples_per_region_edge;
    chunk_sample_span = desc.samples_per_chunk - 1U;
    step = 1U << lod_level;
    {
        uint32_t region_chunk_x = (uint32_t)chunk_id.x % desc.chunks_per_region_edge;
        uint32_t region_chunk_z = (uint32_t)chunk_id.z % desc.chunks_per_region_edge;
        uint32_t base_x = region_chunk_x * chunk_sample_span;
        uint32_t base_z = region_chunk_z * chunk_sample_span;
        uint32_t samples_per_side = chunk_sample_span / step + 1U;
        for (local_z = 0U; local_z < samples_per_side; ++local_z)
        {
            for (local_x = 0U; local_x < samples_per_side; ++local_x)
            {
                uint32_t sample_x = local_x * step;
                uint32_t sample_z = local_z * step;
                uint32_t source_x = base_x + sample_x;
                uint32_t source_z = base_z + sample_z;
                uint32_t left_x = sample_x > 0U ? sample_x - step : sample_x;
                uint32_t right_x = sample_x + step <= chunk_sample_span ? sample_x + step : sample_x;
                uint32_t down_z = sample_z > 0U ? sample_z - step : sample_z;
                uint32_t up_z = sample_z + step <= chunk_sample_span ? sample_z + step : sample_z;
                float height;
                float left_height;
                float right_height;
                float down_height;
                float up_height;
                float normal_x;
                float normal_y;
                float normal_z;
                float normal_length;
                const henka_terrain_sample* source;
                if (henka_terrain_mesh_get_sample(samples, region_sample_edge, source_x, source_z, &height) != HENKA_SUCCESS ||
                    henka_terrain_mesh_get_sample(samples, region_sample_edge, base_x + left_x, source_z, &left_height) != HENKA_SUCCESS ||
                    henka_terrain_mesh_get_sample(samples, region_sample_edge, base_x + right_x, source_z, &right_height) != HENKA_SUCCESS ||
                    henka_terrain_mesh_get_sample(samples, region_sample_edge, source_x, base_z + down_z, &down_height) != HENKA_SUCCESS ||
                    henka_terrain_mesh_get_sample(samples, region_sample_edge, source_x, base_z + up_z, &up_height) != HENKA_SUCCESS)
                {
                    return HENKA_ERROR_NUMERIC_RANGE;
                }
                normal_x = left_height - right_height;
                normal_y = (float)(2U * step) * (float)desc.base_sample_spacing_meters;
                normal_z = down_height - up_height;
                normal_length = sqrtf(normal_x * normal_x + normal_y * normal_y + normal_z * normal_z);
                if (!isfinite(normal_length) || normal_length <= 0.0F)
                {
                    return HENKA_ERROR_NUMERIC_RANGE;
                }
                source = &samples[henka_terrain_mesh_sample_index(
                    region_sample_edge, source_x, source_z)];
                io_mesh->vertices[vertex_index++] = (henka_terrain_mesh_vertex){
                    {(float)(chunk_id.x * (int32_t)desc.chunk_edge_meters) +
                        (float)(sample_x * desc.base_sample_spacing_meters),
                        height,
                        (float)(chunk_id.z * (int32_t)desc.chunk_edge_meters) +
                        (float)(sample_z * desc.base_sample_spacing_meters)},
                    {normal_x / normal_length, normal_y / normal_length, normal_z / normal_length},
                    {(float)(chunk_id.x * desc.chunk_edge_meters + sample_x * desc.base_sample_spacing_meters) /
                        (float)desc.world_width_meters,
                     (float)(chunk_id.z * desc.chunk_edge_meters + sample_z * desc.base_sample_spacing_meters) /
                        (float)desc.world_depth_meters},
                    {source->material_weights[0], source->material_weights[1],
                     source->material_weights[2], source->material_weights[3]}};
            }
        }
        for (local_z = 0U; local_z + 1U < samples_per_side; ++local_z)
        {
            for (local_x = 0U; local_x + 1U < samples_per_side; ++local_x)
            {
                uint32_t first = local_z * samples_per_side + local_x;
                uint32_t second = first + samples_per_side;
                io_mesh->indices[index_index++] = first;
                io_mesh->indices[index_index++] = second;
                io_mesh->indices[index_index++] = first + 1U;
                io_mesh->indices[index_index++] = second;
                io_mesh->indices[index_index++] = second + 1U;
                io_mesh->indices[index_index++] = first + 1U;
            }
        }
    }
    return HENKA_SUCCESS;
}
