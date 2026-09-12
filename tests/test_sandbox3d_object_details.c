#include "test_suite.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <henka/assets.h>
#include <henka/engine.h>
#include <henka/mesh.h>
#include <henka/scene.h>

#include "../engine/src/henka_internal.h"
#include "../examples/sandbox3d/object_details_tools.h"

static bool sandbox3d_object_details_write_file(
    const char* path,
    const void* data,
    size_t size)
{
    FILE* file;
    size_t written;

    if (path == NULL || data == NULL || size == 0U) return false;
    if (fopen_s(&file, path, "wb") != 0 || file == NULL) return false;
    written = fwrite(data, 1U, size, file);
    if (fclose(file) != 0) return false;
    return written == size;
}

static void henka_test_sandbox3d_object_details_file_backed_refresh(void)
{
    static const unsigned char bmp_a[] =
    {
        0x42, 0x4d, 0x3a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x36, 0x00, 0x00, 0x00,
        0x28, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00,
        0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0xff, 0x00, 0x00
    };
    static const unsigned char bmp_b[] =
    {
        0x42, 0x4d, 0x3a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x36, 0x00, 0x00, 0x00,
        0x28, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00,
        0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0xff, 0x00, 0x00, 0x00
    };
    static const char* gltf_a =
        "{\"asset\":{\"version\":\"2.0\"},"
        "\"buffers\":[{\"uri\":\"data:application/octet-stream;base64,"
        "AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAA\",\"byteLength\":36}],"
        "\"bufferViews\":[{\"buffer\":0,\"byteLength\":36}],"
        "\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\"}],"
        "\"images\":[{\"uri\":\"editor-material-a.bmp\"}],"
        "\"textures\":[{\"source\":0}],"
        "\"materials\":[{\"pbrMetallicRoughness\":{"
        "\"baseColorFactor\":[0.8,0.2,0.1,1.0],\"baseColorTexture\":{\"index\":0},"
        "\"roughnessFactor\":0.25}}],"
        "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0},\"material\":0}]}]}";
    static const char* gltf_b =
        "{\"asset\":{\"version\":\"2.0\"},"
        "\"buffers\":[{\"uri\":\"data:application/octet-stream;base64,"
        "AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAA\",\"byteLength\":36}],"
        "\"bufferViews\":[{\"buffer\":0,\"byteLength\":36}],"
        "\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\"}],"
        "\"images\":[{\"uri\":\"editor-material-b.bmp\"}],"
        "\"textures\":[{\"source\":0}],"
        "\"materials\":[{\"pbrMetallicRoughness\":{"
        "\"baseColorFactor\":[0.1,0.7,0.9,1.0],\"baseColorTexture\":{\"index\":0},"
        "\"roughnessFactor\":0.65}}],"
        "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0},\"material\":0}]}]}";
    const char* gltf_path = "build/test_tmp/editor-material-instance-refresh.gltf";
    const char* image_a_path = "build/test_tmp/editor-material-a.bmp";
    const char* image_b_path = "build/test_tmp/editor-material-b.bmp";
    henka_engine_config config = {0};
    henka_engine* engine = NULL;
    henka_asset_manager* manager = NULL;
    henka_shader* shader = NULL;
    henka_material_asset* asset = NULL;
    henka_material_asset* reloaded_asset = NULL;
    henka_material material = henka_material_default();
    henka_material scene_material = henka_material_default();
    henka_mesh* mesh = NULL;
    henka_scene* scene = NULL;
    henka_entity entity = HENKA_INVALID_ENTITY;
    sandbox3d_material_editor_binding binding = {0};
    sandbox3d_selected_material_view view = {0};
    uint64_t scene_material_revision = 0U;
    bool scene_material_overridden = false;

#define HENKA_OBJECT_DETAILS_REQUIRE(condition) \
    do \
    { \
        if (!(condition)) \
        { \
            fprintf(stderr, "assertion failed at %s:%d: %s\n", __FILE__, __LINE__, #condition); \
            ++g_henka_test_failures; \
            goto cleanup; \
        } \
    } while (0)

    config.application_name = "Henka File Material Editor Refresh Test";
    config.window_width = 320;
    config.window_height = 240;
    config.enable_vsync = false;
    config.asset_base_path = ".";
    HENKA_OBJECT_DETAILS_REQUIRE(henka_engine_create(&config, &engine) == HENKA_SUCCESS);
    manager = henka_engine_get_asset_manager(engine);
    HENKA_OBJECT_DETAILS_REQUIRE(manager != NULL);
    HENKA_OBJECT_DETAILS_REQUIRE(henka_assets_load_shader(
        manager,
        "assets/shaders/basic_lit.vert",
        "assets/shaders/basic_lit.frag",
        &shader) == HENKA_SUCCESS);
    HENKA_OBJECT_DETAILS_REQUIRE(sandbox3d_object_details_write_file(
        image_a_path, bmp_a, sizeof(bmp_a)));
    HENKA_OBJECT_DETAILS_REQUIRE(sandbox3d_object_details_write_file(
        image_b_path, bmp_b, sizeof(bmp_b)));
    HENKA_OBJECT_DETAILS_REQUIRE(sandbox3d_object_details_write_file(
        gltf_path, gltf_a, strlen(gltf_a)));
    HENKA_OBJECT_DETAILS_REQUIRE(henka_assets_load_gltf_material_asset(
        manager, gltf_path, shader, &asset) == HENKA_SUCCESS);
    HENKA_OBJECT_DETAILS_REQUIRE(henka_assets_get_material_asset_material(
        asset, &material) == HENKA_SUCCESS);
    HENKA_OBJECT_DETAILS_REQUIRE(material.base_color_texture != NULL);
    HENKA_OBJECT_DETAILS_REQUIRE(henka_scene_create(&scene) == HENKA_SUCCESS);
    entity = henka_scene_create_entity_named(scene, "File Material Editor Target");
    HENKA_OBJECT_DETAILS_REQUIRE(entity != HENKA_INVALID_ENTITY);
    HENKA_OBJECT_DETAILS_REQUIRE(henka_mesh_create_cube(engine, &mesh) == HENKA_SUCCESS);
    HENKA_OBJECT_DETAILS_REQUIRE(henka_scene_set_entity_mesh(scene, entity, mesh) == HENKA_SUCCESS);
    HENKA_OBJECT_DETAILS_REQUIRE(henka_scene_apply_material_asset(
        scene, entity, asset, material, asset->revision) == HENKA_SUCCESS);
    HENKA_OBJECT_DETAILS_REQUIRE(sandbox3d_prepare_material_editor_binding(
        scene, entity, &binding, 1U) == HENKA_SUCCESS);
    HENKA_OBJECT_DETAILS_REQUIRE(binding.valid && binding.asset == asset);
    HENKA_OBJECT_DETAILS_REQUIRE(binding.instance == &binding.owned_instance);
    HENKA_OBJECT_DETAILS_REQUIRE(sandbox3d_resolve_selected_material(
        scene, entity, &binding, 1U, &view) == HENKA_SUCCESS);
    HENKA_OBJECT_DETAILS_REQUIRE(
        view.access == SANDBOX3D_MATERIAL_ACCESS_EDITABLE_INSTANCE);
    HENKA_OBJECT_DETAILS_REQUIRE(henka_assets_material_instance_set_float(
        binding.instance, HENKA_MATERIAL_INSTANCE_ROUGHNESS, 0.91f) == HENKA_SUCCESS);
    HENKA_OBJECT_DETAILS_REQUIRE(sandbox3d_object_details_write_file(
        gltf_path, gltf_b, strlen(gltf_b)));
    HENKA_OBJECT_DETAILS_REQUIRE(henka_assets_reload_material_asset(
        manager, asset, &reloaded_asset) == HENKA_SUCCESS);
    HENKA_OBJECT_DETAILS_REQUIRE(reloaded_asset == asset && asset->revision == 2U);
    HENKA_OBJECT_DETAILS_REQUIRE(sandbox3d_prepare_material_editor_binding(
        scene, entity, &binding, 1U) == HENKA_SUCCESS);
    HENKA_OBJECT_DETAILS_REQUIRE(binding.instance->definition_revision == asset->revision);
    HENKA_OBJECT_DETAILS_REQUIRE(fabsf(binding.instance->material.base_color.z - 0.9f) <= 0.0001f);
    HENKA_OBJECT_DETAILS_REQUIRE(fabsf(binding.instance->material.roughness - 0.91f) <= 0.0001f);
    HENKA_OBJECT_DETAILS_REQUIRE(henka_scene_get_entity_material(
        scene, entity, &scene_material) == HENKA_SUCCESS);
    HENKA_OBJECT_DETAILS_REQUIRE(fabsf(scene_material.base_color.y - 0.7f) <= 0.0001f);
    HENKA_OBJECT_DETAILS_REQUIRE(fabsf(scene_material.roughness - 0.91f) <= 0.0001f);
    HENKA_OBJECT_DETAILS_REQUIRE(henka_scene_get_entity_material_asset_state(
        scene, entity, &scene_material_revision, &scene_material_overridden) == HENKA_SUCCESS);
    HENKA_OBJECT_DETAILS_REQUIRE(scene_material_revision == 0U);
    HENKA_OBJECT_DETAILS_REQUIRE(scene_material_overridden);

cleanup:
    sandbox3d_destroy_material_editor_bindings(&binding, 1U);
    henka_scene_destroy(scene);
    henka_engine_destroy(engine);
    (void)remove(gltf_path);
    (void)remove(image_a_path);
    (void)remove(image_b_path);
#undef HENKA_OBJECT_DETAILS_REQUIRE
}

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

    henka_test_sandbox3d_object_details_file_backed_refresh();

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
        henka_scene_set_entity_material_asset(scene, entity, NULL) ==
            HENKA_SUCCESS);
    HENKA_TEST_ASSERT(
        sandbox3d_prepare_material_editor_binding(
            scene,
            entity,
            scene_bindings,
            2U) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(!scene_bindings[0].valid);

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
