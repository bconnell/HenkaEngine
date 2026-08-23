#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#include "test_suite.h"

#include "../examples/sandbox3d/object_authoring_tools.h"

static size_t henka_test_quad_grid_vertex_index(
    size_t column,
    size_t row,
    size_t vertex_columns)
{
    return row * vertex_columns + column;
}

static henka_result henka_test_make_quad_grid(
    henka_authoring_mesh* mesh,
    size_t face_columns,
    size_t face_rows,
    henka_authoring_vertex_id* out_vertices,
    size_t vertex_capacity)
{
    size_t vertex_columns;
    size_t vertex_rows;
    size_t vertex_count;
    size_t row;
    size_t column;
    henka_result result;

    if (mesh == NULL || out_vertices == NULL || face_columns == 0U || face_rows == 0U ||
        face_columns == SIZE_MAX || face_rows == SIZE_MAX)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    vertex_columns = face_columns + 1U;
    vertex_rows = face_rows + 1U;
    if (vertex_columns > SIZE_MAX / vertex_rows)
    {
        return HENKA_ERROR_LIMIT;
    }
    vertex_count = vertex_columns * vertex_rows;
    if (vertex_count > vertex_capacity)
    {
        return HENKA_ERROR_LIMIT;
    }
    for (row = 0U; row < vertex_rows; ++row)
    {
        for (column = 0U; column < vertex_columns; ++column)
        {
            result = henka_authoring_mesh_add_vertex(
                mesh,
                (henka_vec3){(float)column, 0.0f, (float)row},
                (henka_vec2){(float)column, (float)row},
                0U,
                &out_vertices[henka_test_quad_grid_vertex_index(column, row, vertex_columns)]);
            if (result != HENKA_SUCCESS)
            {
                return result;
            }
        }
    }
    for (row = 0U; row < face_rows; ++row)
    {
        for (column = 0U; column < face_columns; ++column)
        {
            const henka_authoring_vertex_id face_vertices[4] = {
                out_vertices[henka_test_quad_grid_vertex_index(column, row, vertex_columns)],
                out_vertices[henka_test_quad_grid_vertex_index(column + 1U, row, vertex_columns)],
                out_vertices[henka_test_quad_grid_vertex_index(column + 1U, row + 1U, vertex_columns)],
                out_vertices[henka_test_quad_grid_vertex_index(column, row + 1U, vertex_columns)]};
            henka_authoring_face_id face_id;
            result = henka_authoring_mesh_add_face(
                mesh, face_vertices, 4U, 0U, true, &face_id);
            if (result != HENKA_SUCCESS)
            {
                return result;
            }
        }
    }
    return HENKA_SUCCESS;
}

static henka_result henka_test_find_edge_between_vertices(
    const henka_authoring_mesh* mesh,
    henka_authoring_vertex_id first,
    henka_authoring_vertex_id second,
    henka_authoring_edge_id* out_edge)
{
    size_t incident_count;
    size_t incident_index;
    size_t match_count = 0U;
    henka_authoring_edge_id match = HENKA_AUTHORING_INVALID_ID;

    if (mesh == NULL || out_edge == NULL || first == HENKA_AUTHORING_INVALID_ID ||
        second == HENKA_AUTHORING_INVALID_ID)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    incident_count = henka_authoring_mesh_get_vertex_edge_count(mesh, first);
    *out_edge = HENKA_AUTHORING_INVALID_ID;
    for (incident_index = 0U; incident_index < incident_count; ++incident_index)
    {
        henka_authoring_edge_id edge_id;
        const henka_authoring_edge* edge;
        if (henka_authoring_mesh_get_vertex_edge_at(
                mesh, first, incident_index, &edge_id) != HENKA_SUCCESS)
        {
            return HENKA_ERROR_UNKNOWN;
        }
        edge = henka_authoring_mesh_get_edge(mesh, edge_id);
        if (edge != NULL &&
            ((edge->vertices[0] == first && edge->vertices[1] == second) ||
             (edge->vertices[0] == second && edge->vertices[1] == first)))
        {
            match = edge_id;
            ++match_count;
        }
    }
    if (match_count != 1U)
    {
        return HENKA_ERROR_UNKNOWN;
    }
    *out_edge = match;
    return HENKA_SUCCESS;
}

