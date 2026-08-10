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
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_set_selected_face_material_region(
        object, 17U) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_authoring_mesh_get_face(
        sandbox3d_authoring_object_get_mesh(object),
        sandbox3d_authoring_object_get_selected_face(object))->material_region == 17U);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_undo(object) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_authoring_mesh_get_face(
        sandbox3d_authoring_object_get_mesh(object),
        sandbox3d_authoring_object_get_selected_face(object))->material_region != 17U);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_redo(object) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_authoring_mesh_get_face(
        sandbox3d_authoring_object_get_mesh(object),
        sandbox3d_authoring_object_get_selected_face(object))->material_region == 17U);
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
    HENKA_TEST_ASSERT(henka_scene_get_entity_mesh(scene, entity, &render_mesh) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(render_mesh != NULL);
    HENKA_TEST_ASSERT(henka_scene_get_entity_local_bounds(scene, entity, &bounds) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(bounds.extents.x > 0.0f && bounds.extents.y > 0.0f && bounds.extents.z > 0.0f);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_reload_source(object, "build/test_tmp/authoring_object_missing.hams") != HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_authoring_mesh_get_counts(sandbox3d_authoring_object_get_mesh(object)).faces == saved_counts.faces);

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

void henka_test_sandbox3d_object_authoring(void)
{
    henka_test_sandbox3d_object_authoring_scene_policy();
    henka_test_sandbox3d_object_authoring_duplicate();
    henka_test_sandbox3d_object_authoring_source_persistence();
}
