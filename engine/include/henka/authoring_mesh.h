#ifndef HENKA_AUTHORING_MESH_H
#define HENKA_AUTHORING_MESH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <henka/math.h>
#include <henka/result.h>

/* Bounded polygonal authoring data. Logical IDs are opaque, monotonic handles
 * for the mesh lifetime; physical vertex, edge, and face storage is bounded
 * and reusable after deletion. */
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
typedef struct henka_authoring_mesh_history henka_authoring_mesh_history;

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
    henka_vec2* uvs;
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
henka_result henka_authoring_mesh_clone(const henka_authoring_mesh* source, henka_authoring_mesh** out_mesh);
henka_result henka_authoring_mesh_copy(henka_authoring_mesh* destination, const henka_authoring_mesh* source);
henka_authoring_mesh_counts henka_authoring_mesh_get_counts(const henka_authoring_mesh* mesh);
/* Returns the bounded physical capacities for the authoring storage. */
henka_authoring_mesh_desc henka_authoring_mesh_get_desc(const henka_authoring_mesh* mesh);
/* Enumerates active logical IDs in deterministic physical-slot order. The
 * ordinal is a storage position, not the logical ID and is never used for
 * identity resolution. */
henka_result henka_authoring_mesh_get_vertex_id_at(
    const henka_authoring_mesh* mesh,
    size_t physical_slot,
    henka_authoring_vertex_id* out_id);
henka_result henka_authoring_mesh_get_edge_id_at(
    const henka_authoring_mesh* mesh,
    size_t physical_slot,
    henka_authoring_edge_id* out_id);
henka_result henka_authoring_mesh_get_face_id_at(
    const henka_authoring_mesh* mesh,
    size_t physical_slot,
    henka_authoring_face_id* out_id);
bool henka_authoring_mesh_validate(const henka_authoring_mesh* mesh);
/* Returns bounds from active source vertices. */
henka_result henka_authoring_mesh_get_bounds(
    const henka_authoring_mesh* mesh,
    henka_vec3* out_center,
    henka_vec3* out_extents);

henka_result henka_authoring_mesh_add_vertex(henka_authoring_mesh* mesh, henka_vec3 position, henka_vec2 uv, uint32_t material_region, henka_authoring_vertex_id* out_id);
henka_result henka_authoring_mesh_remove_vertex(henka_authoring_mesh* mesh, henka_authoring_vertex_id id);
const henka_authoring_vertex* henka_authoring_mesh_get_vertex(const henka_authoring_mesh* mesh, henka_authoring_vertex_id id);
henka_result henka_authoring_mesh_set_vertex_position(
    henka_authoring_mesh* mesh,
    henka_authoring_vertex_id id,
    henka_vec3 position);
henka_result henka_authoring_mesh_set_vertex_uv(henka_authoring_mesh* mesh, henka_authoring_vertex_id id, henka_vec2 uv);

/* Adds one bounded standalone edge between two distinct active vertices. The
 * edge may later become face-backed through add_face, but it is valid with no
 * incident faces and remains a stable source component while face-less. It can
 * be removed explicitly before it is consumed by a face. Duplicate endpoint
 * pairs and invalid vertices are rejected. */
henka_result henka_authoring_mesh_add_edge(
    henka_authoring_mesh* mesh,
    henka_authoring_vertex_id first_vertex_id,
    henka_authoring_vertex_id second_vertex_id,
    bool hard,
    henka_authoring_edge_id* out_id);
henka_result henka_authoring_mesh_remove_edge(
    henka_authoring_mesh* mesh,
    henka_authoring_edge_id id);

henka_result henka_authoring_mesh_add_face(henka_authoring_mesh* mesh, const henka_authoring_vertex_id* vertices, size_t corner_count, uint32_t material_region, bool smooth, henka_authoring_face_id* out_id);
henka_result henka_authoring_mesh_remove_face(henka_authoring_mesh* mesh, henka_authoring_face_id id);
const henka_authoring_face* henka_authoring_mesh_get_face(const henka_authoring_mesh* mesh, henka_authoring_face_id id);
const henka_authoring_edge* henka_authoring_mesh_get_edge(const henka_authoring_mesh* mesh, henka_authoring_edge_id id);
henka_result henka_authoring_mesh_set_face_material_region(
    henka_authoring_mesh* mesh,
    henka_authoring_face_id id,
    uint32_t material_region);
