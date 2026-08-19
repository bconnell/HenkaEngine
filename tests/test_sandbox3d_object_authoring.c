#include <string.h>
#include <stdint.h>

#include "test_suite.h"

#include "../examples/sandbox3d/object_authoring_tools.h"

static void henka_test_sandbox3d_object_authoring_scene_policy(void)
{
    henka_scene* scene;
    henka_entity first;
    henka_entity second;
    henka_entity helper;
    henka_entity stale;
    henka_entity entities[4];
    size_t count;

    HENKA_TEST_ASSERT(henka_scene_create(&scene) == HENKA_SUCCESS);
    first = henka_scene_create_entity_named(scene, "First");
    second = henka_scene_create_entity_named(scene, "Second");
    helper = henka_scene_create_entity_named(scene, "Editor Helper");
    HENKA_TEST_ASSERT(first != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(second != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(helper != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(henka_scene_set_entity_flags(scene, helper, HENKA_SCENE_ENTITY_FLAG_HELPER) == HENKA_SUCCESS);

    count = sandbox3d_object_authoring_collect_user_entities(scene, entities, 1U);
    HENKA_TEST_ASSERT(count == 2U);
    HENKA_TEST_ASSERT(entities[0] == first);
    count = sandbox3d_object_authoring_collect_user_entities(scene, entities, 4U);
    HENKA_TEST_ASSERT(count == 2U);
    HENKA_TEST_ASSERT(entities[0] == first);
    HENKA_TEST_ASSERT(entities[1] == second);
    HENKA_TEST_ASSERT(sandbox3d_object_authoring_collect_user_entities(scene, NULL, 0U) == 2U);
    HENKA_TEST_ASSERT(!sandbox3d_object_authoring_can_edit_entity(scene, helper));

    henka_scene_destroy_entity(scene, second);
    stale = second;
    HENKA_TEST_ASSERT(!henka_scene_is_entity_valid(scene, stale));
    HENKA_TEST_ASSERT(!sandbox3d_object_authoring_can_edit_entity(scene, stale));
    HENKA_TEST_ASSERT(sandbox3d_object_authoring_collect_user_entities(scene, entities, 4U) == 1U);
    HENKA_TEST_ASSERT(entities[0] == first);

    henka_scene_destroy(scene);
}

static void henka_test_sandbox3d_object_authoring_duplicate(void)
{
    henka_scene* scene;
    henka_entity source;
    henka_entity duplicate;
    henka_entity helper;
    henka_transform source_transform;
    henka_transform duplicate_transform;
    henka_bounds bounds;
    henka_interaction_desc interaction;
    henka_material material;
    const henka_material_asset* material_asset;
    const henka_material_asset* duplicate_material_asset;
    henka_result result;

    HENKA_TEST_ASSERT(henka_scene_create(&scene) == HENKA_SUCCESS);
    source = henka_scene_create_entity_named(scene, "Source");
    helper = henka_scene_create_entity_named(scene, "Helper");
    HENKA_TEST_ASSERT(source != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(helper != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(henka_scene_set_entity_tag(scene, source, "authoring") == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_set_entity_visible(scene, source, false) == HENKA_SUCCESS);
    source_transform = henka_transform_identity();
    source_transform.position = (henka_vec3){2.0f, 3.0f, 4.0f};
    source_transform.scale = (henka_vec3){2.0f, 1.5f, 0.5f};
    HENKA_TEST_ASSERT(henka_scene_set_entity_transform(scene, source, source_transform) == HENKA_SUCCESS);
    bounds = (henka_bounds){{1.0f, 2.0f, 3.0f}, {0.5f, 0.75f, 1.0f}};
    HENKA_TEST_ASSERT(henka_scene_set_entity_local_bounds(scene, source, bounds) == HENKA_SUCCESS);
    interaction = (henka_interaction_desc){true, 7.0f, "Use"};
    HENKA_TEST_ASSERT(henka_scene_set_entity_interaction(scene, source, &interaction) == HENKA_SUCCESS);
    material = henka_material_default();
    material.name = "Source Material";
    material.shader = (henka_shader*)(uintptr_t)1U;
    material.metallic = 0.8f;
    HENKA_TEST_ASSERT(henka_scene_set_entity_material(scene, source, material) == HENKA_SUCCESS);
    material_asset = (const henka_material_asset*)(uintptr_t)2U;
    HENKA_TEST_ASSERT(henka_scene_set_entity_material_asset(scene, source, material_asset) == HENKA_SUCCESS);

    result = sandbox3d_object_authoring_duplicate_entity(scene, source, "Duplicate", &duplicate);
    HENKA_TEST_ASSERT(result == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(duplicate != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(strcmp(henka_scene_get_entity_name(scene, duplicate), "Duplicate") == 0);
    HENKA_TEST_ASSERT(strcmp(henka_scene_get_entity_tag(scene, duplicate), "authoring") == 0);
    HENKA_TEST_ASSERT(!henka_scene_is_entity_visible(scene, duplicate));
    HENKA_TEST_ASSERT(henka_scene_get_entity_transform(scene, duplicate, &duplicate_transform) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(duplicate_transform.position.x, 2.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(duplicate_transform.scale.z, 0.5f, 0.0001f);
    HENKA_TEST_ASSERT(henka_scene_get_entity_local_bounds(scene, duplicate, &bounds) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(bounds.extents.y, 0.75f, 0.0001f);
    HENKA_TEST_ASSERT(henka_scene_get_entity_interaction(scene, duplicate, &interaction) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(interaction.enabled);
    HENKA_TEST_ASSERT(strcmp(interaction.prompt, "Use") == 0);
    HENKA_TEST_ASSERT(henka_scene_get_entity_material(scene, duplicate, &material) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(material.metallic, 0.8f, 0.0001f);
    HENKA_TEST_ASSERT(henka_scene_get_entity_material_asset(
        scene, duplicate, &duplicate_material_asset) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(duplicate_material_asset == material_asset);

    duplicate_transform.position.x = 99.0f;
    HENKA_TEST_ASSERT(henka_scene_set_entity_transform(scene, duplicate, duplicate_transform) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_get_entity_transform(scene, source, &source_transform) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(source_transform.position.x, 2.0f, 0.0001f);

    HENKA_TEST_ASSERT(sandbox3d_object_authoring_duplicate_entity(scene, helper, "Invalid", &duplicate) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(duplicate == HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(sandbox3d_object_authoring_duplicate_entity(scene, source, NULL, &duplicate) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(duplicate == HENKA_INVALID_ENTITY);

    henka_scene_destroy(scene);
}

static void henka_test_sandbox3d_object_authoring_source_persistence(void)
{
    henka_engine_config config = {0};
    henka_engine* engine = NULL;
    henka_scene* scene = NULL;
    sandbox3d_authoring_object* object = NULL;
    henka_entity entity;
    henka_authoring_mesh_counts saved_counts;
    henka_authoring_mesh_counts changed_counts;
    henka_authoring_mesh_counts restored_counts;
    henka_mesh* render_mesh = NULL;
    henka_mesh* uv_render_mesh = NULL;
    henka_mesh* previous_mesh = NULL;
    henka_physics_world* physics_world = NULL;
    henka_physics_body_id physics_body = HENKA_INVALID_PHYSICS_BODY_ID;
    henka_physics_body_state physics_state;
    henka_physics_body_desc physics_desc = {0};
    henka_bounds edited_physics_bounds;
    const henka_bounds previous_bounds = {{3.0f, 4.0f, 5.0f}, {0.25f, 0.5f, 0.75f}};
    henka_bounds bounds;
    henka_transform project_transform = henka_transform_identity();
    henka_transform loaded_transform;

    config.application_name = "Henka Authoring Persistence Test";
    config.window_width = 320;
    config.window_height = 240;
    config.enable_vsync = false;
    HENKA_TEST_ASSERT(henka_engine_create(&config, &engine) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_create(&scene) == HENKA_SUCCESS);
    entity = henka_scene_create_entity_named(scene, "Authoring Source");
    HENKA_TEST_ASSERT(entity != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(henka_mesh_create_cube(engine, &previous_mesh) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_set_entity_mesh(scene, entity, previous_mesh) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_set_entity_local_bounds(scene, entity, previous_bounds) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_create_box(
        engine, scene, entity, 1.0f, 1.0f, 1.0f, NULL, 8U, &object) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(
        sandbox3d_authoring_object_get_selected_component_count(object) == 0U);
    HENKA_TEST_ASSERT(
        sandbox3d_authoring_object_get_selected_face(object) == HENKA_AUTHORING_INVALID_ID);
    HENKA_TEST_ASSERT(
        sandbox3d_authoring_object_get_active_component_id(object) == HENKA_AUTHORING_INVALID_ID);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_save_source(
        object, "build/test_tmp/authoring_object_empty.hams") == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_reload_source(
        object, "build/test_tmp/authoring_object_empty.hams") == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(
        sandbox3d_authoring_object_get_selected_component_count(object) == 0U);
    HENKA_TEST_ASSERT(
        sandbox3d_authoring_object_get_selected_face(object) == HENKA_AUTHORING_INVALID_ID);
    HENKA_TEST_ASSERT(
        sandbox3d_authoring_object_get_active_component_id(object) == HENKA_AUTHORING_INVALID_ID);
    HENKA_TEST_ASSERT(henka_physics_world_create(&physics_world) == HENKA_SUCCESS);
    physics_desc.type = HENKA_PHYSICS_BODY_STATIC;
    physics_desc.transform = henka_transform_identity();
    physics_desc.mass = 1.0f;
    physics_desc.material = henka_physics_material_default();
    physics_desc.collider = henka_physics_collider_box((henka_vec3){0.5f, 0.5f, 0.5f});
    physics_desc.linked_scene = scene;
    physics_desc.linked_entity = entity;
    HENKA_TEST_ASSERT(henka_physics_body_create(
        physics_world, &physics_desc, &physics_body) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_bind_physics(
        object, physics_world, physics_body) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_body_get_state(
        physics_world, physics_body, &physics_state) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(physics_state.collider.data.box.half_extents.x, 0.5f, 0.0001f);
    HENKA_TEST_ASSERT(
        sandbox3d_authoring_object_select_component(object, 1U, false) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_set_selected_face_material_region(
        object, 17U) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_authoring_mesh_get_face(
        sandbox3d_authoring_object_get_mesh(object),
        sandbox3d_authoring_object_get_selected_face(object))->material_region == 17U);
    {
        uint32_t minimum_region = UINT32_MAX;
        uint32_t maximum_region = 0U;
        HENKA_TEST_ASSERT(
            sandbox3d_authoring_object_get_render_material_region_range(
                object, &minimum_region, &maximum_region) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(minimum_region == 0U && maximum_region == 17U);
    }
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_undo(object) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_authoring_mesh_get_face(
        sandbox3d_authoring_object_get_mesh(object),
        sandbox3d_authoring_object_get_selected_face(object))->material_region != 17U);
    {
        uint32_t minimum_region = UINT32_MAX;
        uint32_t maximum_region = 0U;
        HENKA_TEST_ASSERT(
            sandbox3d_authoring_object_get_render_material_region_range(
                object, &minimum_region, &maximum_region) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(minimum_region == 0U && maximum_region == 0U);
    }
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_redo(object) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_authoring_mesh_get_face(
        sandbox3d_authoring_object_get_mesh(object),
        sandbox3d_authoring_object_get_selected_face(object))->material_region == 17U);
    {
        uint32_t minimum_region = UINT32_MAX;
        uint32_t maximum_region = 0U;
        HENKA_TEST_ASSERT(
            sandbox3d_authoring_object_get_render_material_region_range(
                object, &minimum_region, &maximum_region) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(minimum_region == 0U && maximum_region == 17U);
    }
    {
        const henka_ray pick_ray = {{0.0f, 0.0f, 5.0f}, {0.0f, 0.0f, -1.0f}};
        const henka_ray miss_ray = {{0.0f, 0.0f, 5.0f}, {0.0f, 1.0f, 0.0f}};
        henka_authoring_face_id picked_face;
        HENKA_TEST_ASSERT(sandbox3d_authoring_object_pick_face(object, pick_ray, 100.0f) == HENKA_SUCCESS);
        picked_face = sandbox3d_authoring_object_get_selected_face(object);
        HENKA_TEST_ASSERT(henka_authoring_mesh_get_face(
            sandbox3d_authoring_object_get_mesh(object), picked_face) != NULL);
        HENKA_TEST_ASSERT(sandbox3d_authoring_object_pick_face(object, miss_ray, 100.0f) != HENKA_SUCCESS);
        HENKA_TEST_ASSERT(sandbox3d_authoring_object_get_selected_face(object) == picked_face);
        {
            const henka_authoring_face_id selected_before_extrude = picked_face;
            henka_authoring_face_id selected_after_extrude;
            HENKA_TEST_ASSERT(sandbox3d_authoring_object_extrude_selected_face(object, 0.125f) == HENKA_SUCCESS);
            selected_after_extrude = sandbox3d_authoring_object_get_selected_face(object);
            HENKA_TEST_ASSERT(selected_after_extrude != selected_before_extrude);
            HENKA_TEST_ASSERT(henka_authoring_mesh_get_face(
                sandbox3d_authoring_object_get_mesh(object), selected_after_extrude) != NULL);
            HENKA_TEST_ASSERT(sandbox3d_authoring_object_undo(object) == HENKA_SUCCESS);
            HENKA_TEST_ASSERT(sandbox3d_authoring_object_get_selected_face(object) == selected_before_extrude);
            HENKA_TEST_ASSERT(henka_authoring_mesh_get_face(
                sandbox3d_authoring_object_get_mesh(object), selected_before_extrude) != NULL);
            HENKA_TEST_ASSERT(sandbox3d_authoring_object_redo(object) == HENKA_SUCCESS);
            HENKA_TEST_ASSERT(sandbox3d_authoring_object_get_selected_face(object) == selected_after_extrude);
            HENKA_TEST_ASSERT(henka_authoring_mesh_get_face(
                sandbox3d_authoring_object_get_mesh(object), selected_after_extrude) != NULL);
            HENKA_TEST_ASSERT(sandbox3d_authoring_object_undo(object) == HENKA_SUCCESS);
            HENKA_TEST_ASSERT(sandbox3d_authoring_object_set_selected_face_material_region(object, 23U) == HENKA_SUCCESS);
            HENKA_TEST_ASSERT(sandbox3d_authoring_object_redo(object) != HENKA_SUCCESS);
            HENKA_TEST_ASSERT(sandbox3d_authoring_object_get_selected_face(object) == selected_before_extrude);
        }
        {
            const henka_authoring_mesh_counts before_bevel = henka_authoring_mesh_get_counts(
                sandbox3d_authoring_object_get_mesh(object));
            HENKA_TEST_ASSERT(sandbox3d_authoring_object_bevel_selected_face(object, 0.1f) == HENKA_SUCCESS);
            HENKA_TEST_ASSERT(henka_authoring_mesh_get_counts(
                sandbox3d_authoring_object_get_mesh(object)).faces > before_bevel.faces);
            HENKA_TEST_ASSERT(sandbox3d_authoring_object_undo(object) == HENKA_SUCCESS);
            HENKA_TEST_ASSERT(sandbox3d_authoring_object_subdivide_selected_face(object) == HENKA_SUCCESS);
            HENKA_TEST_ASSERT(henka_authoring_mesh_get_counts(
                sandbox3d_authoring_object_get_mesh(object)).vertices > before_bevel.vertices);
            HENKA_TEST_ASSERT(sandbox3d_authoring_object_undo(object) == HENKA_SUCCESS);
        }
    }

    HENKA_TEST_ASSERT(henka_scene_get_entity_mesh(scene, entity, &render_mesh) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_project_selected_face_uv(
        object, HENKA_AUTHORING_UV_PROJECT_Z) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_authoring_mesh_face_uvs_are_finite(
        sandbox3d_authoring_object_get_mesh(object),
        sandbox3d_authoring_object_get_selected_face(object)));
    HENKA_TEST_ASSERT(henka_scene_get_entity_mesh(scene, entity, &uv_render_mesh) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(uv_render_mesh != NULL && uv_render_mesh != render_mesh);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_pack_selected_face_uv(object, 0.02f) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_undo(object) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_undo(object) == HENKA_SUCCESS);

    HENKA_TEST_ASSERT(sandbox3d_authoring_object_extrude_selected_face(object, 0.25f) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_get_entity_local_bounds(
        scene, entity, &edited_physics_bounds) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_body_get_state(
        physics_world, physics_body, &physics_state) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        physics_state.collider.data.box.half_extents.x, edited_physics_bounds.extents.x, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        physics_state.collider.data.box.half_extents.y, edited_physics_bounds.extents.y, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        physics_state.collider.data.box.half_extents.z, edited_physics_bounds.extents.z, 0.0001f);
    saved_counts = henka_authoring_mesh_get_counts(sandbox3d_authoring_object_get_mesh(object));
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_save_source(object, "build/test_tmp/authoring_object_source.hams") == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_inset_selected_face(object, 0.75f) == HENKA_SUCCESS);
    changed_counts = henka_authoring_mesh_get_counts(sandbox3d_authoring_object_get_mesh(object));
    HENKA_TEST_ASSERT(changed_counts.faces != saved_counts.faces);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_reload_source(object, "build/test_tmp/authoring_object_source.hams") == HENKA_SUCCESS);
    restored_counts = henka_authoring_mesh_get_counts(sandbox3d_authoring_object_get_mesh(object));
    HENKA_TEST_ASSERT(restored_counts.faces == saved_counts.faces);
    HENKA_TEST_ASSERT(henka_authoring_mesh_get_face(
        sandbox3d_authoring_object_get_mesh(object),
        sandbox3d_authoring_object_get_selected_face(object)) != NULL);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_redo(object) != HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_get_entity_mesh(scene, entity, &render_mesh) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(render_mesh != NULL);
    HENKA_TEST_ASSERT(henka_scene_get_entity_local_bounds(scene, entity, &bounds) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(bounds.extents.x > 0.0f && bounds.extents.y > 0.0f && bounds.extents.z > 0.0f);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_reload_source(object, "build/test_tmp/authoring_object_missing.hams") != HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_authoring_mesh_get_counts(sandbox3d_authoring_object_get_mesh(object)).faces == saved_counts.faces);

    project_transform.position = (henka_vec3){6.0f, 7.0f, 8.0f};
    project_transform.scale = (henka_vec3){1.25f, 0.75f, 1.5f};
    HENKA_TEST_ASSERT(henka_scene_set_entity_transform(scene, entity, project_transform) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_save_project(
        object,
        "build/test_tmp/authoring_project.henka",
        "build/test_tmp/authoring_project.hams") == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(strcmp(
        sandbox3d_authoring_object_get_source_path(object),
        "build/test_tmp/authoring_project.hams") == 0);
    project_transform.position.x = 42.0f;
    HENKA_TEST_ASSERT(henka_scene_set_entity_transform(scene, entity, project_transform) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_load_project(
        object, "build/test_tmp/authoring_project.henka") == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_get_entity_transform(scene, entity, &loaded_transform) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(loaded_transform.position.x, 6.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(loaded_transform.scale.y, 0.75f, 0.0001f);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_load_project(
        object, "build/test_tmp/authoring_project_missing.henka") != HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_get_entity_transform(scene, entity, &loaded_transform) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(loaded_transform.position.x, 6.0f, 0.0001f);

    HENKA_TEST_ASSERT(henka_physics_body_get_state(
        physics_world, physics_body, &physics_state) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(physics_state.collider.data.box.half_extents.y, bounds.extents.y, 0.0001f);
    sandbox3d_authoring_object_unbind_physics(object);
    sandbox3d_authoring_object_destroy(object);
    HENKA_TEST_ASSERT(henka_physics_body_destroy(physics_world, physics_body) == HENKA_SUCCESS);
    henka_physics_world_destroy(physics_world);
    HENKA_TEST_ASSERT(henka_scene_get_entity_mesh(scene, entity, &render_mesh) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(render_mesh == previous_mesh);
    HENKA_TEST_ASSERT(henka_scene_get_entity_local_bounds(scene, entity, &bounds) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(bounds.center.x, previous_bounds.center.x, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(bounds.center.y, previous_bounds.center.y, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(bounds.center.z, previous_bounds.center.z, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(bounds.extents.x, previous_bounds.extents.x, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(bounds.extents.y, previous_bounds.extents.y, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(bounds.extents.z, previous_bounds.extents.z, 0.0001f);
    henka_mesh_destroy(previous_mesh);
    henka_scene_destroy(scene);
    henka_engine_destroy(engine);
}

static void henka_test_sandbox3d_object_authoring_clone_bridge(void)
{
    henka_engine_config config = {0};
    henka_engine* engine = NULL;
    henka_scene* scene = NULL;
    sandbox3d_authoring_object* source_object = NULL;
    sandbox3d_authoring_object* clone_object = NULL;
    henka_authoring_mesh* source_clone = NULL;
    henka_entity source_entity;
    henka_entity clone_entity;
    henka_mesh* clone_previous_mesh = NULL;
    henka_mesh* clone_render_mesh = NULL;
    henka_authoring_mesh_counts source_counts;

    config.application_name = "Henka Authoring Clone Test";
    config.window_width = 320;
    config.window_height = 240;
    config.enable_vsync = false;
    HENKA_TEST_ASSERT(henka_engine_create(&config, &engine) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_create(&scene) == HENKA_SUCCESS);
    source_entity = henka_scene_create_entity_named(scene, "Authoring Source");
    clone_entity = henka_scene_create_entity_named(scene, "Authoring Clone");
    HENKA_TEST_ASSERT(source_entity != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(clone_entity != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(henka_mesh_create_cube(engine, &clone_previous_mesh) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_set_entity_mesh(scene, clone_entity, clone_previous_mesh) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_create_box(
        engine, scene, source_entity, 1.0f, 1.0f, 1.0f, NULL, 8U, &source_object) == HENKA_SUCCESS);
    source_counts = henka_authoring_mesh_get_counts(sandbox3d_authoring_object_get_mesh(source_object));
    HENKA_TEST_ASSERT(henka_authoring_mesh_clone(
        sandbox3d_authoring_object_get_mesh(source_object), &source_clone) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_create_from_mesh(
        engine, scene, clone_entity, source_clone, 8U, &clone_object) == HENKA_SUCCESS);
    henka_authoring_mesh_destroy(source_clone);
    source_clone = NULL;
    HENKA_TEST_ASSERT(henka_authoring_mesh_get_counts(
        sandbox3d_authoring_object_get_mesh(clone_object)).faces == source_counts.faces);
    HENKA_TEST_ASSERT(henka_scene_get_entity_mesh(scene, clone_entity, &clone_render_mesh) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(clone_render_mesh != clone_previous_mesh);
    sandbox3d_authoring_object_set_selection_mode(clone_object, SANDBOX3D_AUTHORING_SELECTION_VERTEX);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_select_component(clone_object, 1U, false) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_move_selected_components(
        clone_object, (henka_vec3){0.25f, 0.0f, 0.0f}) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_authoring_mesh_get_vertex(
        sandbox3d_authoring_object_get_mesh(source_object), 1U)->position.x !=
        henka_authoring_mesh_get_vertex(sandbox3d_authoring_object_get_mesh(clone_object), 1U)->position.x);
    sandbox3d_authoring_object_destroy(clone_object);
    clone_object = NULL;
    HENKA_TEST_ASSERT(henka_scene_get_entity_mesh(scene, clone_entity, &clone_render_mesh) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(clone_render_mesh == clone_previous_mesh);
    henka_scene_destroy_entity(scene, clone_entity);
    sandbox3d_authoring_object_destroy(source_object);
    henka_mesh_destroy(clone_previous_mesh);
    henka_scene_destroy(scene);
    henka_engine_destroy(engine);
}

static void henka_test_sandbox3d_object_authoring_model_primitive_bridge(void)
{
    henka_engine_config config = {0};
    henka_engine* engine = NULL;
    henka_scene* scene = NULL;
    sandbox3d_authoring_object* object = NULL;
    henka_entity entity;
    henka_mesh* previous_mesh = NULL;
    henka_mesh* authored_mesh = NULL;
    const henka_material_asset* material_asset =
        (const henka_material_asset*)(uintptr_t)3U;
    henka_bounds authored_bounds;
    henka_model_vertex vertices[4] = {0};
    uint32_t indices[6] = {0U, 1U, 2U, 0U, 2U, 3U};
    henka_model_scene_primitive primitive = {0};
    henka_authoring_mesh_counts counts;

    config.application_name = "Henka Imported Authoring Bridge Test";
    config.window_width = 320;
    config.window_height = 240;
    config.enable_vsync = false;
    HENKA_TEST_ASSERT(henka_engine_create(&config, &engine) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_create(&scene) == HENKA_SUCCESS);
    entity = henka_scene_create_entity_named(scene, "Imported Showcase Primitive");
    HENKA_TEST_ASSERT(entity != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(henka_mesh_create_cube(engine, &previous_mesh) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_set_entity_mesh(scene, entity, previous_mesh) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_set_entity_material_asset(scene, entity, material_asset) == HENKA_SUCCESS);

    vertices[0].position = (henka_vec3){-1.0f, -1.0f, 0.0f};
    vertices[1].position = (henka_vec3){1.0f, -1.0f, 0.0f};
    vertices[2].position = (henka_vec3){1.0f, 1.0f, 0.0f};
    vertices[3].position = (henka_vec3){-1.0f, 1.0f, 0.0f};
    vertices[0].uv = (henka_vec2){0.0f, 0.0f};
    vertices[1].uv = (henka_vec2){1.0f, 0.0f};
    vertices[2].uv = (henka_vec2){1.0f, 1.0f};
    vertices[3].uv = (henka_vec2){0.0f, 1.0f};
    vertices[0].material_region = 2U;
    vertices[1].material_region = 2U;
    vertices[2].material_region = 2U;
    vertices[3].material_region = 4U;
    primitive.vertices = vertices;
    primitive.vertex_count = 4U;
    primitive.indices = indices;
    primitive.index_count = 6U;

    indices[5] = 4U;
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_create_from_model_primitive(
        engine, scene, entity, &primitive, 8U, &object) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(object == NULL);
    HENKA_TEST_ASSERT(henka_scene_get_entity_mesh(scene, entity, &authored_mesh) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(authored_mesh == previous_mesh);
    indices[5] = 3U;

    HENKA_TEST_ASSERT(sandbox3d_authoring_object_create_from_model_primitive(
        engine, scene, entity, &primitive, 8U, &object) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(object != NULL);
    counts = henka_authoring_mesh_get_counts(sandbox3d_authoring_object_get_mesh(object));
    HENKA_TEST_ASSERT(counts.vertices == 4U && counts.faces == 2U && counts.edges == 5U);
    HENKA_TEST_ASSERT(henka_scene_get_entity_mesh(scene, entity, &authored_mesh) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(authored_mesh != NULL && authored_mesh != previous_mesh);
    HENKA_TEST_ASSERT(henka_scene_get_entity_material_asset(scene, entity, &material_asset) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(material_asset == (const henka_material_asset*)(uintptr_t)3U);
    HENKA_TEST_ASSERT(henka_scene_get_entity_local_bounds(scene, entity, &authored_bounds) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(authored_bounds.extents.x > 0.0f && authored_bounds.extents.y > 0.0f);

    {
        const henka_authoring_mesh_counts before_profile =
            henka_authoring_mesh_get_counts(sandbox3d_authoring_object_get_mesh(object));
        const henka_vec3 before_position =
            henka_authoring_mesh_get_vertex(
                sandbox3d_authoring_object_get_mesh(object), 3U)->position;
        const sandbox3d_authoring_region_transform profile = {
            {-1.0f, -1.0f, -0.1f},
            {1.0f, 1.0f, 0.1f},
            {0.0f, 0.0f, 0.0f},
            {1.25f, 1.0f, 0.75f},
            {0.125f, 0.0f, 0.0f}};
        size_t affected_vertices = 0U;
        HENKA_TEST_ASSERT(sandbox3d_authoring_object_transform_vertex_region(
            object, &profile, &affected_vertices) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(affected_vertices == 4U);
        HENKA_TEST_ASSERT(henka_authoring_mesh_validate(
            sandbox3d_authoring_object_get_mesh(object)));
        HENKA_TEST_ASSERT(henka_authoring_mesh_get_counts(
            sandbox3d_authoring_object_get_mesh(object)).vertices == before_profile.vertices);
        HENKA_TEST_ASSERT(henka_authoring_mesh_get_counts(
            sandbox3d_authoring_object_get_mesh(object)).faces == before_profile.faces);
        {
            const henka_authoring_mesh* profiled_mesh =
                sandbox3d_authoring_object_get_mesh(object);
            const henka_authoring_mesh_counts profiled_counts =
                henka_authoring_mesh_get_counts(profiled_mesh);
            size_t active_vertices = 0U;
            size_t active_edges = 0U;
            size_t active_faces = 0U;
            uint32_t id;
            float minimum_x = 1000000.0f;
            float maximum_x = -1000000.0f;
            float minimum_y = 1000000.0f;
            float maximum_y = -1000000.0f;
            for (id = 1U; id <= HENKA_AUTHORING_MESH_HARD_MAX_VERTICES; ++id)
            {
                const henka_authoring_vertex* vertex =
                    henka_authoring_mesh_get_vertex(profiled_mesh, id);
                if (vertex == NULL) continue;
                ++active_vertices;
                if (vertex->position.x < minimum_x) minimum_x = vertex->position.x;
                if (vertex->position.x > maximum_x) maximum_x = vertex->position.x;
                if (vertex->position.y < minimum_y) minimum_y = vertex->position.y;
                if (vertex->position.y > maximum_y) maximum_y = vertex->position.y;
            }
            for (id = 1U; id <= HENKA_AUTHORING_MESH_HARD_MAX_EDGES; ++id)
            {
                const henka_authoring_edge* edge =
                    henka_authoring_mesh_get_edge(profiled_mesh, id);
                size_t face_index;
                if (edge == NULL) continue;
                ++active_edges;
                HENKA_TEST_ASSERT(henka_authoring_mesh_get_edge_face_count(profiled_mesh, id) >= 1U);
                HENKA_TEST_ASSERT(henka_authoring_mesh_get_edge_face_count(profiled_mesh, id) <= 2U);
                for (face_index = 0U; face_index < edge->face_count; ++face_index)
                {
                    henka_authoring_face_id face_id = HENKA_AUTHORING_INVALID_ID;
                    HENKA_TEST_ASSERT(henka_authoring_mesh_get_edge_face_at(
                        profiled_mesh, id, face_index, &face_id) == HENKA_SUCCESS);
                    HENKA_TEST_ASSERT(henka_authoring_mesh_get_face(profiled_mesh, face_id) != NULL);
                }
            }
            for (id = 1U; id <= HENKA_AUTHORING_MESH_HARD_MAX_FACES; ++id)
            {
                if (henka_authoring_mesh_get_face(profiled_mesh, id) != NULL) ++active_faces;
            }
            HENKA_TEST_ASSERT(active_vertices == profiled_counts.vertices);
            HENKA_TEST_ASSERT(active_edges == profiled_counts.edges);
            HENKA_TEST_ASSERT(active_faces == profiled_counts.faces);
            HENKA_TEST_ASSERT(maximum_x > minimum_x && maximum_y > minimum_y);
            HENKA_TEST_ASSERT(minimum_x < -0.9f && maximum_x > 0.9f);
            HENKA_TEST_ASSERT(minimum_y < -0.9f && maximum_y > 0.9f);
        }
        HENKA_TEST_ASSERT_FLOAT_CLOSE(
            henka_authoring_mesh_get_vertex(
                sandbox3d_authoring_object_get_mesh(object), 3U)->position.x,
            before_position.x * 1.25f + 0.125f,
            0.0001f);
        HENKA_TEST_ASSERT(sandbox3d_authoring_object_undo(object) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(
            henka_authoring_mesh_get_vertex(
                sandbox3d_authoring_object_get_mesh(object), 3U)->position.x,
            before_position.x,
            0.0001f);
        HENKA_TEST_ASSERT(sandbox3d_authoring_object_redo(object) == HENKA_SUCCESS);
    }

    {
        const henka_authoring_mesh_counts before_regions =
            henka_authoring_mesh_get_counts(sandbox3d_authoring_object_get_mesh(object));
        const sandbox3d_authoring_region_transform regions[] = {
            {{-2.0f, -1.0f, -1.0f}, {2.0f, 0.0f, 1.0f}, {0.0f, -0.55f, 0.0f},
                {1.10f, 0.95f, 1.0f}, {0.10f, 0.0f, 0.0f}},
            {{-2.0f, 0.0f, -1.0f}, {2.0f, 1.0f, 1.0f}, {0.0f, 0.55f, 0.0f},
                {0.90f, 1.08f, 1.0f}, {-0.08f, 0.05f, 0.0f}}};
        size_t affected_regions = 0U;
        HENKA_TEST_ASSERT(sandbox3d_authoring_object_transform_vertex_regions(
            object, regions, sizeof(regions) / sizeof(regions[0]),
            &affected_regions) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(affected_regions == 4U);
        HENKA_TEST_ASSERT(henka_authoring_mesh_validate(
            sandbox3d_authoring_object_get_mesh(object)));
        HENKA_TEST_ASSERT(henka_authoring_mesh_get_counts(
            sandbox3d_authoring_object_get_mesh(object)).vertices == before_regions.vertices);
        HENKA_TEST_ASSERT(henka_authoring_mesh_get_counts(
            sandbox3d_authoring_object_get_mesh(object)).faces == before_regions.faces);
        HENKA_TEST_ASSERT(sandbox3d_authoring_object_undo(object) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(henka_authoring_mesh_get_counts(
            sandbox3d_authoring_object_get_mesh(object)).vertices == before_regions.vertices);
        HENKA_TEST_ASSERT(sandbox3d_authoring_object_redo(object) == HENKA_SUCCESS);
    }

    sandbox3d_authoring_object_set_selection_mode(object, SANDBOX3D_AUTHORING_SELECTION_FACE);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_select_face(object, 1U) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_bevel_selected_face(object, 0.1f) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_authoring_mesh_get_counts(
        sandbox3d_authoring_object_get_mesh(object)).faces > counts.faces);

    sandbox3d_authoring_object_destroy(object);
    HENKA_TEST_ASSERT(henka_scene_get_entity_mesh(scene, entity, &authored_mesh) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(authored_mesh == previous_mesh);
    henka_scene_destroy(scene);
    henka_mesh_destroy(previous_mesh);
    henka_engine_destroy(engine);
}

static void henka_test_sandbox3d_object_authoring_component_selection(void)
{
    henka_engine_config config = {0};
    henka_engine* engine = NULL;
    henka_scene* scene = NULL;
    sandbox3d_authoring_object* object = NULL;
    henka_mesh* previous_mesh = NULL;
    henka_entity entity;
    uint32_t id;

    config.application_name = "Henka Authoring Components Test";
    config.window_width = 320;
    config.window_height = 240;
    config.enable_vsync = false;
    HENKA_TEST_ASSERT(henka_engine_create(&config, &engine) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_create(&scene) == HENKA_SUCCESS);
    entity = henka_scene_create_entity_named(scene, "Component Source");
    HENKA_TEST_ASSERT(entity != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(henka_mesh_create_cube(engine, &previous_mesh) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_set_entity_mesh(scene, entity, previous_mesh) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_create_box(
        engine, scene, entity, 1.0f, 1.0f, 1.0f, NULL, 8U, &object) == HENKA_SUCCESS);

    sandbox3d_authoring_object_set_selection_mode(object, SANDBOX3D_AUTHORING_SELECTION_VERTEX);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_select_component(object, 1U, false) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_get_active_component_id(object) == 1U);
    {
        const henka_authoring_mesh* mesh = sandbox3d_authoring_object_get_mesh(object);
        const henka_authoring_vertex* selected_before = henka_authoring_mesh_get_vertex(mesh, 1U);
        henka_vec3 selected_before_position;
        henka_authoring_edge_id edge_id = HENKA_AUTHORING_INVALID_ID;
        const henka_authoring_edge* edge;
        henka_authoring_vertex_id neighbor_id;
        const henka_authoring_vertex* neighbor_before;
        henka_vec3 neighbor_before_position;
        const henka_authoring_vertex* selected_after;
        const henka_authoring_vertex* neighbor_after;
        float selected_delta;
        float neighbor_delta;
        HENKA_TEST_ASSERT(selected_before != NULL);
        selected_before_position = selected_before->position;
        HENKA_TEST_ASSERT(henka_authoring_mesh_get_vertex_edge_at(
            mesh, 1U, 0U, &edge_id) == HENKA_SUCCESS);
        edge = henka_authoring_mesh_get_edge(mesh, edge_id);
        HENKA_TEST_ASSERT(edge != NULL);
        neighbor_id = edge->vertices[0] == 1U ? edge->vertices[1] : edge->vertices[0];
        neighbor_before = henka_authoring_mesh_get_vertex(mesh, neighbor_id);
        HENKA_TEST_ASSERT(neighbor_before != NULL);
        neighbor_before_position = neighbor_before->position;
        HENKA_TEST_ASSERT(sandbox3d_authoring_object_proportional_move_selected_components(
            object, (henka_vec3){0.0f, 0.4f, 0.0f}, 1U) == HENKA_SUCCESS);
        selected_after = henka_authoring_mesh_get_vertex(
            sandbox3d_authoring_object_get_mesh(object), 1U);
        neighbor_after = henka_authoring_mesh_get_vertex(
            sandbox3d_authoring_object_get_mesh(object), neighbor_id);
        HENKA_TEST_ASSERT(selected_after != NULL && neighbor_after != NULL);
        selected_delta = selected_after->position.y - selected_before_position.y;
        neighbor_delta = neighbor_after->position.y - neighbor_before_position.y;
        HENKA_TEST_ASSERT(selected_delta > 0.39f && selected_delta < 0.41f);
        HENKA_TEST_ASSERT(neighbor_delta > 0.19f && neighbor_delta < 0.21f);
        HENKA_TEST_ASSERT(henka_authoring_mesh_validate(
            sandbox3d_authoring_object_get_mesh(object)));
        HENKA_TEST_ASSERT(sandbox3d_authoring_object_undo(object) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(sandbox3d_authoring_object_redo(object) == HENKA_SUCCESS);
    }
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_select_component(object, 2U, true) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_get_active_component_id(object) == 2U);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_get_selected_component_count(object) == 2U);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_get_selected_component_at(object, 1U, &id) == HENKA_SUCCESS && id == 2U);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_move_selected_components(
        object, (henka_vec3){0.25f, 0.0f, 0.0f}) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_grow_component_selection(object) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_get_selected_component_count(object) > 2U);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_select_connected_components(object) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_get_selected_component_count(object) == 8U);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_scale_selected_components(
        object, (henka_vec3){1.05f, 1.0f, 1.05f}) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_authoring_mesh_validate(
        sandbox3d_authoring_object_get_mesh(object)));

    sandbox3d_authoring_object_set_selection_mode(object, SANDBOX3D_AUTHORING_SELECTION_EDGE);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_select_component(object, 1U, false) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_get_active_component_id(object) == 1U);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_get_selected_component_count(object) == 1U);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_select_edge_loop(object) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_get_active_component_id(object) == 1U);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_get_selected_component_count(object) == 4U);
    {
        size_t selected_index;
        for (selected_index = 0U;
             selected_index < sandbox3d_authoring_object_get_selected_component_count(object);
             ++selected_index)
        {
            uint32_t selected_edge_id = HENKA_AUTHORING_INVALID_ID;
            const henka_authoring_edge* selected_edge;
            HENKA_TEST_ASSERT(sandbox3d_authoring_object_get_selected_component_at(
                object, selected_index, &selected_edge_id) == HENKA_SUCCESS);
            selected_edge = henka_authoring_mesh_get_edge(
                sandbox3d_authoring_object_get_mesh(object), selected_edge_id);
            HENKA_TEST_ASSERT(selected_edge != NULL);
            HENKA_TEST_ASSERT(selected_edge->face_count >= 1U);
        }
    }
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_grow_component_selection(object) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_get_selected_component_count(object) >= 1U);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_select_connected_components(object) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_get_selected_component_count(object) == 12U);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_scale_selected_components(
        object, (henka_vec3){1.0f, 1.05f, 1.0f}) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_move_selected_components(
        object, (henka_vec3){0.0f, 0.25f, 0.0f}) == HENKA_SUCCESS);

    sandbox3d_authoring_object_set_selection_mode(object, SANDBOX3D_AUTHORING_SELECTION_FACE);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_select_component(object, 1U, false) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_get_active_component_id(object) == 1U);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_get_selected_face(object) == 1U);
    {
        const henka_authoring_mesh* before_mesh = sandbox3d_authoring_object_get_mesh(object);
        const henka_authoring_face* before_face = henka_authoring_mesh_get_face(before_mesh, 1U);
        const henka_authoring_vertex* before_first;
        const henka_authoring_vertex* before_second;
        const henka_authoring_vertex* before_third;
        henka_vec3 before_normal;
        henka_vec3 before_position;
        henka_authoring_vertex_id first_vertex_id;
        const henka_authoring_vertex* after_vertex;
        HENKA_TEST_ASSERT(before_face != NULL && before_face->corner_count >= 3U);
        first_vertex_id = before_face->vertices[0];
        before_first = henka_authoring_mesh_get_vertex(before_mesh, before_face->vertices[0]);
        before_second = henka_authoring_mesh_get_vertex(before_mesh, before_face->vertices[1]);
        before_third = henka_authoring_mesh_get_vertex(before_mesh, before_face->vertices[2]);
        HENKA_TEST_ASSERT(before_first != NULL && before_second != NULL && before_third != NULL);
        before_position = before_first->position;
        before_normal = henka_vec3_normalize(henka_vec3_cross(
            henka_vec3_subtract(before_second->position, before_first->position),
            henka_vec3_subtract(before_third->position, before_first->position)));
        HENKA_TEST_ASSERT(sandbox3d_authoring_object_move_selected_face_normal(object, 0.25f) == HENKA_SUCCESS);
        after_vertex = henka_authoring_mesh_get_vertex(
            sandbox3d_authoring_object_get_mesh(object), first_vertex_id);
        HENKA_TEST_ASSERT(after_vertex != NULL);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(
            after_vertex->position.x,
            before_position.x + before_normal.x * 0.25f,
            0.0001f);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(
            after_vertex->position.y,
            before_position.y + before_normal.y * 0.25f,
            0.0001f);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(
            after_vertex->position.z,
            before_position.z + before_normal.z * 0.25f,
            0.0001f);
        HENKA_TEST_ASSERT(henka_authoring_mesh_validate(
            sandbox3d_authoring_object_get_mesh(object)));
    }
    {
        const henka_authoring_mesh* mesh = sandbox3d_authoring_object_get_mesh(object);
        const henka_authoring_face* face = henka_authoring_mesh_get_face(mesh, 1U);
        henka_authoring_vertex_id ordered[HENKA_AUTHORING_MESH_HARD_MAX_FACE_CORNERS];
        size_t ordered_count = 0U;
        size_t corner;
        HENKA_TEST_ASSERT(face != NULL);
        HENKA_TEST_ASSERT(sandbox3d_authoring_object_get_face_ordered_corners(
            object,
            1U,
            ordered,
            sizeof(ordered) / sizeof(ordered[0]),
            &ordered_count) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(ordered_count == face->corner_count);
        for (corner = 0U; corner < ordered_count; ++corner)
        {
            HENKA_TEST_ASSERT(ordered[corner] == face->vertices[corner]);
            HENKA_TEST_ASSERT(ordered[(corner + 1U) % ordered_count] != ordered[corner]);
        }
        HENKA_TEST_ASSERT(sandbox3d_authoring_object_get_face_ordered_corners(
            object,
            1U,
            ordered,
            2U,
            &ordered_count) != HENKA_SUCCESS);
        HENKA_TEST_ASSERT(ordered_count == 0U);
    }
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_grow_component_selection(object) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_get_selected_component_count(object) >= 1U);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_select_connected_components(object) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_get_selected_component_count(object) == 6U);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_scale_selected_components(
        object, (henka_vec3){1.02f, 1.02f, 1.02f}) == HENKA_SUCCESS);
    {
        const henka_authoring_mesh_counts before_delete = henka_authoring_mesh_get_counts(
            sandbox3d_authoring_object_get_mesh(object));
        HENKA_TEST_ASSERT(sandbox3d_authoring_object_select_face(object, 1U) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(sandbox3d_authoring_object_delete_selected_faces(object) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(henka_authoring_mesh_get_counts(
            sandbox3d_authoring_object_get_mesh(object)).faces < before_delete.faces);
        HENKA_TEST_ASSERT(henka_authoring_mesh_validate(
            sandbox3d_authoring_object_get_mesh(object)));
        HENKA_TEST_ASSERT(sandbox3d_authoring_object_undo(object) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(henka_authoring_mesh_get_counts(
            sandbox3d_authoring_object_get_mesh(object)).faces == before_delete.faces);
        HENKA_TEST_ASSERT(henka_authoring_mesh_get_face(
            sandbox3d_authoring_object_get_mesh(object), 1U) != NULL);
        HENKA_TEST_ASSERT(sandbox3d_authoring_object_select_face(object, 1U) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(sandbox3d_authoring_object_select_connected_components(object) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(sandbox3d_authoring_object_delete_selected_faces(object) != HENKA_SUCCESS);
        HENKA_TEST_ASSERT(henka_authoring_mesh_get_counts(
            sandbox3d_authoring_object_get_mesh(object)).faces == before_delete.faces);
        HENKA_TEST_ASSERT(henka_authoring_mesh_validate(
            sandbox3d_authoring_object_get_mesh(object)));
    }
    sandbox3d_authoring_object_destroy(object);
    henka_mesh_destroy(previous_mesh);
    henka_scene_destroy(scene);
    henka_engine_destroy(engine);
}

/* HENKA_T2B_QUAD_WORKFLOW_TEST_V1 */
static void henka_test_sandbox3d_object_authoring_quad_recovery_workflow(void)
{
    henka_engine_config config = {0};
    henka_engine* engine = NULL;
    henka_scene* scene = NULL;
    henka_mesh* previous_mesh = NULL;
    henka_authoring_mesh* source = NULL;
    sandbox3d_authoring_object* object = NULL;
    henka_entity entity;
    henka_authoring_mesh_desc desc = {16U, 32U, 16U, 8U};
    henka_authoring_vertex_id vertices[4];
    henka_authoring_vertex_id first[3];
    henka_authoring_vertex_id second[3];
    henka_authoring_face_id face_id;
    henka_authoring_mesh_counts counts;
    size_t recovered = 0U;

    config.application_name = "Henka Quad Recovery Workflow Test";
    config.window_width = 320;
    config.window_height = 240;
    config.enable_vsync = false;

    HENKA_TEST_ASSERT(
        henka_engine_create(&config, &engine) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(
        henka_scene_create(&scene) == HENKA_SUCCESS);

    entity = henka_scene_create_entity_named(
        scene,
        "Quad Recovery Object");

    HENKA_TEST_ASSERT(entity != HENKA_INVALID_ENTITY);

    HENKA_TEST_ASSERT(
        henka_mesh_create_cube(engine, &previous_mesh) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(
        henka_scene_set_entity_mesh(
            scene,
            entity,
            previous_mesh) == HENKA_SUCCESS);

    HENKA_TEST_ASSERT(
        henka_authoring_mesh_create(
            &desc,
            &source) == HENKA_SUCCESS);

    HENKA_TEST_ASSERT(
        henka_authoring_mesh_add_vertex(
            source,
            (henka_vec3){-1.0f, 0.0f, -1.0f},
            (henka_vec2){0.0f, 0.0f},
            0U,
            &vertices[0]) == HENKA_SUCCESS);

    HENKA_TEST_ASSERT(
        henka_authoring_mesh_add_vertex(
            source,
            (henka_vec3){1.0f, 0.0f, -1.0f},
            (henka_vec2){1.0f, 0.0f},
            0U,
            &vertices[1]) == HENKA_SUCCESS);

    HENKA_TEST_ASSERT(
        henka_authoring_mesh_add_vertex(
            source,
            (henka_vec3){1.0f, 0.0f, 1.0f},
            (henka_vec2){1.0f, 1.0f},
            0U,
            &vertices[2]) == HENKA_SUCCESS);

    HENKA_TEST_ASSERT(
        henka_authoring_mesh_add_vertex(
            source,
            (henka_vec3){-1.0f, 0.0f, 1.0f},
            (henka_vec2){0.0f, 1.0f},
            0U,
            &vertices[3]) == HENKA_SUCCESS);

    first[0] = vertices[0];
    first[1] = vertices[1];
    first[2] = vertices[2];

    second[0] = vertices[0];
    second[1] = vertices[2];
    second[2] = vertices[3];

    HENKA_TEST_ASSERT(
        henka_authoring_mesh_add_face(
            source,
            first,
            3U,
            0U,
            true,
            &face_id) == HENKA_SUCCESS);

    HENKA_TEST_ASSERT(
        henka_authoring_mesh_add_face(
            source,
            second,
            3U,
            0U,
            true,
            &face_id) == HENKA_SUCCESS);

    HENKA_TEST_ASSERT(
        sandbox3d_authoring_object_create_from_mesh(
            engine,
            scene,
            entity,
            source,
            8U,
            &object) == HENKA_SUCCESS);

    HENKA_TEST_ASSERT(
        sandbox3d_authoring_object_recover_quads(
            object,
            0.94f,
            1.10f,
            0.0001f,
            &recovered) == HENKA_SUCCESS);

    HENKA_TEST_ASSERT(recovered == 1U);

    counts = henka_authoring_mesh_get_counts(
        sandbox3d_authoring_object_get_mesh(object));

    HENKA_TEST_ASSERT(counts.vertices == 4U);
    HENKA_TEST_ASSERT(counts.edges == 4U);
    HENKA_TEST_ASSERT(counts.faces == 1U);
    HENKA_TEST_ASSERT(
        sandbox3d_authoring_object_get_selected_component_count(object) == 0U);

    HENKA_TEST_ASSERT(
        sandbox3d_authoring_object_undo(object) == HENKA_SUCCESS);

    counts = henka_authoring_mesh_get_counts(
        sandbox3d_authoring_object_get_mesh(object));

    HENKA_TEST_ASSERT(counts.faces == 2U);
    HENKA_TEST_ASSERT(counts.edges == 5U);

    HENKA_TEST_ASSERT(
        sandbox3d_authoring_object_redo(object) == HENKA_SUCCESS);

    counts = henka_authoring_mesh_get_counts(
        sandbox3d_authoring_object_get_mesh(object));

    HENKA_TEST_ASSERT(counts.faces == 1U);
    HENKA_TEST_ASSERT(counts.edges == 4U);

    sandbox3d_authoring_object_destroy(object);
    henka_authoring_mesh_destroy(source);
    henka_mesh_destroy(previous_mesh);
    henka_scene_destroy(scene);
    henka_engine_destroy(engine);
}

void henka_test_sandbox3d_object_authoring(void)
{
    henka_test_sandbox3d_object_authoring_scene_policy();
    henka_test_sandbox3d_object_authoring_quad_recovery_workflow();
    henka_test_sandbox3d_object_authoring_duplicate();
    henka_test_sandbox3d_object_authoring_source_persistence();
    henka_test_sandbox3d_object_authoring_clone_bridge();
    henka_test_sandbox3d_object_authoring_model_primitive_bridge();
    henka_test_sandbox3d_object_authoring_component_selection();
}
