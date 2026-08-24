#ifndef SANDBOX3D_OBJECT_AUTHORING_TOOLS_H
#define SANDBOX3D_OBJECT_AUTHORING_TOOLS_H

#include <stdbool.h>
#include <stddef.h>

#include <henka/authoring_mesh.h>
#include <henka/authoring_modeling.h>
#include <henka/authoring_uv.h>
#include <henka/engine.h>
#include <henka/model.h>
#include <henka/physics.h>
#include <henka/scene.h>

typedef struct sandbox3d_authoring_object sandbox3d_authoring_object;

typedef enum sandbox3d_authoring_selection_mode
{
    SANDBOX3D_AUTHORING_SELECTION_VERTEX = 0,
    SANDBOX3D_AUTHORING_SELECTION_EDGE,
    SANDBOX3D_AUTHORING_SELECTION_FACE
} sandbox3d_authoring_selection_mode;

typedef enum sandbox3d_authoring_selection_query_kind
{
    SANDBOX3D_AUTHORING_SELECTION_QUERY_BOUNDARY = 0,
    SANDBOX3D_AUTHORING_SELECTION_QUERY_HARD_EDGE,
    SANDBOX3D_AUTHORING_SELECTION_QUERY_MATERIAL_REGION,
    SANDBOX3D_AUTHORING_SELECTION_QUERY_FACE_SIDE_COUNT,
    SANDBOX3D_AUTHORING_SELECTION_QUERY_SIMILAR_NORMAL,
    SANDBOX3D_AUTHORING_SELECTION_QUERY_SIMILAR_MATERIAL_REGION
} sandbox3d_authoring_selection_query_kind;

typedef struct sandbox3d_authoring_selection_query
{
    sandbox3d_authoring_selection_query_kind kind;
    uint32_t material_region;
    size_t face_side_count;
    float minimum_normal_dot;
} sandbox3d_authoring_selection_query;

typedef enum sandbox3d_authoring_pivot_mode
{
    SANDBOX3D_AUTHORING_PIVOT_MEDIAN = 0,
    SANDBOX3D_AUTHORING_PIVOT_ACTIVE,
    SANDBOX3D_AUTHORING_PIVOT_INDIVIDUAL
} sandbox3d_authoring_pivot_mode;

typedef enum sandbox3d_authoring_orientation_mode
{
    SANDBOX3D_AUTHORING_ORIENTATION_WORLD = 0,
    SANDBOX3D_AUTHORING_ORIENTATION_LOCAL,
    SANDBOX3D_AUTHORING_ORIENTATION_NORMAL
} sandbox3d_authoring_orientation_mode;

/* Describes a bounded, local-space vertex-region transform.  The operation
 * preserves the source topology and material regions while publishing the
 * changed mesh through the same scene, history, bounds, and physics
 * transaction used by the other native modeling tools. */
typedef struct sandbox3d_authoring_region_transform
{
    henka_vec3 minimum;
    henka_vec3 maximum;
    henka_vec3 pivot;
    henka_vec3 scale;
    henka_vec3 offset;
} sandbox3d_authoring_region_transform;

/* Returns all active non-helper entities in scene enumeration order. */
size_t sandbox3d_object_authoring_collect_user_entities(
    const henka_scene* scene,
    henka_entity* out_entities,
    size_t capacity);

/* Returns whether an entity is a valid non-helper target for editor mutation. */
bool sandbox3d_object_authoring_can_edit_entity(
    const henka_scene* scene,
    henka_entity entity);

/* Duplicates the complete scene-owned object state and leaves the source unchanged. */
henka_result sandbox3d_object_authoring_duplicate_entity(
    henka_scene* scene,
    henka_entity source,
    const char* duplicate_name,
    henka_entity* out_duplicate);

/* A bounded scene-connected authoring source of truth for one editable entity.
 * The bridge owns its authoring mesh, history, and evaluated render mesh. */
henka_result sandbox3d_authoring_object_create_box(
    henka_engine* engine,
    henka_scene* scene,
    henka_entity entity,
    float width,
    float height,
    float depth,
    const henka_authoring_mesh_desc* mesh_desc,
    size_t history_steps,
    sandbox3d_authoring_object** out_object);
