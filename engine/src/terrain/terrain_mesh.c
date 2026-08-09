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

static uint32_t henka_terrain_mesh_transition_vertex(
    uint32_t side,
    uint32_t x,
    uint32_t z,
    uint32_t edge_transition_mask)
{
    (void)edge_transition_mask;
    return henka_terrain_mesh_sample_index(side, x, z);
}

static void henka_terrain_mesh_morph_transition_vertex(
    henka_terrain_mesh_vertex* target,
    const henka_terrain_mesh_vertex* first,
    const henka_terrain_mesh_vertex* second)
{
    uint32_t component;

    if (target == NULL || first == NULL || second == NULL)
    {
        return;
    }
    for (component = 0U; component < 3U; ++component)
    {
        target->position[component] =
            (first->position[component] + second->position[component]) * 0.5f;
        target->normal[component] =
            (first->normal[component] + second->normal[component]) * 0.5f;
    }
    for (component = 0U; component < 4U; ++component)
    {
        target->tangent[component] =
            (first->tangent[component] + second->tangent[component]) * 0.5f;
    }
    target->tangent[3] = fabsf(first->tangent[3]) >= 0.5f
        ? first->tangent[3]
        : second->tangent[3];
    for (component = 0U; component < 2U; ++component)
    {
        target->uv[component] = (first->uv[component] + second->uv[component]) * 0.5f;
    }
    for (component = 0U; component < HENKA_TERRAIN_ACTIVE_MATERIAL_COUNT; ++component)
    {
        target->material_weights[component] = (uint8_t)(
            ((uint16_t)first->material_weights[component] +
                (uint16_t)second->material_weights[component]) / 2U);
    }
}

static float henka_terrain_mesh_height_meters(const henka_terrain_sample* sample)
{
    return (float)sample->height_millimeters / 1000.0F;
}

typedef struct henka_terrain_mesh_region_view
{
    henka_terrain_region_id id;
    const henka_terrain_sample* samples;
} henka_terrain_mesh_region_view;

