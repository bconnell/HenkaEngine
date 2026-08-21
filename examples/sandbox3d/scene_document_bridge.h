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

/* Copies only runtime-owned presentation values back into a candidate object
 * and commits them through the Scene Document validation boundary. */
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
