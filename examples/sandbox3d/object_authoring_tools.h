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
size_t sandbox3d_authoring_object_get_selected_component_count(
    const sandbox3d_authoring_object* object);
henka_result sandbox3d_authoring_object_get_selected_component_at(
    const sandbox3d_authoring_object* object,
    size_t ordinal,
    uint32_t* out_id);
henka_result sandbox3d_authoring_object_select_component(
    sandbox3d_authoring_object* object,
    uint32_t component_id,
    bool additive);
henka_result sandbox3d_authoring_object_pick_component(
    sandbox3d_authoring_object* object,
    henka_ray ray,
    float maximum_distance,
    bool additive);
henka_result sandbox3d_authoring_object_move_selected_components(
    sandbox3d_authoring_object* object,
    henka_vec3 offset);
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
henka_result sandbox3d_authoring_object_pick_face(
    sandbox3d_authoring_object* object,
    henka_ray ray,
    float maximum_distance);
henka_result sandbox3d_authoring_object_select_face(
    sandbox3d_authoring_object* object,
    henka_authoring_face_id face_id);
henka_result sandbox3d_authoring_object_set_selected_face_material_region(
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