/* Creates an independent authoring bridge by cloning an existing source mesh.
 * The clone owns its topology, history, evaluated render mesh, and scene
 * replacement; the caller retains ownership of the source. */
henka_result sandbox3d_authoring_object_create_from_mesh(
    henka_engine* engine,
    henka_scene* scene,
    henka_entity entity,
    const henka_authoring_mesh* source,
    size_t history_steps,
    sandbox3d_authoring_object** out_object);
/* Builds a user-owned authoring source from one validated imported model
 * primitive. The caller retains the imported source data; the bridge owns the
 * converted topology and publishes its evaluated mesh transactionally. */
henka_result sandbox3d_authoring_object_create_from_model_primitive(
    henka_engine* engine,
    henka_scene* scene,
    henka_entity entity,
    const henka_model_scene_primitive* primitive,
    size_t history_steps,
    sandbox3d_authoring_object** out_object);
void sandbox3d_authoring_object_destroy(sandbox3d_authoring_object* object);
henka_entity sandbox3d_authoring_object_get_entity(const sandbox3d_authoring_object* object);
const henka_authoring_mesh* sandbox3d_authoring_object_get_mesh(const sandbox3d_authoring_object* object);
/* Presents an owned candidate without replacing the authoritative source or
 * creating an undo entry.  The candidate is retained only after success; the
 * caller retains ownership on failure. */
henka_result sandbox3d_authoring_object_preview_candidate(
    sandbox3d_authoring_object* object,
    henka_authoring_mesh* candidate);
/* Publishes the active preview candidate as one source/render/history/
 * physics transaction. */
henka_result sandbox3d_authoring_object_commit_preview(
    sandbox3d_authoring_object* object);
/* Restores the authoritative source/render/bounds state and discards the
 * active preview without changing history. */
henka_result sandbox3d_authoring_object_cancel_preview(
    sandbox3d_authoring_object* object);
/* Reconstructs compatible triangle pairs as authoring quads through the
 * normal scene/render/history/physics transaction. */
henka_result sandbox3d_authoring_object_recover_quads(
    sandbox3d_authoring_object* object,
    float minimum_normal_dot,
    float minimum_diagonal_ratio,
    float uv_epsilon,
    size_t* out_recovered_pairs);
/* Copies one active face's authoritative ordered corner loop without
 * inferring order from IDs or screen-space coordinates.  The accessor fails
 * closed when the face is missing, malformed, over the bounded capacity, or
 * references a missing/duplicate vertex. */
henka_result sandbox3d_authoring_object_get_face_ordered_corners(
    const sandbox3d_authoring_object* object,
    henka_authoring_face_id face_id,
    henka_authoring_vertex_id* out_vertices,
    size_t vertex_capacity,
    size_t* out_count);
/* Returns the material-region range retained by the evaluated render mesh. */
henka_result sandbox3d_authoring_object_get_render_material_region_range(
    const sandbox3d_authoring_object* object,
    uint32_t* out_min_region,
    uint32_t* out_max_region);
henka_authoring_face_id sandbox3d_authoring_object_get_selected_face(const sandbox3d_authoring_object* object);
void sandbox3d_authoring_object_set_selection_mode(
    sandbox3d_authoring_object* object,
    sandbox3d_authoring_selection_mode mode);
sandbox3d_authoring_selection_mode sandbox3d_authoring_object_get_selection_mode(
    const sandbox3d_authoring_object* object);
void sandbox3d_authoring_object_clear_component_selection(
    sandbox3d_authoring_object* object);
henka_result sandbox3d_authoring_object_select_all_components(
    sandbox3d_authoring_object* object);
henka_result sandbox3d_authoring_object_select_none_components(
    sandbox3d_authoring_object* object);
henka_result sandbox3d_authoring_object_invert_component_selection(
    sandbox3d_authoring_object* object);
henka_result sandbox3d_authoring_object_shrink_component_selection(
    sandbox3d_authoring_object* object);
size_t sandbox3d_authoring_object_get_selected_component_count(
    const sandbox3d_authoring_object* object);
