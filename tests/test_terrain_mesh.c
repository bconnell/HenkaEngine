#include <math.h>
#include <string.h>

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

int main(void)
{
    return test_chunk_mesh_lod_and_identity() ? 0 : 1;
}
