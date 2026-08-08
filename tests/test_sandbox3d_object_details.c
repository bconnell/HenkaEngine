#include "test_suite.h"

#include <stdint.h>

#include <henka/assets.h>
#include <henka/scene.h>

#include "../examples/sandbox3d/object_details_tools.h"

void henka_test_sandbox3d_object_details(void)
{
    henka_entity entity;
    henka_entity no_mesh_entity;
    henka_material material;
    henka_material_instance instance;
    henka_scene* scene;
    henka_mesh* fake_mesh;
    henka_shader* fake_shader;
    sandbox3d_material_editor_binding binding;
    sandbox3d_material_editor_binding duplicate_bindings[2];
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
        0.25f,
        0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        view.material.roughness,
        0.75f,
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

    henka_scene_destroy(scene);
}
