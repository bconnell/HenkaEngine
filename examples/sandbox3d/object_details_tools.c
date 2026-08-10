#include "object_details_tools.h"

#include <stdio.h>
#include <string.h>

static void sandbox3d_clear_material_editor_binding(
    sandbox3d_material_editor_binding* binding)
{
    if (binding != NULL)
    {
        memset(binding, 0, sizeof(*binding));
        binding->entity = HENKA_INVALID_ENTITY;
    }
}

henka_result sandbox3d_prepare_material_editor_binding(
    henka_scene* scene,
    henka_entity entity,
    sandbox3d_material_editor_binding* bindings,
    size_t binding_count)
{
    const henka_material_asset* scene_asset = NULL;
    sandbox3d_material_editor_binding* binding = NULL;
    size_t index;
    henka_result result;

    if (scene == NULL || entity == HENKA_INVALID_ENTITY ||
        (bindings == NULL && binding_count != 0U) ||
        !henka_scene_is_entity_valid(scene, entity))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    /* Scene identity is the lifetime boundary for these stack-owned editor
     * instances. Reclaim entries for deleted entities or entities whose
     * material definition was detached before looking for a free slot. */
    for (index = 0U; index < binding_count; ++index)
    {
        const henka_material_asset* binding_asset = NULL;

        if (!bindings[index].valid ||
            !henka_scene_is_entity_valid(scene, bindings[index].entity) ||
            henka_scene_get_entity_material_asset(
                scene, bindings[index].entity, &binding_asset) != HENKA_SUCCESS ||
            binding_asset == NULL)
        {
            if (bindings[index].valid)
            {
                sandbox3d_clear_material_editor_binding(&bindings[index]);
            }
        }
    }

    for (index = 0U; index < binding_count; ++index)
    {
        if (bindings[index].valid && bindings[index].entity == entity)
        {
            binding = &bindings[index];
            break;
        }
    }

    if (henka_scene_get_entity_material_asset(
            scene, entity, &scene_asset) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    if (scene_asset == NULL)
    {
        if (binding != NULL)
        {
            sandbox3d_clear_material_editor_binding(binding);
        }
        return HENKA_SUCCESS;
    }

    if (binding == NULL)
    {
        for (index = 0U; index < binding_count; ++index)
        {
            if (!bindings[index].valid)
            {
                binding = &bindings[index];
                break;
            }
        }
        if (binding == NULL)
        {
            return HENKA_ERROR_LIMIT;
        }
        sandbox3d_clear_material_editor_binding(binding);
        binding->entity = entity;
        binding->asset = (henka_material_asset*)scene_asset;
        result = henka_assets_create_material_instance(
            scene_asset, &binding->owned_instance);
        if (result != HENKA_SUCCESS)
        {
            sandbox3d_clear_material_editor_binding(binding);
            return result;
        }
        binding->instance = &binding->owned_instance;
        binding->valid = true;
        return HENKA_SUCCESS;
    }

    if (binding->asset != (henka_material_asset*)scene_asset ||
        binding->instance == NULL ||
        binding->instance->definition != scene_asset)
    {
        henka_material_instance candidate;

        result = henka_assets_create_material_instance(
            scene_asset, &candidate);
        if (result != HENKA_SUCCESS)
        {
            return result;
        }
        binding->owned_instance = candidate;
        binding->instance = &binding->owned_instance;
        binding->asset = (henka_material_asset*)scene_asset;
        binding->entity = entity;
        binding->valid = true;
        return HENKA_SUCCESS;
    }

    {
        uint64_t revision = 0U;

        result = henka_assets_get_material_asset_revision(
            scene_asset, &revision);
        if (result != HENKA_SUCCESS)
        {
            return result;
        }
        if (binding->instance->definition_revision != revision)
        {
            henka_material_instance candidate = *binding->instance;

            result = henka_assets_refresh_material_instance(&candidate);
            if (result != HENKA_SUCCESS)
            {
                return result;
            }
            result = henka_assets_apply_material_instance_to_entity(
                &candidate, scene, entity);
            if (result != HENKA_SUCCESS)
            {
                return result;
            }
            binding->owned_instance = candidate;
            binding->instance = &binding->owned_instance;
        }
    }

    return HENKA_SUCCESS;
}

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

static const char* sandbox3d_material_alpha_mode_label(
    henka_material_alpha_mode mode)
{
    switch (mode)
    {
        case HENKA_MATERIAL_ALPHA_MASKED:
            return "Masked";
        case HENKA_MATERIAL_ALPHA_BLENDED:
            return "Blended";
        case HENKA_MATERIAL_ALPHA_OPAQUE:
        default:
            return "Opaque";
    }
}

henka_result sandbox3d_format_selected_material_view(
    const sandbox3d_selected_material_view* view,
    sandbox3d_selected_material_display* out_display)
{
    const henka_material* material;

    if (view == NULL || out_display == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    memset(out_display, 0, sizeof(*out_display));
    snprintf(
        out_display->material_slot,
        sizeof(out_display->material_slot),
        "%s",
        view->access == SANDBOX3D_MATERIAL_ACCESS_NONE ?
            "None" :
            "Present");
    snprintf(
        out_display->mode,
        sizeof(out_display->mode),
        "%s",
        view->access == SANDBOX3D_MATERIAL_ACCESS_EDITABLE_INSTANCE ?
            "Editable instance" :
            (view->access == SANDBOX3D_MATERIAL_ACCESS_READ_ONLY ?
                "Read-only" :
                "None"));

    if (view->access == SANDBOX3D_MATERIAL_ACCESS_NONE)
    {
        snprintf(out_display->name, sizeof(out_display->name), "%s", "(none)");
        snprintf(out_display->base_color, sizeof(out_display->base_color), "%s", "(none)");
        snprintf(out_display->metallic, sizeof(out_display->metallic), "%s", "(none)");
        snprintf(out_display->roughness, sizeof(out_display->roughness), "%s", "(none)");
        snprintf(out_display->normal_map, sizeof(out_display->normal_map), "%s", "None");
        snprintf(out_display->emissive, sizeof(out_display->emissive), "%s", "(none)");
        snprintf(out_display->alpha_mode, sizeof(out_display->alpha_mode), "%s", "(none)");
        snprintf(out_display->double_sided, sizeof(out_display->double_sided), "%s", "(none)");
        snprintf(out_display->ior, sizeof(out_display->ior), "%s", "(none)");
        snprintf(out_display->transmission, sizeof(out_display->transmission), "%s", "(none)");
        return HENKA_SUCCESS;
    }

    material = &view->material;
    snprintf(
        out_display->name,
        sizeof(out_display->name),
        "%s",
        material->name != NULL && material->name[0] != '\0' ?
            material->name :
            "Material");
    snprintf(
        out_display->base_color,
        sizeof(out_display->base_color),
        "%.2f %.2f %.2f %.2f",
        material->base_color.x,
        material->base_color.y,
        material->base_color.z,
        material->base_color.w);
    snprintf(
        out_display->metallic,
        sizeof(out_display->metallic),
        "%.2f",
        material->metallic);
    snprintf(
        out_display->roughness,
        sizeof(out_display->roughness),
        "%.2f",
        material->roughness);
    snprintf(
        out_display->normal_map,
        sizeof(out_display->normal_map),
        "%s",
        material->normal_texture != NULL ?
            "Assigned" :
            "None");
    snprintf(
        out_display->emissive,
        sizeof(out_display->emissive),
        "%.2f %.2f %.2f x%.2f",
        material->emissive_color.x,
        material->emissive_color.y,
        material->emissive_color.z,
        material->emissive_strength);
    snprintf(
        out_display->alpha_mode,
        sizeof(out_display->alpha_mode),
        "%s",
        sandbox3d_material_alpha_mode_label(
            material->alpha_mode));
    snprintf(
        out_display->double_sided,
        sizeof(out_display->double_sided),
        "%s",
        material->double_sided ? "Yes" : "No");
    snprintf(
        out_display->ior,
        sizeof(out_display->ior),
        "%.2f",
        material->ior);
    snprintf(
        out_display->transmission,
        sizeof(out_display->transmission),
        "%.2f",
        material->transmission);
    return HENKA_SUCCESS;
}
