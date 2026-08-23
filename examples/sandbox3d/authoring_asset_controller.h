#ifndef SANDBOX3D_AUTHORING_ASSET_CONTROLLER_H
#define SANDBOX3D_AUTHORING_ASSET_CONTROLLER_H

#include <stddef.h>

#include <henka/engine.h>

#include "authoring_asset_ui.h"

typedef struct sandbox3d_authoring_asset_controller
    sandbox3d_authoring_asset_controller;

/* Allocates one generic editor authoring controller. The controller owns its
 * UI/document state after creation and never owns showcase fixture state. */
henka_result sandbox3d_authoring_asset_controller_create(
    sandbox3d_authoring_asset_controller** out_controller);
void sandbox3d_authoring_asset_controller_destroy(
    sandbox3d_authoring_asset_controller* controller);

/* The UI view is exposed only as a bridge for editor orchestration that must
 * register physics, selection, or game-authoring bindings around a generic
 * document operation. The controller remains the lifecycle owner. */
sandbox3d_authoring_asset_ui*
sandbox3d_authoring_asset_controller_get_ui(
    sandbox3d_authoring_asset_controller* controller);
sandbox3d_authoring_asset_document*
sandbox3d_authoring_asset_controller_get_document(
    sandbox3d_authoring_asset_controller* controller);

/* Transfers document ownership into or out of the controller.  Attach is
 * intentionally exclusive: callers must complete external scene/editor
 * binding work before publishing a candidate document. */
henka_result sandbox3d_authoring_asset_controller_attach_document(
    sandbox3d_authoring_asset_controller* controller,
    henka_engine* engine,
    henka_scene* scene,
    sandbox3d_authoring_asset_document* document);
sandbox3d_authoring_asset_document*
sandbox3d_authoring_asset_controller_detach_document(
    sandbox3d_authoring_asset_controller* controller);

henka_result sandbox3d_authoring_asset_controller_new_asset(
    sandbox3d_authoring_asset_controller* controller,
    henka_engine* engine,
    henka_scene* scene,
    const char* asset_name);
henka_result sandbox3d_authoring_asset_controller_add_primitive(
    sandbox3d_authoring_asset_controller* controller,
    henka_engine* engine,
    henka_scene* scene,
    const char* asset_name,
    sandbox3d_authoring_asset_ui_action action,
    const char* part_prefix,
    size_t* out_part_index);
henka_result sandbox3d_authoring_asset_controller_save(
    sandbox3d_authoring_asset_controller* controller,
    henka_engine* engine);

/* Loads a candidate document and publishes it only after the complete load
 * succeeds. A failed load leaves the existing controller document untouched. */
henka_result sandbox3d_authoring_asset_controller_load(
    sandbox3d_authoring_asset_controller* controller,
    henka_engine* engine,
    henka_scene* scene,
    const char* asset_name,
    const henka_material* material_template);

#endif
