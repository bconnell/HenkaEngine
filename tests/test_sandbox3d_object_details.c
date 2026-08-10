#include "test_suite.h"

#include <stdint.h>
#include <string.h>

#include <henka/assets.h>
#include <henka/scene.h>

#include "../engine/src/henka_internal.h"
#include "../examples/sandbox3d/object_details_tools.h"

void henka_test_sandbox3d_object_details(void)
{
    henka_entity entity;
    henka_entity no_mesh_entity;
    henka_material material;
    henka_material_asset scene_asset;
    henka_material_instance instance;
    henka_scene* scene;
    henka_mesh* fake_mesh;
    henka_shader* fake_shader;
    sandbox3d_material_editor_binding binding;
    sandbox3d_material_editor_binding duplicate_bindings[2];
    sandbox3d_material_editor_binding scene_bindings[2];
    sandbox3d_selected_material_view view;

    scene = NULL;
    HENKA_TEST_ASSERT(
        henka_scene_create(&scene) == HENKA_SUCCESS);

    entity =
        henka_scene_create_entity_named(
            scene,
            "Material Entity");
    no_mesh_entity =
        henka_scene_create_entity_named(
            scene,
            "No Mesh Entity");

    HENKA_TEST_ASSERT(entity != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(no_mesh_entity != HENKA_INVALID_ENTITY);

    HENKA_TEST_ASSERT(
        sandbox3d_resolve_selected_material(
            scene,
            no_mesh_entity,
            NULL,
            0U,
            &view) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(
        view.access == SANDBOX3D_MATERIAL_ACCESS_NONE);
    HENKA_TEST_ASSERT(view.editor_binding == NULL);

    fake_shader = (henka_shader*)(uintptr_t)4U;
    fake_mesh = (henka_mesh*)(uintptr_t)1U;
    HENKA_TEST_ASSERT(
        henka_scene_set_entity_mesh(
            scene,
            entity,
            fake_mesh) == HENKA_SUCCESS);

    material = henka_material_default();
    material.shader = fake_shader;
    material.name = "Scene Material";
    material.metallic = 0.25f;
    material.roughness = 0.75f;
    HENKA_TEST_ASSERT(
        henka_material_validate(&material) == HENKA_SUCCESS);

    HENKA_TEST_ASSERT(
        henka_scene_set_entity_material(
            scene,
            entity,
            material) == HENKA_SUCCESS);

    memset(&scene_asset, 0, sizeof(scene_asset));
    scene_asset.material = material;
    scene_asset.material.name = "Shared Scene Material";
    scene_asset.material.roughness = 0.80f;
    scene_asset.revision = 3U;
    memset(scene_bindings, 0, sizeof(scene_bindings));
    HENKA_TEST_ASSERT(
        henka_scene_set_entity_material_asset(
            scene,
            entity,
            &scene_asset) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(
        sandbox3d_prepare_material_editor_binding(
            scene,
            entity,
            scene_bindings,
            2U) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(scene_bindings[0].valid);
    HENKA_TEST_ASSERT(scene_bindings[0].entity == entity);
    HENKA_TEST_ASSERT(scene_bindings[0].asset == &scene_asset);
    HENKA_TEST_ASSERT(scene_bindings[0].instance == &scene_bindings[0].owned_instance);
    HENKA_TEST_ASSERT(scene_bindings[0].instance->definition == &scene_asset);

    HENKA_TEST_ASSERT(
        henka_assets_material_instance_set_float(
            scene_bindings[0].instance,
            HENKA_MATERIAL_INSTANCE_METALLIC,
            0.65f) == HENKA_SUCCESS);
    scene_asset.material.roughness = 0.42f;
    scene_asset.revision = 4U;
    HENKA_TEST_ASSERT(
        sandbox3d_prepare_material_editor_binding(
            scene,
            entity,
            scene_bindings,
            2U) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(scene_bindings[0].instance->definition_revision == 4U);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        scene_bindings[0].instance->material.metallic,
        0.65f,
        0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        scene_bindings[0].instance->material.roughness,
        0.42f,
        0.0001f);
    HENKA_TEST_ASSERT(
        henka_scene_get_entity_material(
            scene,
            entity,
            &material) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(material.metallic, 0.65f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(material.roughness, 0.42f, 0.0001f);

    HENKA_TEST_ASSERT(
        sandbox3d_resolve_selected_material(
            scene,
            entity,
            NULL,
            0U,
            &view) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(
        view.access ==
            SANDBOX3D_MATERIAL_ACCESS_READ_ONLY);
    HENKA_TEST_ASSERT(view.editor_binding == NULL);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        view.material.metallic,
        0.65f,
        0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        view.material.roughness,
        0.42f,
        0.0001f);

    instance = (henka_material_instance){0};
    instance.definition =
        (const henka_material_asset*)(uintptr_t)2U;
    instance.material = henka_material_default();
    instance.material.shader = fake_shader;
    instance.material.name = "Editable Material";
    instance.material.metallic = 0.60f;
    instance.material.roughness = 0.35f;
    instance.definition_revision = 1U;

    HENKA_TEST_ASSERT(
        henka_material_validate(&instance.material) ==
            HENKA_SUCCESS);

    binding.entity = entity;
    binding.instance = &instance;
    binding.asset =
        (henka_material_asset*)(uintptr_t)2U;
    binding.valid = true;

    HENKA_TEST_ASSERT(
        sandbox3d_resolve_selected_material(
            scene,
            entity,
            &binding,
            1U,
            &view) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(
        view.access ==
            SANDBOX3D_MATERIAL_ACCESS_EDITABLE_INSTANCE);
    HENKA_TEST_ASSERT(view.editor_binding == &binding);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        view.material.metallic,
        0.60f,
        0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        view.material.roughness,
        0.35f,
        0.0001f);

    binding.valid = false;
    HENKA_TEST_ASSERT(
        sandbox3d_resolve_selected_material(
            scene,
            entity,
            &binding,
            1U,
            &view) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(
        view.access ==
            SANDBOX3D_MATERIAL_ACCESS_READ_ONLY);

    binding.valid = true;
    binding.asset =
        (henka_material_asset*)(uintptr_t)3U;
    HENKA_TEST_ASSERT(
        sandbox3d_resolve_selected_material(
            scene,
            entity,
            &binding,
            1U,
            &view) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(
        view.access ==
            SANDBOX3D_MATERIAL_ACCESS_READ_ONLY);

    duplicate_bindings[0] = binding;
    duplicate_bindings[0].asset =
        (henka_material_asset*)(uintptr_t)2U;
    duplicate_bindings[1] = duplicate_bindings[0];

    HENKA_TEST_ASSERT(
        sandbox3d_resolve_selected_material(
            scene,
            entity,
            duplicate_bindings,
            2U,
            &view) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(
        view.access == SANDBOX3D_MATERIAL_ACCESS_NONE);
    HENKA_TEST_ASSERT(view.editor_binding == NULL);

    HENKA_TEST_ASSERT(
        sandbox3d_resolve_selected_material(
            NULL,
            entity,
            NULL,
            0U,
            &view) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(
        sandbox3d_resolve_selected_material(
            scene,
            HENKA_INVALID_ENTITY,
            NULL,
            0U,
            &view) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(
        sandbox3d_resolve_selected_material(
            scene,
            entity,
            NULL,
            1U,
            &view) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(
        sandbox3d_resolve_selected_material(
            scene,
            entity,
            NULL,
            0U,
            NULL) == HENKA_ERROR_INVALID_ARGUMENT);


    {
        sandbox3d_selected_material_display display;
        sandbox3d_selected_material_view display_view;

        display_view = (sandbox3d_selected_material_view){0};
        display_view.access = SANDBOX3D_MATERIAL_ACCESS_NONE;
        HENKA_TEST_ASSERT(
            sandbox3d_format_selected_material_view(
                &display_view,
                &display) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(
            strcmp(display.material_slot, "None") == 0);
        HENKA_TEST_ASSERT(
            strcmp(display.mode, "None") == 0);

        display_view.access =
            SANDBOX3D_MATERIAL_ACCESS_READ_ONLY;
        display_view.material = henka_material_default();
        display_view.material.name = "Ground";
        display_view.material.base_color =
            (henka_vec4){0.10f, 0.20f, 0.30f, 1.0f};
        display_view.material.metallic = 0.25f;
        display_view.material.roughness = 0.75f;
        display_view.material.emissive_color =
            (henka_vec3){0.40f, 0.50f, 0.60f};
        display_view.material.emissive_strength = 2.0f;
        display_view.material.alpha_mode =
            HENKA_MATERIAL_ALPHA_MASKED;
        display_view.material.double_sided = true;
        display_view.material.ior = 1.45f;
        display_view.material.transmission = 0.35f;
        display_view.material.normal_texture =
            (henka_texture*)(uintptr_t)5U;

        HENKA_TEST_ASSERT(
            sandbox3d_format_selected_material_view(
                &display_view,
                &display) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(
            strcmp(display.material_slot, "Present") == 0);
        HENKA_TEST_ASSERT(
            strcmp(display.name, "Ground") == 0);
        HENKA_TEST_ASSERT(
            strcmp(display.base_color, "0.10 0.20 0.30 1.00") == 0);
        HENKA_TEST_ASSERT(
            strcmp(display.metallic, "0.25") == 0);
        HENKA_TEST_ASSERT(
            strcmp(display.roughness, "0.75") == 0);
        HENKA_TEST_ASSERT(
            strcmp(display.normal_map, "Assigned") == 0);
        HENKA_TEST_ASSERT(
            strcmp(display.emissive, "0.40 0.50 0.60 x2.00") == 0);
        HENKA_TEST_ASSERT(
            strcmp(display.alpha_mode, "Masked") == 0);
        HENKA_TEST_ASSERT(
            strcmp(display.double_sided, "Yes") == 0);
        HENKA_TEST_ASSERT(
            strcmp(display.ior, "1.45") == 0);
        HENKA_TEST_ASSERT(
            strcmp(display.transmission, "0.35") == 0);
        HENKA_TEST_ASSERT(
            strcmp(display.mode, "Read-only") == 0);

        display_view.access =
            SANDBOX3D_MATERIAL_ACCESS_EDITABLE_INSTANCE;
        HENKA_TEST_ASSERT(
            sandbox3d_format_selected_material_view(
                &display_view,
                &display) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(
            strcmp(display.mode, "Editable instance") == 0);

        HENKA_TEST_ASSERT(
            sandbox3d_format_selected_material_view(
                NULL,
                &display) == HENKA_ERROR_INVALID_ARGUMENT);
        HENKA_TEST_ASSERT(
            sandbox3d_format_selected_material_view(
                &display_view,
                NULL) == HENKA_ERROR_INVALID_ARGUMENT);
    }
    henka_scene_destroy(scene);
}
