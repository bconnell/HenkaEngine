#include "authoring_asset_ui.h"

#include <stdint.h>
#include <string.h>

static henka_result sandbox3d_authoring_asset_ui_action_to_primitive_kind(
    sandbox3d_authoring_asset_ui_action action,
    sandbox3d_authoring_primitive_kind* out_kind)
{
    if (out_kind == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    switch (action)
    {
        case SANDBOX3D_AUTHORING_ASSET_UI_ACTION_ADD_BOX:
            *out_kind = SANDBOX3D_AUTHORING_PRIMITIVE_BOX;
            return HENKA_SUCCESS;
        case SANDBOX3D_AUTHORING_ASSET_UI_ACTION_ADD_PLANE:
            *out_kind = SANDBOX3D_AUTHORING_PRIMITIVE_PLANE;
            return HENKA_SUCCESS;
        case SANDBOX3D_AUTHORING_ASSET_UI_ACTION_ADD_CYLINDER:
            *out_kind = SANDBOX3D_AUTHORING_PRIMITIVE_CYLINDER;
            return HENKA_SUCCESS;
        case SANDBOX3D_AUTHORING_ASSET_UI_ACTION_ADD_CONE:
            *out_kind = SANDBOX3D_AUTHORING_PRIMITIVE_CONE;
            return HENKA_SUCCESS;
        case SANDBOX3D_AUTHORING_ASSET_UI_ACTION_ADD_UV_SPHERE:
            *out_kind = SANDBOX3D_AUTHORING_PRIMITIVE_UV_SPHERE;
            return HENKA_SUCCESS;
        case SANDBOX3D_AUTHORING_ASSET_UI_ACTION_ADD_QUAD_SPHERE:
            *out_kind = SANDBOX3D_AUTHORING_PRIMITIVE_QUAD_SPHERE;
            return HENKA_SUCCESS;
        default:
            return HENKA_ERROR_INVALID_ARGUMENT;
    }
}

void sandbox3d_authoring_asset_ui_init(sandbox3d_authoring_asset_ui* ui)
{
    if (ui == NULL)
    {
        return;
    }

    memset(ui, 0, sizeof(*ui));
    ui->active_part_index = SIZE_MAX;
}

void sandbox3d_authoring_asset_ui_destroy(sandbox3d_authoring_asset_ui* ui)
{
    if (ui == NULL)
    {
        return;
    }

    sandbox3d_authoring_asset_document_destroy(ui->document);
    memset(ui, 0, sizeof(*ui));
    ui->active_part_index = SIZE_MAX;
}

henka_result sandbox3d_authoring_asset_ui_execute(
    sandbox3d_authoring_asset_ui* ui,
    henka_engine* engine,
    henka_scene* scene,
    const sandbox3d_authoring_asset_ui_request* request)
{
    sandbox3d_authoring_asset_document* candidate_document = NULL;
    sandbox3d_authoring_primitive_kind kind;
    henka_result result;
    size_t part_index;

    if (ui == NULL || engine == NULL || scene == NULL || request == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    if (request->action == SANDBOX3D_AUTHORING_ASSET_UI_ACTION_NEW_ASSET)
    {
        result = sandbox3d_authoring_asset_document_create(
            engine, scene, request->name, &candidate_document);
        if (result != HENKA_SUCCESS)
        {
            return result;
        }

        sandbox3d_authoring_asset_document_destroy(ui->document);
        ui->engine = engine;
        ui->scene = scene;
        ui->document = candidate_document;
        ui->active_part_index = SIZE_MAX;
        return HENKA_SUCCESS;
    }

    if (ui->document == NULL || ui->engine != engine || ui->scene != scene)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    result = sandbox3d_authoring_asset_ui_action_to_primitive_kind(
        request->action, &kind);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    result = sandbox3d_authoring_asset_document_add_primitive(
        ui->document,
        request->name,
        kind,
        &request->primitive,
        request->history_steps,
        &part_index);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    ui->active_part_index = part_index;
    return HENKA_SUCCESS;
}

const sandbox3d_authoring_asset_document*
sandbox3d_authoring_asset_ui_get_document(const sandbox3d_authoring_asset_ui* ui)
{
    return ui == NULL ? NULL : ui->document;
}

sandbox3d_authoring_object*
sandbox3d_authoring_asset_ui_get_active_part(const sandbox3d_authoring_asset_ui* ui)
{
    if (ui == NULL || ui->document == NULL || ui->active_part_index == SIZE_MAX)
    {
        return NULL;
    }
    return sandbox3d_authoring_asset_document_get_part(
        ui->document, ui->active_part_index);
}
