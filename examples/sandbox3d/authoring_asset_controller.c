#include "authoring_asset_controller.h"

#include <henka/memory.h>

#include "authoring_asset_commands.h"

struct sandbox3d_authoring_asset_controller
{
    sandbox3d_authoring_asset_ui ui;
};

henka_result sandbox3d_authoring_asset_controller_create(
    sandbox3d_authoring_asset_controller** out_controller)
{
    sandbox3d_authoring_asset_controller* controller;

    if (out_controller == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_controller = NULL;
    controller = (sandbox3d_authoring_asset_controller*)henka_malloc(
        sizeof(*controller));
    if (controller == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    sandbox3d_authoring_asset_ui_init(&controller->ui);
    *out_controller = controller;
    return HENKA_SUCCESS;
}

void sandbox3d_authoring_asset_controller_destroy(
    sandbox3d_authoring_asset_controller* controller)
{
    if (controller == NULL)
    {
        return;
    }
    sandbox3d_authoring_asset_ui_destroy(&controller->ui);
    henka_free(controller);
}

sandbox3d_authoring_asset_ui*
sandbox3d_authoring_asset_controller_get_ui(
    sandbox3d_authoring_asset_controller* controller)
{
    return controller == NULL ? NULL : &controller->ui;
}

sandbox3d_authoring_asset_document*
sandbox3d_authoring_asset_controller_get_document(
    sandbox3d_authoring_asset_controller* controller)
{
    return controller == NULL ? NULL : controller->ui.document;
}

henka_result sandbox3d_authoring_asset_controller_attach_document(
    sandbox3d_authoring_asset_controller* controller,
    henka_engine* engine,
    henka_scene* scene,
    sandbox3d_authoring_asset_document* document)
{
    if (controller == NULL || engine == NULL || scene == NULL ||
        document == NULL || controller->ui.document != NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    controller->ui.engine = engine;
    controller->ui.scene = scene;
    controller->ui.document = document;
    controller->ui.active_part_index =
        sandbox3d_authoring_asset_document_get_part_count(document) == 0U
            ? SIZE_MAX : 0U;
    return HENKA_SUCCESS;
}

sandbox3d_authoring_asset_document*
sandbox3d_authoring_asset_controller_detach_document(
    sandbox3d_authoring_asset_controller* controller)
{
    sandbox3d_authoring_asset_document* document;

    if (controller == NULL)
    {
        return NULL;
    }
    document = controller->ui.document;
    controller->ui.engine = NULL;
    controller->ui.scene = NULL;
    controller->ui.document = NULL;
    controller->ui.active_part_index = SIZE_MAX;
    return document;
}

henka_result sandbox3d_authoring_asset_controller_new_asset(
    sandbox3d_authoring_asset_controller* controller,
    henka_engine* engine,
    henka_scene* scene,
    const char* asset_name)
{
    if (controller == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return sandbox3d_authoring_asset_commands_new_asset(
        &controller->ui, engine, scene, asset_name);
}

henka_result sandbox3d_authoring_asset_controller_add_primitive(
    sandbox3d_authoring_asset_controller* controller,
    henka_engine* engine,
    henka_scene* scene,
    const char* asset_name,
    sandbox3d_authoring_asset_ui_action action,
    const char* part_prefix,
    size_t* out_part_index)
{
    if (controller == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return sandbox3d_authoring_asset_commands_add_primitive(
        &controller->ui,
        engine,
        scene,
        asset_name,
        action,
        part_prefix,
        out_part_index);
}

henka_result sandbox3d_authoring_asset_controller_save(
    sandbox3d_authoring_asset_controller* controller,
    henka_engine* engine)
{
    if (controller == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return sandbox3d_authoring_asset_commands_save(&controller->ui, engine);
}

henka_result sandbox3d_authoring_asset_controller_load(
    sandbox3d_authoring_asset_controller* controller,
    henka_engine* engine,
    henka_scene* scene,
    const char* asset_name,
    const henka_material* material_template)
{
    sandbox3d_authoring_asset_document* candidate = NULL;
    sandbox3d_authoring_asset_document* previous;
    char relative_manifest_path[128];
    henka_result result;

    if (controller == NULL || engine == NULL || scene == NULL ||
        !sandbox3d_authoring_asset_document_name_is_valid(asset_name))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = sandbox3d_authoring_asset_commands_get_manifest_path(
        asset_name, relative_manifest_path, sizeof(relative_manifest_path));
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    result = sandbox3d_authoring_asset_document_load(
        engine,
        scene,
        henka_engine_get_user_data_base_path(engine),
        relative_manifest_path,
        32U,
        material_template,
        &candidate);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    previous = controller->ui.document;
    controller->ui.engine = engine;
    controller->ui.scene = scene;
    controller->ui.document = candidate;
    controller->ui.active_part_index =
        sandbox3d_authoring_asset_document_get_part_count(candidate) == 0U
            ? SIZE_MAX : 0U;
    sandbox3d_authoring_asset_document_destroy(previous);
    return HENKA_SUCCESS;
}
