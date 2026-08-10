#ifndef SANDBOX3D_OBJECT_AUTHORING_TOOLS_H
#define SANDBOX3D_OBJECT_AUTHORING_TOOLS_H

#include <stdbool.h>
#include <stddef.h>

#include <henka/authoring_mesh.h>
#include <henka/authoring_modeling.h>
#include <henka/authoring_uv.h>
#include <henka/engine.h>
#include <henka/physics.h>
#include <henka/scene.h>

typedef struct sandbox3d_authoring_object sandbox3d_authoring_object;

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
void sandbox3d_authoring_object_destroy(sandbox3d_authoring_object* object);
henka_entity sandbox3d_authoring_object_get_entity(const sandbox3d_authoring_object* object);
const henka_authoring_mesh* sandbox3d_authoring_object_get_mesh(const sandbox3d_authoring_object* object);
henka_authoring_face_id sandbox3d_authoring_object_get_selected_face(const sandbox3d_authoring_object* object);
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