/* Returns the most recently picked component in the active topology mode.
 * This is the edit target that receives the strongest viewport cue when a
 * multi-component selection is present. */
uint32_t sandbox3d_authoring_object_get_active_component_id(
    const sandbox3d_authoring_object* object);
henka_result sandbox3d_authoring_object_get_selected_component_at(
    const sandbox3d_authoring_object* object,
    size_t ordinal,
    uint32_t* out_id);
henka_result sandbox3d_authoring_object_select_component(
    sandbox3d_authoring_object* object,
    uint32_t component_id,
    bool additive);
/* Atomically replaces the active-mode component selection. IDs must be
 * strictly increasing and active in the current source mesh. */
henka_result sandbox3d_authoring_object_replace_component_selection(
    sandbox3d_authoring_object* object,
    const uint32_t* component_ids,
    size_t component_count,
    uint32_t active_component_id);
henka_result sandbox3d_authoring_object_select_matching_components(
    sandbox3d_authoring_object* object,
    const sandbox3d_authoring_selection_query* query);
/* Finds the nearest source-authoritative component in the active selection
 * mode without changing selection, active-component, or face state. */
henka_result sandbox3d_authoring_object_find_component(
    const sandbox3d_authoring_object* object,
    henka_ray ray,
    float maximum_distance,
    uint32_t* out_component_id);
henka_result sandbox3d_authoring_object_pick_component(
    sandbox3d_authoring_object* object,
    henka_ray ray,
    float maximum_distance,
    bool additive);
henka_result sandbox3d_authoring_object_move_selected_components(
    sandbox3d_authoring_object* object,
    henka_vec3 offset);
/* Moves the active face selection along its evaluated local-space normal.
 * The shared face vertices are moved once through the normal transactional
 * source, preserving topology continuity while providing a direct profile
 * shaping tool for native-authored geometry. */
henka_result sandbox3d_authoring_object_move_selected_face_normal(
    sandbox3d_authoring_object* object,
    float distance);
/* Moves the vertices touched by the active selection with a bounded
 * topology-aware falloff. The selected vertices receive the full offset;
 * each connected ring up to ring_count receives a linearly reduced offset.
 * The result is published through the normal render, bounds, physics, and
 * undo transaction. */
henka_result sandbox3d_authoring_object_proportional_move_selected_components(
    sandbox3d_authoring_object* object,
    henka_vec3 offset,
    size_t ring_count);
/* Adds one topology-adjacent ring to the current vertex, edge, or face
 * selection. The operation is bounded by the editor selection budget and
 * does not mutate mesh topology. */
henka_result sandbox3d_authoring_object_grow_component_selection(
    sandbox3d_authoring_object* object);
/* Expands the active selection through all reachable topology until the
 * bounded editor selection budget is reached or the connected component is
 * complete. This never mutates mesh topology. */
henka_result sandbox3d_authoring_object_select_connected_components(
    sandbox3d_authoring_object* object);
/* Selects the maximal unambiguous connected edge chain through regular quad
 * topology. Adjacent selected loop edges share vertices. Boundaries and poles
 * terminate traversal; malformed/nonmanifold topology fails without
 * replacing the prior selection. */
henka_result sandbox3d_authoring_object_select_edge_loop(
    sandbox3d_authoring_object* object);
/* Selects the maximal unambiguous edge ring by crossing to opposite edges
 * through ordered quad faces. Ring edges need not share vertices. Boundaries
 * and non-quad faces terminate traversal; malformed/nonmanifold topology
 * fails without replacing the prior selection. */
henka_result sandbox3d_authoring_object_select_edge_ring(
    sandbox3d_authoring_object* object);
/* Dissolves one selected compatible interior edge through the authoritative
 * source/render/bounds/physics/undo transaction. Multiple-edge selections are
 * rejected until a stable batch remapping contract is available. */
henka_result sandbox3d_authoring_object_dissolve_selected_edge(
    sandbox3d_authoring_object* object);
/* Deletes one selected edge and its incident faces through the authoritative
 * source/render/bounds/physics/undo transaction. Multiple-edge selections are
 * rejected until a stable batch deletion contract is available. */
