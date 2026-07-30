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
    henka_asset_metadata metadata;
    henka_asset_texture_entry texture_entries[2];
    henka_mesh fallback_mesh;
    henka_mesh* mesh;
    char* resolved_path;
    henka_shader managed_shader;
    henka_texture fallback_texture;
    henka_texture* texture;

    HENKA_TEST_ASSERT(strcmp(henka_assets_get_type_label(HENKA_ASSET_TYPE_SHADER), "Shader") == 0);
    HENKA_TEST_ASSERT(strcmp(henka_assets_get_type_label(HENKA_ASSET_TYPE_TEXTURE), "Texture") == 0);
    HENKA_TEST_ASSERT(strcmp(henka_assets_get_type_label(HENKA_ASSET_TYPE_MESH), "Mesh") == 0);
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
