#ifndef HENKA_AUTHORING_MESH_H
#define HENKA_AUTHORING_MESH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <henka/math.h>
#include <henka/result.h>

/* Bounded polygonal authoring data. IDs are slot identities and are never
 * reused during the lifetime of a mesh; deleted elements remain tombstoned. */
#define HENKA_AUTHORING_MESH_DEFAULT_MAX_VERTICES 4096U
#define HENKA_AUTHORING_MESH_DEFAULT_MAX_EDGES 8192U
#define HENKA_AUTHORING_MESH_DEFAULT_MAX_FACES 2048U
#define HENKA_AUTHORING_MESH_DEFAULT_MAX_FACE_CORNERS 8U
#define HENKA_AUTHORING_MESH_HARD_MAX_VERTICES 65536U
#define HENKA_AUTHORING_MESH_HARD_MAX_EDGES 131072U
#define HENKA_AUTHORING_MESH_HARD_MAX_FACES 65536U
#define HENKA_AUTHORING_MESH_HARD_MAX_FACE_CORNERS 32U

typedef uint32_t henka_authoring_vertex_id;
typedef uint32_t henka_authoring_edge_id;
typedef uint32_t henka_authoring_face_id;

#define HENKA_AUTHORING_INVALID_ID UINT32_MAX

typedef struct henka_authoring_mesh henka_authoring_mesh;

typedef struct henka_authoring_mesh_desc
{
    size_t max_vertices;
    size_t max_edges;
    size_t max_faces;
    size_t max_face_corners;
} henka_authoring_mesh_desc;

typedef struct henka_authoring_vertex
{
    henka_authoring_vertex_id id;
    henka_vec3 position;
    henka_vec2 uv;
    uint32_t material_region;
    bool active;
} henka_authoring_vertex;

typedef struct henka_authoring_edge
{
    henka_authoring_edge_id id;
    henka_authoring_vertex_id vertices[2];
    henka_authoring_face_id faces[2];
    size_t face_count;
    bool hard;
    bool active;
} henka_authoring_edge;

typedef struct henka_authoring_face
{
    henka_authoring_face_id id;
    size_t corner_count;
    henka_authoring_vertex_id* vertices;
    henka_authoring_edge_id* edges;
    uint32_t material_region;
    bool smooth;
    bool active;
} henka_authoring_face;

typedef struct henka_authoring_mesh_counts
{
    size_t vertices;
    size_t edges;
    size_t faces;
} henka_authoring_mesh_counts;

typedef struct henka_authoring_render_vertex
{
    henka_vec3 position;
    henka_vec3 normal;
    henka_vec4 tangent;
    henka_vec2 uv;
    uint32_t material_region;
} henka_authoring_render_vertex;

typedef struct henka_authoring_render_data
{
    henka_authoring_render_vertex* vertices;
    size_t vertex_capacity;
    size_t vertex_count;
    uint32_t* indices;
    size_t index_capacity;
    size_t index_count;
} henka_authoring_render_data;

henka_authoring_mesh_desc henka_authoring_mesh_desc_default(void);
henka_result henka_authoring_mesh_create(const henka_authoring_mesh_desc* desc, henka_authoring_mesh** out_mesh);
void henka_authoring_mesh_destroy(henka_authoring_mesh* mesh);
henka_authoring_mesh_counts henka_authoring_mesh_get_counts(const henka_authoring_mesh* mesh);
bool henka_authoring_mesh_validate(const henka_authoring_mesh* mesh);

henka_result henka_authoring_mesh_add_vertex(henka_authoring_mesh* mesh, henka_vec3 position, henka_vec2 uv, uint32_t material_region, henka_authoring_vertex_id* out_id);
henka_result henka_authoring_mesh_remove_vertex(henka_authoring_mesh* mesh, henka_authoring_vertex_id id);
const henka_authoring_vertex* henka_authoring_mesh_get_vertex(const henka_authoring_mesh* mesh, henka_authoring_vertex_id id);
henka_result henka_authoring_mesh_set_vertex_uv(henka_authoring_mesh* mesh, henka_authoring_vertex_id id, henka_vec2 uv);

henka_result henka_authoring_mesh_add_face(henka_authoring_mesh* mesh, const henka_authoring_vertex_id* vertices, size_t corner_count, uint32_t material_region, bool smooth, henka_authoring_face_id* out_id);
henka_result henka_authoring_mesh_remove_face(henka_authoring_mesh* mesh, henka_authoring_face_id id);
const henka_authoring_face* henka_authoring_mesh_get_face(const henka_authoring_mesh* mesh, henka_authoring_face_id id);
const henka_authoring_edge* henka_authoring_mesh_get_edge(const henka_authoring_mesh* mesh, henka_authoring_edge_id id);
henka_result henka_authoring_mesh_set_face_smoothing(henka_authoring_mesh* mesh, henka_authoring_face_id id, bool smooth);
henka_result henka_authoring_mesh_set_edge_hard(henka_authoring_mesh* mesh, henka_authoring_edge_id id, bool hard);

size_t henka_authoring_mesh_get_vertex_edge_count(const henka_authoring_mesh* mesh, henka_authoring_vertex_id vertex_id);
henka_result henka_authoring_mesh_get_vertex_edge_at(const henka_authoring_mesh* mesh, henka_authoring_vertex_id vertex_id, size_t ordinal, henka_authoring_edge_id* out_edge_id);
size_t henka_authoring_mesh_get_edge_face_count(const henka_authoring_mesh* mesh, henka_authoring_edge_id edge_id);
henka_result henka_authoring_mesh_get_edge_face_at(const henka_authoring_mesh* mesh, henka_authoring_edge_id edge_id, size_t ordinal, henka_authoring_face_id* out_face_id);
bool henka_authoring_mesh_edge_is_boundary(const henka_authoring_mesh* mesh, henka_authoring_edge_id edge_id);

/* Converts polygons to deterministic fan triangles and computes normals from
 * winding plus smooth-face and hard-edge intent. Output buffers are borrowed. */
henka_result henka_authoring_mesh_evaluate(const henka_authoring_mesh* mesh, henka_authoring_render_data* out_data);

#endif