henka_result sandbox3d_authoring_object_delete_selected_edge(
    sandbox3d_authoring_object* object);
/* Bevels one selected compatible boundary edge or isolated two-quad interior
 * edge through the authoritative source/render/bounds/physics/undo
 * transaction. Broader interior topology and ambiguous endpoint fans are
 * rejected by the core contract. */
henka_result sandbox3d_authoring_object_bevel_selected_edge(
    sandbox3d_authoring_object* object);
/* Scales the vertices touched by the current component selection around their
 * centroid and publishes the result through the normal transactional source,
 * render, bounds, physics, and undo path. */
henka_result sandbox3d_authoring_object_scale_selected_components(
    sandbox3d_authoring_object* object,
    henka_vec3 scale);
henka_result sandbox3d_authoring_object_scale_selected_components_with_pivot(
    sandbox3d_authoring_object* object,
    henka_vec3 scale,
    sandbox3d_authoring_pivot_mode pivot_mode);
henka_result sandbox3d_authoring_object_rotate_selected_components(
    sandbox3d_authoring_object* object,
    henka_vec3 axis,
    float radians,
    sandbox3d_authoring_pivot_mode pivot_mode,
    sandbox3d_authoring_orientation_mode orientation_mode);
henka_result sandbox3d_authoring_object_transform_vertex_region(
    sandbox3d_authoring_object* object,
    const sandbox3d_authoring_region_transform* transform,
    size_t* out_affected_vertices);
/* Applies a bounded ordered set of local-space region transforms as one
 * scene/render/history/physics transaction. A vertex matched by more than one
 * region receives the transforms in array order and is counted once. */
henka_result sandbox3d_authoring_object_transform_vertex_regions(
    sandbox3d_authoring_object* object,
    const sandbox3d_authoring_region_transform* transforms,
    size_t transform_count,
    size_t* out_affected_vertices);
henka_result sandbox3d_authoring_object_merge_selected_vertices_center(
    sandbox3d_authoring_object* object);
henka_result sandbox3d_authoring_object_merge_selected_vertices_active(
    sandbox3d_authoring_object* object);
henka_result sandbox3d_authoring_object_merge_selected_vertices_by_distance(
    sandbox3d_authoring_object* object);
henka_result sandbox3d_authoring_object_dissolve_selected_vertices(
    sandbox3d_authoring_object* object);
henka_result sandbox3d_authoring_object_delete_selected_vertices(
    sandbox3d_authoring_object* object);
henka_result sandbox3d_authoring_object_connect_selected_vertices(
    sandbox3d_authoring_object* object);
henka_result sandbox3d_authoring_object_bevel_selected_vertices(
    sandbox3d_authoring_object* object);
/* Extrudes one selected boundary corner through the authoritative
 * source/render/bounds/physics/undo transaction. Multi-face vertex fans are
 * rejected until their remapping contract is stable. */
henka_result sandbox3d_authoring_object_extrude_selected_vertex(
    sandbox3d_authoring_object* object,
    float distance);
float sandbox3d_authoring_object_get_bevel_width(
    const sandbox3d_authoring_object* object);
henka_result sandbox3d_authoring_object_set_bevel_width(
    sandbox3d_authoring_object* object,
    float width);
float sandbox3d_authoring_object_get_merge_distance(
    const sandbox3d_authoring_object* object);
henka_result sandbox3d_authoring_object_set_merge_distance(
    sandbox3d_authoring_object* object,
    float distance);
henka_result sandbox3d_authoring_object_pick_face(
    sandbox3d_authoring_object* object,
    henka_ray ray,
    float maximum_distance);
henka_result sandbox3d_authoring_object_select_face(
    sandbox3d_authoring_object* object,
    henka_authoring_face_id face_id);
/* Selects the active face whose centroid is furthest along the supplied local
 * axis.  This is a bounded, non-mutating selection aid for caps, extremities,
 * and other authored profiles; it never changes mesh topology. */
henka_result sandbox3d_authoring_object_select_extreme_face(
    sandbox3d_authoring_object* object,
    henka_vec3 local_axis,
    bool maximum);
