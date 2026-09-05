#ifndef SANDBOX3D_SCENE_DOCUMENT_BRIDGE_H
#define SANDBOX3D_SCENE_DOCUMENT_BRIDGE_H

#include <stddef.h>

#include <henka/physics.h>
#include <henka/scene_document.h>

typedef struct sandbox3d_scene_document_bridge sandbox3d_scene_document_bridge;

/* The bridge borrows the document, scene, and physics world. It owns only a
 * bounded persistent-ID to generation-checked-runtime-handle mapping. */
henka_result sandbox3d_scene_document_bridge_create(
    henka_scene_document* document,
    henka_scene* scene,
    sandbox3d_scene_document_bridge** out_bridge);
void sandbox3d_scene_document_bridge_destroy(
    sandbox3d_scene_document_bridge* bridge);
size_t sandbox3d_scene_document_bridge_get_binding_count(
    const sandbox3d_scene_document_bridge* bridge);
henka_result sandbox3d_scene_document_bridge_get_binding_at(
    const sandbox3d_scene_document_bridge* bridge,
    size_t index,
    henka_scene_document_id* out_document_id,
    henka_entity* out_entity);
/* Validation requires a one-to-one set of persistent document IDs and live
 * runtime bindings; names and array positions are not identity. */
henka_result sandbox3d_scene_document_bridge_validate(
    const sandbox3d_scene_document_bridge* bridge);
henka_result sandbox3d_scene_document_bridge_begin_play(
    sandbox3d_scene_document_bridge* bridge);
henka_result sandbox3d_scene_document_bridge_end_play(
    sandbox3d_scene_document_bridge* bridge);
bool sandbox3d_scene_document_bridge_is_play_locked(
    const sandbox3d_scene_document_bridge* bridge);
henka_scene* sandbox3d_scene_document_bridge_get_scene(
    const sandbox3d_scene_document_bridge* bridge);
const henka_scene_document* sandbox3d_scene_document_bridge_get_document(
    const sandbox3d_scene_document_bridge* bridge);
henka_result sandbox3d_scene_document_bridge_get_object(
    const sandbox3d_scene_document_bridge* bridge,
    henka_scene_document_id document_id,
    henka_scene_document_object* out_object);

henka_result sandbox3d_scene_document_bridge_bind(
    sandbox3d_scene_document_bridge* bridge,
    henka_scene_document_id document_id,
    henka_entity entity);
henka_result sandbox3d_scene_document_bridge_unbind(
    sandbox3d_scene_document_bridge* bridge,
    henka_scene_document_id document_id);
henka_result sandbox3d_scene_document_bridge_get_entity(
    const sandbox3d_scene_document_bridge* bridge,
    henka_scene_document_id document_id,
    henka_entity* out_entity);

/* Applies validated authoring presentation values to the already-bound
 * runtime entity. Runtime resources and physics bodies remain external. */
henka_result sandbox3d_scene_document_bridge_apply_object(
    const sandbox3d_scene_document_bridge* bridge,
    henka_scene_document_id document_id);
/* Applies all bound object presentations after staging every fallible scene
 * allocation. A failure leaves the live scene unchanged. */
henka_result sandbox3d_scene_document_bridge_apply_objects(
    const sandbox3d_scene_document_bridge* bridge);
/* Applies one supplied object value to a candidate scene. The candidate must
 * be discarded unless all related authoring operations also succeed; this
 * helper does not mutate the borrowed Scene Document. */
henka_result sandbox3d_scene_document_bridge_apply_object_candidate(
    const sandbox3d_scene_document_bridge* bridge,
    henka_scene_document_id document_id,
    const henka_scene_document_object* object);

/* Applies all persisted parent links to bound runtime entities in parent-first
 * order while preserving each entity's authored world transform. The bridge
 * rejects bound children whose document parent is not bound and restores the
 * prior runtime parent links if a runtime parenting operation fails. */
henka_result sandbox3d_scene_document_bridge_apply_hierarchy(
    const sandbox3d_scene_document_bridge* bridge);
/* Applies hierarchy using one supplied candidate object in place of the
 * persisted document value. The borrowed Scene Document is not changed. */
henka_result sandbox3d_scene_document_bridge_apply_hierarchy_candidate(
    const sandbox3d_scene_document_bridge* bridge,
    henka_scene_document_id document_id,
    const henka_scene_document_object* object);

/* Applies an optional authored scene camera. Legacy documents without a
 * camera leave the current runtime camera unchanged. */
henka_result sandbox3d_scene_document_bridge_apply_camera(
    const sandbox3d_scene_document_bridge* bridge);

/* Copies the current runtime camera into the authored document for saving. */
henka_result sandbox3d_scene_document_bridge_sync_camera(
    sandbox3d_scene_document_bridge* bridge);

/* Copies runtime-owned presentation and hierarchy values back into a
 * candidate object and commits them through the Scene Document validation
 * boundary. A non-root runtime parent must be another bound document entity;
 * otherwise the sync fails closed. */
henka_result sandbox3d_scene_document_bridge_sync_object(
    sandbox3d_scene_document_bridge* bridge,
    henka_scene_document_id document_id);

/* Converts the pure authored physics component into the existing runtime
 * body descriptor without serializing runtime body IDs or pointers. */
henka_result sandbox3d_scene_document_bridge_make_physics_body_desc(
    const sandbox3d_scene_document_bridge* bridge,
    henka_scene_document_id document_id,
    henka_physics_body_desc* out_desc);

#endif