static bool henka_test_selected_edge_contains(
    const sandbox3d_authoring_object* object,
    henka_authoring_edge_id edge_id)
{
    const size_t selected_count = sandbox3d_authoring_object_get_selected_component_count(object);
    size_t selected_index;
    for (selected_index = 0U; selected_index < selected_count; ++selected_index)
    {
        uint32_t selected_id = HENKA_AUTHORING_INVALID_ID;
        if (sandbox3d_authoring_object_get_selected_component_at(
                object, selected_index, &selected_id) == HENKA_SUCCESS &&
            selected_id == edge_id)
        {
            return true;
        }
    }
    return false;
}

static bool henka_test_edge_set_matches(
    const henka_authoring_mesh* mesh,
    const sandbox3d_authoring_object* object,
    const henka_authoring_edge_id* expected_edges,
    size_t expected_count)
{
    henka_authoring_mesh_desc desc;
    size_t edge_id;
    size_t expected_index;
    uint32_t previous_id = 0U;

    if (mesh == NULL || object == NULL || expected_edges == NULL ||
        sandbox3d_authoring_object_get_selected_component_count(object) != expected_count)
    {
        return false;
    }
    desc = henka_authoring_mesh_get_desc(mesh);
    for (expected_index = 0U; expected_index < expected_count; ++expected_index)
    {
        uint32_t selected_id = HENKA_AUTHORING_INVALID_ID;
        if (sandbox3d_authoring_object_get_selected_component_at(
                object, expected_index, &selected_id) != HENKA_SUCCESS ||
            !henka_test_selected_edge_contains(object, expected_edges[expected_index]) ||
            (expected_index > 0U && selected_id <= previous_id))
        {
            return false;
        }
        previous_id = selected_id;
    }
    for (edge_id = 1U; edge_id <= desc.max_edges; ++edge_id)
    {
        if (henka_authoring_mesh_get_edge(mesh, (henka_authoring_edge_id)edge_id) != NULL)
        {
            bool expected = false;
            for (expected_index = 0U; expected_index < expected_count; ++expected_index)
            {
                if (expected_edges[expected_index] == (henka_authoring_edge_id)edge_id)
                {
                    expected = true;
                    break;
                }
            }
            if (henka_test_selected_edge_contains(
                    object, (henka_authoring_edge_id)edge_id) != expected)
            {
                return false;
            }
        }
    }
    return true;
}

