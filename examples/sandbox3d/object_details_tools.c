#include "object_details_tools.h"

#include <string.h>

henka_result sandbox3d_resolve_selected_material(
    const henka_scene* scene,
    henka_entity entity,
    sandbox3d_material_editor_binding* bindings,
    size_t binding_count,
    sandbox3d_selected_material_view* out_view)
{
    henka_material scene_material;
    henka_mesh* mesh;
    sandbox3d_material_editor_binding* matched_binding;
    size_t index;
    size_t match_count;

    if (out_view != NULL)
    {
        memset(out_view, 0, sizeof(*out_view));
        out_view->access = SANDBOX3D_MATERIAL_ACCESS_NONE;
        out_view->material = henka_material_default();
    }

    if (scene == NULL ||
        entity == HENKA_INVALID_ENTITY ||
        out_view == NULL ||
        (bindings == NULL && binding_count != 0U) ||
        !henka_scene_is_entity_valid(scene, entity))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    mesh = NULL;
    if (henka_scene_get_entity_mesh(
            scene,
            entity,
            &mesh) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    if (mesh == NULL)
    {
        return HENKA_SUCCESS;
    }

    if (henka_scene_get_entity_material(
            scene,
            entity,
            &scene_material) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    out_view->access = SANDBOX3D_MATERIAL_ACCESS_READ_ONLY;
    out_view->material = scene_material;

    matched_binding = NULL;
    match_count = 0U;

    for (index = 0U; index < binding_count; ++index)
    {
        sandbox3d_material_editor_binding* binding =
            &bindings[index];

        if (!binding->valid || binding->entity != entity)
        {
            continue;
        }

        ++match_count;
        matched_binding = binding;
    }

    if (match_count > 1U)
    {
        memset(out_view, 0, sizeof(*out_view));
        out_view->access = SANDBOX3D_MATERIAL_ACCESS_NONE;
        out_view->material = henka_material_default();
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    if (matched_binding != NULL &&
        matched_binding->instance != NULL &&
        matched_binding->asset != NULL &&
        matched_binding->instance->definition ==
            matched_binding->asset)
    {
        henka_material effective_material;

        if (henka_assets_get_material_instance_material(
                matched_binding->instance,
                &effective_material) == HENKA_SUCCESS)
        {
            out_view->access =
                SANDBOX3D_MATERIAL_ACCESS_EDITABLE_INSTANCE;
            out_view->material = effective_material;
            out_view->editor_binding = matched_binding;
        }
    }

    return HENKA_SUCCESS;
}
