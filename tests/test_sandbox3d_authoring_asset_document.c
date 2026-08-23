#include "test_suite.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <henka/memory.h>
#include <henka/persistence.h>
#include <henka/assets.h>

#include "../examples/sandbox3d/authoring_asset_document.h"
#include "../examples/sandbox3d/object_authoring_tools.h"

static FILE* henka_test_open_binary_read(const char* path)
{
    FILE* file = NULL;

    if (path == NULL)
    {
        return NULL;
    }
#ifdef _WIN32
    if (fopen_s(&file, path, "rb") != 0)
    {
        return NULL;
    }
#else
    file = fopen(path, "rb");
#endif
    return file;
}

static bool henka_test_bounds_have_horizontal_clearance(
    henka_bounds first,
    henka_bounds second,
    float clearance)
{
    return fabsf(first.center.x - second.center.x) >=
            first.extents.x + second.extents.x + clearance ||
        fabsf(first.center.z - second.center.z) >=
            first.extents.z + second.extents.z + clearance;
}

void henka_test_sandbox3d_authoring_asset_document(void)
{
    henka_engine_config config = {0};
    henka_engine* engine = NULL;
    henka_scene* scene = NULL;
    sandbox3d_authoring_asset_document* document = NULL;
    sandbox3d_authoring_object* cylinder = NULL;
    sandbox3d_authoring_object* adopted_part = NULL;
    henka_authoring_mesh_counts cylinder_counts;
    henka_entity cylinder_entity;
    henka_entity adopted_entity;
    henka_entity reloaded_entities[2] = {HENKA_INVALID_ENTITY, HENKA_INVALID_ENTITY};
    henka_entity second_reloaded_entities[2] = {HENKA_INVALID_ENTITY, HENKA_INVALID_ENTITY};
    sandbox3d_authoring_primitive_kind adopted_kind;
    char part_name[32];
    size_t capacity_index;
    size_t part_index = SIZE_MAX;
    size_t second_part_index = SIZE_MAX;
    char* project_root = NULL;
    char* manifest_path = NULL;
    henka_settings* settings = NULL;
    sandbox3d_authoring_asset_document* reloaded_document = NULL;
    sandbox3d_authoring_asset_document* second_reloaded_document = NULL;
    henka_transform transform;
    henka_bounds cylinder_bounds;
    henka_bounds second_part_bounds;
    henka_material persisted_material;
    henka_material reloaded_material;
    henka_shader* basic_shader = NULL;

    config.application_name = "Henka Native Authoring Asset Document Test";
    config.window_width = 320;
    config.window_height = 240;
    config.enable_vsync = false;
    /* CTest fixes this suite's working directory at the repository root, so
     * source-controlled test shaders resolve from the canonical asset root. */
    config.asset_base_path = ".";

    HENKA_TEST_ASSERT(henka_engine_create(&config, &engine) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_assets_load_shader(
        henka_engine_get_asset_manager(engine),
        "assets/shaders/basic_lit.vert",
        "assets/shaders/basic_lit.frag",
        &basic_shader) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_create(&scene) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_asset_document_create(
        engine, scene, "test_asset", &document) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(document != NULL);
    HENKA_TEST_ASSERT(strcmp(
        sandbox3d_authoring_asset_document_get_name(document), "test_asset") == 0);
    HENKA_TEST_ASSERT(
        sandbox3d_authoring_asset_document_get_part_count(document) == 0U);
    HENKA_TEST_ASSERT(
        sandbox3d_authoring_asset_document_get_provenance(document) ==
        SANDBOX3D_AUTHORING_PROVENANCE_PRODUCT_NATIVE_AUTHORED);
    HENKA_TEST_ASSERT(sandbox3d_authoring_asset_document_add_primitive(
        document,
        "cylinder_body",
        SANDBOX3D_AUTHORING_PRIMITIVE_CYLINDER,
        &(sandbox3d_authoring_primitive_desc){
            .height = 2.0f,
            .radius = 0.5f,
            .segments = 24U},
        8U,
        &part_index) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(part_index == 0U);
    HENKA_TEST_ASSERT(
        sandbox3d_authoring_asset_document_get_part_count(document) == 1U);
    cylinder = sandbox3d_authoring_asset_document_get_part(document, part_index);
    HENKA_TEST_ASSERT(cylinder != NULL);
    cylinder_counts = henka_authoring_mesh_get_counts(
        sandbox3d_authoring_object_get_mesh(cylinder));
    HENKA_TEST_ASSERT(cylinder_counts.vertices == 48U);
    HENKA_TEST_ASSERT(cylinder_counts.faces == 26U);
    HENKA_TEST_ASSERT(henka_scene_get_entity_transform(
        scene,
        sandbox3d_authoring_object_get_entity(cylinder),
        &transform) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(transform.position.y > 1.2f && transform.position.y < 1.3f);
    HENKA_TEST_ASSERT(transform.position.z < -1.0f);

    HENKA_TEST_ASSERT(sandbox3d_authoring_asset_document_add_primitive(
        document,
        "second_part",
        SANDBOX3D_AUTHORING_PRIMITIVE_BOX,
        &(sandbox3d_authoring_primitive_desc){
            .width = 1.0f,
            .height = 1.0f,
            .depth = 1.0f},
        8U,
        &second_part_index) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(second_part_index == 1U);
    HENKA_TEST_ASSERT(henka_scene_get_entity_transform(
        scene,
        sandbox3d_authoring_object_get_entity(
            sandbox3d_authoring_asset_document_get_part(document, second_part_index)),
        &transform) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(transform.position.x != 0.0f);
    HENKA_TEST_ASSERT(henka_scene_get_entity_world_bounds(
        scene,
        sandbox3d_authoring_object_get_entity(cylinder),
        &cylinder_bounds) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_get_entity_world_bounds(
        scene,
        sandbox3d_authoring_object_get_entity(
            sandbox3d_authoring_asset_document_get_part(document, second_part_index)),
        &second_part_bounds) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_test_bounds_have_horizontal_clearance(
        cylinder_bounds,
        second_part_bounds,
        0.5f));
    HENKA_TEST_ASSERT(sandbox3d_authoring_asset_document_discard_part(
        document, second_part_index) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_asset_document_get_part_count(document) == 1U);

    adopted_entity = henka_scene_create_entity_named(scene, "adopted_cylinder");
    HENKA_TEST_ASSERT(adopted_entity != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_create_from_mesh(
        engine,
        scene,
        adopted_entity,
        sandbox3d_authoring_object_get_mesh(cylinder),
        8U,
        &adopted_part) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_asset_document_adopt_part(
        document,
        adopted_part,
        SANDBOX3D_AUTHORING_PRIMITIVE_CYLINDER,
        &part_index) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_asset_document_find_part(
        document, adopted_part, &part_index));
    HENKA_TEST_ASSERT(sandbox3d_authoring_asset_document_get_part_kind(
        document, adopted_part, &adopted_kind) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(adopted_kind == SANDBOX3D_AUTHORING_PRIMITIVE_CYLINDER);
    HENKA_TEST_ASSERT(sandbox3d_authoring_asset_document_release_part_ownership(
        document, part_index, &adopted_part) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_asset_document_forget_released_part(
        document, adopted_part));
    sandbox3d_authoring_object_destroy(adopted_part);
    adopted_part = NULL;
    henka_scene_destroy_entity(scene, adopted_entity);
    HENKA_TEST_ASSERT(sandbox3d_authoring_asset_document_get_part_count(document) == 1U);

    HENKA_TEST_ASSERT(henka_path_resolve_confined(
        henka_engine_get_user_data_base_path(engine),
        "authoring_asset_document_test",
        &project_root) == HENKA_SUCCESS);
    transform = henka_transform_identity();
    transform.position = (henka_vec3){1.0f, 2.0f, 3.0f};
    transform.rotation = henka_quat_from_axis_angle((henka_vec3){0.0f, 1.0f, 0.0f}, 0.5f);
    transform.scale = (henka_vec3){1.5f, 0.75f, 2.0f};
    HENKA_TEST_ASSERT(henka_scene_set_entity_transform(
        scene, sandbox3d_authoring_object_get_entity(cylinder), transform) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_set_entity_visible(
        scene, sandbox3d_authoring_object_get_entity(cylinder), false) == HENKA_SUCCESS);
    persisted_material = henka_material_default();
    persisted_material.name = "Authoring persistence material";
    persisted_material.base_color = (henka_vec4){0.22f, 0.48f, 0.73f, 0.93f};
    persisted_material.type = HENKA_MATERIAL_TYPE_LIT;
    persisted_material.shader = basic_shader;
    persisted_material.emissive_color = (henka_vec3){0.04f, 0.08f, 0.12f};
    persisted_material.metallic = 0.61f;
    persisted_material.roughness = 0.37f;
    persisted_material.specular_factor = 0.72f;
    persisted_material.specular_color = (henka_vec3){0.81f, 0.74f, 0.66f};
    persisted_material.ior = 1.31f;
    persisted_material.transmission = 0.18f;
    persisted_material.thickness = 0.42f;
    persisted_material.attenuation_distance = 2.5f;
    persisted_material.attenuation_color = (henka_vec3){0.72f, 0.82f, 0.92f};
    persisted_material.subsurface = 0.11f;
    persisted_material.subsurface_color = (henka_vec3){0.63f, 0.47f, 0.31f};
    persisted_material.normal_scale = 0.35f;
    persisted_material.occlusion_strength = 0.64f;
    persisted_material.emissive_strength = 0.27f;
    persisted_material.clearcoat = 0.18f;
    persisted_material.clearcoat_roughness = 0.24f;
    persisted_material.alpha_cutoff = 0.43f;
    persisted_material.sheen_color = (henka_vec3){0.36f, 0.26f, 0.16f};
    persisted_material.sheen_roughness = 0.29f;
    persisted_material.base_color_uv_set = 0;
    persisted_material.normal_uv_set = 1;
    persisted_material.metallic_roughness_uv_set = 0;
    persisted_material.occlusion_uv_set = 1;
    persisted_material.emissive_uv_set = 0;
    persisted_material.transmission_uv_set = 1;
    persisted_material.thickness_uv_set = 0;
    persisted_material.use_texture = false;
    persisted_material.use_lighting = true;
    persisted_material.depth_test = true;
    persisted_material.alpha_mode = HENKA_MATERIAL_ALPHA_MASKED;
    persisted_material.double_sided = true;
    persisted_material.cast_shadows = true;
    persisted_material.receive_shadows = false;
    persisted_material.terrain_layers_enabled = false;
    persisted_material.base_color_texture = NULL;
    persisted_material.normal_texture = NULL;
    persisted_material.metallic_roughness_texture = NULL;
    persisted_material.occlusion_texture = NULL;
    persisted_material.emissive_texture = NULL;
    persisted_material.transmission_texture = NULL;
    persisted_material.thickness_texture = NULL;
    HENKA_TEST_ASSERT(henka_scene_set_entity_material(
        scene, sandbox3d_authoring_object_get_entity(cylinder), persisted_material) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_asset_document_save(
        document, project_root, "round_trip.asset") == HENKA_SUCCESS);
    {
        henka_material invalid_material = persisted_material;
        char* preflight_source_path = NULL;
        char* preflight_material_path = NULL;
        FILE* unexpected_file;
        int source_present;
        int material_present;

        invalid_material.terrain_layers_enabled = true;
        HENKA_TEST_ASSERT(henka_scene_set_entity_material(
            scene,
            sandbox3d_authoring_object_get_entity(cylinder),
            invalid_material) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(henka_path_resolve_confined(
            project_root,
            "authored_assets/test_asset/rev2/cylinder_body.hams",
            &preflight_source_path) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(henka_path_resolve_confined(
            project_root,
            "authored_assets/test_asset/rev2/cylinder_body.material",
            &preflight_material_path) == HENKA_SUCCESS);
        (void)remove(preflight_source_path);
        (void)remove(preflight_material_path);
        HENKA_TEST_ASSERT(sandbox3d_authoring_asset_document_save(
            document, project_root, "round_trip.asset") != HENKA_SUCCESS);
        unexpected_file = henka_test_open_binary_read(preflight_source_path);
        source_present = unexpected_file != NULL;
        if (unexpected_file != NULL) (void)fclose(unexpected_file);
        unexpected_file = henka_test_open_binary_read(preflight_material_path);
        material_present = unexpected_file != NULL;
        if (unexpected_file != NULL) (void)fclose(unexpected_file);
        HENKA_TEST_ASSERT(!source_present);
        HENKA_TEST_ASSERT(!material_present);
        HENKA_TEST_ASSERT(henka_scene_set_entity_material(
            scene,
            sandbox3d_authoring_object_get_entity(cylinder),
            persisted_material) == HENKA_SUCCESS);
        henka_free(preflight_source_path);
        henka_free(preflight_material_path);
    }
    HENKA_TEST_ASSERT(sandbox3d_authoring_asset_document_load(
        engine,
        scene,
        project_root,
        "round_trip.asset",
        8U,
        &persisted_material,
        &reloaded_document) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(reloaded_document != NULL);
    HENKA_TEST_ASSERT(sandbox3d_authoring_asset_document_get_part_count(
        reloaded_document) == 1U);
    HENKA_TEST_ASSERT(henka_scene_get_entity_transform(
        scene,
        sandbox3d_authoring_object_get_entity(
            sandbox3d_authoring_asset_document_get_part(reloaded_document, 0U)),
        &transform) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(fabsf(transform.position.x - 1.0f) < 0.0001f);
    HENKA_TEST_ASSERT(fabsf(transform.scale.z - 2.0f) < 0.0001f);
    HENKA_TEST_ASSERT(!henka_scene_is_entity_visible(
        scene,
        sandbox3d_authoring_object_get_entity(
            sandbox3d_authoring_asset_document_get_part(reloaded_document, 0U))));
    HENKA_TEST_ASSERT(henka_scene_get_entity_material(
        scene,
        sandbox3d_authoring_object_get_entity(
            sandbox3d_authoring_asset_document_get_part(reloaded_document, 0U)),
        &reloaded_material) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(reloaded_material.base_color.z, 0.73f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(reloaded_material.metallic, 0.61f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(reloaded_material.roughness, 0.37f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(reloaded_material.ior, 1.31f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(reloaded_material.clearcoat, 0.18f, 0.0001f);
    HENKA_TEST_ASSERT(reloaded_material.normal_uv_set == 1);
    HENKA_TEST_ASSERT(reloaded_material.transmission_uv_set == 1);
    HENKA_TEST_ASSERT(reloaded_material.alpha_mode == HENKA_MATERIAL_ALPHA_MASKED);
    HENKA_TEST_ASSERT(reloaded_material.double_sided);
    HENKA_TEST_ASSERT(!reloaded_material.receive_shadows);
    HENKA_TEST_ASSERT(sandbox3d_authoring_asset_document_add_primitive(
        reloaded_document,
        "reopened_plane",
        SANDBOX3D_AUTHORING_PRIMITIVE_PLANE,
        &(sandbox3d_authoring_primitive_desc){
            .width = 1.0f,
            .depth = 1.0f},
        8U,
        &part_index) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_set_entity_material(
        scene,
        sandbox3d_authoring_object_get_entity(
            sandbox3d_authoring_asset_document_get_part(reloaded_document, part_index)),
        persisted_material) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_asset_document_save(
        reloaded_document, project_root, "round_trip_second.asset") == HENKA_SUCCESS);
    reloaded_entities[0] = sandbox3d_authoring_object_get_entity(
        sandbox3d_authoring_asset_document_get_part(reloaded_document, 0U));
    reloaded_entities[1] = sandbox3d_authoring_object_get_entity(
        sandbox3d_authoring_asset_document_get_part(reloaded_document, 1U));
    sandbox3d_authoring_asset_document_destroy(reloaded_document);
    reloaded_document = NULL;
    HENKA_TEST_ASSERT(!henka_scene_is_entity_valid(scene, reloaded_entities[0]));
    HENKA_TEST_ASSERT(!henka_scene_is_entity_valid(scene, reloaded_entities[1]));
    HENKA_TEST_ASSERT(sandbox3d_authoring_asset_document_load(
        engine,
        scene,
        project_root,
        "round_trip_second.asset",
        8U,
        NULL,
        &second_reloaded_document) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_asset_document_get_part_count(
        second_reloaded_document) == 2U);
    second_reloaded_entities[0] = sandbox3d_authoring_object_get_entity(
        sandbox3d_authoring_asset_document_get_part(second_reloaded_document, 0U));
    second_reloaded_entities[1] = sandbox3d_authoring_object_get_entity(
        sandbox3d_authoring_asset_document_get_part(second_reloaded_document, 1U));
    sandbox3d_authoring_asset_document_destroy(second_reloaded_document);
    second_reloaded_document = NULL;
    HENKA_TEST_ASSERT(!henka_scene_is_entity_valid(scene, second_reloaded_entities[0]));
    HENKA_TEST_ASSERT(!henka_scene_is_entity_valid(scene, second_reloaded_entities[1]));
    HENKA_TEST_ASSERT(henka_path_resolve_confined(
        project_root, "round_trip.asset", &manifest_path) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_settings_create(&settings) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_settings_load_file(settings, manifest_path) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_settings_get_int(settings, "asset.revision", 0) == 1);
    HENKA_TEST_ASSERT(strcmp(
        henka_settings_get_string(settings, "asset.provenance", ""),
        SANDBOX3D_AUTHORING_PROVENANCE_LABEL) == 0);
    HENKA_TEST_ASSERT(strstr(
        henka_settings_get_string(settings, "part.0.source_path", ""),
        "/rev1/") != NULL);
    HENKA_TEST_ASSERT(henka_settings_set_string(
        settings, "asset.provenance", "GENERATED_TEST_FIXTURE") == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_settings_save_file(settings, manifest_path) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_asset_document_load(
        engine, scene, project_root, "round_trip.asset", 8U, NULL,
        &reloaded_document) != HENKA_SUCCESS);
    HENKA_TEST_ASSERT(reloaded_document == NULL);
    HENKA_TEST_ASSERT(sandbox3d_authoring_asset_document_get_part_count(document) == 1U);
    HENKA_TEST_ASSERT(sandbox3d_authoring_asset_document_save(
        document, project_root, "round_trip.asset") == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_settings_set_int(settings, "asset.version", 999) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_settings_save_file(settings, manifest_path) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_asset_document_load(
        engine, scene, project_root, "round_trip.asset", 8U, NULL,
        &reloaded_document) != HENKA_SUCCESS);
    HENKA_TEST_ASSERT(reloaded_document == NULL);
    HENKA_TEST_ASSERT(sandbox3d_authoring_asset_document_get_part_count(document) == 1U);
    HENKA_TEST_ASSERT(sandbox3d_authoring_asset_document_save(
        document, project_root, "round_trip.asset") == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_settings_load_file(settings, manifest_path) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_settings_set_string(
        settings, "part.0.source_path", "../outside.hams") == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_settings_save_file(settings, manifest_path) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_asset_document_load(
        engine, scene, project_root, "round_trip.asset", 8U, NULL,
        &reloaded_document) != HENKA_SUCCESS);
    HENKA_TEST_ASSERT(reloaded_document == NULL);
    HENKA_TEST_ASSERT(sandbox3d_authoring_asset_document_save(
        document, project_root, "round_trip.asset") == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_settings_load_file(settings, manifest_path) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_settings_set_string(
        settings, "part.0.material_path", "../outside.material") == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_settings_save_file(settings, manifest_path) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_asset_document_load(
        engine, scene, project_root, "round_trip.asset", 8U, NULL,
        &reloaded_document) != HENKA_SUCCESS);
    HENKA_TEST_ASSERT(reloaded_document == NULL);
    HENKA_TEST_ASSERT(sandbox3d_authoring_asset_document_save(
        document, project_root, "round_trip.asset") == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_settings_load_file(settings, manifest_path) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_settings_set_string(
        settings, "part.0.position.x", "nan") == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_settings_save_file(settings, manifest_path) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_asset_document_load(
        engine, scene, project_root, "round_trip.asset", 8U, NULL,
        &reloaded_document) != HENKA_SUCCESS);
    HENKA_TEST_ASSERT(reloaded_document == NULL);
    HENKA_TEST_ASSERT(sandbox3d_authoring_asset_document_get_part_count(document) == 1U);
    henka_settings_destroy(settings);
    settings = NULL;
    {
        FILE* truncated = fopen(manifest_path, "wb");
        HENKA_TEST_ASSERT(truncated != NULL);
        HENKA_TEST_ASSERT(fputs("asset.version=1\n", truncated) >= 0);
        HENKA_TEST_ASSERT(fclose(truncated) == 0);
    }
    HENKA_TEST_ASSERT(sandbox3d_authoring_asset_document_load(
        engine, scene, project_root, "round_trip.asset", 8U, NULL,
        &reloaded_document) != HENKA_SUCCESS);
    HENKA_TEST_ASSERT(reloaded_document == NULL);
    HENKA_TEST_ASSERT(sandbox3d_authoring_asset_document_get_part_count(document) == 1U);
    part_index = 0U;
    HENKA_TEST_ASSERT(sandbox3d_authoring_asset_document_release_part_ownership(
        document, part_index, &cylinder) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(cylinder != NULL);
    HENKA_TEST_ASSERT(sandbox3d_authoring_asset_document_get_part(
        document, part_index) == cylinder);
    HENKA_TEST_ASSERT(sandbox3d_authoring_asset_document_forget_released_part(
        document, cylinder));
    HENKA_TEST_ASSERT(
        sandbox3d_authoring_asset_document_get_part_count(document) == 0U);
    for (capacity_index = 0U;
         capacity_index < SANDBOX3D_AUTHORING_ASSET_PART_CAPACITY;
         ++capacity_index)
    {
        HENKA_TEST_ASSERT(snprintf(
            part_name, sizeof(part_name), "plane_%zu", capacity_index) > 0);
        HENKA_TEST_ASSERT(sandbox3d_authoring_asset_document_add_primitive(
            document,
            part_name,
            SANDBOX3D_AUTHORING_PRIMITIVE_PLANE,
            &(sandbox3d_authoring_primitive_desc){
                .width = 1.0f,
                .depth = 1.0f},
            1U,
            &part_index) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(part_index == capacity_index);
        HENKA_TEST_ASSERT(henka_scene_set_entity_material(
            scene,
            sandbox3d_authoring_object_get_entity(
                sandbox3d_authoring_asset_document_get_part(document, part_index)),
            persisted_material) == HENKA_SUCCESS);
    }
    HENKA_TEST_ASSERT(sandbox3d_authoring_asset_document_add_primitive(
        document,
        "plane_limit",
        SANDBOX3D_AUTHORING_PRIMITIVE_PLANE,
        &(sandbox3d_authoring_primitive_desc){
            .width = 1.0f,
            .depth = 1.0f},
        1U,
        &part_index) == HENKA_ERROR_LIMIT);
    HENKA_TEST_ASSERT(sandbox3d_authoring_asset_document_save(
        document, project_root, "capacity.asset") == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_asset_document_load(
        engine,
        scene,
        project_root,
        "capacity.asset",
        1U,
        NULL,
        &reloaded_document) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_asset_document_get_part_count(
        reloaded_document) == SANDBOX3D_AUTHORING_ASSET_PART_CAPACITY);
    sandbox3d_authoring_asset_document_destroy(reloaded_document);
    reloaded_document = NULL;
    henka_free(manifest_path);
    manifest_path = NULL;
    henka_free(project_root);
    project_root = NULL;
    cylinder_entity = sandbox3d_authoring_object_get_entity(cylinder);
    sandbox3d_authoring_asset_document_destroy(document);
    sandbox3d_authoring_object_destroy(cylinder);
    henka_scene_destroy_entity(scene, cylinder_entity);
    henka_scene_destroy(scene);
    henka_engine_destroy(engine);
}
