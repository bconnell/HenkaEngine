#include "test_suite.h"

#include <string.h>

#include <henka/assets.h>
#include <henka/memory.h>

#include "../engine/src/core/checked.h"
#include "../engine/src/henka_internal.h"

#if defined(HENKA_WITH_KTX2_TRANSCODER)
#include <ktx.h>
#include <stdlib.h>
#endif


void henka_test_assets(void)
{
#if defined(HENKA_WITH_KTX2_TRANSCODER)
    static const unsigned char malformed_ktx2[] =
        {0xABU, 0x4BU, 0x54U, 0x58U, 0x20U, 0x32U, 0x30U, 0xBBU, 0x0DU, 0x0AU, 0x1AU, 0x0AU};
    unsigned char* decoded_ktx2 = NULL;
    size_t decoded_ktx2_size = 0U;
    int decoded_ktx2_width = 0;
    int decoded_ktx2_height = 0;
    bool decoded_ktx2_is_srgb = false;
    HENKA_TEST_ASSERT(henka_ktx2_decode_rgba8(
        malformed_ktx2, sizeof(malformed_ktx2), &decoded_ktx2, &decoded_ktx2_size,
        &decoded_ktx2_width, &decoded_ktx2_height, &decoded_ktx2_is_srgb) == HENKA_ERROR_ASSET_SOURCE);
    HENKA_TEST_ASSERT(decoded_ktx2 == NULL && decoded_ktx2_size == 0U);

    {
        ktxTextureCreateInfo create_info;
        ktxTexture2* generated_texture = NULL;
        ktx_uint8_t* generated_bytes = NULL;
        ktx_size_t generated_size = 0U;
        unsigned char level_zero[64] = {0};
        unsigned char level_one[16] = {0};
        henka_ktx2_upload upload;

        memset(&create_info, 0, sizeof(create_info));
        create_info.vkFormat = 43U; /* VK_FORMAT_R8G8B8A8_SRGB */
        create_info.baseWidth = 4U;
        create_info.baseHeight = 4U;
        create_info.baseDepth = 1U;
        create_info.numDimensions = 2U;
        create_info.numLevels = 2U;
        create_info.numLayers = 1U;
        create_info.numFaces = 1U;
        HENKA_TEST_ASSERT(ktxTexture2_Create(
            &create_info,
            KTX_TEXTURE_CREATE_ALLOC_STORAGE,
            &generated_texture) == KTX_SUCCESS);
        HENKA_TEST_ASSERT(generated_texture != NULL);
        HENKA_TEST_ASSERT(ktxTexture_SetImageFromMemory(
            ktxTexture(generated_texture), 0U, 0U, 0U,
            level_zero, sizeof(level_zero)) == KTX_SUCCESS);
        HENKA_TEST_ASSERT(ktxTexture_SetImageFromMemory(
            ktxTexture(generated_texture), 1U, 0U, 0U,
            level_one, sizeof(level_one)) == KTX_SUCCESS);
        HENKA_TEST_ASSERT(ktxTexture_WriteToMemory(
            ktxTexture(generated_texture), &generated_bytes, &generated_size) == KTX_SUCCESS);
        ktxTexture_Destroy(ktxTexture(generated_texture));
        generated_texture = NULL;
        memset(&upload, 0, sizeof(upload));
        HENKA_TEST_ASSERT(henka_ktx2_prepare_upload(
            generated_bytes,
            (size_t)generated_size,
            HENKA_TEXTURE_USAGE_COLOR,
            HENKA_TEXTURE_COLOR_SPACE_SRGB,
            0U,
            &upload) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(!upload.compressed);
        HENKA_TEST_ASSERT(upload.level_count == 2U);
        HENKA_TEST_ASSERT(upload.total_level_count == 2U);
        HENKA_TEST_ASSERT(upload.width == 4 && upload.height == 4);
        HENKA_TEST_ASSERT(upload.levels[0].size == sizeof(level_zero));
        HENKA_TEST_ASSERT(upload.levels[1].size == sizeof(level_one));
        HENKA_TEST_ASSERT(upload.levels[1].width == 2 && upload.levels[1].height == 2);
        henka_ktx2_upload_dispose(&upload);
        memset(&upload, 0, sizeof(upload));
        HENKA_TEST_ASSERT(henka_ktx2_prepare_upload_with_mip_limit(
            generated_bytes,
            (size_t)generated_size,
            HENKA_TEXTURE_USAGE_COLOR,
            HENKA_TEXTURE_COLOR_SPACE_SRGB,
            0U,
            1U,
            &upload) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(upload.level_count == 1U && upload.total_level_count == 2U);
        HENKA_TEST_ASSERT(upload.data_size == sizeof(level_zero));
        HENKA_TEST_ASSERT(upload.levels[0].size == sizeof(level_zero));
        henka_ktx2_upload_dispose(&upload);
        free(generated_bytes);

        generated_bytes = NULL;
        memset(&create_info, 0, sizeof(create_info));
        create_info.vkFormat = 132U; /* VK_FORMAT_BC1_RGB_SRGB_BLOCK */
        create_info.baseWidth = 4U;
        create_info.baseHeight = 4U;
        create_info.baseDepth = 1U;
        create_info.numDimensions = 2U;
        create_info.numLevels = 2U;
        create_info.numLayers = 1U;
        create_info.numFaces = 1U;
        HENKA_TEST_ASSERT(ktxTexture2_Create(
            &create_info,
            KTX_TEXTURE_CREATE_ALLOC_STORAGE,
            &generated_texture) == KTX_SUCCESS);
        HENKA_TEST_ASSERT(generated_texture != NULL);
        HENKA_TEST_ASSERT(ktxTexture_SetImageFromMemory(
            ktxTexture(generated_texture), 0U, 0U, 0U,
            level_one, 8U) == KTX_SUCCESS);
        HENKA_TEST_ASSERT(ktxTexture_SetImageFromMemory(
            ktxTexture(generated_texture), 1U, 0U, 0U,
            level_one, 8U) == KTX_SUCCESS);
        HENKA_TEST_ASSERT(ktxTexture_WriteToMemory(
            ktxTexture(generated_texture), &generated_bytes, &generated_size) == KTX_SUCCESS);
        ktxTexture_Destroy(ktxTexture(generated_texture));
        generated_texture = NULL;
        memset(&upload, 0, sizeof(upload));
        HENKA_TEST_ASSERT(henka_ktx2_prepare_upload(
            generated_bytes,
            (size_t)generated_size,
            HENKA_TEXTURE_USAGE_COLOR,
            HENKA_TEXTURE_COLOR_SPACE_SRGB,
            HENKA_KTX2_CAPABILITY_BC1_3,
            &upload) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(upload.compressed);
        HENKA_TEST_ASSERT(upload.format == HENKA_KTX2_GPU_FORMAT_BC1);
        HENKA_TEST_ASSERT(upload.level_count == 2U);
        HENKA_TEST_ASSERT(upload.levels[0].size == 8U && upload.levels[1].size == 8U);
        henka_ktx2_upload_dispose(&upload);
        memset(&upload, 0, sizeof(upload));
        HENKA_TEST_ASSERT(henka_ktx2_prepare_upload(
            generated_bytes,
            (size_t)generated_size,
            HENKA_TEXTURE_USAGE_COLOR,
            HENKA_TEXTURE_COLOR_SPACE_SRGB,
            0U,
            &upload) == HENKA_ERROR_ASSET_SOURCE);
        HENKA_TEST_ASSERT(upload.data == NULL && upload.level_count == 0U);
        free(generated_bytes);
    }
#endif
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
    henka_gltf_scene_asset* scene_asset;
    henka_gltf_scene_asset scene_entry;
    henka_gltf_scene_asset* scene_entry_array[1];
    henka_material material;
    henka_material_instance material_instance;
    henka_material_dependency_info material_dependencies;
    uint64_t material_revision;
    size_t processed_residency_requests;
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
    HENKA_TEST_ASSERT(strcmp(henka_assets_get_type_label(HENKA_ASSET_TYPE_GLTF_SCENE), "glTF Scene") == 0);
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
    scene_asset = (henka_gltf_scene_asset*)1;
    HENKA_TEST_ASSERT(henka_assets_load_gltf_scene_asset(
        NULL, "assets/models/sample.gltf", NULL, &scene_asset) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(scene_asset == NULL);
    scene_asset = (henka_gltf_scene_asset*)1;
    HENKA_TEST_ASSERT(henka_assets_reload_gltf_scene_asset(
        NULL, "assets/models/sample.gltf", &scene_asset) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(scene_asset == NULL);
    HENKA_TEST_ASSERT(henka_assets_instantiate_gltf_scene(
        NULL, NULL, NULL, NULL, NULL) == HENKA_ERROR_INVALID_ARGUMENT);
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
    fallback_texture.backend_data = (void*)1;
    fallback_texture.width = 2;
    fallback_texture.height = 2;
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
    {
        henka_texture_residency_diagnostics residency;
        HENKA_TEST_ASSERT(henka_assets_set_texture_residency_budget(
            NULL, 1024U) == HENKA_ERROR_INVALID_ARGUMENT);
        HENKA_TEST_ASSERT(henka_assets_get_texture_residency_diagnostics(
            &manager, &residency) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(residency.managed_texture_count == 2U);
        HENKA_TEST_ASSERT(residency.fallback_texture_count == 2U);
        HENKA_TEST_ASSERT(residency.resident_bytes == 0U);
        HENKA_TEST_ASSERT(henka_assets_set_texture_residency_budget(
            &manager, 1024U) == HENKA_SUCCESS);
        manager.texture_resident_bytes = 2048U;
        HENKA_TEST_ASSERT(henka_assets_set_texture_residency_budget(
            &manager, 1024U) == HENKA_ERROR_LIMIT);
        HENKA_TEST_ASSERT(henka_assets_set_texture_residency_budget(
            &manager, 0U) == HENKA_SUCCESS);
        manager.texture_resident_bytes = 0U;
        {
            size_t evicted_texture_count = 123U;
            HENKA_TEST_ASSERT(henka_assets_trim_texture_residency(
                NULL, 1024U, 0U, &evicted_texture_count) == HENKA_ERROR_INVALID_ARGUMENT);
            HENKA_TEST_ASSERT(henka_assets_trim_texture_residency(
                &manager, 1024U, 0U, &evicted_texture_count) == HENKA_SUCCESS);
            HENKA_TEST_ASSERT(evicted_texture_count == 0U);
            HENKA_TEST_ASSERT(henka_assets_enforce_texture_residency_budget(
                &manager, 0U, &evicted_texture_count) == HENKA_SUCCESS);
            HENKA_TEST_ASSERT(evicted_texture_count == 0U);
            HENKA_TEST_ASSERT(henka_assets_enforce_texture_residency_budget(
                NULL, 0U, &evicted_texture_count) == HENKA_ERROR_INVALID_ARGUMENT);
        }
    }
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
    texture_entries[0].metadata.fallback = false;
    texture_entries[0].owns_texture = true;
    texture_entries[0].resident_gpu_bytes = 1U;
    manager.texture_resident_bytes = 1U;
    HENKA_TEST_ASSERT(henka_assets_queue_texture_residency_request(
        &manager, &fallback_texture, 1U) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_assets_queue_texture_residency_request(
        &manager, &fallback_texture, 2U) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(manager.texture_residency_request_mips[0] == 2U);
    HENKA_TEST_ASSERT(henka_assets_queue_texture_residency_request(
        &manager, &fallback_texture, 1U) == HENKA_SUCCESS);
    {
        henka_texture_residency_diagnostics residency;
        HENKA_TEST_ASSERT(henka_assets_get_texture_residency_diagnostics(
            &manager, &residency) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(residency.queued_request_count == 1U);
        HENKA_TEST_ASSERT(henka_assets_process_texture_residency_requests(
            &manager, 1U, &processed_residency_requests) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(processed_residency_requests == 1U);
        HENKA_TEST_ASSERT(henka_assets_get_texture_residency_diagnostics(
            &manager, &residency) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(residency.queued_request_count == 0U);
        HENKA_TEST_ASSERT(residency.failed_request_count == 1U);
    }
    texture_entries[0].metadata.fallback = true;
    texture_entries[0].owns_texture = false;
    texture_entries[0].resident_gpu_bytes = 0U;
    manager.texture_resident_bytes = 0U;
    memset(&material_entry, 0, sizeof(material_entry));
    material_entry.key = "assets/models/reload.gltf";
    material_entry.source_path = "assets/models/reload.gltf";
    material_entry.material = henka_material_default();
    material_entry.material.shader = &managed_shader;
    material_entry.material.use_texture = true;
    material_entry.material.base_color_texture = &fallback_texture;
    fallback_texture.descriptor = henka_texture_descriptor_default_color();
    material_entry.revision = 4U;
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
    HENKA_TEST_ASSERT(henka_assets_get_material_asset_revision(
        &material_entry, &material_revision) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(material_revision == 4U);
    HENKA_TEST_ASSERT(henka_assets_get_material_asset_dependencies(
        &material_entry, &material_dependencies) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(material_dependencies.definition_revision == 4U);
    HENKA_TEST_ASSERT(material_dependencies.dependency_count == 1U);
    HENKA_TEST_ASSERT(material_dependencies.dependencies[0].slot ==
        HENKA_MATERIAL_TEXTURE_SLOT_BASE_COLOR);
    HENKA_TEST_ASSERT(henka_assets_create_material_instance(
        &material_entry, &material_instance) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_assets_material_instance_set_float(
        &material_instance, HENKA_MATERIAL_INSTANCE_METALLIC, 0.8f) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_assets_material_instance_set_float(
        &material_instance, HENKA_MATERIAL_INSTANCE_TRANSMISSION, 0.65f) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_assets_material_instance_set_float(
        &material_instance, HENKA_MATERIAL_INSTANCE_THICKNESS, 0.35f) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_assets_material_instance_set_float(
        &material_instance, HENKA_MATERIAL_INSTANCE_ATTENUATION_DISTANCE, 3.0f) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_assets_material_instance_set_vec3(
        &material_instance, HENKA_MATERIAL_INSTANCE_ATTENUATION_COLOR,
        (henka_vec3){0.5f, 0.6f, 0.7f}) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_assets_material_instance_set_bool(
        &material_instance, HENKA_MATERIAL_INSTANCE_DOUBLE_SIDED, true) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_assets_material_instance_set_alpha_mode(
        &material_instance, HENKA_MATERIAL_ALPHA_MASKED) == HENKA_SUCCESS);
    material_entry.material.roughness = 0.8f;
    material_entry.revision = 5U;
    HENKA_TEST_ASSERT(henka_assets_refresh_material_instance(&material_instance) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(material_instance.definition_revision == 5U);
    HENKA_TEST_ASSERT(material_instance.material.metallic == 0.8f);
    HENKA_TEST_ASSERT(material_instance.material.transmission == 0.65f);
    HENKA_TEST_ASSERT(material_instance.material.thickness == 0.35f);
    HENKA_TEST_ASSERT(material_instance.material.attenuation_distance == 3.0f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(material_instance.material.attenuation_color.z, 0.7f, 0.0001f);
    HENKA_TEST_ASSERT(material_instance.material.roughness == 0.8f);
    HENKA_TEST_ASSERT(material_instance.material.double_sided);
    HENKA_TEST_ASSERT(material_instance.material.alpha_mode == HENKA_MATERIAL_ALPHA_MASKED);
    memset(&scene_entry, 0, sizeof(scene_entry));
    scene_entry.key = "assets/models/reload-scene.gltf";
    scene_entry.source_path = "assets/models/reload-scene.gltf";
    scene_entry.shader = &managed_shader;
    scene_entry.data.scene_count = 2U;
    scene_entry.data.active_scene_index = 0U;
    scene_entry.metadata.type = HENKA_ASSET_TYPE_GLTF_SCENE;
    scene_entry_array[0] = &scene_entry;
    manager.gltf_scene_entries = scene_entry_array;
    manager.gltf_scene_count = 1U;
    manager.gltf_scene_capacity = 1U;
    scene_asset = (henka_gltf_scene_asset*)1;
    HENKA_TEST_ASSERT(henka_assets_reload_gltf_scene_asset(
        &manager,
        "assets/models/reload-scene.gltf",
        &scene_asset) != HENKA_SUCCESS);
    HENKA_TEST_ASSERT(scene_asset == NULL);
    HENKA_TEST_ASSERT(manager.gltf_scene_entries[0] == &scene_entry);
    HENKA_TEST_ASSERT(scene_entry.data.primitive_count == 0U);
    HENKA_TEST_ASSERT(henka_assets_set_gltf_scene_active_scene(&scene_entry, 2U) != HENKA_SUCCESS);
    HENKA_TEST_ASSERT(scene_entry.data.active_scene_index == 0U);
    HENKA_TEST_ASSERT(henka_assets_set_gltf_scene_active_scene(&scene_entry, 1U) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(scene_entry.data.active_scene_index == 1U);
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
