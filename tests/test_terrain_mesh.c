#include <math.h>
#include <string.h>

#include <henka/memory.h>
#include <henka/terrain_edit.h>
#include <henka/terrain_mesh.h>

static int test_chunk_mesh_lod_and_identity(void)
{
    henka_terrain_world_desc desc = henka_terrain_world_desc_default();
    henka_terrain_layout layout;
    henka_terrain_world* world = NULL;
    henka_terrain_mesh_vertex vertices[HENKA_TERRAIN_MESH_MAX_VERTICES];
    uint32_t indices[HENKA_TERRAIN_MESH_MAX_INDICES];
    henka_terrain_mesh_data mesh = {0};
    henka_terrain_region_state state;
    int result = 0;

    desc.max_resident_regions = 1U;
    if (henka_terrain_world_desc_get_layout(&desc, &layout) != HENKA_SUCCESS ||
        henka_terrain_world_create(&desc, &world) != HENKA_SUCCESS ||
        henka_terrain_world_reserve_region(world, (henka_terrain_region_id){0, 0}) != HENKA_SUCCESS ||
        henka_terrain_world_set_region_residency(
            world, (henka_terrain_region_id){0, 0}, false, true, false) != HENKA_SUCCESS ||
        henka_terrain_world_set_region_revision(
            world, (henka_terrain_region_id){0, 0}, 12U, 4U, false) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    mesh.vertices = vertices;
    mesh.vertex_capacity = HENKA_TERRAIN_MESH_MAX_VERTICES;
    mesh.indices = indices;
    mesh.index_capacity = HENKA_TERRAIN_MESH_MAX_INDICES;
    if (henka_terrain_mesh_build_chunk(
            world, (henka_terrain_chunk_id){0, 0}, 2U, &mesh) != HENKA_SUCCESS ||
        mesh.vertex_count != 289U || mesh.index_count != 1536U ||
        mesh.revision != 12U || mesh.generation != 4U ||
        !isfinite(mesh.vertices[0].normal[0]) ||
        !isfinite(mesh.vertices[0].normal[1]) ||
        !isfinite(mesh.vertices[0].normal[2]) ||
        mesh.vertices[0].material_weights[0] != 255U ||
        mesh.vertices[0].material_weights[1] != 0U ||
        mesh.indices[0] != 0U || mesh.indices[1] != 17U || mesh.indices[2] != 1U)
    {
        goto cleanup;
    }
    if (henka_terrain_world_get_region_state(world, (henka_terrain_region_id){0, 0}, &state) != HENKA_SUCCESS ||
        state.revision != 12U || state.generation != 4U)
    {
        goto cleanup;
    }
    result = 1;

cleanup:
    henka_terrain_world_destroy(world);
    return result;
}

static int test_chunk_mesh_tangent_basis(void)
{
    henka_terrain_world_desc desc = henka_terrain_world_desc_default();
    henka_terrain_world* world = NULL;
    henka_terrain_mesh_vertex vertices[HENKA_TERRAIN_MESH_MAX_VERTICES];
    uint32_t indices[HENKA_TERRAIN_MESH_MAX_INDICES];
    henka_terrain_mesh_data mesh = {0};
    const henka_terrain_mesh_vertex* vertex;
    float tangent_dot_normal;
    int result = 0;

    desc.max_resident_regions = 1U;
    if (henka_terrain_world_create(&desc, &world) != HENKA_SUCCESS ||
        henka_terrain_world_reserve_region(world, (henka_terrain_region_id){0, 0}) != HENKA_SUCCESS ||
        henka_terrain_world_set_region_residency(
            world, (henka_terrain_region_id){0, 0}, false, true, false) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    mesh.vertices = vertices;
    mesh.vertex_capacity = HENKA_TERRAIN_MESH_MAX_VERTICES;
    mesh.indices = indices;
    mesh.index_capacity = HENKA_TERRAIN_MESH_MAX_INDICES;
    if (henka_terrain_mesh_build_chunk(
            world, (henka_terrain_chunk_id){0, 0}, 0U, &mesh) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    vertex = &mesh.vertices[0];
    tangent_dot_normal = vertex->normal[0] * vertex->tangent[0] +
        vertex->normal[1] * vertex->tangent[1] +
        vertex->normal[2] * vertex->tangent[2];
    if (!isfinite(vertex->tangent[0]) || !isfinite(vertex->tangent[1]) ||
        !isfinite(vertex->tangent[2]) || !isfinite(vertex->tangent[3]) ||
        fabsf(vertex->tangent[3]) < 0.5f ||
        !isfinite(tangent_dot_normal) || fabsf(tangent_dot_normal) > 0.001f ||
        !isfinite(vertex->uv[0]) || !isfinite(vertex->uv[1]))
    {
        goto cleanup;
    }
    result = 1;

cleanup:
    henka_terrain_world_destroy(world);
    return result;
}

static int test_neighbor_border_normals(void)
{
    henka_terrain_world_desc desc = henka_terrain_world_desc_default();
    henka_terrain_layout layout;
    henka_terrain_world* world = NULL;
    henka_terrain_sample* neighbor_samples = NULL;
    henka_terrain_mesh_vertex left_vertices[HENKA_TERRAIN_MESH_MAX_VERTICES];
    henka_terrain_mesh_vertex right_vertices[HENKA_TERRAIN_MESH_MAX_VERTICES];
    uint32_t left_indices[HENKA_TERRAIN_MESH_MAX_INDICES];
    uint32_t right_indices[HENKA_TERRAIN_MESH_MAX_INDICES];
    henka_terrain_mesh_data left_mesh = {0};
    henka_terrain_mesh_data right_mesh = {0};
    size_t sample_index;
    uint32_t local_z;
    int result = 0;

    desc.max_resident_regions = 2U;
    if (henka_terrain_world_desc_get_layout(&desc, &layout) != HENKA_SUCCESS ||
        henka_terrain_world_create(&desc, &world) != HENKA_SUCCESS ||
        henka_terrain_world_reserve_region(world, (henka_terrain_region_id){0, 0}) != HENKA_SUCCESS ||
        henka_terrain_world_reserve_region(world, (henka_terrain_region_id){1, 0}) != HENKA_SUCCESS ||
        henka_terrain_world_set_region_residency(world, (henka_terrain_region_id){0, 0}, false, true, false) != HENKA_SUCCESS ||
        henka_terrain_world_set_region_residency(world, (henka_terrain_region_id){1, 0}, false, true, false) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    neighbor_samples = henka_calloc(layout.samples_per_region, sizeof(*neighbor_samples));
    if (neighbor_samples == NULL)
    {
        goto cleanup;
    }
    for (sample_index = 0U; sample_index < layout.samples_per_region; ++sample_index)
    {
        neighbor_samples[sample_index].material_weights[0] = 255U;
    }
    for (local_z = 0U; local_z < layout.samples_per_region_edge; ++local_z)
    {
        neighbor_samples[local_z * layout.samples_per_region_edge].height_millimeters = 1000;
        neighbor_samples[local_z * layout.samples_per_region_edge + 1U].height_millimeters = 1000;
    }
    if (henka_terrain_world_apply_region_snapshot(
            world,
            (henka_terrain_region_storage_info){{1, 0}, 2U, 1U},
            neighbor_samples,
            layout.samples_per_region) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    left_mesh.vertices = left_vertices;
    left_mesh.vertex_capacity = HENKA_TERRAIN_MESH_MAX_VERTICES;
    left_mesh.indices = left_indices;
    left_mesh.index_capacity = HENKA_TERRAIN_MESH_MAX_INDICES;
    right_mesh.vertices = right_vertices;
    right_mesh.vertex_capacity = HENKA_TERRAIN_MESH_MAX_VERTICES;
    right_mesh.indices = right_indices;
    right_mesh.index_capacity = HENKA_TERRAIN_MESH_MAX_INDICES;
    if (henka_terrain_mesh_build_chunk(world, (henka_terrain_chunk_id){7, 0}, 0U, &left_mesh) != HENKA_SUCCESS ||
        henka_terrain_mesh_build_chunk(world, (henka_terrain_chunk_id){8, 0}, 0U, &right_mesh) != HENKA_SUCCESS ||
        fabsf(left_vertices[64U].normal[0]) < 0.05f ||
        fabsf(right_vertices[0U].normal[0]) < 0.05f ||
        fabsf(left_vertices[64U].normal[0] - right_vertices[0U].normal[0]) > 0.001f)
    {
        goto cleanup;
    }
    result = 1;

cleanup:
    henka_free(neighbor_samples);
    henka_terrain_world_destroy(world);
    return result;
}

static int test_transition_topology(void)
{
    henka_terrain_world_desc desc = henka_terrain_world_desc_default();
    henka_terrain_layout layout;
    henka_terrain_world* world = NULL;
    henka_terrain_sample* samples = NULL;
    henka_terrain_mesh_vertex base_vertices[HENKA_TERRAIN_MESH_MAX_VERTICES];
    henka_terrain_mesh_vertex vertices[HENKA_TERRAIN_MESH_MAX_VERTICES];
    henka_terrain_mesh_vertex neighbor_vertices[HENKA_TERRAIN_MESH_MAX_VERTICES];
    uint32_t base_indices[HENKA_TERRAIN_MESH_MAX_INDICES];
    uint32_t indices[HENKA_TERRAIN_MESH_MAX_INDICES];
    uint32_t neighbor_indices[HENKA_TERRAIN_MESH_MAX_INDICES];
    henka_terrain_mesh_data mesh = {0};
    henka_terrain_mesh_data base_mesh = {0};
    henka_terrain_mesh_data neighbor_mesh = {0};
    uint32_t index;
    uint32_t z;
    int result = 0;

    desc.max_resident_regions = 1U;
    if (henka_terrain_world_desc_get_layout(&desc, &layout) != HENKA_SUCCESS ||
        henka_terrain_world_create(&desc, &world) != HENKA_SUCCESS ||
        henka_terrain_world_reserve_region(world, (henka_terrain_region_id){0, 0}) != HENKA_SUCCESS ||
        henka_terrain_world_set_region_residency(
            world, (henka_terrain_region_id){0, 0}, false, true, false) != HENKA_SUCCESS)
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
    for (z = 0U; z < desc.samples_per_chunk; ++z)
    {
        samples[z * layout.samples_per_region_edge + 64U].height_millimeters =
            (z & 1U) != 0U ? 1000 : 0;
    }
    if (henka_terrain_world_apply_region_snapshot(
            world,
            (henka_terrain_region_storage_info){{0, 0}, 2U, 1U},
            samples,
            layout.samples_per_region) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    mesh.vertices = vertices;
    mesh.vertex_capacity = HENKA_TERRAIN_MESH_MAX_VERTICES;
    mesh.indices = indices;
    mesh.index_capacity = HENKA_TERRAIN_MESH_MAX_INDICES;
    base_mesh.vertices = base_vertices;
    base_mesh.vertex_capacity = HENKA_TERRAIN_MESH_MAX_VERTICES;
    base_mesh.indices = base_indices;
    base_mesh.index_capacity = HENKA_TERRAIN_MESH_MAX_INDICES;
    neighbor_mesh.vertices = neighbor_vertices;
    neighbor_mesh.vertex_capacity = HENKA_TERRAIN_MESH_MAX_VERTICES;
    neighbor_mesh.indices = neighbor_indices;
    neighbor_mesh.index_capacity = HENKA_TERRAIN_MESH_MAX_INDICES;
    if (henka_terrain_mesh_build_chunk(
            world,
            (henka_terrain_chunk_id){0, 0},
            0U,
            &base_mesh) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    if (henka_terrain_mesh_build_chunk_with_edge_mask(
            world,
            (henka_terrain_chunk_id){0, 0},
            0U,
            HENKA_TERRAIN_MESH_EDGE_EAST,
            &mesh) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    if (mesh.index_count >= base_mesh.index_count)
    {
        goto cleanup;
    }
    if (henka_terrain_mesh_build_chunk(
            world,
            (henka_terrain_chunk_id){1, 0},
            1U,
            &neighbor_mesh) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    for (z = 1U; z < desc.samples_per_chunk - 1U; z += 2U)
    {
        const float expected_height =
            (vertices[(z - 1U) * desc.samples_per_chunk + 64U].position[1] +
                vertices[(z + 1U) * desc.samples_per_chunk + 64U].position[1]) *
            0.5f;
        if (fabsf(vertices[z * desc.samples_per_chunk + 64U].position[1] - expected_height) >
                0.0001f ||
            fabsf(vertices[z * desc.samples_per_chunk + 64U].position[1] -
                neighbor_vertices[(z + 1U) / 2U * 33U].position[1]) > 0.0001f)
        {
            goto cleanup;
        }
    }
    for (index = 0U; index < mesh.index_count; index += 3U)
    {
        if (mesh.indices[index] == mesh.indices[index + 1U] ||
            mesh.indices[index] == mesh.indices[index + 2U] ||
            mesh.indices[index + 1U] == mesh.indices[index + 2U])
        {
            goto cleanup;
        }
    }
    for (index = 0U; index < mesh.vertex_count; ++index)
    {
        if (!isfinite(mesh.vertices[index].normal[0]) ||
            !isfinite(mesh.vertices[index].normal[1]) ||
            !isfinite(mesh.vertices[index].normal[2]) ||
            !isfinite(mesh.vertices[index].tangent[0]) ||
            !isfinite(mesh.vertices[index].tangent[1]) ||
            !isfinite(mesh.vertices[index].tangent[2]) ||
            !isfinite(mesh.vertices[index].tangent[3]) ||
            fabsf(mesh.vertices[index].tangent[3]) < 0.5f)
        {
            goto cleanup;
        }
    }
    if (henka_terrain_mesh_build_chunk_with_edge_mask(
            world,
            (henka_terrain_chunk_id){0, 0},
            0U,
            HENKA_TERRAIN_MESH_EDGE_NORTH |
                HENKA_TERRAIN_MESH_EDGE_WEST,
            &mesh) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    for (index = 0U; index < mesh.index_count; ++index)
    {
        if (mesh.indices[index] >= mesh.vertex_count)
        {
            goto cleanup;
        }
    }
    if (henka_terrain_mesh_build_chunk_with_edge_mask(
            world,
            (henka_terrain_chunk_id){0, 0},
            0U,
            HENKA_TERRAIN_MESH_EDGE_NORTH |
                HENKA_TERRAIN_MESH_EDGE_SOUTH |
                HENKA_TERRAIN_MESH_EDGE_EAST |
                HENKA_TERRAIN_MESH_EDGE_WEST,
            &mesh) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    for (index = 0U; index < mesh.index_count; index += 3U)
    {
        if (mesh.indices[index] == mesh.indices[index + 1U] ||
            mesh.indices[index] == mesh.indices[index + 2U] ||
            mesh.indices[index + 1U] == mesh.indices[index + 2U])
        {
            goto cleanup;
        }
    }
    for (index = 0U; index < mesh.vertex_count; ++index)
    {
        if (!isfinite(mesh.vertices[index].position[0]) ||
            !isfinite(mesh.vertices[index].position[1]) ||
            !isfinite(mesh.vertices[index].position[2]) ||
            !isfinite(mesh.vertices[index].normal[0]) ||
            !isfinite(mesh.vertices[index].normal[1]) ||
            !isfinite(mesh.vertices[index].normal[2]) ||
            !isfinite(mesh.vertices[index].tangent[0]) ||
            !isfinite(mesh.vertices[index].tangent[1]) ||
            !isfinite(mesh.vertices[index].tangent[2]) ||
            !isfinite(mesh.vertices[index].tangent[3]) ||
            fabsf(mesh.vertices[index].tangent[3]) < 0.5f)
        {
            goto cleanup;
        }
    }
    for (index = 0U; index < mesh.vertex_count; ++index)
    {
        const henka_terrain_mesh_vertex* vertex = &mesh.vertices[index];
        const float normal_length = sqrtf(
            vertex->normal[0] * vertex->normal[0] +
            vertex->normal[1] * vertex->normal[1] +
            vertex->normal[2] * vertex->normal[2]);
        const float tangent_length = sqrtf(
            vertex->tangent[0] * vertex->tangent[0] +
            vertex->tangent[1] * vertex->tangent[1] +
            vertex->tangent[2] * vertex->tangent[2]);
        const float tangent_dot_normal =
            vertex->normal[0] * vertex->tangent[0] +
            vertex->normal[1] * vertex->tangent[1] +
            vertex->normal[2] * vertex->tangent[2];
        uint32_t weight_sum = 0U;
        uint32_t component;

        for (component = 0U; component < HENKA_TERRAIN_ACTIVE_MATERIAL_COUNT; ++component)
        {
            weight_sum += vertex->material_weights[component];
        }
        if (!isfinite(normal_length) || fabsf(normal_length - 1.0f) > 0.001f ||
            !isfinite(tangent_length) || fabsf(tangent_length - 1.0f) > 0.001f ||
            !isfinite(tangent_dot_normal) || fabsf(tangent_dot_normal) > 0.001f ||
            weight_sum != 255U)
        {
            goto cleanup;
        }
    }
    for (z = 1U; z < desc.samples_per_chunk - 1U; z += 2U)
    {
        const uint32_t west_index = z * desc.samples_per_chunk;
        const uint32_t east_index = west_index + desc.samples_per_chunk - 1U;
        const float west_expected = (base_vertices[west_index - desc.samples_per_chunk].position[1] +
            base_vertices[west_index + desc.samples_per_chunk].position[1]) * 0.5f;
        const float east_expected = (base_vertices[east_index - desc.samples_per_chunk].position[1] +
            base_vertices[east_index + desc.samples_per_chunk].position[1]) * 0.5f;
        const uint32_t north_index = z;
        const uint32_t south_index = (desc.samples_per_chunk - 1U) * desc.samples_per_chunk + z;
        const float north_expected = (base_vertices[north_index - 1U].position[1] +
            base_vertices[north_index + 1U].position[1]) * 0.5f;
        const float south_expected = (base_vertices[south_index - 1U].position[1] +
            base_vertices[south_index + 1U].position[1]) * 0.5f;
        if (fabsf(vertices[west_index].position[1] - west_expected) > 0.0001f ||
            fabsf(vertices[east_index].position[1] - east_expected) > 0.0001f ||
            fabsf(vertices[north_index].position[1] - north_expected) > 0.0001f ||
            fabsf(vertices[south_index].position[1] - south_expected) > 0.0001f)
        {
            goto cleanup;
        }
    }
    result = 1;

cleanup:
    henka_free(samples);
    henka_terrain_world_destroy(world);
    return result;
}

static uint32_t test_terrain_mesh_vertex_index(uint32_t x, uint32_t z)
{
    return z * 65U + x;
}

static int test_four_way_transition_boundary_correspondence(void)
{
    henka_terrain_world_desc desc = henka_terrain_world_desc_default();
    henka_terrain_layout layout;
    henka_terrain_world* world = NULL;
    henka_terrain_sample* samples = NULL;
    henka_terrain_mesh_vertex* center_vertices = NULL;
    henka_terrain_mesh_vertex* north_vertices = NULL;
    henka_terrain_mesh_vertex* east_vertices = NULL;
    henka_terrain_mesh_vertex* south_vertices = NULL;
    henka_terrain_mesh_vertex* west_vertices = NULL;
    uint32_t* center_indices = NULL;
    uint32_t* neighbor_indices = NULL;
    henka_terrain_mesh_data center = {0};
    henka_terrain_mesh_data north = {0};
    henka_terrain_mesh_data east = {0};
    henka_terrain_mesh_data south = {0};
    henka_terrain_mesh_data west = {0};
    uint32_t index;
    uint32_t edge;
    int result = 0;

    /* A 3x3 chunk region gives the center chunk all four neighbors without
     * allocating the default 8 km region for this focused topology proof. */
    desc.world_width_meters = 192U;
    desc.world_depth_meters = 192U;
    desc.region_edge_meters = 192U;
    desc.chunk_edge_meters = 64U;
    desc.samples_per_chunk = 65U;
    desc.base_sample_spacing_meters = 1U;
    desc.chunks_per_region_edge = 3U;
    desc.regions_across = 1U;
    desc.regions_down = 1U;
    desc.max_resident_regions = 1U;
    if (henka_terrain_world_desc_get_layout(&desc, &layout) != HENKA_SUCCESS ||
        henka_terrain_world_create(&desc, &world) != HENKA_SUCCESS ||
        henka_terrain_world_reserve_region(world, (henka_terrain_region_id){0, 0}) != HENKA_SUCCESS ||
        henka_terrain_world_set_region_residency(
            world, (henka_terrain_region_id){0, 0}, false, true, false) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    samples = henka_calloc(layout.samples_per_region, sizeof(*samples));
    center_vertices = henka_calloc(HENKA_TERRAIN_MESH_MAX_VERTICES, sizeof(*center_vertices));
    north_vertices = henka_calloc(HENKA_TERRAIN_MESH_MAX_VERTICES, sizeof(*north_vertices));
    east_vertices = henka_calloc(HENKA_TERRAIN_MESH_MAX_VERTICES, sizeof(*east_vertices));
    south_vertices = henka_calloc(HENKA_TERRAIN_MESH_MAX_VERTICES, sizeof(*south_vertices));
    west_vertices = henka_calloc(HENKA_TERRAIN_MESH_MAX_VERTICES, sizeof(*west_vertices));
    center_indices = henka_calloc(HENKA_TERRAIN_MESH_MAX_INDICES, sizeof(*center_indices));
    neighbor_indices = henka_calloc(HENKA_TERRAIN_MESH_MAX_INDICES, sizeof(*neighbor_indices));
    if (samples == NULL || center_vertices == NULL || north_vertices == NULL ||
        east_vertices == NULL || south_vertices == NULL || west_vertices == NULL ||
        center_indices == NULL || neighbor_indices == NULL)
    {
        goto cleanup;
    }
    for (uint32_t z = 0U; z < layout.samples_per_region_edge; ++z)
    {
        for (uint32_t x = 0U; x < layout.samples_per_region_edge; ++x)
        {
            henka_terrain_sample* sample = &samples[
                (size_t)z * layout.samples_per_region_edge + x];
            sample->height_millimeters = (int32_t)(x * 3U + z * 5U);
            sample->material_weights[0] = 200U;
            sample->material_weights[1] = 20U;
            sample->material_weights[2] = 20U;
            sample->material_weights[3] = 15U;
        }
    }
    if (henka_terrain_world_apply_region_snapshot(
            world,
            (henka_terrain_region_storage_info){{0, 0}, 1U, 1U},
            samples,
            layout.samples_per_region) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    center.vertices = center_vertices;
    center.vertex_capacity = HENKA_TERRAIN_MESH_MAX_VERTICES;
    center.indices = center_indices;
    center.index_capacity = HENKA_TERRAIN_MESH_MAX_INDICES;
    north.vertices = north_vertices;
    north.vertex_capacity = HENKA_TERRAIN_MESH_MAX_VERTICES;
    north.indices = neighbor_indices;
    north.index_capacity = HENKA_TERRAIN_MESH_MAX_INDICES;
    east.vertices = east_vertices;
    east.vertex_capacity = HENKA_TERRAIN_MESH_MAX_VERTICES;
    east.indices = neighbor_indices;
    east.index_capacity = HENKA_TERRAIN_MESH_MAX_INDICES;
    south.vertices = south_vertices;
    south.vertex_capacity = HENKA_TERRAIN_MESH_MAX_VERTICES;
    south.indices = neighbor_indices;
    south.index_capacity = HENKA_TERRAIN_MESH_MAX_INDICES;
    west.vertices = west_vertices;
    west.vertex_capacity = HENKA_TERRAIN_MESH_MAX_VERTICES;
    west.indices = neighbor_indices;
    west.index_capacity = HENKA_TERRAIN_MESH_MAX_INDICES;
    if (henka_terrain_mesh_build_chunk_with_edge_mask(
            world, (henka_terrain_chunk_id){1, 1}, 0U,
            HENKA_TERRAIN_MESH_EDGE_ALL, &center) != HENKA_SUCCESS ||
        henka_terrain_mesh_build_chunk(
            world, (henka_terrain_chunk_id){1, 0}, 1U, &north) != HENKA_SUCCESS ||
        henka_terrain_mesh_build_chunk(
            world, (henka_terrain_chunk_id){2, 1}, 1U, &east) != HENKA_SUCCESS ||
        henka_terrain_mesh_build_chunk(
            world, (henka_terrain_chunk_id){1, 2}, 1U, &south) != HENKA_SUCCESS ||
        henka_terrain_mesh_build_chunk(
            world, (henka_terrain_chunk_id){0, 1}, 1U, &west) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    for (edge = 0U; edge <= 32U; ++edge)
    {
        const henka_terrain_mesh_vertex* center_north =
            &center_vertices[test_terrain_mesh_vertex_index(edge * 2U, 0U)];
        const henka_terrain_mesh_vertex* neighbor_north =
            &north_vertices[32U * 33U + edge];
        const henka_terrain_mesh_vertex* center_east =
            &center_vertices[test_terrain_mesh_vertex_index(64U, edge * 2U)];
        const henka_terrain_mesh_vertex* neighbor_east =
            &east_vertices[edge * 33U];
        const henka_terrain_mesh_vertex* center_south =
            &center_vertices[test_terrain_mesh_vertex_index(edge * 2U, 64U)];
        const henka_terrain_mesh_vertex* neighbor_south =
            &south_vertices[edge];
        const henka_terrain_mesh_vertex* center_west =
            &center_vertices[test_terrain_mesh_vertex_index(0U, edge * 2U)];
        const henka_terrain_mesh_vertex* neighbor_west =
            &west_vertices[edge * 33U + 32U];
        const henka_terrain_mesh_vertex* centers[] = {
            center_north, center_east, center_south, center_west};
        const henka_terrain_mesh_vertex* neighbors[] = {
            neighbor_north, neighbor_east, neighbor_south, neighbor_west};
        for (index = 0U; index < 4U; ++index)
        {
            uint32_t component;
            for (component = 0U; component < 3U; ++component)
            {
                if (!isfinite(centers[index]->position[component]) ||
                    !isfinite(neighbors[index]->position[component]) ||
                    fabsf(centers[index]->position[component] -
                        neighbors[index]->position[component]) > 0.0001f)
                {
                    goto cleanup;
                }
            }
        }
    }
    for (index = 0U; index < center.index_count; ++index)
    {
        const uint32_t vertex = center.indices[index];
        const uint32_t x = vertex % 65U;
        const uint32_t z = vertex / 65U;
        if ((z == 0U || z == 64U || x == 0U || x == 64U) &&
            (((z == 0U || z == 64U) && (x & 1U) != 0U) ||
                ((x == 0U || x == 64U) && (z & 1U) != 0U)))
        {
            goto cleanup;
        }
    }
    for (index = 0U; index < center.index_count; index += 3U)
    {
        if (center.indices[index] == center.indices[index + 1U] ||
            center.indices[index] == center.indices[index + 2U] ||
            center.indices[index + 1U] == center.indices[index + 2U])
        {
            goto cleanup;
        }
    }
    result = 1;

cleanup:
    henka_free(neighbor_indices);
    henka_free(center_indices);
    henka_free(west_vertices);
    henka_free(south_vertices);
    henka_free(east_vertices);
    henka_free(north_vertices);
    henka_free(center_vertices);
    henka_free(samples);
    henka_terrain_world_destroy(world);
    return result;
}

int main(void)
{
    return test_chunk_mesh_lod_and_identity() &&
        test_chunk_mesh_tangent_basis() &&
        test_neighbor_border_normals() &&
        test_transition_topology() &&
        test_four_way_transition_boundary_correspondence() ? 0 : 1;
}
