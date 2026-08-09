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
    henka_terrain_mesh_vertex vertices[HENKA_TERRAIN_MESH_MAX_VERTICES];
    henka_terrain_mesh_vertex neighbor_vertices[HENKA_TERRAIN_MESH_MAX_VERTICES];
    uint32_t indices[HENKA_TERRAIN_MESH_MAX_INDICES];
    uint32_t neighbor_indices[HENKA_TERRAIN_MESH_MAX_INDICES];
    henka_terrain_mesh_data mesh = {0};
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
    neighbor_mesh.vertices = neighbor_vertices;
    neighbor_mesh.vertex_capacity = HENKA_TERRAIN_MESH_MAX_VERTICES;
    neighbor_mesh.indices = neighbor_indices;
    neighbor_mesh.index_capacity = HENKA_TERRAIN_MESH_MAX_INDICES;
    if (henka_terrain_mesh_build_chunk_with_edge_mask(
            world,
            (henka_terrain_chunk_id){0, 0},
            0U,
            HENKA_TERRAIN_MESH_EDGE_EAST,
            &mesh) != HENKA_SUCCESS)
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
    result = 1;

cleanup:
    henka_free(samples);
    henka_terrain_world_destroy(world);
    return result;
}

int main(void)
{
    return test_chunk_mesh_lod_and_identity() &&
        test_chunk_mesh_tangent_basis() &&
        test_neighbor_border_normals() &&
        test_transition_topology() ? 0 : 1;
}