static bool henka_test_edges_share_vertex(
    const henka_authoring_mesh* mesh,
    henka_authoring_edge_id first_id,
    henka_authoring_edge_id second_id)
{
    const henka_authoring_edge* first = henka_authoring_mesh_get_edge(mesh, first_id);
    const henka_authoring_edge* second = henka_authoring_mesh_get_edge(mesh, second_id);
    return first != NULL && second != NULL &&
        ((first->vertices[0] == second->vertices[0]) ||
         (first->vertices[0] == second->vertices[1]) ||
         (first->vertices[1] == second->vertices[0]) ||
         (first->vertices[1] == second->vertices[1]));
}

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
    henka_mesh* render_before_extrude = NULL;
    henka_mesh* render_after_extrude = NULL;
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
            henka_authoring_mesh_counts before_multi_extrude;
            henka_authoring_mesh_counts after_multi_extrude;
            uint32_t selected_id = HENKA_AUTHORING_INVALID_ID;
            uint32_t selected_id_two = HENKA_AUTHORING_INVALID_ID;
            sandbox3d_authoring_object_set_selection_mode(
                object, SANDBOX3D_AUTHORING_SELECTION_FACE);
            HENKA_TEST_ASSERT(sandbox3d_authoring_object_select_component(object, 1U, false) == HENKA_SUCCESS);
            HENKA_TEST_ASSERT(sandbox3d_authoring_object_select_component(object, 3U, true) == HENKA_SUCCESS);
            HENKA_TEST_ASSERT(sandbox3d_authoring_object_get_selected_component_count(object) == 2U);
            HENKA_TEST_ASSERT(sandbox3d_authoring_object_set_selected_faces_material_region(
                object, 31U) == HENKA_SUCCESS);
            HENKA_TEST_ASSERT(henka_authoring_mesh_get_face(
                sandbox3d_authoring_object_get_mesh(object), 1U)->material_region == 31U);
            HENKA_TEST_ASSERT(henka_authoring_mesh_get_face(
                sandbox3d_authoring_object_get_mesh(object), 3U)->material_region == 31U);
            HENKA_TEST_ASSERT(sandbox3d_authoring_object_undo(object) == HENKA_SUCCESS);
            HENKA_TEST_ASSERT(sandbox3d_authoring_object_select_component(object, 1U, false) == HENKA_SUCCESS);
            HENKA_TEST_ASSERT(sandbox3d_authoring_object_select_component(object, 3U, true) == HENKA_SUCCESS);
            before_multi_extrude = henka_authoring_mesh_get_counts(
                sandbox3d_authoring_object_get_mesh(object));
            HENKA_TEST_ASSERT(sandbox3d_authoring_object_extrude_selected_faces(
                object, 0.125f) == HENKA_SUCCESS);
            after_multi_extrude = henka_authoring_mesh_get_counts(
                sandbox3d_authoring_object_get_mesh(object));
            HENKA_TEST_ASSERT(after_multi_extrude.faces > before_multi_extrude.faces);
            HENKA_TEST_ASSERT(sandbox3d_authoring_object_get_selected_component_count(object) == 2U);
            HENKA_TEST_ASSERT(sandbox3d_authoring_object_get_selected_component_at(
                object, 0U, &selected_id) == HENKA_SUCCESS);
            HENKA_TEST_ASSERT(sandbox3d_authoring_object_get_selected_component_at(
                object, 1U, &selected_id_two) == HENKA_SUCCESS);
            HENKA_TEST_ASSERT(selected_id < selected_id_two);
            HENKA_TEST_ASSERT(henka_authoring_mesh_get_face(
                sandbox3d_authoring_object_get_mesh(object), selected_id) != NULL);
            HENKA_TEST_ASSERT(henka_authoring_mesh_get_face(
                sandbox3d_authoring_object_get_mesh(object), selected_id_two) != NULL);
            HENKA_TEST_ASSERT(sandbox3d_authoring_object_undo(object) == HENKA_SUCCESS);
            HENKA_TEST_ASSERT(henka_authoring_mesh_get_counts(
                sandbox3d_authoring_object_get_mesh(object)).faces == before_multi_extrude.faces);
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

    HENKA_TEST_ASSERT(henka_scene_get_entity_mesh(
        scene, entity, &render_before_extrude) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_extrude_selected_face(object, 0.25f) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_get_entity_mesh(
        scene, entity, &render_after_extrude) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(render_before_extrude != NULL &&
        render_after_extrude != NULL &&
        render_after_extrude != render_before_extrude);
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

    sandbox3d_authoring_object_set_selection_mode(object, SANDBOX3D_AUTHORING_SELECTION_FACE);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_select_extreme_face_band(
        object, (henka_vec3){0.0f, 1.0f, 0.0f}, true, 0.1f) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_get_selected_component_count(object) == 1U);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_get_selected_face(object) != HENKA_AUTHORING_INVALID_ID);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_select_extreme_face_band(
        object, (henka_vec3){0.0f, 1.0f, 0.0f}, true, 0.0f) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_get_selected_component_count(object) == 1U);

    sandbox3d_authoring_object_set_selection_mode(object, SANDBOX3D_AUTHORING_SELECTION_VERTEX);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_select_all_components(object) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_get_selected_component_count(object) == 8U);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_get_active_component_id(object) == 1U);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_invert_component_selection(object) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_get_selected_component_count(object) == 0U);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_select_none_components(object) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_select_component(object, 1U, false) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_get_active_component_id(object) == 1U);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_select_component(object, 2U, true) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_shrink_component_selection(object) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_get_selected_component_count(object) == 0U);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_select_component(object, 1U, false) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_select_component(object, 2U, true) == HENKA_SUCCESS);
    {
        const henka_authoring_mesh* before_mesh = sandbox3d_authoring_object_get_mesh(object);
        const henka_authoring_vertex* before_first = henka_authoring_mesh_get_vertex(before_mesh, 1U);
        const henka_authoring_vertex* before_second = henka_authoring_mesh_get_vertex(before_mesh, 2U);
        henka_vec3 pivot;
        henka_quat rotation;
        henka_vec3 expected_first;
        henka_vec3 expected_second;
        HENKA_TEST_ASSERT(before_first != NULL && before_second != NULL);
        pivot = henka_vec3_scale(
            henka_vec3_add(before_first->position, before_second->position), 0.5f);
        rotation = henka_quat_from_axis_angle(
            (henka_vec3){0.0f, 1.0f, 0.0f}, 1.57079632679f);
        expected_first = henka_vec3_add(
            pivot,
            henka_quat_rotate_vec3(
                rotation,
                henka_vec3_subtract(before_first->position, pivot)));
        expected_second = henka_vec3_add(
            pivot,
            henka_quat_rotate_vec3(
                rotation,
                henka_vec3_subtract(before_second->position, pivot)));
        HENKA_TEST_ASSERT(sandbox3d_authoring_object_rotate_selected_components(
            object,
            (henka_vec3){0.0f, 1.0f, 0.0f},
            1.57079632679f,
            SANDBOX3D_AUTHORING_PIVOT_MEDIAN,
            SANDBOX3D_AUTHORING_ORIENTATION_LOCAL) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(
            henka_authoring_mesh_get_vertex(
                sandbox3d_authoring_object_get_mesh(object), 1U)->position.x,
            expected_first.x,
            0.0001f);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(
            henka_authoring_mesh_get_vertex(
                sandbox3d_authoring_object_get_mesh(object), 2U)->position.z,
            expected_second.z,
            0.0001f);
        HENKA_TEST_ASSERT(henka_authoring_mesh_validate(
            sandbox3d_authoring_object_get_mesh(object)));
        HENKA_TEST_ASSERT(sandbox3d_authoring_object_undo(object) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(sandbox3d_authoring_object_redo(object) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(sandbox3d_authoring_object_rotate_selected_components(
            object,
            (henka_vec3){0.0f, 0.0f, 0.0f},
            1.0f,
            SANDBOX3D_AUTHORING_PIVOT_MEDIAN,
            SANDBOX3D_AUTHORING_ORIENTATION_LOCAL) != HENKA_SUCCESS);
    }
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_select_component(object, 1U, false) == HENKA_SUCCESS);
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
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_get_selected_component_count(object) >= 1U);
    HENKA_TEST_ASSERT(henka_test_selected_edge_contains(object, 1U));
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
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_select_edge_ring(object) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_get_active_component_id(object) == 1U);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_get_selected_component_count(object) > 1U);
    {
        size_t selected_index;
        for (selected_index = 0U;
             selected_index < sandbox3d_authoring_object_get_selected_component_count(object);
             ++selected_index)
        {
            uint32_t selected_edge_id = HENKA_AUTHORING_INVALID_ID;
            HENKA_TEST_ASSERT(sandbox3d_authoring_object_get_selected_component_at(
                object, selected_index, &selected_edge_id) == HENKA_SUCCESS);
            HENKA_TEST_ASSERT(henka_authoring_mesh_get_edge(
                sandbox3d_authoring_object_get_mesh(object), selected_edge_id) != NULL);
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
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_select_face(object, 1U) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_scale_selected_components_with_pivot(
        object,
        (henka_vec3){1.01f, 1.01f, 1.01f},
        SANDBOX3D_AUTHORING_PIVOT_ACTIVE) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_rotate_selected_components(
        object,
        (henka_vec3){0.0f, 1.0f, 0.0f},
        0.1f,
        SANDBOX3D_AUTHORING_PIVOT_ACTIVE,
        SANDBOX3D_AUTHORING_ORIENTATION_NORMAL) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_rotate_selected_components(
        object,
        (henka_vec3){0.0f, 1.0f, 0.0f},
        0.1f,
        SANDBOX3D_AUTHORING_PIVOT_INDIVIDUAL,
        SANDBOX3D_AUTHORING_ORIENTATION_LOCAL) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_authoring_mesh_validate(
        sandbox3d_authoring_object_get_mesh(object)));
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

static void henka_test_sandbox3d_object_authoring_edge_ring_exact_grid(void)
{
    henka_engine_config config = {0};
    henka_engine* engine = NULL;
    henka_scene* scene = NULL;
    henka_authoring_mesh* source = NULL;
    sandbox3d_authoring_object* object = NULL;
    henka_authoring_mesh_desc desc = {16U, 32U, 16U, 4U};
    henka_authoring_vertex_id vertices[8];
    henka_authoring_edge_id expected_edges[4];
    henka_entity entity;
    size_t column;

    config.application_name = "Henka Edge Ring Exact Grid Test";
    config.window_width = 320;
    config.window_height = 240;
    config.enable_vsync = false;
    HENKA_TEST_ASSERT(henka_engine_create(&config, &engine) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_create(&scene) == HENKA_SUCCESS);
    entity = henka_scene_create_entity_named(scene, "Edge Ring Grid");
    HENKA_TEST_ASSERT(entity != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(henka_authoring_mesh_create(&desc, &source) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_test_make_quad_grid(source, 3U, 1U, vertices, 8U) == HENKA_SUCCESS);
    for (column = 0U; column < 4U; ++column)
    {
        HENKA_TEST_ASSERT(henka_test_find_edge_between_vertices(
            source, vertices[column], vertices[column + 4U], &expected_edges[column]) == HENKA_SUCCESS);
    }
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_create_from_mesh(
        engine, scene, entity, source, 8U, &object) == HENKA_SUCCESS);
    sandbox3d_authoring_object_set_selection_mode(object, SANDBOX3D_AUTHORING_SELECTION_EDGE);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_select_component(
        object, expected_edges[1], false) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_select_edge_ring(object) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_test_edge_set_matches(source, object, expected_edges, 4U));
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_get_active_component_id(object) == expected_edges[1]);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_select_edge_loop(object) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_get_selected_component_count(object) == 1U);
    HENKA_TEST_ASSERT(henka_test_selected_edge_contains(object, expected_edges[1]));

    sandbox3d_authoring_object_destroy(object);
    henka_authoring_mesh_destroy(source);
    henka_scene_destroy(scene);
    henka_engine_destroy(engine);
}

