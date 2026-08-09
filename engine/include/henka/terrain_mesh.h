#ifndef HENKA_TERRAIN_MESH_H
#define HENKA_TERRAIN_MESH_H

#include <stdint.h>

#include <henka/terrain.h>

#define HENKA_TERRAIN_MESH_MAX_LOD_LEVEL 3U
#define HENKA_TERRAIN_MESH_MAX_VERTICES (HENKA_TERRAIN_DEFAULT_SAMPLES_PER_CHUNK * HENKA_TERRAIN_DEFAULT_SAMPLES_PER_CHUNK)
#define HENKA_TERRAIN_MESH_MAX_INDICES ((HENKA_TERRAIN_DEFAULT_CHUNK_EDGE_METERS / HENKA_TERRAIN_DEFAULT_SAMPLE_SPACING_METERS) * (HENKA_TERRAIN_DEFAULT_CHUNK_EDGE_METERS / HENKA_TERRAIN_DEFAULT_SAMPLE_SPACING_METERS) * 6U)

typedef struct henka_terrain_mesh_vertex
{
    float position[3];
    float normal[3];
    float tangent[4];
    float uv[2];
    uint8_t material_weights[HENKA_TERRAIN_ACTIVE_MATERIAL_COUNT];
} henka_terrain_mesh_vertex;

typedef struct henka_terrain_mesh_data
{
    henka_terrain_chunk_id chunk_id;
    uint32_t lod_level;
    henka_terrain_revision revision;
    henka_terrain_generation generation;
    henka_terrain_mesh_vertex* vertices;
    uint32_t vertex_capacity;
    uint32_t vertex_count;
    uint32_t* indices;
    uint32_t index_capacity;
    uint32_t index_count;
} henka_terrain_mesh_data;

henka_result henka_terrain_mesh_build_chunk(
    const henka_terrain_world* world,
    henka_terrain_chunk_id chunk_id,
    uint32_t lod_level,
    henka_terrain_mesh_data* io_mesh);

#endif