/* Selects every active face whose centroid lies within band_width of the
 * extreme projection along local_axis.  This is a bounded geometric
 * selection aid for shaping rings, caps, and authored profiles; it never
 * changes topology and preserves the existing transactional selection state
 * when validation or allocation fails. */
henka_result sandbox3d_authoring_object_select_extreme_face_band(
    sandbox3d_authoring_object* object,
    henka_vec3 local_axis,
    bool maximum,
    float band_width);
/* Removes the selected faces as one bounded topology transaction.  The
 * operation fails closed when it would leave no renderable face, preserving
 * the source, evaluated mesh, bounds, physics, and history on failure. */
henka_result sandbox3d_authoring_object_delete_selected_faces(
    sandbox3d_authoring_object* object);
henka_result sandbox3d_authoring_object_set_selected_face_material_region(
    sandbox3d_authoring_object* object,
    uint32_t material_region);
/* Extrudes every selected face as one bounded topology transaction.  The
 * selected faces are validated before any candidate is published; on failure
 * the source, evaluated mesh, bounds, physics, selection, and history remain
 * unchanged.  The newly created cap faces become the active face selection. */
henka_result sandbox3d_authoring_object_extrude_selected_faces(
    sandbox3d_authoring_object* object,
    float distance);
/* Applies one material-region identity to the complete face selection as one
 * bounded metadata transaction.  This changes no topology and is useful for
 * authored material regions that share a common semantic surface. */
henka_result sandbox3d_authoring_object_set_selected_faces_material_region(
    sandbox3d_authoring_object* object,
    uint32_t material_region);
henka_result sandbox3d_authoring_object_extrude_selected_face(
    sandbox3d_authoring_object* object,
    float distance);
henka_result sandbox3d_authoring_object_inset_selected_face(
    sandbox3d_authoring_object* object,
    float factor);
henka_result sandbox3d_authoring_object_bevel_selected_face(
    sandbox3d_authoring_object* object,
    float width);
/* Splits one selected isolated quad face into two quads through the normal
 * source/render/bounds/physics/undo transaction. */
henka_result sandbox3d_authoring_object_loop_cut_selected_face(
    sandbox3d_authoring_object* object);
henka_result sandbox3d_authoring_object_subdivide_selected_face(
    sandbox3d_authoring_object* object);
henka_result sandbox3d_authoring_object_project_selected_face_uv(
    sandbox3d_authoring_object* object,
    henka_authoring_uv_projection_axis axis);
henka_result sandbox3d_authoring_object_pack_selected_face_uv(
    sandbox3d_authoring_object* object,
    float padding);
henka_result sandbox3d_authoring_object_save_source(
    const sandbox3d_authoring_object* object,
    const char* path);
henka_result sandbox3d_authoring_object_reload_source(
    sandbox3d_authoring_object* object,
    const char* path);
/* Saves a bounded project manifest beside the existing versioned mesh source.
 * The manifest owns no renderer or asset-manager state; it records only the
 * source path, entity transform, and visibility needed to reopen this bridge. */
henka_result sandbox3d_authoring_object_save_project(
    const sandbox3d_authoring_object* object,
    const char* project_path,
    const char* source_path);
/* Loads and validates the manifest and referenced authoring source
 * transactionally. The current scene representation is retained on failure. */
henka_result sandbox3d_authoring_object_load_project(
    sandbox3d_authoring_object* object,
    const char* project_path);
const char* sandbox3d_authoring_object_get_source_path(
    const sandbox3d_authoring_object* object);
/* Binds the evaluated local bounds to an existing box collider.  The bridge
 * does not own the physics world or body; callers must unbind before either
 * is destroyed.  Modeling and source reload then update the collider as one
 * transaction with the scene render replacement. */
henka_result sandbox3d_authoring_object_bind_physics(
    sandbox3d_authoring_object* object,
    henka_physics_world* world,
    henka_physics_body_id body);
void sandbox3d_authoring_object_unbind_physics(sandbox3d_authoring_object* object);
henka_result sandbox3d_authoring_object_undo(sandbox3d_authoring_object* object);
henka_result sandbox3d_authoring_object_redo(sandbox3d_authoring_object* object);

#endif