static void henka_test_sandbox3d_object_authoring_edge_loop_exact_grid(void)
{
    henka_engine_config config = {0};
    henka_engine* engine = NULL;
    henka_scene* scene = NULL;
    henka_authoring_mesh* source = NULL;
    sandbox3d_authoring_object* object = NULL;
    henka_authoring_mesh_desc desc = {16U, 32U, 16U, 4U};
    henka_authoring_vertex_id vertices[12];
    henka_authoring_edge_id expected_edges[3];
    henka_entity entity;
    size_t row;
    size_t column;
    henka_authoring_edge_id edge_id;

    config.application_name = "Henka Edge Loop Exact Grid Test";
    config.window_width = 320;
    config.window_height = 240;
    config.enable_vsync = false;
    HENKA_TEST_ASSERT(henka_engine_create(&config, &engine) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_create(&scene) == HENKA_SUCCESS);
    entity = henka_scene_create_entity_named(scene, "Edge Loop Grid");
    HENKA_TEST_ASSERT(entity != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(henka_authoring_mesh_create(&desc, &source) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_test_make_quad_grid(source, 2U, 3U, vertices, 12U) == HENKA_SUCCESS);
    for (row = 0U; row < 3U; ++row)
    {
        HENKA_TEST_ASSERT(henka_test_find_edge_between_vertices(
            source,
            vertices[1U + row * 3U],
            vertices[1U + (row + 1U) * 3U],
            &expected_edges[row]) == HENKA_SUCCESS);
    }
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_create_from_mesh(
        engine, scene, entity, source, 8U, &object) == HENKA_SUCCESS);
    sandbox3d_authoring_object_set_selection_mode(object, SANDBOX3D_AUTHORING_SELECTION_EDGE);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_select_component(
        object, expected_edges[1], false) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_select_edge_loop(object) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_test_edge_set_matches(source, object, expected_edges, 3U));
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_get_active_component_id(object) == expected_edges[1]);
    HENKA_TEST_ASSERT(henka_test_edges_share_vertex(source, expected_edges[0], expected_edges[1]));
    HENKA_TEST_ASSERT(henka_test_edges_share_vertex(source, expected_edges[1], expected_edges[2]));
    for (row = 0U; row < 3U; ++row)
    {
        for (column = 0U; column <= 2U; column += 2U)
        {
            HENKA_TEST_ASSERT(henka_test_find_edge_between_vertices(
                source,
                vertices[column + row * 3U],
                vertices[column + (row + 1U) * 3U],
                &edge_id) == HENKA_SUCCESS);
            HENKA_TEST_ASSERT(!henka_test_selected_edge_contains(object, edge_id));
        }
    }
    for (row = 0U; row < 4U; ++row)
    {
        for (column = 0U; column < 2U; ++column)
        {
            HENKA_TEST_ASSERT(henka_test_find_edge_between_vertices(
                source,
                vertices[column + row * 3U],
                vertices[column + 1U + row * 3U],
                &edge_id) == HENKA_SUCCESS);
            HENKA_TEST_ASSERT(!henka_test_selected_edge_contains(object, edge_id));
        }
    }

    sandbox3d_authoring_object_destroy(object);
    henka_authoring_mesh_destroy(source);
    henka_scene_destroy(scene);
    henka_engine_destroy(engine);
}

