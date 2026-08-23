#include "test_suite.h"

#include <string.h>

#include "../examples/sandbox3d/authoring_asset_commands.h"
#include "../examples/sandbox3d/authoring_asset_controller.h"
#include "../examples/sandbox3d/authoring_asset_ui.h"

void henka_test_sandbox3d_authoring_asset_ui(void)
{
    henka_engine_config config = {0};
    henka_engine* engine = NULL;
    henka_scene* scene = NULL;
    sandbox3d_authoring_asset_ui ui;
    sandbox3d_authoring_asset_ui_request request = {0};
    const sandbox3d_authoring_asset_document* document;
    henka_shader* basic_shader = NULL;
    size_t part_index = SIZE_MAX;
    char manifest_path[64];

    config.application_name = "Henka Native Authoring Asset UI Test";
    config.window_width = 320;
    config.window_height = 240;
    config.enable_vsync = false;
    config.asset_base_path = ".";
    HENKA_TEST_ASSERT(henka_engine_create(&config, &engine) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_assets_load_shader(
        henka_engine_get_asset_manager(engine),
        "assets/shaders/basic_lit.vert",
        "assets/shaders/basic_lit.frag",
        &basic_shader) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_create(&scene) == HENKA_SUCCESS);

    sandbox3d_authoring_asset_ui_init(&ui);
    request.action = SANDBOX3D_AUTHORING_ASSET_UI_ACTION_NEW_ASSET;
    request.name = "ui_asset";
    HENKA_TEST_ASSERT(sandbox3d_authoring_asset_ui_execute(
        &ui, engine, scene, &request) == HENKA_SUCCESS);
    document = sandbox3d_authoring_asset_ui_get_document(&ui);
    HENKA_TEST_ASSERT(document != NULL);

    request.name = "invalid name";
    HENKA_TEST_ASSERT(sandbox3d_authoring_asset_ui_execute(
        &ui, engine, scene, &request) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(sandbox3d_authoring_asset_ui_get_document(&ui) == document);
    HENKA_TEST_ASSERT(sandbox3d_authoring_asset_ui_get_active_part(&ui) == NULL);

    request.action = (sandbox3d_authoring_asset_ui_action)99;
    request.name = "invalid_action";
    HENKA_TEST_ASSERT(sandbox3d_authoring_asset_ui_execute(
        &ui, engine, scene, &request) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(sandbox3d_authoring_asset_document_get_part_count(document) == 0U);

    request.action = SANDBOX3D_AUTHORING_ASSET_UI_ACTION_ADD_CYLINDER;
    request.name = "ui_cylinder";
    request.primitive.radius = 0.5f;
    request.primitive.height = 2.0f;
    request.primitive.segments = 8U;
    request.history_steps = 8U;
    HENKA_TEST_ASSERT(sandbox3d_authoring_asset_ui_execute(
        &ui, engine, scene, &request) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_asset_ui_get_active_part(&ui) != NULL);
    HENKA_TEST_ASSERT(sandbox3d_authoring_asset_document_get_part_count(
        sandbox3d_authoring_asset_ui_get_document(&ui)) == 1U);

    HENKA_TEST_ASSERT(sandbox3d_authoring_asset_commands_get_manifest_path(
        "commands_asset", manifest_path, sizeof(manifest_path)) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(strcmp(manifest_path, "saves/commands_asset.asset") == 0);
    HENKA_TEST_ASSERT(sandbox3d_authoring_asset_commands_get_manifest_path(
        "../outside", manifest_path, sizeof(manifest_path)) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(sandbox3d_authoring_asset_commands_get_manifest_path(
        "nested/name", manifest_path, sizeof(manifest_path)) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(sandbox3d_authoring_asset_document_name_is_valid("safe_name-01"));
    HENKA_TEST_ASSERT(!sandbox3d_authoring_asset_document_name_is_valid("unsafe/name"));
    HENKA_TEST_ASSERT(sandbox3d_authoring_asset_commands_new_asset(
        &ui, engine, scene, "commands_asset") == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_asset_commands_add_primitive(
        &ui,
        engine,
        scene,
        "commands_asset",
        SANDBOX3D_AUTHORING_ASSET_UI_ACTION_ADD_BOX,
        "box",
        &part_index) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(part_index == 0U);
    HENKA_TEST_ASSERT(sandbox3d_authoring_asset_document_get_part_count(
        sandbox3d_authoring_asset_ui_get_document(&ui)) == 1U);

    HENKA_TEST_ASSERT(sandbox3d_authoring_asset_commands_add_primitive(
        &ui,
        engine,
        scene,
        "commands_asset",
        SANDBOX3D_AUTHORING_ASSET_UI_ACTION_ADD_QUAD_SPHERE,
        "quad_sphere",
        &part_index) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(part_index == 1U);
    {
        sandbox3d_authoring_object* quad_sphere =
            sandbox3d_authoring_asset_document_get_part(
                (sandbox3d_authoring_asset_document*)
                    sandbox3d_authoring_asset_ui_get_document(&ui),
                part_index);
        sandbox3d_authoring_primitive_kind kind =
            SANDBOX3D_AUTHORING_PRIMITIVE_BOX;
        henka_authoring_mesh_counts counts;
        HENKA_TEST_ASSERT(quad_sphere != NULL);
        counts = henka_authoring_mesh_get_counts(
            sandbox3d_authoring_object_get_mesh(quad_sphere));
        HENKA_TEST_ASSERT(counts.vertices == 386U);
        HENKA_TEST_ASSERT(counts.edges == 768U);
        HENKA_TEST_ASSERT(counts.faces == 384U);
        HENKA_TEST_ASSERT(sandbox3d_authoring_asset_document_get_part_kind(
            sandbox3d_authoring_asset_ui_get_document(&ui),
            quad_sphere,
            &kind) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(kind == SANDBOX3D_AUTHORING_PRIMITIVE_QUAD_SPHERE);
    }

    {
        sandbox3d_authoring_asset_controller* controller = NULL;
        const sandbox3d_authoring_asset_document* before_failed_load;
        size_t controller_part_index = SIZE_MAX;
        size_t controller_quad_sphere_index = SIZE_MAX;

        HENKA_TEST_ASSERT(sandbox3d_authoring_asset_controller_create(
            &controller) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(sandbox3d_authoring_asset_controller_new_asset(
            controller, engine, scene, "controller_asset") == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(sandbox3d_authoring_asset_controller_add_primitive(
            controller,
            engine,
            scene,
            "controller_asset",
            SANDBOX3D_AUTHORING_ASSET_UI_ACTION_ADD_BOX,
            "body",
            &controller_part_index) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(controller_part_index == 0U);
        HENKA_TEST_ASSERT(sandbox3d_authoring_asset_controller_add_primitive(
            controller,
            engine,
            scene,
            "controller_asset",
            SANDBOX3D_AUTHORING_ASSET_UI_ACTION_ADD_QUAD_SPHERE,
            "quad_sphere",
            &controller_quad_sphere_index) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(controller_quad_sphere_index == 1U);
        {
            henka_material controller_material = henka_material_default();
            controller_material.name = "Controller Material";
            controller_material.type = HENKA_MATERIAL_TYPE_LIT;
            controller_material.shader = basic_shader;
            controller_material.use_lighting = true;
            HENKA_TEST_ASSERT(henka_scene_set_entity_material(
                scene,
                sandbox3d_authoring_object_get_entity(
                    sandbox3d_authoring_asset_document_get_part(
                        sandbox3d_authoring_asset_controller_get_document(controller),
                        controller_part_index)),
                controller_material) == HENKA_SUCCESS);
            HENKA_TEST_ASSERT(henka_scene_set_entity_material(
                scene,
                sandbox3d_authoring_object_get_entity(
                    sandbox3d_authoring_asset_document_get_part(
                        sandbox3d_authoring_asset_controller_get_document(controller),
                        controller_quad_sphere_index)),
                controller_material) == HENKA_SUCCESS);
        }
        HENKA_TEST_ASSERT(sandbox3d_authoring_asset_controller_save(
            controller, engine) == HENKA_SUCCESS);
        before_failed_load = sandbox3d_authoring_asset_controller_get_document(
            controller);
        HENKA_TEST_ASSERT(before_failed_load != NULL);
        HENKA_TEST_ASSERT(sandbox3d_authoring_asset_controller_load(
            controller, engine, scene, "../outside", NULL) ==
            HENKA_ERROR_INVALID_ARGUMENT);
        HENKA_TEST_ASSERT(sandbox3d_authoring_asset_controller_get_document(
            controller) == before_failed_load);
        HENKA_TEST_ASSERT(sandbox3d_authoring_asset_controller_load(
            controller, engine, scene, "controller_asset", NULL) ==
            HENKA_SUCCESS);
        HENKA_TEST_ASSERT(sandbox3d_authoring_asset_controller_get_document(
            controller) != before_failed_load);
        HENKA_TEST_ASSERT(sandbox3d_authoring_asset_document_get_part_count(
            sandbox3d_authoring_asset_controller_get_document(controller)) == 2U);
        {
            sandbox3d_authoring_primitive_kind reloaded_kind =
                SANDBOX3D_AUTHORING_PRIMITIVE_BOX;
            sandbox3d_authoring_object* reloaded_quad_sphere =
                sandbox3d_authoring_asset_document_get_part(
                    (sandbox3d_authoring_asset_document*)
                        sandbox3d_authoring_asset_controller_get_document(controller),
                    controller_quad_sphere_index);
            HENKA_TEST_ASSERT(reloaded_quad_sphere != NULL);
            HENKA_TEST_ASSERT(sandbox3d_authoring_asset_document_get_part_kind(
                sandbox3d_authoring_asset_controller_get_document(controller),
                reloaded_quad_sphere,
                &reloaded_kind) == HENKA_SUCCESS);
            HENKA_TEST_ASSERT(
                reloaded_kind == SANDBOX3D_AUTHORING_PRIMITIVE_QUAD_SPHERE);
        }
        sandbox3d_authoring_asset_controller_destroy(controller);
    }

    {
        sandbox3d_authoring_asset_controller* controller = NULL;
        sandbox3d_authoring_asset_document* detached_document = NULL;
        sandbox3d_authoring_asset_document* candidate = NULL;

        HENKA_TEST_ASSERT(sandbox3d_authoring_asset_controller_create(
            &controller) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(sandbox3d_authoring_asset_document_create(
            engine, scene, "attached_asset", &candidate) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(sandbox3d_authoring_asset_controller_attach_document(
            controller, engine, scene, candidate) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(sandbox3d_authoring_asset_controller_get_document(
            controller) == candidate);
        HENKA_TEST_ASSERT(sandbox3d_authoring_asset_controller_get_ui(
            controller)->active_part_index == SIZE_MAX);
        HENKA_TEST_ASSERT(sandbox3d_authoring_asset_controller_detach_document(
            controller) == candidate);
        HENKA_TEST_ASSERT(sandbox3d_authoring_asset_controller_get_document(
            controller) == NULL);
        detached_document = candidate;
        sandbox3d_authoring_asset_controller_destroy(controller);
        sandbox3d_authoring_asset_document_destroy(detached_document);
    }

    sandbox3d_authoring_asset_ui_destroy(&ui);
    henka_scene_destroy(scene);
    henka_engine_destroy(engine);
}
