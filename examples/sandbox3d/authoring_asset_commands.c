#include "authoring_asset_commands.h"

#include <stdio.h>
#include <string.h>

static bool sandbox3d_authoring_asset_commands_is_primitive_action(
    sandbox3d_authoring_asset_ui_action action)
{
    return action >= SANDBOX3D_AUTHORING_ASSET_UI_ACTION_ADD_BOX &&
        action <= SANDBOX3D_AUTHORING_ASSET_UI_ACTION_ADD_QUAD_SPHERE;
}

henka_result sandbox3d_authoring_asset_commands_get_manifest_path(
    const char* asset_name,
    char* output,
    size_t output_capacity)
{
    int length;

    if (!sandbox3d_authoring_asset_document_name_is_valid(asset_name) ||
        output == NULL ||
        output_capacity == 0U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    length = snprintf(output, output_capacity, "saves/%s.asset", asset_name);
    if (length <= 0 || (size_t)length >= output_capacity)
    {
        output[0] = '\0';
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return HENKA_SUCCESS;
}

henka_result sandbox3d_authoring_asset_commands_new_asset(
    sandbox3d_authoring_asset_ui* ui,
    henka_engine* engine,
    henka_scene* scene,
    const char* asset_name)
{
    sandbox3d_authoring_asset_ui_request request;

    if (ui == NULL || engine == NULL || scene == NULL ||
        !sandbox3d_authoring_asset_document_name_is_valid(asset_name))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    memset(&request, 0, sizeof(request));
    request.action = SANDBOX3D_AUTHORING_ASSET_UI_ACTION_NEW_ASSET;
    request.name = asset_name;
    return sandbox3d_authoring_asset_ui_execute(ui, engine, scene, &request);
}

henka_result sandbox3d_authoring_asset_commands_add_primitive(
    sandbox3d_authoring_asset_ui* ui,
    henka_engine* engine,
    henka_scene* scene,
    const char* asset_name,
    sandbox3d_authoring_asset_ui_action action,
    const char* part_prefix,
    size_t* out_part_index)
{
    sandbox3d_authoring_asset_ui_request request;
    sandbox3d_authoring_asset_document* document;
    char part_name[64];
    int name_length;
    bool created_here = false;
    henka_result result;
    size_t part_count;

    if (ui == NULL || engine == NULL || scene == NULL ||
        !sandbox3d_authoring_asset_document_name_is_valid(asset_name) ||
        part_prefix == NULL ||
        part_prefix[0] == '\0' || out_part_index == NULL ||
        !sandbox3d_authoring_asset_commands_is_primitive_action(action))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_part_index = SIZE_MAX;

    document = (sandbox3d_authoring_asset_document*)
        sandbox3d_authoring_asset_ui_get_document(ui);
    if (document == NULL)
    {
        result = sandbox3d_authoring_asset_commands_new_asset(
            ui, engine, scene, asset_name);
        if (result != HENKA_SUCCESS)
        {
            return result;
        }
        created_here = true;
        document = (sandbox3d_authoring_asset_document*)
            sandbox3d_authoring_asset_ui_get_document(ui);
    }
    if (document == NULL)
    {
        return HENKA_ERROR_UNKNOWN;
    }
    if (strcmp(
            sandbox3d_authoring_asset_document_get_name(document),
            asset_name) != 0)
    {
        if (created_here)
        {
            sandbox3d_authoring_asset_ui_destroy(ui);
        }
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    name_length = snprintf(
        part_name,
        sizeof(part_name),
        "%s_%zu",
        part_prefix,
        sandbox3d_authoring_asset_document_get_part_count(document) + 1U);
    if (name_length <= 0 || (size_t)name_length >= sizeof(part_name))
    {
        if (created_here)
        {
            sandbox3d_authoring_asset_ui_destroy(ui);
        }
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    memset(&request, 0, sizeof(request));
    request.action = action;
    request.name = part_name;
    request.history_steps = 32U;
    request.primitive.width = 1.0f;
    request.primitive.height = 1.0f;
    request.primitive.depth = 1.0f;
    request.primitive.radius = 0.5f;
    /* New native assets should begin with a smooth, authorable surface.  Keep
     * the tessellation bounded by the public constructor hard limit while
     * avoiding a visibly faceted low-detail source that users must repair
     * before ordinary modeling can even be evaluated. */
    request.primitive.segments = HENKA_AUTHORING_MESH_HARD_MAX_FACE_CORNERS;
    request.primitive.latitude_segments = 24U;
    request.primitive.subdivisions = 8U;
    if (action == SANDBOX3D_AUTHORING_ASSET_UI_ACTION_ADD_CYLINDER ||
        action == SANDBOX3D_AUTHORING_ASSET_UI_ACTION_ADD_CONE)
    {
        request.primitive.height = 2.0f;
    }

    result = sandbox3d_authoring_asset_ui_execute(ui, engine, scene, &request);
    if (result != HENKA_SUCCESS && created_here)
    {
        sandbox3d_authoring_asset_ui_destroy(ui);
    }
    if (result == HENKA_SUCCESS)
    {
        part_count = sandbox3d_authoring_asset_document_get_part_count(document);
        if (part_count == 0U)
        {
            return HENKA_ERROR_UNKNOWN;
        }
        *out_part_index = part_count - 1U;
    }
    return result;
}

henka_result sandbox3d_authoring_asset_commands_save(
    sandbox3d_authoring_asset_ui* ui,
    henka_engine* engine)
{
    sandbox3d_authoring_asset_document* document;
    char relative_manifest_path[128];
    henka_result result;

    if (ui == NULL || engine == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    document = ui->document;
    if (document == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = sandbox3d_authoring_asset_commands_get_manifest_path(
        sandbox3d_authoring_asset_document_get_name(document),
        relative_manifest_path,
        sizeof(relative_manifest_path));
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    return sandbox3d_authoring_asset_document_save(
        document,
        henka_engine_get_user_data_base_path(engine),
        relative_manifest_path);
}