static const henka_terrain_sample* henka_terrain_mesh_find_world_sample(
    const henka_terrain_mesh_region_view views[3][3],
    henka_terrain_region_id current_region,
    const henka_terrain_sample* current_samples,
    uint32_t region_sample_span,
    uint32_t global_x,
    uint32_t global_z)
{
    uint32_t view_z;
    uint32_t view_x;

    for (view_z = 0U; view_z < 3U; ++view_z)
    {
        for (view_x = 0U; view_x < 3U; ++view_x)
        {
            const henka_terrain_mesh_region_view* view = &views[view_z][view_x];
            uint32_t base_x;
            uint32_t base_z;
            uint32_t local_x;
            uint32_t local_z;
            if (view->samples == NULL)
            {
                continue;
            }
            base_x = (uint32_t)view->id.x * region_sample_span;
            base_z = (uint32_t)view->id.z * region_sample_span;
            if (global_x < base_x || global_z < base_z ||
                global_x > base_x + region_sample_span ||
                global_z > base_z + region_sample_span)
            {
                continue;
            }
            local_x = global_x - base_x;
            local_z = global_z - base_z;
            return &view->samples[henka_terrain_mesh_sample_index(
                region_sample_span + 1U, local_x, local_z)];
        }
    }

    /* A render-resident region may legitimately have no resident neighbor yet.
     * Keep that chunk renderable while using the neighbor whenever it exists. */
    {
        uint32_t current_base_x = (uint32_t)current_region.x * region_sample_span;
        uint32_t current_base_z = (uint32_t)current_region.z * region_sample_span;
        uint32_t local_x = global_x < current_base_x ? 0U : global_x - current_base_x;
        uint32_t local_z = global_z < current_base_z ? 0U : global_z - current_base_z;
        if (local_x > region_sample_span) local_x = region_sample_span;
        if (local_z > region_sample_span) local_z = region_sample_span;
        return &current_samples[henka_terrain_mesh_sample_index(
            region_sample_span + 1U, local_x, local_z)];
    }
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

henka_result henka_terrain_mesh_build_chunk_with_edge_mask(
    const henka_terrain_world* world,
    henka_terrain_chunk_id chunk_id,
    uint32_t lod_level,
    uint32_t edge_transition_mask,
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
    henka_terrain_mesh_region_view region_views[3][3] = {{{0}}};

    if (world == NULL || io_mesh == NULL ||
        (edge_transition_mask & ~HENKA_TERRAIN_MESH_EDGE_ALL) != 0U ||
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
        int32_t view_z;
        int32_t view_x;
        for (view_z = -1; view_z <= 1; ++view_z)
        {
            for (view_x = -1; view_x <= 1; ++view_x)
            {
                henka_terrain_region_id view_id = {
                    region_id.x + view_x, region_id.z + view_z};
                if (henka_terrain_region_id_is_valid(&desc, view_id))
                {
                    size_t view_sample_count = 0U;
                    region_views[view_z + 1][view_x + 1].id = view_id;
                    if (henka_terrain_world_get_region_samples(
                        world,
                        view_id,
                        &region_views[view_z + 1][view_x + 1].samples,
                        &view_sample_count) != HENKA_SUCCESS ||
                        view_sample_count != layout.samples_per_region)
                    {
                        region_views[view_z + 1][view_x + 1].samples = NULL;
                    }
                }
            }
        }
    }
    {
        uint32_t samples_per_side = chunk_sample_span / step + 1U;
        uint32_t region_sample_span = region_sample_edge - 1U;
        uint32_t world_sample_x_max = desc.world_width_meters / desc.base_sample_spacing_meters;
        uint32_t world_sample_z_max = desc.world_depth_meters / desc.base_sample_spacing_meters;
        for (local_z = 0U; local_z < samples_per_side; ++local_z)
        {
            for (local_x = 0U; local_x < samples_per_side; ++local_x)
            {
                uint32_t sample_x = local_x * step;
                uint32_t sample_z = local_z * step;
                uint32_t source_x = (uint32_t)chunk_id.x * chunk_sample_span + sample_x;
                uint32_t source_z = (uint32_t)chunk_id.z * chunk_sample_span + sample_z;
                uint32_t left_x = source_x > step ? source_x - step : 0U;
                uint32_t right_x = source_x + step < world_sample_x_max ? source_x + step : world_sample_x_max;
                uint32_t down_z = source_z > step ? source_z - step : 0U;
                uint32_t up_z = source_z + step < world_sample_z_max ? source_z + step : world_sample_z_max;
                float height;
                float left_height;
                float right_height;
                float down_height;
                float up_height;
                float normal_x;
                float normal_y;
                float normal_z;
                float normal_length;
                float tangent_x;
                float tangent_y;
                float tangent_z;
                float tangent_dot_normal;
                float tangent_length;
                float tangent_handedness;
                const henka_terrain_sample* source;
                const henka_terrain_sample* left_sample = henka_terrain_mesh_find_world_sample(
                    region_views, region_id, samples, region_sample_span, left_x, source_z);
                const henka_terrain_sample* right_sample = henka_terrain_mesh_find_world_sample(
                    region_views, region_id, samples, region_sample_span, right_x, source_z);
                const henka_terrain_sample* down_sample = henka_terrain_mesh_find_world_sample(
                    region_views, region_id, samples, region_sample_span, source_x, down_z);
                const henka_terrain_sample* up_sample = henka_terrain_mesh_find_world_sample(
                    region_views, region_id, samples, region_sample_span, source_x, up_z);
                source = henka_terrain_mesh_find_world_sample(
                    region_views, region_id, samples, region_sample_span, source_x, source_z);
                if (source == NULL || left_sample == NULL || right_sample == NULL ||
                    down_sample == NULL || up_sample == NULL ||
                    !isfinite((height = henka_terrain_mesh_height_meters(source))) ||
                    !isfinite((left_height = henka_terrain_mesh_height_meters(left_sample))) ||
                    !isfinite((right_height = henka_terrain_mesh_height_meters(right_sample))) ||
                    !isfinite((down_height = henka_terrain_mesh_height_meters(down_sample))) ||
                    !isfinite((up_height = henka_terrain_mesh_height_meters(up_sample))))
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
                normal_x /= normal_length;
                normal_y /= normal_length;
                normal_z /= normal_length;
                tangent_x = 2.0f * (float)step * (float)desc.base_sample_spacing_meters;
                tangent_y = right_height - left_height;
                tangent_z = 0.0f;
                tangent_dot_normal = tangent_x * normal_x + tangent_y * normal_y;
                tangent_x -= tangent_dot_normal * normal_x;
                tangent_y -= tangent_dot_normal * normal_y;
                tangent_z -= tangent_dot_normal * normal_z;
                tangent_length = sqrtf(tangent_x * tangent_x + tangent_y * tangent_y + tangent_z * tangent_z);
                if (!isfinite(tangent_length) || tangent_length <= 0.0F)
                {
                    tangent_x = fabsf(normal_y) > 0.5f ? 1.0f : 0.0f;
                    tangent_y = 0.0f;
                    tangent_z = fabsf(normal_y) > 0.5f ? 0.0f : 1.0f;
                    tangent_dot_normal = tangent_x * normal_x + tangent_y * normal_y + tangent_z * normal_z;
                    tangent_x -= tangent_dot_normal * normal_x;
                    tangent_y -= tangent_dot_normal * normal_y;
                    tangent_z -= tangent_dot_normal * normal_z;
                    tangent_length = sqrtf(tangent_x * tangent_x + tangent_y * tangent_y + tangent_z * tangent_z);
                }
                if (!isfinite(tangent_length) || tangent_length <= 0.0F)
                {
                    return HENKA_ERROR_NUMERIC_RANGE;
                }
                tangent_x /= tangent_length;
                tangent_y /= tangent_length;
                tangent_z /= tangent_length;
                tangent_handedness = normal_z * tangent_x - normal_x * tangent_z < 0.0f ? 1.0f : -1.0f;
                io_mesh->vertices[vertex_index++] = (henka_terrain_mesh_vertex){
                    {(float)(chunk_id.x * (int32_t)desc.chunk_edge_meters) +
                        (float)(sample_x * desc.base_sample_spacing_meters),
                        height,
                        (float)(chunk_id.z * (int32_t)desc.chunk_edge_meters) +
                        (float)(sample_z * desc.base_sample_spacing_meters)},
                    {normal_x, normal_y, normal_z},
                    {tangent_x, tangent_y, tangent_z, tangent_handedness},
                    {(float)(chunk_id.x * desc.chunk_edge_meters + sample_x * desc.base_sample_spacing_meters) /
                        (float)desc.world_width_meters,
                     (float)(chunk_id.z * desc.chunk_edge_meters + sample_z * desc.base_sample_spacing_meters) /
                        (float)desc.world_depth_meters},
                    {source->material_weights[0], source->material_weights[1],
                     source->material_weights[2], source->material_weights[3]}};
            }
        }
        for (local_z = 0U; local_z < samples_per_side; ++local_z)
        {
            for (local_x = 0U; local_x < samples_per_side; ++local_x)
            {
                uint32_t previous_index;
                uint32_t next_index;
                henka_terrain_mesh_vertex* target;

                if ((local_z == 0U &&
                        (edge_transition_mask & HENKA_TERRAIN_MESH_EDGE_NORTH) != 0U &&
                        (local_x & 1U) != 0U && local_x + 1U < samples_per_side) ||
                    (local_z + 1U == samples_per_side &&
                        (edge_transition_mask & HENKA_TERRAIN_MESH_EDGE_SOUTH) != 0U &&
                        (local_x & 1U) != 0U && local_x + 1U < samples_per_side))
                {
                    previous_index = henka_terrain_mesh_sample_index(
                        samples_per_side, local_x - 1U, local_z);
                    next_index = henka_terrain_mesh_sample_index(
                        samples_per_side, local_x + 1U, local_z);
                }
                else if ((local_x == 0U &&
                            (edge_transition_mask & HENKA_TERRAIN_MESH_EDGE_WEST) != 0U &&
                            (local_z & 1U) != 0U && local_z + 1U < samples_per_side) ||
                    (local_x + 1U == samples_per_side &&
                        (edge_transition_mask & HENKA_TERRAIN_MESH_EDGE_EAST) != 0U &&
                        (local_z & 1U) != 0U && local_z + 1U < samples_per_side))
                {
                    previous_index = henka_terrain_mesh_sample_index(
                        samples_per_side, local_x, local_z - 1U);
                    next_index = henka_terrain_mesh_sample_index(
                        samples_per_side, local_x, local_z + 1U);
                }
                else
                {
                    continue;
                }
                target = &io_mesh->vertices[
                    henka_terrain_mesh_sample_index(samples_per_side, local_x, local_z)];
                henka_terrain_mesh_morph_transition_vertex(
                    target,
                    &io_mesh->vertices[previous_index],
                    &io_mesh->vertices[next_index]);
            }
        }
        for (local_z = 0U; local_z + 1U < samples_per_side; ++local_z)
        {
            for (local_x = 0U; local_x + 1U < samples_per_side; ++local_x)
            {
                uint32_t first = henka_terrain_mesh_transition_vertex(
                    samples_per_side, local_x, local_z, edge_transition_mask);
                uint32_t second = henka_terrain_mesh_transition_vertex(
                    samples_per_side, local_x, local_z + 1U, edge_transition_mask);
                uint32_t third = henka_terrain_mesh_transition_vertex(
                    samples_per_side, local_x + 1U, local_z, edge_transition_mask);
                uint32_t fourth = henka_terrain_mesh_transition_vertex(
                    samples_per_side, local_x + 1U, local_z + 1U, edge_transition_mask);
                io_mesh->indices[index_index++] = first;
                io_mesh->indices[index_index++] = second;
                io_mesh->indices[index_index++] = third;
                io_mesh->indices[index_index++] = second;
                io_mesh->indices[index_index++] = fourth;
                io_mesh->indices[index_index++] = third;
            }
        }
    }
    return HENKA_SUCCESS;
}

henka_result henka_terrain_mesh_build_chunk(
    const henka_terrain_world* world,
    henka_terrain_chunk_id chunk_id,
    uint32_t lod_level,
    henka_terrain_mesh_data* io_mesh)
{
    return henka_terrain_mesh_build_chunk_with_edge_mask(
        world, chunk_id, lod_level, 0U, io_mesh);
}
