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
    char* resolved_path;
    henka_mesh* mesh;

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

    mesh = NULL;
    HENKA_TEST_ASSERT(henka_assets_retry_failed_obj_mesh(NULL, "assets/models/missing.obj", &mesh) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_assets_retry_failed_obj_mesh(NULL, NULL, &mesh) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_assets_retry_failed_obj_mesh(NULL, "assets/models/missing.obj", NULL) == HENKA_ERROR_INVALID_ARGUMENT);

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
