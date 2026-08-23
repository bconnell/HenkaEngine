#ifndef SANDBOX3D_AUTHORING_ASSET_UI_H
#define SANDBOX3D_AUTHORING_ASSET_UI_H

#include "authoring_asset_document.h"

typedef enum sandbox3d_authoring_asset_ui_action
{
    SANDBOX3D_AUTHORING_ASSET_UI_ACTION_NEW_ASSET = 0,
    SANDBOX3D_AUTHORING_ASSET_UI_ACTION_ADD_BOX,
    SANDBOX3D_AUTHORING_ASSET_UI_ACTION_ADD_PLANE,
    SANDBOX3D_AUTHORING_ASSET_UI_ACTION_ADD_CYLINDER,
    SANDBOX3D_AUTHORING_ASSET_UI_ACTION_ADD_CONE,
    SANDBOX3D_AUTHORING_ASSET_UI_ACTION_ADD_UV_SPHERE,
    SANDBOX3D_AUTHORING_ASSET_UI_ACTION_ADD_QUAD_SPHERE
} sandbox3d_authoring_asset_ui_action;

typedef struct sandbox3d_authoring_asset_ui_request
{
    sandbox3d_authoring_asset_ui_action action;
    const char* name;
    sandbox3d_authoring_primitive_desc primitive;
    size_t history_steps;
} sandbox3d_authoring_asset_ui_request;

typedef struct sandbox3d_authoring_asset_ui
{
    henka_engine* engine;
    henka_scene* scene;
    sandbox3d_authoring_asset_document* document;
    size_t active_part_index;
} sandbox3d_authoring_asset_ui;

void sandbox3d_authoring_asset_ui_init(sandbox3d_authoring_asset_ui* ui);
void sandbox3d_authoring_asset_ui_destroy(sandbox3d_authoring_asset_ui* ui);
henka_result sandbox3d_authoring_asset_ui_execute(
    sandbox3d_authoring_asset_ui* ui,
    henka_engine* engine,
    henka_scene* scene,
    const sandbox3d_authoring_asset_ui_request* request);
const sandbox3d_authoring_asset_document*
sandbox3d_authoring_asset_ui_get_document(const sandbox3d_authoring_asset_ui* ui);
sandbox3d_authoring_object*
sandbox3d_authoring_asset_ui_get_active_part(const sandbox3d_authoring_asset_ui* ui);

#endif