static void henka_test_sandbox3d_object_authoring_scalable_selection(void)
{
    henka_engine_config config = {0};
    henka_engine* engine = NULL;
    henka_scene* scene = NULL;
    henka_authoring_mesh* source = NULL;
    sandbox3d_authoring_object* object = NULL;
    henka_authoring_mesh_desc desc = {256U, 256U, 128U, 8U};
    henka_authoring_vertex_id vertices[3];
    henka_authoring_face_id face_id;
    henka_entity entity;
    size_t face_index;
    size_t ordinal;

    config.application_name = "Henka Scalable Selection Test";
    config.window_width = 320;
    config.window_height = 240;
    config.enable_vsync = false;
    HENKA_TEST_ASSERT(henka_engine_create(&config, &engine) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_create(&scene) == HENKA_SUCCESS);
    entity = henka_scene_create_entity_named(scene, "Scalable Selection");
    HENKA_TEST_ASSERT(entity != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(henka_authoring_mesh_create(&desc, &source) == HENKA_SUCCESS);

    for (face_index = 0U; face_index < 70U; ++face_index)
    {
        const float x = (float)face_index * 2.0f;
        HENKA_TEST_ASSERT(henka_authoring_mesh_add_vertex(
            source, (henka_vec3){x, 0.0f, 0.0f}, (henka_vec2){0.0f, 0.0f}, 0U, &vertices[0]) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(henka_authoring_mesh_add_vertex(
            source, (henka_vec3){x + 0.5f, 1.0f, 0.0f}, (henka_vec2){0.5f, 1.0f}, 0U, &vertices[1]) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(henka_authoring_mesh_add_vertex(
            source, (henka_vec3){x + 1.0f, 0.0f, 0.0f}, (henka_vec2){1.0f, 0.0f}, 0U, &vertices[2]) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(henka_authoring_mesh_add_face(
            source, vertices, 3U, 0U, true, &face_id) == HENKA_SUCCESS);
    }
    HENKA_TEST_ASSERT(henka_authoring_mesh_validate(source));
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_create_from_mesh(
        engine, scene, entity, source, 8U, &object) == HENKA_SUCCESS);
    sandbox3d_authoring_object_set_selection_mode(object, SANDBOX3D_AUTHORING_SELECTION_FACE);

    for (face_index = 70U; face_index > 0U; --face_index)
    {
        HENKA_TEST_ASSERT(sandbox3d_authoring_object_select_component(
            object, (uint32_t)face_index, true) == HENKA_SUCCESS);
    }
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_get_selected_component_count(object) == 70U);
    for (ordinal = 0U; ordinal < 70U; ++ordinal)
    {
        uint32_t selected_id = HENKA_AUTHORING_INVALID_ID;
        HENKA_TEST_ASSERT(sandbox3d_authoring_object_get_selected_component_at(
            object, ordinal, &selected_id) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(selected_id == ordinal + 1U);
    }
    sandbox3d_authoring_object_set_selection_mode(object, SANDBOX3D_AUTHORING_SELECTION_VERTEX);
    for (face_index = 210U; face_index > 0U; --face_index)
    {
        HENKA_TEST_ASSERT(sandbox3d_authoring_object_select_component(
            object, (uint32_t)face_index, true) == HENKA_SUCCESS);
    }
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_get_selected_component_count(object) == 210U);
    for (ordinal = 0U; ordinal < 210U; ++ordinal)
    {
        uint32_t selected_id = HENKA_AUTHORING_INVALID_ID;
        HENKA_TEST_ASSERT(sandbox3d_authoring_object_get_selected_component_at(
            object, ordinal, &selected_id) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(selected_id == ordinal + 1U);
    }
    sandbox3d_authoring_object_clear_component_selection(object);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_get_selected_component_count(object) == 0U);
    sandbox3d_authoring_object_set_selection_mode(object, SANDBOX3D_AUTHORING_SELECTION_EDGE);
    for (face_index = 210U; face_index > 0U; --face_index)
    {
        HENKA_TEST_ASSERT(sandbox3d_authoring_object_select_component(
            object, (uint32_t)face_index, true) == HENKA_SUCCESS);
    }
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_get_selected_component_count(object) == 210U);
    for (ordinal = 0U; ordinal < 210U; ++ordinal)
    {
        uint32_t selected_id = HENKA_AUTHORING_INVALID_ID;
        HENKA_TEST_ASSERT(sandbox3d_authoring_object_get_selected_component_at(
            object, ordinal, &selected_id) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(selected_id == ordinal + 1U);
    }
    sandbox3d_authoring_object_clear_component_selection(object);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_get_selected_component_count(object) == 0U);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_select_component(object, 1U, false) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_select_edge_ring(object) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_get_selected_component_count(object) == 1U);
    {
        uint32_t selected_id = HENKA_AUTHORING_INVALID_ID;
        HENKA_TEST_ASSERT(sandbox3d_authoring_object_get_selected_component_at(
            object, 0U, &selected_id) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(selected_id == 1U);
    }

    sandbox3d_authoring_object_destroy(object);
    henka_authoring_mesh_destroy(source);
    henka_scene_destroy(scene);
    henka_engine_destroy(engine);
}

static void henka_test_sandbox3d_object_authoring_connected_scalable_selection(void)
{
    henka_engine_config config = {0};
    henka_engine* engine = NULL;
    henka_scene* scene = NULL;
    henka_authoring_mesh* source = NULL;
    sandbox3d_authoring_object* object = NULL;
    henka_authoring_mesh_desc desc = {128U, 256U, 128U, 8U};
    henka_authoring_vertex_id vertices[100];
    henka_authoring_mesh_counts counts;
    henka_entity entity;
    const sandbox3d_authoring_selection_mode modes[] = {
        SANDBOX3D_AUTHORING_SELECTION_VERTEX,
        SANDBOX3D_AUTHORING_SELECTION_EDGE,
        SANDBOX3D_AUTHORING_SELECTION_FACE};
    const size_t expected_counts[] = {100U, 180U, 81U};
    size_t mode_index;

    config.application_name = "Henka Connected Scalable Selection Test";
    config.window_width = 320;
    config.window_height = 240;
    config.enable_vsync = false;
    HENKA_TEST_ASSERT(henka_engine_create(&config, &engine) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_create(&scene) == HENKA_SUCCESS);
    entity = henka_scene_create_entity_named(scene, "Connected Scalable Selection");
    HENKA_TEST_ASSERT(entity != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(henka_authoring_mesh_create(&desc, &source) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_test_make_quad_grid(source, 9U, 9U, vertices, 100U) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_authoring_mesh_validate(source));
    counts = henka_authoring_mesh_get_counts(source);
    HENKA_TEST_ASSERT(counts.vertices == 100U);
    HENKA_TEST_ASSERT(counts.edges == 180U);
    HENKA_TEST_ASSERT(counts.faces == 81U);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_create_from_mesh(
        engine, scene, entity, source, 8U, &object) == HENKA_SUCCESS);

    for (mode_index = 0U; mode_index < sizeof(modes) / sizeof(modes[0]); ++mode_index)
    {
        size_t selected_index;
        uint32_t previous_id = 0U;

        sandbox3d_authoring_object_set_selection_mode(object, modes[mode_index]);
        HENKA_TEST_ASSERT(sandbox3d_authoring_object_select_component(
            object, 1U, false) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(sandbox3d_authoring_object_select_connected_components(
            object) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(sandbox3d_authoring_object_get_selected_component_count(object) ==
            expected_counts[mode_index]);
        for (selected_index = 0U;
             selected_index < expected_counts[mode_index];
             ++selected_index)
        {
            uint32_t selected_id = HENKA_AUTHORING_INVALID_ID;
            HENKA_TEST_ASSERT(sandbox3d_authoring_object_get_selected_component_at(
                object, selected_index, &selected_id) == HENKA_SUCCESS);
            HENKA_TEST_ASSERT(selected_id > previous_id);
            previous_id = selected_id;
        }

        HENKA_TEST_ASSERT(sandbox3d_authoring_object_select_none_components(object) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(sandbox3d_authoring_object_select_all_components(object) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(sandbox3d_authoring_object_get_selected_component_count(object) ==
            expected_counts[mode_index]);
        previous_id = 0U;
        for (selected_index = 0U;
             selected_index < expected_counts[mode_index];
             ++selected_index)
        {
            uint32_t selected_id = HENKA_AUTHORING_INVALID_ID;
            HENKA_TEST_ASSERT(sandbox3d_authoring_object_get_selected_component_at(
                object, selected_index, &selected_id) == HENKA_SUCCESS);
            HENKA_TEST_ASSERT(selected_id > previous_id);
            previous_id = selected_id;
        }
    }

    sandbox3d_authoring_object_destroy(object);
    henka_authoring_mesh_destroy(source);
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
    henka_test_sandbox3d_object_authoring_edge_ring_exact_grid();
    henka_test_sandbox3d_object_authoring_edge_loop_exact_grid();
    henka_test_sandbox3d_object_authoring_scalable_selection();
    henka_test_sandbox3d_object_authoring_connected_scalable_selection();
}
