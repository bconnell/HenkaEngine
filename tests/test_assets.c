#include "test_suite.h"

#include <string.h>

#include <henka/assets.h>
#include <henka/memory.h>

#include "../engine/src/core/checked.h"
#include "../engine/src/henka_internal.h"

void henka_test_assets(void)
{
    char* display_name;
    char display_name_source[] = "assets/textures/cube_albedo.png";
    char overlong_path[HENKA_MAX_ASSET_PATH_BYTES + 2U];
    char* canonical_key;
    char* canonical_key_variant;
    const char* invalid_asset_paths[] =
    {
        "../outside.obj",
        "assets/../outside.obj",
        "/assets/models/tree.obj",
        "\\Windows\\texture.png",
        "\\\\server\\share\\model.obj",
        "C:/assets/model.obj",
        "C:assets/model.obj",
        "\\\\?\\C:\\assets\\model.obj",
        "\\\\.\\C:\\assets\\model.obj",
        "file://assets/model.obj",
        "https://example.invalid/model.obj"
    };
    size_t invalid_path_index;
    henka_asset_manager manager;
    henka_asset_mesh_entry mesh_entries[2];
    henka_material_asset material_entry;
    henka_material_asset* material_entry_array[1];
    henka_asset_metadata metadata;
    henka_mesh* gltf_mesh;
    henka_material_asset* material_asset;
    henka_material material;
    henka_asset_texture_entry texture_entries[2];
    henka_mesh fallback_mesh;
    henka_mesh* mesh;
    char* resolved_path;
    henka_shader managed_shader;
    henka_texture fallback_texture;
    henka_texture* texture;
    henka_texture* texture_alias;
    henka_texture* texture_replacement;
    henka_texture* stable_texture_identity;
    henka_engine fake_engine;
    henka_renderer fake_renderer;
    size_t allocations_before_alias;
    static const unsigned char one_pixel[] =
    {
        255U, 255U, 255U, 255U
    };

    HENKA_TEST_ASSERT(strcmp(henka_assets_get_type_label(HENKA_ASSET_TYPE_SHADER), "Shader") == 0);
    HENKA_TEST_ASSERT(strcmp(henka_assets_get_type_label(HENKA_ASSET_TYPE_TEXTURE), "Texture") == 0);
    gltf_mesh = (henka_mesh*)1;
    HENKA_TEST_ASSERT(henka_assets_load_gltf_mesh(NULL, "assets/models/sample.gltf", &gltf_mesh) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(gltf_mesh == NULL);
    gltf_mesh = (henka_mesh*)1;
    HENKA_TEST_ASSERT(henka_assets_retry_failed_gltf_mesh(NULL, "assets/models/sample.gltf", &gltf_mesh) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(gltf_mesh == NULL);
    HENKA_TEST_ASSERT(strcmp(henka_assets_get_type_label(HENKA_ASSET_TYPE_MESH), "Mesh") == 0);
    HENKA_TEST_ASSERT(strcmp(henka_assets_get_type_label(HENKA_ASSET_TYPE_MATERIAL), "Material") == 0);
    material_asset = (henka_material_asset*)1;
    HENKA_TEST_ASSERT(henka_assets_load_gltf_material_asset(
        NULL, "assets/models/sample.gltf", NULL, &material_asset) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(material_asset == NULL);
    material_asset = (henka_material_asset*)1;
    HENKA_TEST_ASSERT(henka_assets_reload_gltf_material_asset(
        NULL, "assets/models/sample.gltf", &material_asset) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(material_asset == NULL);
    material = henka_material_default();
    HENKA_TEST_ASSERT(henka_assets_get_material_asset_material(NULL, &material) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(material.shader == NULL);
    HENKA_TEST_ASSERT(henka_assets_get_metadata_count(NULL) == 0U);

    display_name = henka_asset_copy_display_name(display_name_source);
    HENKA_TEST_ASSERT(display_name != NULL);
    HENKA_TEST_ASSERT(strcmp(display_name, "cube_albedo.png") == 0);
    display_name_source[0] = 'X';
    HENKA_TEST_ASSERT(strcmp(display_name, "cube_albedo.png") == 0);
    henka_free(display_name);

    memset(overlong_path, 'a', sizeof(overlong_path));
    overlong_path[sizeof(overlong_path) - 1U] = '\0';
    HENKA_TEST_ASSERT(henka_asset_copy_display_name(overlong_path) == NULL);

    canonical_key = NULL;
    HENKA_TEST_ASSERT(henka_assets_make_canonical_key(
        "assets\\textures\\.\\cube_albedo.png",
        &canonical_key) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(strcmp(
        canonical_key,
        "assets/textures/cube_albedo.png") == 0);
    henka_free(canonical_key);

    canonical_key = NULL;
    canonical_key_variant = NULL;
    HENKA_TEST_ASSERT(henka_assets_make_canonical_key(
        "Assets//Textures///Cube_Albedo.PNG",
        &canonical_key) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_assets_make_canonical_key(
        "Assets\\Textures\\.\\Cube_Albedo.PNG",
        &canonical_key_variant) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(strcmp(canonical_key, canonical_key_variant) == 0);
    henka_free(canonical_key_variant);
    canonical_key_variant = NULL;
    HENKA_TEST_ASSERT(henka_assets_make_canonical_key(
        "assets/textures/cube_albedo.png",
        &canonical_key_variant) == HENKA_SUCCESS);
#if defined(_WIN32)
    HENKA_TEST_ASSERT(strcmp(
        canonical_key,
        "assets/textures/cube_albedo.png") == 0);
    HENKA_TEST_ASSERT(strcmp(canonical_key, canonical_key_variant) == 0);
#else
    HENKA_TEST_ASSERT(strcmp(
        canonical_key,
        "Assets/Textures/Cube_Albedo.PNG") == 0);
    HENKA_TEST_ASSERT(strcmp(canonical_key, canonical_key_variant) != 0);
#endif
    henka_free(canonical_key_variant);
    henka_free(canonical_key);

    for (invalid_path_index = 0U;
        invalid_path_index < sizeof(invalid_asset_paths) / sizeof(invalid_asset_paths[0]);
        ++invalid_path_index)
    {
        canonical_key = (char*)1;
        HENKA_TEST_ASSERT(henka_assets_make_canonical_key(
            invalid_asset_paths[invalid_path_index],
            &canonical_key) == HENKA_ERROR_INVALID_ARGUMENT);
        HENKA_TEST_ASSERT(canonical_key == NULL);
    }

    mesh = (henka_mesh*)1;
    HENKA_TEST_ASSERT(henka_assets_retry_failed_obj_mesh(NULL, "assets/models/missing.obj", &mesh) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(mesh == NULL);
    HENKA_TEST_ASSERT(henka_assets_retry_failed_obj_mesh(NULL, NULL, &mesh) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_assets_retry_failed_obj_mesh(NULL, "assets/models/missing.obj", NULL) == HENKA_ERROR_INVALID_ARGUMENT);

    texture = (henka_texture*)1;
    HENKA_TEST_ASSERT(henka_assets_retry_failed_texture(
        NULL,
        "assets/textures/missing.png",
        &texture) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(texture == NULL);
    HENKA_TEST_ASSERT(henka_assets_retry_failed_texture(
        NULL,
        "assets/textures/missing.png",
        NULL) == HENKA_ERROR_INVALID_ARGUMENT);

    texture = (henka_texture*)1;
    HENKA_TEST_ASSERT(henka_texture_create_from_rgba8(
        NULL,
        1,
        1,
        one_pixel,
        &texture) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(texture == NULL);

    texture = (henka_texture*)1;
    HENKA_TEST_ASSERT(henka_renderer_create_texture_from_rgba8(
        NULL,
        1,
        1,
        one_pixel,
        &texture) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(texture == NULL);

    memset(&fake_renderer, 0, sizeof(fake_renderer));
    memset(&fallback_texture, 0, sizeof(fallback_texture));
    fallback_texture.renderer = &fake_renderer;
    fallback_texture.backend_data = (void*)1;
    fallback_texture.owns_backend = true;
    fallback_texture.width = 2;
    fallback_texture.height = 2;

    allocations_before_alias =
        henka_memory_get_allocation_count();
    texture_alias = (henka_texture*)1;
    HENKA_TEST_ASSERT(henka_texture_create_borrowed_alias(
        &fallback_texture,
        &texture_alias) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(texture_alias != NULL);
    HENKA_TEST_ASSERT(texture_alias != &fallback_texture);
    HENKA_TEST_ASSERT(texture_alias->backend_data ==
        fallback_texture.backend_data);
    HENKA_TEST_ASSERT(!texture_alias->owns_backend);
    HENKA_TEST_ASSERT(texture_alias->width == 2);
    HENKA_TEST_ASSERT(texture_alias->height == 2);

    texture_replacement = henka_calloc(
        1U,
        sizeof(*texture_replacement));
    HENKA_TEST_ASSERT(texture_replacement != NULL);
    texture_replacement->renderer = &fake_renderer;
    texture_replacement->backend_data = (void*)2;
    texture_replacement->owns_backend = true;
    texture_replacement->width = 8;
    texture_replacement->height = 4;
    stable_texture_identity = texture_alias;
    HENKA_TEST_ASSERT(henka_texture_adopt_owned_payload(
        texture_alias,
        texture_replacement) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(texture_alias ==
        stable_texture_identity);
    HENKA_TEST_ASSERT(texture_alias->backend_data ==
        (void*)2);
    HENKA_TEST_ASSERT(texture_alias->owns_backend);
    HENKA_TEST_ASSERT(texture_alias->width == 8);
    HENKA_TEST_ASSERT(texture_alias->height == 4);

    texture_alias->backend_data = NULL;
    texture_alias->owns_backend = false;
    henka_texture_destroy_owned(texture_alias);
    HENKA_TEST_ASSERT(
        henka_memory_get_allocation_count() ==
        allocations_before_alias);

    memset(&metadata, 0x5a, sizeof(metadata));
    HENKA_TEST_ASSERT(henka_assets_get_metadata_at_index(
        NULL,
        0U,
        &metadata) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(metadata.type == HENKA_ASSET_TYPE_UNKNOWN);
    HENKA_TEST_ASSERT(metadata.source_path == NULL);
    HENKA_TEST_ASSERT(!metadata.loaded);
    HENKA_TEST_ASSERT(!metadata.reload_supported);

    memset(&manager, 0, sizeof(manager));
    memset(&fallback_texture, 0, sizeof(fallback_texture));
    memset(&fallback_mesh, 0, sizeof(fallback_mesh));
    memset(texture_entries, 0, sizeof(texture_entries));
    memset(mesh_entries, 0, sizeof(mesh_entries));
    manager.error_texture = &fallback_texture;
    manager.fallback_mesh = &fallback_mesh;
    manager.texture_entries = texture_entries;
    manager.texture_count = 2U;
    manager.mesh_entries = mesh_entries;
    manager.mesh_count = 2U;
    texture_entries[0].key = "assets/textures/a.png";
    texture_entries[0].source_path = "assets/textures/a.png";
    texture_entries[0].texture = &fallback_texture;
    texture_entries[0].metadata.type = HENKA_ASSET_TYPE_TEXTURE;
    texture_entries[0].metadata.source_path = texture_entries[0].key;
    texture_entries[0].metadata.fallback = true;
    texture_entries[1].key = "assets/textures/b.png";
    texture_entries[1].source_path = "Assets/Textures/B.png";
    texture_entries[1].texture = &fallback_texture;
    texture_entries[1].metadata.type = HENKA_ASSET_TYPE_TEXTURE;
    texture_entries[1].metadata.source_path = texture_entries[1].source_path;
    texture_entries[1].metadata.fallback = true;
    mesh_entries[0].key = "assets/models/a.obj";
    mesh_entries[0].source_path = "Assets/Models/A.obj";
    mesh_entries[0].mesh = &fallback_mesh;
    mesh_entries[0].metadata.type = HENKA_ASSET_TYPE_MESH;
    mesh_entries[0].metadata.source_path = mesh_entries[0].source_path;
    mesh_entries[0].metadata.fallback = true;
    mesh_entries[1].key = "assets/models/b.obj";
    mesh_entries[1].source_path = "assets/models/b.obj";
    mesh_entries[1].mesh = &fallback_mesh;
    mesh_entries[1].metadata.type = HENKA_ASSET_TYPE_MESH;
    mesh_entries[1].metadata.source_path = mesh_entries[1].key;
    mesh_entries[1].metadata.fallback = true;

    memset(&metadata, 0x5a, sizeof(metadata));
    HENKA_TEST_ASSERT(henka_assets_get_texture_metadata(
        &manager,
        &fallback_texture,
        &metadata) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(metadata.source_path == NULL);
#if defined(_WIN32)
    HENKA_TEST_ASSERT(henka_assets_get_texture_metadata_for_path(
        &manager,
        "ASSETS\\textures\\.\\B.PNG",
        &metadata) == HENKA_SUCCESS);
#else
    HENKA_TEST_ASSERT(henka_assets_get_texture_metadata_for_path(
        &manager,
        "assets\\textures\\.\\b.png",
        &metadata) == HENKA_SUCCESS);
#endif
    HENKA_TEST_ASSERT(strcmp(
        metadata.source_path,
        "Assets/Textures/B.png") == 0);

    memset(&fake_engine, 0, sizeof(fake_engine));
    fake_engine.renderer = NULL;
    fake_engine.asset_base_path = "";
    manager.engine = &fake_engine;
    memset(&material_entry, 0, sizeof(material_entry));
    material_entry.key = "assets/models/reload.gltf";
    material_entry.source_path = "assets/models/reload.gltf";
    material_entry.material = henka_material_default();
    material_entry.material.shader = &managed_shader;
    material_entry_array[0] = &material_entry;
    manager.material_entries = material_entry_array;
    manager.material_count = 1U;
    manager.material_capacity = 1U;
    material_asset = (henka_material_asset*)1;
    HENKA_TEST_ASSERT(henka_assets_reload_gltf_material_asset(
        &manager,
        "assets/models/reload.gltf",
        &material_asset) != HENKA_SUCCESS);
    HENKA_TEST_ASSERT(material_asset == NULL);
    HENKA_TEST_ASSERT(manager.material_entries[0] == &material_entry);
    HENKA_TEST_ASSERT(material_entry.material.shader == &managed_shader);
    texture = (henka_texture*)1;
    HENKA_TEST_ASSERT(henka_assets_retry_failed_texture(
        &manager,
        "assets/textures/a.png",
        &texture) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(texture == NULL);
    HENKA_TEST_ASSERT(texture_entries[0].texture ==
        &fallback_texture);
    HENKA_TEST_ASSERT(texture_entries[0].metadata.fallback);

    memset(&metadata, 0x5a, sizeof(metadata));
    HENKA_TEST_ASSERT(henka_assets_get_mesh_metadata(
        &manager,
        &fallback_mesh,
        &metadata) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(metadata.source_path == NULL);
#if defined(_WIN32)
    HENKA_TEST_ASSERT(henka_assets_get_mesh_metadata_for_path(
        &manager,
        "ASSETS\\models\\.\\A.OBJ",
        &metadata) == HENKA_SUCCESS);
#else
    HENKA_TEST_ASSERT(henka_assets_get_mesh_metadata_for_path(
        &manager,
        "assets\\models\\.\\a.obj",
        &metadata) == HENKA_SUCCESS);
#endif
    HENKA_TEST_ASSERT(strcmp(
        metadata.source_path,
        "Assets/Models/A.obj") == 0);

    memset(&metadata, 0x5a, sizeof(metadata));
    HENKA_TEST_ASSERT(henka_assets_get_texture_metadata_for_path(
        &manager,
        "/assets/textures/b.png",
        &metadata) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(metadata.source_path == NULL);
    HENKA_TEST_ASSERT(manager.texture_count == 2U);

    memset(&managed_shader, 0, sizeof(managed_shader));
    managed_shader.asset_manager_owned = true;
    henka_shader_destroy(&managed_shader);
    HENKA_TEST_ASSERT(managed_shader.asset_manager_owned);
    fallback_texture.asset_manager_owned = true;
    henka_texture_destroy(&fallback_texture);
    HENKA_TEST_ASSERT(fallback_texture.asset_manager_owned);
    fallback_mesh.asset_manager_owned = true;
    henka_mesh_destroy(&fallback_mesh);
    HENKA_TEST_ASSERT(fallback_mesh.asset_manager_owned);

    resolved_path = NULL;
    HENKA_TEST_ASSERT(henka_assets_resolve_path(NULL, NULL, NULL) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_assets_resolve_path("C:/HenkaSandbox3D", NULL, &resolved_path) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_assets_resolve_path("C:/HenkaSandbox3D", "assets/shaders/basic_lit.vert", NULL) == HENKA_ERROR_INVALID_ARGUMENT);

    HENKA_TEST_ASSERT(henka_assets_resolve_path(
        "C:/HenkaSandbox3D",
        "assets/shaders/basic_lit.vert",
        &resolved_path) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(strcmp(resolved_path, "C:/HenkaSandbox3D/assets/shaders/basic_lit.vert") == 0);
    henka_free(resolved_path);

    HENKA_TEST_ASSERT(henka_assets_resolve_path(
        "C:/HenkaSandbox3D/",
        "assets\\textures\\cube_albedo.png",
        &resolved_path) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(strcmp(resolved_path, "C:/HenkaSandbox3D/assets/textures/cube_albedo.png") == 0);
    henka_free(resolved_path);

    HENKA_TEST_ASSERT(henka_assets_resolve_path(
        "",
        "assets/models/henka_marker.obj",
        &resolved_path) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(strcmp(resolved_path, "assets/models/henka_marker.obj") == 0);
    henka_free(resolved_path);

    resolved_path = NULL;
    HENKA_TEST_ASSERT(henka_assets_resolve_path("C:/HenkaSandbox3D", "../outside.obj", &resolved_path) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(resolved_path == NULL);
    HENKA_TEST_ASSERT(henka_assets_resolve_path("C:/HenkaSandbox3D", "assets/../outside.obj", &resolved_path) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(resolved_path == NULL);
    HENKA_TEST_ASSERT(henka_assets_resolve_path("C:/HenkaSandbox3D", "D:/Shared/henka_marker.obj", &resolved_path) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(resolved_path == NULL);
    HENKA_TEST_ASSERT(henka_assets_resolve_path("C:/HenkaSandbox3D", "\\\\server\\share\\henka_marker.obj", &resolved_path) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(resolved_path == NULL);
    HENKA_TEST_ASSERT(henka_assets_resolve_path("C:/HenkaSandbox3D", "assets/CON/model.obj", &resolved_path) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(resolved_path == NULL);
}