henka_result henka_authoring_mesh_set_face_smoothing(henka_authoring_mesh* mesh, henka_authoring_face_id id, bool smooth);
henka_result henka_authoring_mesh_set_face_corner_uv(henka_authoring_mesh* mesh, henka_authoring_face_id id, size_t corner, henka_vec2 uv);
henka_result henka_authoring_mesh_get_face_corner_uv(const henka_authoring_mesh* mesh, henka_authoring_face_id id, size_t corner, henka_vec2* out_uv);
henka_result henka_authoring_mesh_set_edge_hard(henka_authoring_mesh* mesh, henka_authoring_edge_id id, bool hard);

size_t henka_authoring_mesh_get_vertex_edge_count(const henka_authoring_mesh* mesh, henka_authoring_vertex_id vertex_id);
henka_result henka_authoring_mesh_get_vertex_edge_at(const henka_authoring_mesh* mesh, henka_authoring_vertex_id vertex_id, size_t ordinal, henka_authoring_edge_id* out_edge_id);
size_t henka_authoring_mesh_get_edge_face_count(const henka_authoring_mesh* mesh, henka_authoring_edge_id edge_id);
henka_result henka_authoring_mesh_get_edge_face_at(const henka_authoring_mesh* mesh, henka_authoring_edge_id edge_id, size_t ordinal, henka_authoring_face_id* out_face_id);
bool henka_authoring_mesh_edge_is_boundary(const henka_authoring_mesh* mesh, henka_authoring_edge_id edge_id);
/* Reconstructs likely authoring quads from compatible adjacent triangle
 * pairs. The operation rebuilds the source compactly and transactionally,
 * preserving material boundaries, hard edges, winding, and UV seams.
 *
 * minimum_normal_dot controls surface continuity (0..1).
 * minimum_diagonal_ratio prefers likely former quad diagonals over perimeter
 * edges. uv_epsilon prevents merging across per-corner UV seams. */
henka_result henka_authoring_mesh_recover_quads(
    henka_authoring_mesh* mesh,
    float minimum_normal_dot,
    float minimum_diagonal_ratio,
    float uv_epsilon,
    size_t* out_merged_pairs);

/* Converts polygons to deterministic fan triangles and computes normals from
 * winding plus smooth-face and hard-edge intent. Output buffers are borrowed.
 * Counts are cleared before validation and remain zero on failure. */
henka_result henka_authoring_mesh_evaluate(const henka_authoring_mesh* mesh, henka_authoring_render_data* out_data);

/* History stores bounded topology snapshots. Checkpoint after a successful
 * edit; the initial mesh is captured at history creation. */
henka_result henka_authoring_mesh_history_create(const henka_authoring_mesh* initial_mesh, size_t max_steps, henka_authoring_mesh_history** out_history);
void henka_authoring_mesh_history_destroy(henka_authoring_mesh_history* history);
henka_result henka_authoring_mesh_history_checkpoint(henka_authoring_mesh_history* history, const henka_authoring_mesh* mesh);
bool henka_authoring_mesh_history_can_undo(const henka_authoring_mesh_history* history);
bool henka_authoring_mesh_history_can_redo(const henka_authoring_mesh_history* history);
henka_result henka_authoring_mesh_history_undo(henka_authoring_mesh_history* history, henka_authoring_mesh* mesh);
henka_result henka_authoring_mesh_history_redo(henka_authoring_mesh_history* history, henka_authoring_mesh* mesh);

/* Versioned bounded persistence. Loading parses into a candidate and swaps it
 * only after complete validation, retaining the prior mesh on every failure. */
henka_result henka_authoring_mesh_save_file(const henka_authoring_mesh* mesh, const char* path);
henka_result henka_authoring_mesh_load_file(henka_authoring_mesh* mesh, const char* path);
/* Creates a bounded source from a versioned file after validating the file's
 * declared capacities. The output slot must be empty and is assigned only on
 * successful parse and topology validation. */
henka_result henka_authoring_mesh_load_file_new(const char* path, henka_authoring_mesh** out_mesh);

#endif
