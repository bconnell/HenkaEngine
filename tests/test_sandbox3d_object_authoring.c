#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#include "test_suite.h"

#include <henka/model.h>

#include "../examples/sandbox3d/modeling_operator.h"
#include "../examples/sandbox3d/modeling_selection_commands.h"
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

static henka_result henka_test_make_closed_quad_ring(
    henka_authoring_mesh* mesh,
    henka_authoring_vertex_id* out_vertices,
    size_t vertex_capacity)
{
    const henka_vec3 positions[8] = {
        {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f},
        {1.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f},
        {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 0.0f},
        {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 1.0f}};
    const henka_authoring_vertex_id faces[4][4] = {
        {1U, 5U, 6U, 2U}, {2U, 6U, 7U, 3U},
        {3U, 7U, 8U, 4U}, {4U, 8U, 5U, 1U}};
    henka_result result;
    size_t index;

    if (mesh == NULL || out_vertices == NULL || vertex_capacity < 8U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    for (index = 0U; index < 8U; ++index)
    {
        const henka_vec2 uv = index < 4U
            ? (henka_vec2){0.0f, 0.0f}
            : (henka_vec2){0.0f, 1.0f};
        result = henka_authoring_mesh_add_vertex(
            mesh, positions[index], uv, 0U, &out_vertices[index]);
        if (result != HENKA_SUCCESS)
        {
            return result;
        }
    }
    for (index = 0U; index < 4U; ++index)
    {
        result = henka_authoring_mesh_add_face(
            mesh, faces[index], 4U, 0U, true,
            &(henka_authoring_face_id){0U});
        if (result != HENKA_SUCCESS)
        {
            return result;
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
    size_t physical_slot;
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
    for (physical_slot = 0U; physical_slot < desc.max_edges; ++physical_slot)
    {
        henka_authoring_edge_id edge_id = HENKA_AUTHORING_INVALID_ID;
        if (henka_authoring_mesh_get_edge_id_at(mesh, physical_slot, &edge_id) == HENKA_SUCCESS)
        {
            bool expected = false;
            for (expected_index = 0U; expected_index < expected_count; ++expected_index)
            {
                if (expected_edges[expected_index] == edge_id)
                {
                    expected = true;
                    break;
                }
            }
            if (henka_test_selected_edge_contains(
                    object, edge_id) != expected)
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

static void henka_test_sandbox3d_modeling_operator_session(void)
{
    henka_engine_config config = {0};
    henka_engine* engine = NULL;
    henka_scene* scene = NULL;
    henka_entity entity;
    henka_mesh* previous_mesh = NULL;
    henka_mesh* original_render_mesh = NULL;
    henka_mesh* preview_render_mesh = NULL;
    sandbox3d_authoring_object* object = NULL;
    sandbox3d_modeling_operator_session session = {0};
    const henka_authoring_mesh* mesh;
    const henka_authoring_vertex* vertex;
    henka_authoring_mesh_counts bevel_counts;
    henka_vec3 original_position;
    uint64_t geometry_revision_before_preview;
    uint64_t geometry_revision_after_commit;

    config.application_name = "Henka Modeling Operator Test";
    config.window_width = 320;
    config.window_height = 240;
    config.enable_vsync = false;
    HENKA_TEST_ASSERT(henka_engine_create(&config, &engine) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_create(&scene) == HENKA_SUCCESS);
    entity = henka_scene_create_entity_named(scene, "Modeling Operator Source");
    HENKA_TEST_ASSERT(entity != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(henka_mesh_create_cube(engine, &previous_mesh) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_set_entity_mesh(scene, entity, previous_mesh) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_create_box(
        engine, scene, entity, 1.0f, 1.0f, 1.0f, NULL, 8U, &object) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_get_geometry_revision(object) == 1U);
    geometry_revision_before_preview =
        sandbox3d_authoring_object_get_geometry_revision(object);
    HENKA_TEST_ASSERT(henka_scene_get_entity_mesh(scene, entity, &original_render_mesh) == HENKA_SUCCESS);

    sandbox3d_authoring_object_set_selection_mode(object, SANDBOX3D_AUTHORING_SELECTION_VERTEX);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_select_component(object, 1U, false) == HENKA_SUCCESS);
    mesh = sandbox3d_authoring_object_get_mesh(object);
    vertex = henka_authoring_mesh_get_vertex(mesh, 1U);
    HENKA_TEST_ASSERT(vertex != NULL);
    original_position = vertex->position;

    HENKA_TEST_ASSERT(sandbox3d_modeling_operator_begin(
        &session, object, SANDBOX3D_MODELING_OPERATOR_MOVE) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(session.state == SANDBOX3D_MODELING_OPERATOR_STATE_BEGIN);
    HENKA_TEST_ASSERT(sandbox3d_modeling_operator_set_axis(
        &session, SANDBOX3D_MODELING_OPERATOR_AXIS_X) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_modeling_operator_preview(
        &session, 0.1f, false, false) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(session.state == SANDBOX3D_MODELING_OPERATOR_STATE_PREVIEW);
    HENKA_TEST_ASSERT(session.preview_rebuild_count == 1U);
    HENKA_TEST_ASSERT(
        sandbox3d_authoring_object_get_geometry_revision(object) ==
        geometry_revision_before_preview);
    HENKA_TEST_ASSERT(henka_scene_get_entity_mesh(scene, entity, &preview_render_mesh) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(preview_render_mesh != original_render_mesh);
    vertex = henka_authoring_mesh_get_vertex(
        sandbox3d_authoring_object_get_mesh(object), 1U);
    HENKA_TEST_ASSERT(vertex != NULL);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(vertex->position.x, original_position.x, 0.0001f);

    HENKA_TEST_ASSERT(sandbox3d_modeling_operator_preview(
        &session, 0.1f, false, false) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(session.preview_rebuild_count == 2U);
    vertex = henka_authoring_mesh_get_vertex(
        sandbox3d_authoring_object_get_mesh(object), 1U);
    HENKA_TEST_ASSERT(vertex != NULL);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(vertex->position.x, original_position.x, 0.0001f);
    HENKA_TEST_ASSERT(sandbox3d_modeling_operator_cancel(&session) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(session.state == SANDBOX3D_MODELING_OPERATOR_STATE_IDLE);
    HENKA_TEST_ASSERT(henka_scene_get_entity_mesh(scene, entity, &preview_render_mesh) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(preview_render_mesh == original_render_mesh);

    HENKA_TEST_ASSERT(sandbox3d_modeling_operator_begin(
        &session, object, SANDBOX3D_MODELING_OPERATOR_MOVE) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_modeling_operator_set_axis(
        &session, SANDBOX3D_MODELING_OPERATOR_AXIS_X) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_modeling_operator_numeric_begin(&session) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_modeling_operator_numeric_append(
        &session, "x", 1U) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(strcmp(
        sandbox3d_modeling_operator_get_numeric_text(&session), "") == 0);
    HENKA_TEST_ASSERT(sandbox3d_modeling_operator_numeric_append(
        &session, "0.1", 3U) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(strcmp(
        sandbox3d_modeling_operator_get_numeric_text(&session), "0.1") == 0);
    HENKA_TEST_ASSERT(sandbox3d_modeling_operator_numeric_commit(&session) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(!session.numeric_active);
    HENKA_TEST_ASSERT(session.state == SANDBOX3D_MODELING_OPERATOR_STATE_PREVIEW);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(session.amount, 0.1f, 0.0001f);
    HENKA_TEST_ASSERT(sandbox3d_modeling_operator_commit(&session) == HENKA_SUCCESS);
    geometry_revision_after_commit =
        sandbox3d_authoring_object_get_geometry_revision(object);
    HENKA_TEST_ASSERT(geometry_revision_after_commit > geometry_revision_before_preview);
    vertex = henka_authoring_mesh_get_vertex(
        sandbox3d_authoring_object_get_mesh(object), 1U);
    HENKA_TEST_ASSERT(vertex != NULL);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(vertex->position.x, original_position.x + 0.1f, 0.0001f);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_undo(object) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(
        sandbox3d_authoring_object_get_geometry_revision(object) >
        geometry_revision_after_commit);
    vertex = henka_authoring_mesh_get_vertex(
        sandbox3d_authoring_object_get_mesh(object), 1U);
    HENKA_TEST_ASSERT(vertex != NULL);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(vertex->position.x, original_position.x, 0.0001f);

    HENKA_TEST_ASSERT(sandbox3d_modeling_operator_begin(
        &session, object, SANDBOX3D_MODELING_OPERATOR_MOVE) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_modeling_operator_set_axis(
        &session, SANDBOX3D_MODELING_OPERATOR_AXIS_X) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_modeling_operator_preview(
        &session, 0.1f, false, false) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_modeling_operator_commit(&session) == HENKA_SUCCESS);
    vertex = henka_authoring_mesh_get_vertex(
        sandbox3d_authoring_object_get_mesh(object), 1U);
    HENKA_TEST_ASSERT(vertex != NULL);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(vertex->position.x, original_position.x + 0.1f, 0.0001f);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_undo(object) == HENKA_SUCCESS);
    vertex = henka_authoring_mesh_get_vertex(
        sandbox3d_authoring_object_get_mesh(object), 1U);
    HENKA_TEST_ASSERT(vertex != NULL);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(vertex->position.x, original_position.x, 0.0001f);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_undo(object) != HENKA_SUCCESS);

    sandbox3d_authoring_object_set_selection_mode(
        object, SANDBOX3D_AUTHORING_SELECTION_FACE);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_select_component(
        object, 1U, false) == HENKA_SUCCESS);
    bevel_counts = henka_authoring_mesh_get_counts(
        sandbox3d_authoring_object_get_mesh(object));
    HENKA_TEST_ASSERT(sandbox3d_modeling_operator_begin(
        &session, object, SANDBOX3D_MODELING_OPERATOR_BEVEL) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_modeling_operator_preview(
        &session, 0.1f, false, false) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_authoring_mesh_get_counts(
        sandbox3d_authoring_object_get_mesh(object)).faces == bevel_counts.faces);
    HENKA_TEST_ASSERT(sandbox3d_modeling_operator_cancel(&session) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_modeling_operator_begin(
        &session, object, SANDBOX3D_MODELING_OPERATOR_BEVEL) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_modeling_operator_numeric_begin(&session) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_modeling_operator_numeric_append(
        &session, "0.1", 3U) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_modeling_operator_numeric_commit(&session) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_modeling_operator_commit(&session) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_authoring_mesh_get_counts(
        sandbox3d_authoring_object_get_mesh(object)).faces > bevel_counts.faces);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_undo(object) == HENKA_SUCCESS);

    sandbox3d_authoring_object_set_selection_mode(
        object, SANDBOX3D_AUTHORING_SELECTION_VERTEX);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_select_component(
        object, 1U, false) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_modeling_operator_begin(
        &session, object, SANDBOX3D_MODELING_OPERATOR_BEVEL) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_modeling_operator_preview(
        &session, 0.1f, false, false) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_modeling_operator_cancel(&session) == HENKA_SUCCESS);

    sandbox3d_authoring_object_destroy(object);
    henka_scene_destroy(scene);
    henka_engine_destroy(engine);
}

static void henka_test_sandbox3d_modeling_operator_loose_extrude(void)
{
    henka_engine_config config = {0};
    henka_engine* engine = NULL;
    henka_scene* scene = NULL;
    henka_authoring_mesh* source = NULL;
    sandbox3d_authoring_object* object = NULL;
    sandbox3d_modeling_operator_session session = {0};
    henka_authoring_mesh_counts before;
    henka_authoring_mesh_counts after;
    henka_authoring_vertex_id loose_vertex_id = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_vertex_id loose_edge_first = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_vertex_id loose_edge_second = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_edge_id loose_edge_id = HENKA_AUTHORING_INVALID_ID;
    henka_entity entity;

    config.application_name = "Henka Loose Extrude Operator Test";
    config.window_width = 320;
    config.window_height = 240;
    config.enable_vsync = false;
    HENKA_TEST_ASSERT(henka_engine_create(&config, &engine) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_create(&scene) == HENKA_SUCCESS);
    entity = henka_scene_create_entity_named(scene, "Loose Vertex Extrude Source");
    HENKA_TEST_ASSERT(entity != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(henka_authoring_mesh_create_box(
        &(henka_authoring_mesh_desc){16U, 24U, 12U, 8U}, 1.0f, 1.0f, 1.0f,
        &source) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_authoring_mesh_add_vertex(
        source, (henka_vec3){0.0f, 0.0f, 0.0f}, (henka_vec2){0.0f, 0.0f}, 0U,
        &loose_vertex_id) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_authoring_mesh_add_vertex(
        source, (henka_vec3){2.0f, 0.0f, 0.0f}, (henka_vec2){0.0f, 0.0f}, 0U,
        &loose_edge_first) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_authoring_mesh_add_vertex(
        source, (henka_vec3){3.0f, 0.0f, 0.0f}, (henka_vec2){1.0f, 0.0f}, 0U,
        &loose_edge_second) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_authoring_mesh_add_edge(
        source, loose_edge_first, loose_edge_second, false, &loose_edge_id) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_create_from_mesh(
        engine, scene, entity, source, 8U, &object) == HENKA_SUCCESS);
    sandbox3d_authoring_object_set_selection_mode(
        object, SANDBOX3D_AUTHORING_SELECTION_VERTEX);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_select_component(
        object, loose_vertex_id, false) == HENKA_SUCCESS);
    before = henka_authoring_mesh_get_counts(
        sandbox3d_authoring_object_get_mesh(object));

    HENKA_TEST_ASSERT(sandbox3d_modeling_operator_begin(
        &session, object, SANDBOX3D_MODELING_OPERATOR_EXTRUDE) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_modeling_operator_set_axis(
        &session, SANDBOX3D_MODELING_OPERATOR_AXIS_Y) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_modeling_operator_preview(
        &session, 0.5f, false, false) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_has_preview(object));
    HENKA_TEST_ASSERT(sandbox3d_modeling_operator_cancel(&session) == HENKA_SUCCESS);
    after = henka_authoring_mesh_get_counts(
        sandbox3d_authoring_object_get_mesh(object));
    HENKA_TEST_ASSERT(after.vertices == before.vertices && after.edges == before.edges);

    HENKA_TEST_ASSERT(sandbox3d_modeling_operator_begin(
        &session, object, SANDBOX3D_MODELING_OPERATOR_EXTRUDE) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_modeling_operator_set_axis(
        &session, SANDBOX3D_MODELING_OPERATOR_AXIS_Y) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_modeling_operator_numeric_begin(&session) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_modeling_operator_numeric_append(
        &session, "0.5", 3U) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_modeling_operator_numeric_commit(&session) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_modeling_operator_commit(&session) == HENKA_SUCCESS);
    after = henka_authoring_mesh_get_counts(
        sandbox3d_authoring_object_get_mesh(object));
    HENKA_TEST_ASSERT(after.vertices == before.vertices + 1U &&
        after.edges == before.edges + 1U && after.faces == before.faces &&
        henka_authoring_mesh_validate(
            sandbox3d_authoring_object_get_mesh(object)));

    sandbox3d_authoring_object_set_selection_mode(
        object, SANDBOX3D_AUTHORING_SELECTION_EDGE);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_select_component(
        object, loose_edge_id, false) == HENKA_SUCCESS);
    before = henka_authoring_mesh_get_counts(
        sandbox3d_authoring_object_get_mesh(object));

    HENKA_TEST_ASSERT(sandbox3d_modeling_operator_begin(
        &session, object, SANDBOX3D_MODELING_OPERATOR_EXTRUDE) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_modeling_operator_set_axis(
        &session, SANDBOX3D_MODELING_OPERATOR_AXIS_Z) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_modeling_operator_preview(
        &session, 0.5f, false, false) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_has_preview(object));
    HENKA_TEST_ASSERT(sandbox3d_modeling_operator_cancel(&session) == HENKA_SUCCESS);
    after = henka_authoring_mesh_get_counts(
        sandbox3d_authoring_object_get_mesh(object));
    HENKA_TEST_ASSERT(after.vertices == before.vertices &&
        after.edges == before.edges && after.faces == before.faces);

    HENKA_TEST_ASSERT(sandbox3d_modeling_operator_begin(
        &session, object, SANDBOX3D_MODELING_OPERATOR_EXTRUDE) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_modeling_operator_set_axis(
        &session, SANDBOX3D_MODELING_OPERATOR_AXIS_Z) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_modeling_operator_numeric_begin(&session) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_modeling_operator_numeric_append(
        &session, "0.5", 3U) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_modeling_operator_numeric_commit(&session) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_modeling_operator_commit(&session) == HENKA_SUCCESS);
    after = henka_authoring_mesh_get_counts(
        sandbox3d_authoring_object_get_mesh(object));
    HENKA_TEST_ASSERT(after.vertices == before.vertices + 2U &&
        after.edges == before.edges + 3U && after.faces == before.faces + 1U &&
        henka_authoring_mesh_validate(
            sandbox3d_authoring_object_get_mesh(object)));

    sandbox3d_authoring_object_destroy(object);
    henka_authoring_mesh_destroy(source);
    henka_scene_destroy(scene);
    henka_engine_destroy(engine);
}

static void henka_test_sandbox3d_modeling_operator_surface_vertex_extrude(void)
{
    henka_engine_config config = {0};
    henka_engine* engine = NULL;
    henka_scene* scene = NULL;
    henka_authoring_mesh* source = NULL;
    sandbox3d_authoring_object* object = NULL;
    sandbox3d_modeling_operator_session session = {0};
    const henka_authoring_mesh_desc desc = {16U, 32U, 16U, 8U};
    henka_authoring_mesh_counts before;
    henka_authoring_mesh_counts after;
    henka_entity entity;

    config.application_name = "Henka Surface Vertex Operator Extrude Test";
    config.window_width = 320;
    config.window_height = 240;
    config.enable_vsync = false;
    HENKA_TEST_ASSERT(henka_engine_create(&config, &engine) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_create(&scene) == HENKA_SUCCESS);
    entity = henka_scene_create_entity_named(scene, "Surface Vertex Extrude");
    HENKA_TEST_ASSERT(entity != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(henka_authoring_mesh_create_plane(
        &desc, 2.0f, 2.0f, &source) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_create_from_mesh(
        engine, scene, entity, source, 8U, &object) == HENKA_SUCCESS);
    sandbox3d_authoring_object_set_selection_mode(
        object, SANDBOX3D_AUTHORING_SELECTION_VERTEX);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_select_component(
        object, 1U, false) == HENKA_SUCCESS);
    before = henka_authoring_mesh_get_counts(
        sandbox3d_authoring_object_get_mesh(object));

    HENKA_TEST_ASSERT(sandbox3d_modeling_operator_begin(
        &session, object, SANDBOX3D_MODELING_OPERATOR_EXTRUDE) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_modeling_operator_set_axis(
        &session, SANDBOX3D_MODELING_OPERATOR_AXIS_Y) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_modeling_operator_preview(
        &session, 0.5f, false, false) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_has_preview(object));
    after = henka_authoring_mesh_get_counts(
        sandbox3d_authoring_object_get_mesh(object));
    HENKA_TEST_ASSERT(after.vertices == before.vertices &&
        after.edges == before.edges && after.faces == before.faces);
    HENKA_TEST_ASSERT(sandbox3d_modeling_operator_cancel(&session) == HENKA_SUCCESS);

    HENKA_TEST_ASSERT(sandbox3d_modeling_operator_begin(
        &session, object, SANDBOX3D_MODELING_OPERATOR_EXTRUDE) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_modeling_operator_set_axis(
        &session, SANDBOX3D_MODELING_OPERATOR_AXIS_Y) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_modeling_operator_numeric_begin(
        &session) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_modeling_operator_numeric_append(
        &session, "0.5", 3U) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_modeling_operator_numeric_commit(
        &session) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_modeling_operator_commit(&session) == HENKA_SUCCESS);
    after = henka_authoring_mesh_get_counts(
        sandbox3d_authoring_object_get_mesh(object));
    HENKA_TEST_ASSERT(after.vertices == before.vertices + 1U &&
        after.edges == before.edges + 3U && after.faces == before.faces + 2U);
    HENKA_TEST_ASSERT(henka_authoring_mesh_validate(
        sandbox3d_authoring_object_get_mesh(object)));
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_undo(object) == HENKA_SUCCESS);
    after = henka_authoring_mesh_get_counts(
        sandbox3d_authoring_object_get_mesh(object));
    HENKA_TEST_ASSERT(after.vertices == before.vertices &&
        after.edges == before.edges && after.faces == before.faces);

    sandbox3d_authoring_object_destroy(object);
    henka_authoring_mesh_destroy(source);
    henka_scene_destroy(scene);
    henka_engine_destroy(engine);
}

static void henka_test_sandbox3d_modeling_operator_boundary_edge_extrude(void)
{
    henka_engine_config config = {0};
    henka_engine* engine = NULL;
    henka_scene* scene = NULL;
    henka_authoring_mesh* source = NULL;
    sandbox3d_authoring_object* object = NULL;
    sandbox3d_modeling_operator_session session = {0};
    const henka_authoring_face* source_face;
    henka_authoring_face_id source_face_id = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_edge_id source_edge_id = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_mesh_counts before;
    henka_authoring_mesh_counts after;
    henka_entity entity;

    config.application_name = "Henka Boundary Edge Extrude Operator Test";
    config.window_width = 320;
    config.window_height = 240;
    config.enable_vsync = false;
    HENKA_TEST_ASSERT(henka_engine_create(&config, &engine) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_create(&scene) == HENKA_SUCCESS);
    entity = henka_scene_create_entity_named(scene, "Boundary Edge Extrude Source");
    HENKA_TEST_ASSERT(entity != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(henka_authoring_mesh_create_plane(
        &(henka_authoring_mesh_desc){16U, 32U, 16U, 8U}, 2.0f, 2.0f, &source) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_authoring_mesh_get_face_id_at(
        source, 0U, &source_face_id) == HENKA_SUCCESS);
    source_face = henka_authoring_mesh_get_face(source, source_face_id);
    HENKA_TEST_ASSERT(source_face != NULL && source_face->corner_count == 4U);
    source_edge_id = source_face->edges[0U];
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_create_from_mesh(
        engine, scene, entity, source, 8U, &object) == HENKA_SUCCESS);
    sandbox3d_authoring_object_set_selection_mode(
        object, SANDBOX3D_AUTHORING_SELECTION_EDGE);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_select_component(
        object, source_edge_id, false) == HENKA_SUCCESS);
    before = henka_authoring_mesh_get_counts(
        sandbox3d_authoring_object_get_mesh(object));

    HENKA_TEST_ASSERT(sandbox3d_modeling_operator_begin(
        &session, object, SANDBOX3D_MODELING_OPERATOR_EDGE_EXTRUDE) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_modeling_operator_preview(
        &session, 0.5f, false, false) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_has_preview(object));
    HENKA_TEST_ASSERT(sandbox3d_modeling_operator_cancel(&session) == HENKA_SUCCESS);
    after = henka_authoring_mesh_get_counts(
        sandbox3d_authoring_object_get_mesh(object));
    HENKA_TEST_ASSERT(after.vertices == before.vertices &&
        after.edges == before.edges && after.faces == before.faces);

    HENKA_TEST_ASSERT(sandbox3d_modeling_operator_begin(
        &session, object, SANDBOX3D_MODELING_OPERATOR_EDGE_EXTRUDE) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_modeling_operator_numeric_begin(&session) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_modeling_operator_numeric_append(
        &session, "0.5", 3U) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_modeling_operator_numeric_commit(&session) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_modeling_operator_commit(&session) == HENKA_SUCCESS);
    after = henka_authoring_mesh_get_counts(
        sandbox3d_authoring_object_get_mesh(object));
    HENKA_TEST_ASSERT(after.vertices == before.vertices + 2U &&
        after.edges == before.edges + 3U && after.faces == before.faces + 1U &&
        henka_authoring_mesh_validate(
            sandbox3d_authoring_object_get_mesh(object)));

    sandbox3d_authoring_object_destroy(object);
    henka_authoring_mesh_destroy(source);
    henka_scene_destroy(scene);
    henka_engine_destroy(engine);
}

static void henka_test_sandbox3d_loose_renderer_bridge(void)
{
    henka_engine_config config = {0};
    henka_engine* engine = NULL;
    henka_scene* scene = NULL;
    henka_authoring_mesh* source = NULL;
    sandbox3d_authoring_object* object = NULL;
    henka_authoring_vertex_id first = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_vertex_id second = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_edge_id edge = HENKA_AUTHORING_INVALID_ID;
    henka_entity edge_entity;
    henka_entity point_entity;
    henka_mesh* render_mesh = NULL;
    henka_bounds bounds;

    config.application_name = "Henka Sandbox Loose Renderer Bridge Test";
    config.window_width = 320;
    config.window_height = 240;
    config.enable_vsync = false;
    HENKA_TEST_ASSERT(henka_engine_create(&config, &engine) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_create(&scene) == HENKA_SUCCESS);

    edge_entity = henka_scene_create_entity_named(scene, "Wire Source");
    HENKA_TEST_ASSERT(edge_entity != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(henka_authoring_mesh_create(
        &(henka_authoring_mesh_desc){8U, 8U, 4U, 4U}, &source) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_authoring_mesh_add_vertex(
        source, (henka_vec3){-1.0f, 0.0f, 0.0f}, (henka_vec2){0.0f, 0.0f}, 0U,
        &first) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_authoring_mesh_add_vertex(
        source, (henka_vec3){2.0f, 0.0f, 0.0f}, (henka_vec2){1.0f, 0.0f}, 0U,
        &second) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_authoring_mesh_add_edge(
        source, first, second, false, &edge) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_create_from_mesh(
        engine, scene, edge_entity, source, 8U, &object) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_get_entity_mesh(scene, edge_entity, &render_mesh) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(render_mesh != NULL);
    HENKA_TEST_ASSERT(henka_scene_get_entity_local_bounds(scene, edge_entity, &bounds) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(bounds.center.x, 0.5f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(bounds.extents.x, 1.5f, 0.0001f);
    sandbox3d_authoring_object_destroy(object);
    object = NULL;
    henka_authoring_mesh_destroy(source);
    source = NULL;

    point_entity = henka_scene_create_entity_named(scene, "Point Source");
    HENKA_TEST_ASSERT(point_entity != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(henka_authoring_mesh_create(
        &(henka_authoring_mesh_desc){4U, 4U, 1U, 3U}, &source) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_authoring_mesh_add_vertex(
        source, (henka_vec3){3.0f, 4.0f, 5.0f}, (henka_vec2){0.0f, 0.0f}, 0U,
        &first) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_create_from_mesh(
        engine, scene, point_entity, source, 8U, &object) == HENKA_SUCCESS);
    render_mesh = NULL;
    HENKA_TEST_ASSERT(henka_scene_get_entity_mesh(scene, point_entity, &render_mesh) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(render_mesh != NULL);
    HENKA_TEST_ASSERT(henka_scene_get_entity_local_bounds(scene, point_entity, &bounds) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(bounds.center.x, 3.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(bounds.center.y, 4.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(bounds.center.z, 5.0f, 0.0001f);

    sandbox3d_authoring_object_destroy(object);
    henka_authoring_mesh_destroy(source);
    henka_scene_destroy(scene);
    henka_engine_destroy(engine);
}

static void henka_test_sandbox3d_loose_component_creation(void)
{
    henka_engine_config config = {0};
    henka_engine* engine = NULL;
    henka_scene* scene = NULL;
    henka_authoring_mesh* source = NULL;
    sandbox3d_authoring_object* object = NULL;
    henka_authoring_vertex_id first = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_vertex_id second = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_vertex_id added = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_edge_id edge = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_mesh_counts before;
    henka_authoring_mesh_counts after;
    henka_entity entity;

    config.application_name = "Henka Loose Component Creation Test";
    config.window_width = 320;
    config.window_height = 240;
    config.enable_vsync = false;
    HENKA_TEST_ASSERT(henka_engine_create(&config, &engine) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_create(&scene) == HENKA_SUCCESS);
    entity = henka_scene_create_entity_named(scene, "Loose Component Source");
    HENKA_TEST_ASSERT(entity != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(henka_authoring_mesh_create(
        &(henka_authoring_mesh_desc){8U, 8U, 1U, 3U}, &source) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_authoring_mesh_add_vertex(
        source, (henka_vec3){0.0f, 0.0f, 0.0f}, (henka_vec2){0.0f, 0.0f}, 0U,
        &first) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_authoring_mesh_add_vertex(
        source, (henka_vec3){1.0f, 0.0f, 0.0f}, (henka_vec2){1.0f, 0.0f}, 0U,
        &second) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_create_from_mesh(
        engine, scene, entity, source, 8U, &object) == HENKA_SUCCESS);

    before = henka_authoring_mesh_get_counts(
        sandbox3d_authoring_object_get_mesh(object));
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_add_loose_vertex(
        object, (henka_vec3){2.0f, 0.0f, 0.0f}, (henka_vec2){0.5f, 0.0f}, 3U,
        &added) == HENKA_SUCCESS);
    after = henka_authoring_mesh_get_counts(
        sandbox3d_authoring_object_get_mesh(object));
    HENKA_TEST_ASSERT(added != HENKA_AUTHORING_INVALID_ID);
    HENKA_TEST_ASSERT(after.vertices == before.vertices + 1U &&
        after.edges == before.edges && after.faces == before.faces);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_undo(object) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_authoring_mesh_get_counts(
        sandbox3d_authoring_object_get_mesh(object)).vertices == before.vertices);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_redo(object) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_authoring_mesh_get_counts(
        sandbox3d_authoring_object_get_mesh(object)).vertices == before.vertices + 1U);

    HENKA_TEST_ASSERT(sandbox3d_authoring_object_undo(object) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_add_loose_edge(
        object, first, second, false, &edge) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(edge != HENKA_AUTHORING_INVALID_ID);
    after = henka_authoring_mesh_get_counts(
        sandbox3d_authoring_object_get_mesh(object));
    HENKA_TEST_ASSERT(after.vertices == before.vertices && after.edges == 1U &&
        after.faces == 0U);

    before = after;
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_add_loose_vertex(
        object, (henka_vec3){3.0f, 0.0f, 0.0f}, (henka_vec2){0.0f, 0.0f}, 0U,
        &added) == HENKA_SUCCESS);
    after = henka_authoring_mesh_get_counts(
        sandbox3d_authoring_object_get_mesh(object));
    HENKA_TEST_ASSERT(added != HENKA_AUTHORING_INVALID_ID &&
        after.vertices == before.vertices + 1U &&
        after.edges == before.edges && after.faces == before.faces);

    sandbox3d_authoring_object_destroy(object);
    henka_authoring_mesh_destroy(source);
    henka_scene_destroy(scene);
    henka_engine_destroy(engine);
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
            const henka_authoring_face* before_face = henka_authoring_mesh_get_face(
                sandbox3d_authoring_object_get_mesh(object), picked_face);
            henka_authoring_vertex_id before_vertices[4];
            henka_authoring_edge_id before_edges[4];
            henka_vec2 before_uvs[4];
            size_t corner;
            HENKA_TEST_ASSERT(before_face != NULL && before_face->corner_count == 4U);
            for (corner = 0U; corner < 4U; ++corner)
            {
                before_vertices[corner] = before_face->vertices[corner];
                before_edges[corner] = before_face->edges[corner];
                before_uvs[corner] = before_face->uvs[corner];
            }
            HENKA_TEST_ASSERT(
                sandbox3d_authoring_object_flip_selected_face(object) == HENKA_SUCCESS);
            {
                const henka_authoring_face* flipped_face = henka_authoring_mesh_get_face(
                    sandbox3d_authoring_object_get_mesh(object), picked_face);
                HENKA_TEST_ASSERT(flipped_face != NULL && henka_authoring_mesh_validate(
                    sandbox3d_authoring_object_get_mesh(object)));
                for (corner = 0U; corner < 4U; ++corner)
                {
                    const size_t source_corner = corner == 0U ? 0U : 4U - corner;
                    const size_t source_edge = 3U - corner;
                    HENKA_TEST_ASSERT(
                        flipped_face->vertices[corner] == before_vertices[source_corner] &&
                        flipped_face->edges[corner] == before_edges[source_edge] &&
                        flipped_face->uvs[corner].x == before_uvs[source_corner].x &&
                        flipped_face->uvs[corner].y == before_uvs[source_corner].y);
                }
            }
            HENKA_TEST_ASSERT(sandbox3d_authoring_object_undo(object) == HENKA_SUCCESS);
            {
                const henka_authoring_face* restored_face = henka_authoring_mesh_get_face(
                    sandbox3d_authoring_object_get_mesh(object), picked_face);
                HENKA_TEST_ASSERT(restored_face != NULL);
                for (corner = 0U; corner < 4U; ++corner)
                {
                    HENKA_TEST_ASSERT(
                        restored_face->vertices[corner] == before_vertices[corner] &&
                        restored_face->edges[corner] == before_edges[corner] &&
                        restored_face->uvs[corner].x == before_uvs[corner].x &&
                        restored_face->uvs[corner].y == before_uvs[corner].y);
                }
            }
            HENKA_TEST_ASSERT(sandbox3d_authoring_object_redo(object) == HENKA_SUCCESS);
        }
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
            const henka_authoring_mesh_desc profiled_desc =
                henka_authoring_mesh_get_desc(profiled_mesh);
            size_t active_vertices = 0U;
            size_t active_edges = 0U;
            size_t active_faces = 0U;
            size_t physical_slot;
            float minimum_x = 1000000.0f;
            float maximum_x = -1000000.0f;
            float minimum_y = 1000000.0f;
            float maximum_y = -1000000.0f;
            for (physical_slot = 0U; physical_slot < profiled_desc.max_vertices; ++physical_slot)
            {
                henka_authoring_vertex_id vertex_id = HENKA_AUTHORING_INVALID_ID;
                const henka_authoring_vertex* vertex;
                if (henka_authoring_mesh_get_vertex_id_at(
                        profiled_mesh, physical_slot, &vertex_id) != HENKA_SUCCESS)
                {
                    continue;
                }
                vertex = henka_authoring_mesh_get_vertex(profiled_mesh, vertex_id);
                if (vertex == NULL) continue;
                ++active_vertices;
                if (vertex->position.x < minimum_x) minimum_x = vertex->position.x;
                if (vertex->position.x > maximum_x) maximum_x = vertex->position.x;
                if (vertex->position.y < minimum_y) minimum_y = vertex->position.y;
                if (vertex->position.y > maximum_y) maximum_y = vertex->position.y;
            }
            for (physical_slot = 0U; physical_slot < profiled_desc.max_edges; ++physical_slot)
            {
                henka_authoring_edge_id edge_id = HENKA_AUTHORING_INVALID_ID;
                const henka_authoring_edge* edge;
                size_t face_index;
                if (henka_authoring_mesh_get_edge_id_at(
                        profiled_mesh, physical_slot, &edge_id) != HENKA_SUCCESS)
                {
                    continue;
                }
                edge = henka_authoring_mesh_get_edge(profiled_mesh, edge_id);
                if (edge == NULL) continue;
                ++active_edges;
                HENKA_TEST_ASSERT(henka_authoring_mesh_get_edge_face_count(profiled_mesh, edge_id) >= 1U);
                HENKA_TEST_ASSERT(henka_authoring_mesh_get_edge_face_count(profiled_mesh, edge_id) <= 2U);
                for (face_index = 0U; face_index < edge->face_count; ++face_index)
                {
                    henka_authoring_face_id face_id = HENKA_AUTHORING_INVALID_ID;
                    HENKA_TEST_ASSERT(henka_authoring_mesh_get_edge_face_at(
                        profiled_mesh, edge_id, face_index, &face_id) == HENKA_SUCCESS);
                    HENKA_TEST_ASSERT(henka_authoring_mesh_get_face(profiled_mesh, face_id) != NULL);
                }
            }
            for (physical_slot = 0U; physical_slot < profiled_desc.max_faces; ++physical_slot)
            {
                henka_authoring_face_id face_id = HENKA_AUTHORING_INVALID_ID;
                if (henka_authoring_mesh_get_face_id_at(
                        profiled_mesh, physical_slot, &face_id) == HENKA_SUCCESS &&
                    henka_authoring_mesh_get_face(profiled_mesh, face_id) != NULL)
                {
                    ++active_faces;
                }
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

static void henka_test_sandbox3d_object_authoring_real_obj_import_bridge(void)
{
    static const char* obj_source =
        "# production-loader authoring bridge\n"
        "v -1.0 -1.0 0.0\n"
        "v 1.0 -1.0 0.0\n"
        "v 1.0 1.0 0.0\n"
        "v -1.0 1.0 0.0\n"
        "vt 0.0 0.0\n"
        "vt 1.0 0.0\n"
        "vt 1.0 1.0\n"
        "vt 0.0 1.0\n"
        "f 1/1 2/2 3/3 4/4\n";
    henka_engine_config config = {0};
    henka_engine* engine = NULL;
    henka_scene* scene = NULL;
    henka_model_data imported = {0};
    henka_model_scene_primitive primitive = {0};
    sandbox3d_authoring_object* object = NULL;
    henka_entity entity = HENKA_INVALID_ENTITY;
    henka_mesh* previous_mesh = NULL;
    henka_mesh* current_mesh = NULL;
    henka_material material;
    henka_authoring_mesh_counts counts;
    henka_authoring_mesh_desc mesh_desc;
    size_t face_slot;
    size_t checked_faces = 0U;

    config.application_name = "Henka Real OBJ Authoring Bridge Test";
    config.window_width = 320;
    config.window_height = 240;
    config.enable_vsync = false;
    HENKA_TEST_ASSERT(henka_model_data_load_obj_from_memory(
        obj_source, "authoring-bridge.obj", &imported) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(imported.vertex_count == 6U);
    HENKA_TEST_ASSERT(imported.index_count == 6U);
    HENKA_TEST_ASSERT(imported.vertices != NULL && imported.indices != NULL);
    primitive.vertices = imported.vertices;
    primitive.vertex_count = imported.vertex_count;
    primitive.indices = imported.indices;
    primitive.index_count = imported.index_count;

    HENKA_TEST_ASSERT(henka_engine_create(&config, &engine) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_create(&scene) == HENKA_SUCCESS);
    entity = henka_scene_create_entity_named(scene, "Imported OBJ Authoring Source");
    HENKA_TEST_ASSERT(entity != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(henka_mesh_create_cube(engine, &previous_mesh) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_set_entity_mesh(scene, entity, previous_mesh) == HENKA_SUCCESS);
    material = henka_material_default();
    material.shader = (henka_shader*)(uintptr_t)1U;
    material.base_color = (henka_vec4){0.17f, 0.29f, 0.43f, 1.0f};
    HENKA_TEST_ASSERT(henka_scene_set_entity_material(scene, entity, material) == HENKA_SUCCESS);

    HENKA_TEST_ASSERT(sandbox3d_authoring_object_create_from_model_primitive(
        engine, scene, entity, &primitive, 8U, &object) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(object != NULL);
    counts = henka_authoring_mesh_get_counts(sandbox3d_authoring_object_get_mesh(object));
    HENKA_TEST_ASSERT(counts.vertices == 4U && counts.edges == 5U && counts.faces == 2U);
    HENKA_TEST_ASSERT(henka_scene_get_entity_mesh(scene, entity, &current_mesh) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(current_mesh != NULL && current_mesh != previous_mesh);
    HENKA_TEST_ASSERT(henka_scene_get_entity_material(scene, entity, &material) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(material.base_color.x, 0.17f, 0.0001f);

    mesh_desc = henka_authoring_mesh_get_desc(sandbox3d_authoring_object_get_mesh(object));
    for (face_slot = 0U; face_slot < mesh_desc.max_faces; ++face_slot)
    {
        henka_authoring_face_id face_id = HENKA_AUTHORING_INVALID_ID;
        const henka_authoring_face* face;
        size_t corner;

        if (henka_authoring_mesh_get_face_id_at(
                sandbox3d_authoring_object_get_mesh(object), face_slot, &face_id) != HENKA_SUCCESS)
        {
            continue;
        }
        face = henka_authoring_mesh_get_face(
            sandbox3d_authoring_object_get_mesh(object), face_id);
        HENKA_TEST_ASSERT(face != NULL && face->corner_count == 3U);
        for (corner = 0U; corner < face->corner_count; ++corner)
        {
            henka_vec2 uv;
            HENKA_TEST_ASSERT(henka_authoring_mesh_get_face_corner_uv(
                sandbox3d_authoring_object_get_mesh(object), face_id, corner, &uv) == HENKA_SUCCESS);
            HENKA_TEST_ASSERT(uv.x >= 0.0f && uv.x <= 1.0f && uv.y >= 0.0f && uv.y <= 1.0f);
        }
        ++checked_faces;
    }
    HENKA_TEST_ASSERT(checked_faces == 2U);

    henka_model_data_destroy(&imported);
    HENKA_TEST_ASSERT(henka_scene_get_entity_mesh(scene, entity, &current_mesh) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(current_mesh != NULL && current_mesh != previous_mesh);
    sandbox3d_authoring_object_destroy(object);
    object = NULL;
    HENKA_TEST_ASSERT(henka_scene_get_entity_mesh(scene, entity, &current_mesh) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(current_mesh == previous_mesh);
    henka_mesh_destroy(previous_mesh);
    henka_scene_destroy_entity(scene, entity);
    henka_scene_destroy(scene);
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
    {
        sandbox3d_authoring_selection_query query = {0};
        query.kind = SANDBOX3D_AUTHORING_SELECTION_QUERY_FACE_SIDE_COUNT;
        query.face_side_count = 4U;
        HENKA_TEST_ASSERT(sandbox3d_authoring_object_select_matching_components(
            object, &query) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(sandbox3d_authoring_object_get_selected_component_count(object) == 6U);

        query.kind = SANDBOX3D_AUTHORING_SELECTION_QUERY_MATERIAL_REGION;
        query.material_region = 0U;
        HENKA_TEST_ASSERT(sandbox3d_authoring_object_select_matching_components(
            object, &query) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(sandbox3d_authoring_object_get_selected_component_count(object) == 6U);

        HENKA_TEST_ASSERT(sandbox3d_authoring_object_select_component(
            object, 1U, false) == HENKA_SUCCESS);
        query.kind = SANDBOX3D_AUTHORING_SELECTION_QUERY_SIMILAR_MATERIAL_REGION;
        HENKA_TEST_ASSERT(sandbox3d_authoring_object_select_matching_components(
            object, &query) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(sandbox3d_authoring_object_get_selected_component_count(object) == 6U);
        HENKA_TEST_ASSERT(sandbox3d_authoring_object_get_active_component_id(object) == 1U);

        HENKA_TEST_ASSERT(sandbox3d_authoring_object_select_component(
            object, 1U, false) == HENKA_SUCCESS);
        query.kind = SANDBOX3D_AUTHORING_SELECTION_QUERY_SIMILAR_NORMAL;
        query.minimum_normal_dot = 0.99f;
        HENKA_TEST_ASSERT(sandbox3d_authoring_object_select_matching_components(
            object, &query) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(sandbox3d_authoring_object_get_selected_component_count(object) == 1U);
        HENKA_TEST_ASSERT(sandbox3d_authoring_object_get_active_component_id(object) == 1U);

        query.face_side_count = 2U;
        query.kind = SANDBOX3D_AUTHORING_SELECTION_QUERY_FACE_SIDE_COUNT;
        HENKA_TEST_ASSERT(sandbox3d_authoring_object_select_matching_components(
            object, &query) == HENKA_ERROR_INVALID_ARGUMENT);
        HENKA_TEST_ASSERT(sandbox3d_authoring_object_get_selected_component_count(object) == 1U);
        HENKA_TEST_ASSERT(sandbox3d_authoring_object_get_active_component_id(object) == 1U);
    }

    {
        henka_ui_context* ui = NULL;
        henka_ui_frame_desc frame = {0};
        sandbox3d_modeling_selection_command_result command = {0};
        const henka_ui_rect bounds = {20.0f, 20.0f, 300.0f, 24.0f};

        HENKA_TEST_ASSERT(henka_ui_create(&ui) == HENKA_SUCCESS);
        henka_ui_set_visible(ui, true);
        frame.framebuffer_width = 640;
        frame.framebuffer_height = 480;
        frame.mouse_position = (henka_vec2){40.0f, 30.0f};
        frame.mouse_left_down = true;
        frame.mouse_left_pressed = true;
        HENKA_TEST_ASSERT(henka_ui_begin_frame(ui, &frame) == HENKA_SUCCESS);
        sandbox3d_modeling_selection_commands_draw(
            ui, bounds, object, &command);
        HENKA_TEST_ASSERT(!command.invoked);
        HENKA_TEST_ASSERT(henka_ui_end_frame(ui) == HENKA_SUCCESS);

        frame.mouse_left_down = false;
        frame.mouse_left_pressed = false;
        frame.mouse_left_released = true;
        HENKA_TEST_ASSERT(henka_ui_begin_frame(ui, &frame) == HENKA_SUCCESS);
        sandbox3d_modeling_selection_commands_draw(
            ui, bounds, object, &command);
        HENKA_TEST_ASSERT(command.invoked);
        HENKA_TEST_ASSERT(command.result == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(
            command.kind == SANDBOX3D_AUTHORING_SELECTION_QUERY_FACE_SIDE_COUNT);
        HENKA_TEST_ASSERT(command.selected_count == 6U);
        HENKA_TEST_ASSERT(henka_ui_end_frame(ui) == HENKA_SUCCESS);
        henka_ui_destroy(ui);
    }

    {
        henka_authoring_mesh* plane_mesh = NULL;
        sandbox3d_authoring_object* plane_object = NULL;
        sandbox3d_authoring_selection_query query = {0};
        const henka_authoring_mesh_desc plane_desc =
            henka_authoring_mesh_desc_default();
        henka_authoring_edge_id hard_edge = HENKA_AUTHORING_INVALID_ID;
        const henka_entity plane_entity =
            henka_scene_create_entity_named(scene, "Topology Selection Plane");

        HENKA_TEST_ASSERT(plane_entity != HENKA_INVALID_ENTITY);
        HENKA_TEST_ASSERT(henka_authoring_mesh_create_plane(
            &plane_desc, 1.0f, 1.0f, &plane_mesh) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(henka_authoring_mesh_get_edge_id_at(
            plane_mesh, 0U, &hard_edge) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(henka_authoring_mesh_set_edge_hard(
            plane_mesh, hard_edge, true) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(sandbox3d_authoring_object_create_from_mesh(
            engine, scene, plane_entity, plane_mesh, 8U, &plane_object) == HENKA_SUCCESS);

        sandbox3d_authoring_object_set_selection_mode(
            plane_object, SANDBOX3D_AUTHORING_SELECTION_EDGE);
        query.kind = SANDBOX3D_AUTHORING_SELECTION_QUERY_BOUNDARY;
        HENKA_TEST_ASSERT(sandbox3d_authoring_object_select_matching_components(
            plane_object, &query) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(sandbox3d_authoring_object_get_selected_component_count(
            plane_object) == 4U);

        query.kind = SANDBOX3D_AUTHORING_SELECTION_QUERY_HARD_EDGE;
        HENKA_TEST_ASSERT(sandbox3d_authoring_object_select_matching_components(
            plane_object, &query) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(sandbox3d_authoring_object_get_selected_component_count(
            plane_object) == 1U);
        HENKA_TEST_ASSERT(sandbox3d_authoring_object_get_active_component_id(
            plane_object) == hard_edge);

        sandbox3d_authoring_object_set_selection_mode(
            plane_object, SANDBOX3D_AUTHORING_SELECTION_VERTEX);
        HENKA_TEST_ASSERT(sandbox3d_authoring_object_select_component(
            plane_object, 1U, false) == HENKA_SUCCESS);
        query.kind = SANDBOX3D_AUTHORING_SELECTION_QUERY_SIMILAR_MATERIAL_REGION;
        HENKA_TEST_ASSERT(sandbox3d_authoring_object_select_matching_components(
            plane_object, &query) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(sandbox3d_authoring_object_get_selected_component_count(
            plane_object) == 4U);
        HENKA_TEST_ASSERT(sandbox3d_authoring_object_get_active_component_id(
            plane_object) == 1U);

        sandbox3d_authoring_object_destroy(plane_object);
        henka_authoring_mesh_destroy(plane_mesh);
    }

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
    {
        const uint32_t replacement[] = {3U, 5U, 7U};
        const uint32_t invalid_replacement[] = {3U, 99U};
        const uint32_t unsorted_replacement[] = {5U, 3U};
        HENKA_TEST_ASSERT(sandbox3d_authoring_object_replace_component_selection(
            object, replacement, 3U, 5U) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(sandbox3d_authoring_object_get_selected_component_count(object) == 3U);
        HENKA_TEST_ASSERT(sandbox3d_authoring_object_get_active_component_id(object) == 5U);
        HENKA_TEST_ASSERT(sandbox3d_authoring_object_replace_component_selection(
            object, invalid_replacement, 2U, 3U) == HENKA_ERROR_INVALID_ARGUMENT);
        HENKA_TEST_ASSERT(sandbox3d_authoring_object_get_selected_component_count(object) == 3U);
        HENKA_TEST_ASSERT(sandbox3d_authoring_object_get_active_component_id(object) == 5U);
        HENKA_TEST_ASSERT(sandbox3d_authoring_object_replace_component_selection(
            object, unsorted_replacement, 2U, 3U) == HENKA_ERROR_INVALID_ARGUMENT);
        HENKA_TEST_ASSERT(sandbox3d_authoring_object_get_selected_component_count(object) == 3U);
        HENKA_TEST_ASSERT(sandbox3d_authoring_object_get_active_component_id(object) == 5U);
    }
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

static void henka_test_sandbox3d_object_authoring_hover_query_preserves_selection(void)
{
    static const sandbox3d_authoring_selection_mode modes[] = {
        SANDBOX3D_AUTHORING_SELECTION_FACE,
        SANDBOX3D_AUTHORING_SELECTION_VERTEX,
        SANDBOX3D_AUTHORING_SELECTION_EDGE};
    static const henka_ray hit_rays[] = {
        {{0.0f, 0.0f, 3.0f}, {0.0f, 0.0f, -1.0f}},
        {{0.5f, 0.5f, 3.0f}, {0.0f, 0.0f, -1.0f}},
        {{0.5f, 0.0f, 3.0f}, {0.0f, 0.0f, -1.0f}}};
    const henka_ray miss_ray = {
        {4.0f, 4.0f, 3.0f},
        {0.0f, 0.0f, -1.0f}};
    henka_engine_config config = {0};
    henka_engine* engine = NULL;
    henka_scene* scene = NULL;
    sandbox3d_authoring_object* object = NULL;
    henka_mesh* previous_mesh = NULL;
    henka_entity entity;
    size_t mode_index;

    config.application_name = "Henka Authoring Hover Query Test";
    config.window_width = 320;
    config.window_height = 240;
    config.enable_vsync = false;
    HENKA_TEST_ASSERT(henka_engine_create(&config, &engine) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_create(&scene) == HENKA_SUCCESS);
    entity = henka_scene_create_entity_named(scene, "Hover Query Source");
    HENKA_TEST_ASSERT(entity != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(henka_mesh_create_cube(engine, &previous_mesh) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_set_entity_mesh(scene, entity, previous_mesh) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_create_box(
        engine, scene, entity, 1.0f, 1.0f, 1.0f, NULL, 8U, &object) == HENKA_SUCCESS);

    for (mode_index = 0U; mode_index < sizeof(modes) / sizeof(modes[0]); ++mode_index)
    {
        uint32_t hovered_id = HENKA_AUTHORING_INVALID_ID;
        uint32_t active_before;
        henka_authoring_face_id selected_face_before;
        size_t selected_count_before;
        const henka_authoring_mesh* mesh;

        sandbox3d_authoring_object_set_selection_mode(object, modes[mode_index]);
        HENKA_TEST_ASSERT(
            sandbox3d_authoring_object_select_component(object, 1U, false) == HENKA_SUCCESS);
        active_before = sandbox3d_authoring_object_get_active_component_id(object);
        selected_face_before = sandbox3d_authoring_object_get_selected_face(object);
        selected_count_before = sandbox3d_authoring_object_get_selected_component_count(object);

        HENKA_TEST_ASSERT(
            sandbox3d_authoring_object_find_component(
                object,
                hit_rays[mode_index],
                100.0f,
                &hovered_id) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(hovered_id != HENKA_AUTHORING_INVALID_ID);
        mesh = sandbox3d_authoring_object_get_mesh(object);
        HENKA_TEST_ASSERT(mesh != NULL);
        if (modes[mode_index] == SANDBOX3D_AUTHORING_SELECTION_VERTEX)
        {
            HENKA_TEST_ASSERT(henka_authoring_mesh_get_vertex(mesh, hovered_id) != NULL);
        }
        else if (modes[mode_index] == SANDBOX3D_AUTHORING_SELECTION_EDGE)
        {
            HENKA_TEST_ASSERT(henka_authoring_mesh_get_edge(mesh, hovered_id) != NULL);
        }
        else
        {
            HENKA_TEST_ASSERT(henka_authoring_mesh_get_face(mesh, hovered_id) != NULL);
        }
        HENKA_TEST_ASSERT(
            sandbox3d_authoring_object_get_selected_component_count(object) ==
            selected_count_before);
        HENKA_TEST_ASSERT(
            sandbox3d_authoring_object_get_active_component_id(object) == active_before);
        HENKA_TEST_ASSERT(
            sandbox3d_authoring_object_get_selected_face(object) == selected_face_before);

        hovered_id = 17U;
        HENKA_TEST_ASSERT(
            sandbox3d_authoring_object_find_component(
                object,
                miss_ray,
                100.0f,
                &hovered_id) != HENKA_SUCCESS);
        HENKA_TEST_ASSERT(hovered_id == HENKA_AUTHORING_INVALID_ID);
        HENKA_TEST_ASSERT(
            sandbox3d_authoring_object_get_selected_component_count(object) ==
            selected_count_before);
        HENKA_TEST_ASSERT(
            sandbox3d_authoring_object_get_active_component_id(object) == active_before);
        HENKA_TEST_ASSERT(
            sandbox3d_authoring_object_get_selected_face(object) == selected_face_before);
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

static void henka_test_sandbox3d_object_authoring_edge_dissolve(void)
{
    henka_engine_config config = {0};
    henka_engine* engine = NULL;
    henka_scene* scene = NULL;
    henka_authoring_mesh* source = NULL;
    sandbox3d_authoring_object* object = NULL;
    henka_authoring_mesh_desc desc = {16U, 32U, 16U, 8U};
    henka_authoring_vertex_id vertices[9];
    henka_authoring_edge_id edge_id = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_mesh_counts counts;
    henka_entity entity;

    config.application_name = "Henka Edge Dissolve Test";
    config.window_width = 320;
    config.window_height = 240;
    config.enable_vsync = false;
    HENKA_TEST_ASSERT(henka_engine_create(&config, &engine) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_create(&scene) == HENKA_SUCCESS);
    entity = henka_scene_create_entity_named(scene, "Edge Dissolve Grid");
    HENKA_TEST_ASSERT(entity != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(henka_authoring_mesh_create(&desc, &source) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_test_make_quad_grid(source, 2U, 2U, vertices, 9U) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_test_find_edge_between_vertices(
        source, vertices[1U], vertices[4U], &edge_id) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_create_from_mesh(
        engine, scene, entity, source, 8U, &object) == HENKA_SUCCESS);
    sandbox3d_authoring_object_set_selection_mode(object, SANDBOX3D_AUTHORING_SELECTION_EDGE);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_select_component(
        object, edge_id, false) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_dissolve_selected_edge(object) == HENKA_SUCCESS);
    counts = henka_authoring_mesh_get_counts(
        sandbox3d_authoring_object_get_mesh(object));
    HENKA_TEST_ASSERT(counts.vertices == 9U && counts.edges == 11U && counts.faces == 3U);
    HENKA_TEST_ASSERT(henka_authoring_mesh_validate(
        sandbox3d_authoring_object_get_mesh(object)));
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_get_selected_component_count(object) == 0U);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_undo(object) == HENKA_SUCCESS);
    counts = henka_authoring_mesh_get_counts(
        sandbox3d_authoring_object_get_mesh(object));
    HENKA_TEST_ASSERT(counts.vertices == 9U && counts.edges == 12U && counts.faces == 4U);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_redo(object) == HENKA_SUCCESS);
    counts = henka_authoring_mesh_get_counts(
        sandbox3d_authoring_object_get_mesh(object));
    HENKA_TEST_ASSERT(counts.vertices == 9U && counts.edges == 11U && counts.faces == 3U);

    sandbox3d_authoring_object_destroy(object);
    henka_authoring_mesh_destroy(source);
    henka_scene_destroy(scene);
    henka_engine_destroy(engine);
}

static void henka_test_sandbox3d_object_authoring_edge_delete(void)
{
    henka_engine_config config = {0};
    henka_engine* engine = NULL;
    henka_scene* scene = NULL;
    henka_authoring_mesh* source = NULL;
    sandbox3d_authoring_object* object = NULL;
    henka_authoring_mesh_desc desc = {16U, 32U, 16U, 8U};
    henka_authoring_vertex_id vertices[9];
    henka_authoring_edge_id edge_id = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_mesh_counts counts;
    henka_entity entity;

    config.application_name = "Henka Edge Delete Test";
    config.window_width = 320;
    config.window_height = 240;
    config.enable_vsync = false;
    HENKA_TEST_ASSERT(henka_engine_create(&config, &engine) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_create(&scene) == HENKA_SUCCESS);
    entity = henka_scene_create_entity_named(scene, "Edge Delete Grid");
    HENKA_TEST_ASSERT(entity != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(henka_authoring_mesh_create(&desc, &source) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_test_make_quad_grid(source, 2U, 2U, vertices, 9U) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_test_find_edge_between_vertices(
        source, vertices[1U], vertices[4U], &edge_id) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_create_from_mesh(
        engine, scene, entity, source, 8U, &object) == HENKA_SUCCESS);
    sandbox3d_authoring_object_set_selection_mode(object, SANDBOX3D_AUTHORING_SELECTION_EDGE);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_select_component(
        object, edge_id, false) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_delete_selected_edge(object) == HENKA_SUCCESS);
    counts = henka_authoring_mesh_get_counts(
        sandbox3d_authoring_object_get_mesh(object));
    HENKA_TEST_ASSERT(counts.vertices == 9U && counts.edges == 7U && counts.faces == 2U);
    HENKA_TEST_ASSERT(henka_authoring_mesh_get_edge(
        sandbox3d_authoring_object_get_mesh(object), edge_id) == NULL);
    HENKA_TEST_ASSERT(henka_authoring_mesh_validate(
        sandbox3d_authoring_object_get_mesh(object)));
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_get_selected_component_count(object) == 0U);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_undo(object) == HENKA_SUCCESS);
    counts = henka_authoring_mesh_get_counts(
        sandbox3d_authoring_object_get_mesh(object));
    HENKA_TEST_ASSERT(counts.vertices == 9U && counts.edges == 12U && counts.faces == 4U);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_redo(object) == HENKA_SUCCESS);
    counts = henka_authoring_mesh_get_counts(
        sandbox3d_authoring_object_get_mesh(object));
    HENKA_TEST_ASSERT(counts.vertices == 9U && counts.edges == 7U && counts.faces == 2U);

    sandbox3d_authoring_object_destroy(object);
    henka_authoring_mesh_destroy(source);
    henka_scene_destroy(scene);
    henka_engine_destroy(engine);
}

static void henka_test_sandbox3d_object_authoring_edge_bevel(void)
{
    henka_engine_config config = {0};
    henka_engine* engine = NULL;
    henka_scene* scene = NULL;
    henka_authoring_mesh* source = NULL;
    sandbox3d_authoring_object* object = NULL;
    const henka_authoring_mesh_desc desc = {16U, 32U, 16U, 8U};
    henka_authoring_mesh_counts counts;
    henka_entity entity;

    config.application_name = "Henka Edge Bevel Test";
    config.window_width = 320;
    config.window_height = 240;
    config.enable_vsync = false;
    HENKA_TEST_ASSERT(henka_engine_create(&config, &engine) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_create(&scene) == HENKA_SUCCESS);
    entity = henka_scene_create_entity_named(scene, "Boundary Edge Bevel Plane");
    HENKA_TEST_ASSERT(entity != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(henka_authoring_mesh_create_plane(&desc, 2.0f, 2.0f, &source) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_create_from_mesh(
        engine, scene, entity, source, 8U, &object) == HENKA_SUCCESS);
    sandbox3d_authoring_object_set_selection_mode(object, SANDBOX3D_AUTHORING_SELECTION_EDGE);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_select_component(object, 1U, false) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_set_bevel_width(object, 0.2f) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_bevel_selected_edge(object) == HENKA_SUCCESS);
    counts = henka_authoring_mesh_get_counts(sandbox3d_authoring_object_get_mesh(object));
    HENKA_TEST_ASSERT(counts.vertices == 6U && counts.edges == 7U && counts.faces == 2U);
    HENKA_TEST_ASSERT(henka_authoring_mesh_validate(sandbox3d_authoring_object_get_mesh(object)));
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_get_selected_component_count(object) == 0U);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_undo(object) == HENKA_SUCCESS);
    counts = henka_authoring_mesh_get_counts(sandbox3d_authoring_object_get_mesh(object));
    HENKA_TEST_ASSERT(counts.vertices == 4U && counts.edges == 4U && counts.faces == 1U);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_redo(object) == HENKA_SUCCESS);
    counts = henka_authoring_mesh_get_counts(sandbox3d_authoring_object_get_mesh(object));
    HENKA_TEST_ASSERT(counts.vertices == 6U && counts.edges == 7U && counts.faces == 2U);

    sandbox3d_authoring_object_destroy(object);
    henka_authoring_mesh_destroy(source);
    henka_scene_destroy(scene);
    henka_engine_destroy(engine);
}

static void henka_test_sandbox3d_object_authoring_multi_edge_bevel(void)
{
    const henka_vec3 positions[8] = {
        {-1.5f, -1.0f, 0.0f}, {-0.5f, -1.0f, 0.0f}, {-0.5f, 0.0f, 0.0f},
        {-1.5f, 0.0f, 0.0f}, {1.5f, -1.0f, 0.0f}, {2.5f, -1.0f, 0.0f},
        {2.5f, 0.0f, 0.0f}, {1.5f, 0.0f, 0.0f}};
    const henka_authoring_vertex_id face_vertices[2][4] = {
        {1U, 2U, 3U, 4U}, {5U, 6U, 7U, 8U}};
    henka_engine_config config = {0};
    henka_engine* engine = NULL;
    henka_scene* scene = NULL;
    henka_authoring_mesh* source = NULL;
    sandbox3d_authoring_object* object = NULL;
    const henka_authoring_mesh_desc desc = {32U, 64U, 16U, 8U};
    henka_authoring_mesh_counts counts;
    henka_authoring_face_id face_id = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_edge_id selected_edges[2] = {
        HENKA_AUTHORING_INVALID_ID, HENKA_AUTHORING_INVALID_ID};
    henka_entity entity;
    size_t edge_slot;
    size_t index;

    config.application_name = "Henka Multi-Edge Bevel Test";
    config.window_width = 320;
    config.window_height = 240;
    config.enable_vsync = false;
    HENKA_TEST_ASSERT(henka_engine_create(&config, &engine) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_create(&scene) == HENKA_SUCCESS);
    entity = henka_scene_create_entity_named(scene, "Multi-Edge Bevel Quads");
    HENKA_TEST_ASSERT(entity != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(henka_authoring_mesh_create(&desc, &source) == HENKA_SUCCESS);
    for (index = 0U; index < 8U; ++index)
    {
        HENKA_TEST_ASSERT(henka_authoring_mesh_add_vertex(
            source, positions[index], (henka_vec2){0.0f, 0.0f}, 0U,
            &(henka_authoring_vertex_id){0U}) == HENKA_SUCCESS);
    }
    HENKA_TEST_ASSERT(henka_authoring_mesh_add_face(
        source, face_vertices[0], 4U, 0U, true, &face_id) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_authoring_mesh_add_face(
        source, face_vertices[1], 4U, 0U, true, &face_id) == HENKA_SUCCESS);
    for (edge_slot = 0U;
         edge_slot < henka_authoring_mesh_get_desc(source).max_edges;
         ++edge_slot)
    {
        henka_authoring_edge_id edge_id = HENKA_AUTHORING_INVALID_ID;
        const henka_authoring_edge* edge;
        const henka_result edge_result = henka_authoring_mesh_get_edge_id_at(
            source, edge_slot, &edge_id);
        HENKA_TEST_ASSERT(edge_result == HENKA_SUCCESS ||
            edge_result == HENKA_ERROR_INVALID_ARGUMENT);
        if (edge_result != HENKA_SUCCESS)
        {
            continue;
        }
        edge = henka_authoring_mesh_get_edge(source, edge_id);
        HENKA_TEST_ASSERT(edge != NULL);
        if ((edge->vertices[0] == 1U && edge->vertices[1] == 2U) ||
            (edge->vertices[0] == 2U && edge->vertices[1] == 1U))
        {
            selected_edges[0] = edge_id;
        }
        else if ((edge->vertices[0] == 5U && edge->vertices[1] == 6U) ||
            (edge->vertices[0] == 6U && edge->vertices[1] == 5U))
        {
            selected_edges[1] = edge_id;
        }
    }
    HENKA_TEST_ASSERT(selected_edges[0] != HENKA_AUTHORING_INVALID_ID);
    HENKA_TEST_ASSERT(selected_edges[1] != HENKA_AUTHORING_INVALID_ID);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_create_from_mesh(
        engine, scene, entity, source, 8U, &object) == HENKA_SUCCESS);
    sandbox3d_authoring_object_set_selection_mode(object, SANDBOX3D_AUTHORING_SELECTION_EDGE);
    HENKA_TEST_ASSERT(
        sandbox3d_authoring_object_select_component(
            object, selected_edges[0], false) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(
        sandbox3d_authoring_object_select_component(
            object, selected_edges[1], true) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_set_bevel_width(object, 0.1f) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_bevel_selected_edge(object) == HENKA_SUCCESS);
    counts = henka_authoring_mesh_get_counts(sandbox3d_authoring_object_get_mesh(object));
    HENKA_TEST_ASSERT(counts.vertices == 12U && counts.edges == 14U && counts.faces == 4U);
    HENKA_TEST_ASSERT(henka_authoring_mesh_validate(sandbox3d_authoring_object_get_mesh(object)));
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_get_selected_component_count(object) == 0U);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_undo(object) == HENKA_SUCCESS);
    counts = henka_authoring_mesh_get_counts(sandbox3d_authoring_object_get_mesh(object));
    HENKA_TEST_ASSERT(counts.vertices == 8U && counts.edges == 8U && counts.faces == 2U);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_redo(object) == HENKA_SUCCESS);
    counts = henka_authoring_mesh_get_counts(sandbox3d_authoring_object_get_mesh(object));
    HENKA_TEST_ASSERT(counts.vertices == 12U && counts.edges == 14U && counts.faces == 4U);

    sandbox3d_authoring_object_destroy(object);
    henka_authoring_mesh_destroy(source);
    henka_scene_destroy(scene);
    henka_engine_destroy(engine);
}

static void henka_test_sandbox3d_object_authoring_loop_cut(void)
{
    henka_engine_config config = {0};
    henka_engine* engine = NULL;
    henka_scene* scene = NULL;
    henka_authoring_mesh* source = NULL;
    sandbox3d_authoring_object* object = NULL;
    const henka_authoring_mesh_desc desc = {16U, 32U, 16U, 8U};
    henka_authoring_mesh_counts counts;
    henka_entity entity;

    config.application_name = "Henka Quad Face Cut Test";
    config.window_width = 320;
    config.window_height = 240;
    config.enable_vsync = false;
    HENKA_TEST_ASSERT(henka_engine_create(&config, &engine) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_create(&scene) == HENKA_SUCCESS);
    entity = henka_scene_create_entity_named(scene, "Quad Face Cut Plane");
    HENKA_TEST_ASSERT(entity != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(henka_authoring_mesh_create_plane(&desc, 2.0f, 2.0f, &source) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_create_from_mesh(
        engine, scene, entity, source, 8U, &object) == HENKA_SUCCESS);
    sandbox3d_authoring_object_set_selection_mode(object, SANDBOX3D_AUTHORING_SELECTION_FACE);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_select_component(object, 1U, false) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_loop_cut_selected_face_at_factor(object, 0.0f) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_loop_cut_selected_face_at_factor(object, 1.0f) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_preview_loop_cut_selected_face_at_factor(object, 0.25f) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_has_preview(object));
    counts = henka_authoring_mesh_get_counts(sandbox3d_authoring_object_get_mesh(object));
    HENKA_TEST_ASSERT(counts.vertices == 4U && counts.edges == 4U && counts.faces == 1U);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_cancel_preview(object) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(!sandbox3d_authoring_object_has_preview(object));
    counts = henka_authoring_mesh_get_counts(sandbox3d_authoring_object_get_mesh(object));
    HENKA_TEST_ASSERT(counts.vertices == 4U && counts.edges == 4U && counts.faces == 1U);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_preview_loop_cut_selected_face_at_factor(object, 0.25f) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_commit_preview(object) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(!sandbox3d_authoring_object_has_preview(object));
    counts = henka_authoring_mesh_get_counts(sandbox3d_authoring_object_get_mesh(object));
    HENKA_TEST_ASSERT(counts.vertices == 6U && counts.edges == 7U && counts.faces == 2U);
    HENKA_TEST_ASSERT(
        sandbox3d_authoring_object_get_selected_face(object) != HENKA_AUTHORING_INVALID_ID &&
        henka_authoring_mesh_get_face(
            sandbox3d_authoring_object_get_mesh(object),
            sandbox3d_authoring_object_get_selected_face(object)) != NULL);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_undo(object) == HENKA_SUCCESS);
    counts = henka_authoring_mesh_get_counts(sandbox3d_authoring_object_get_mesh(object));
    HENKA_TEST_ASSERT(counts.vertices == 4U && counts.edges == 4U && counts.faces == 1U);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_preview_loop_cut_selected_face_multi(object, 2U) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_has_preview(object));
    counts = henka_authoring_mesh_get_counts(sandbox3d_authoring_object_get_mesh(object));
    HENKA_TEST_ASSERT(counts.vertices == 4U && counts.edges == 4U && counts.faces == 1U);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_commit_preview(object) == HENKA_SUCCESS);
    counts = henka_authoring_mesh_get_counts(sandbox3d_authoring_object_get_mesh(object));
    HENKA_TEST_ASSERT(counts.vertices == 8U && counts.edges == 10U && counts.faces == 3U);
    HENKA_TEST_ASSERT(henka_authoring_mesh_validate(sandbox3d_authoring_object_get_mesh(object)));
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_undo(object) == HENKA_SUCCESS);
    counts = henka_authoring_mesh_get_counts(sandbox3d_authoring_object_get_mesh(object));
    HENKA_TEST_ASSERT(counts.vertices == 4U && counts.edges == 4U && counts.faces == 1U);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_loop_cut_selected_face_at_factor(object, 0.25f) == HENKA_SUCCESS);
    counts = henka_authoring_mesh_get_counts(sandbox3d_authoring_object_get_mesh(object));
    HENKA_TEST_ASSERT(counts.vertices == 6U && counts.edges == 7U && counts.faces == 2U);
    HENKA_TEST_ASSERT(henka_authoring_mesh_validate(sandbox3d_authoring_object_get_mesh(object)));
    HENKA_TEST_ASSERT(
        sandbox3d_authoring_object_get_selected_face(object) != HENKA_AUTHORING_INVALID_ID &&
        henka_authoring_mesh_get_face(
            sandbox3d_authoring_object_get_mesh(object),
            sandbox3d_authoring_object_get_selected_face(object)) != NULL);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_undo(object) == HENKA_SUCCESS);
    counts = henka_authoring_mesh_get_counts(sandbox3d_authoring_object_get_mesh(object));
    HENKA_TEST_ASSERT(counts.vertices == 4U && counts.edges == 4U && counts.faces == 1U);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_redo(object) == HENKA_SUCCESS);
    counts = henka_authoring_mesh_get_counts(sandbox3d_authoring_object_get_mesh(object));
    HENKA_TEST_ASSERT(counts.vertices == 6U && counts.edges == 7U && counts.faces == 2U);

    sandbox3d_authoring_object_destroy(object);
    henka_authoring_mesh_destroy(source);
    henka_scene_destroy(scene);
    henka_engine_destroy(engine);
}

static void henka_test_sandbox3d_object_authoring_quad_strip_loop_cut(void)
{
    henka_engine_config config = {0};
    henka_engine* engine = NULL;
    henka_scene* scene = NULL;
    henka_authoring_mesh* source = NULL;
    sandbox3d_authoring_object* object = NULL;
    const henka_authoring_mesh_desc desc = {32U, 64U, 16U, 8U};
    const henka_vec3 positions[8] = {
        {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 0.0f},
        {0.0f, 1.0f, 0.0f}, {2.0f, 0.0f, 0.0f}, {2.0f, 1.0f, 0.0f},
        {3.0f, 0.0f, 0.0f}, {3.0f, 1.0f, 0.0f}};
    const henka_authoring_vertex_id faces[3][4] = {
        {1U, 2U, 3U, 4U}, {2U, 5U, 6U, 3U}, {5U, 7U, 8U, 6U}};
    henka_authoring_edge_id edge_id;
    henka_authoring_edge_id start_edge_id = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_mesh_counts counts;
    henka_entity entity;
    size_t index;

    config.application_name = "Henka Quad Strip Loop Cut Test";
    config.window_width = 320;
    config.window_height = 240;
    config.enable_vsync = false;
    HENKA_TEST_ASSERT(henka_engine_create(&config, &engine) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_create(&scene) == HENKA_SUCCESS);
    entity = henka_scene_create_entity_named(scene, "Quad Strip Loop Cut");
    HENKA_TEST_ASSERT(entity != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(henka_authoring_mesh_create(&desc, &source) == HENKA_SUCCESS);
    for (index = 0U; index < 8U; ++index)
    {
        HENKA_TEST_ASSERT(henka_authoring_mesh_add_vertex(
            source, positions[index], (henka_vec2){(float)(index % 4U), (float)(index / 4U)}, 0U,
            &(henka_authoring_vertex_id){0U}) == HENKA_SUCCESS);
    }
    for (index = 0U; index < 3U; ++index)
    {
        HENKA_TEST_ASSERT(henka_authoring_mesh_add_face(
            source, faces[index], 4U, 0U, true,
            &(henka_authoring_face_id){0U}) == HENKA_SUCCESS);
    }
    for (index = 0U; index < henka_authoring_mesh_get_desc(source).max_edges; ++index)
    {
        if (henka_authoring_mesh_get_edge_id_at(source, index, &edge_id) != HENKA_SUCCESS)
        {
            continue;
        }
        {
            const henka_authoring_edge* edge = henka_authoring_mesh_get_edge(source, edge_id);
            if (edge != NULL && edge->face_count == 1U &&
                ((edge->vertices[0] == 1U && edge->vertices[1] == 4U) ||
                 (edge->vertices[0] == 4U && edge->vertices[1] == 1U)))
            {
                start_edge_id = edge_id;
                break;
            }
        }
    }
    HENKA_TEST_ASSERT(start_edge_id != HENKA_AUTHORING_INVALID_ID);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_create_from_mesh(
        engine, scene, entity, source, 8U, &object) == HENKA_SUCCESS);
    sandbox3d_authoring_object_set_selection_mode(object, SANDBOX3D_AUTHORING_SELECTION_FACE);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_select_component(object, 1U, false) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_loop_cut_selected_face(object) == HENKA_SUCCESS);
    counts = henka_authoring_mesh_get_counts(sandbox3d_authoring_object_get_mesh(object));
    HENKA_TEST_ASSERT(counts.vertices == 12U && counts.edges == 17U && counts.faces == 6U);
    HENKA_TEST_ASSERT(henka_authoring_mesh_validate(sandbox3d_authoring_object_get_mesh(object)));
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_undo(object) == HENKA_SUCCESS);
    counts = henka_authoring_mesh_get_counts(sandbox3d_authoring_object_get_mesh(object));
    HENKA_TEST_ASSERT(counts.vertices == 8U && counts.edges == 10U && counts.faces == 3U);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_redo(object) == HENKA_SUCCESS);
    counts = henka_authoring_mesh_get_counts(sandbox3d_authoring_object_get_mesh(object));
    HENKA_TEST_ASSERT(counts.vertices == 12U && counts.edges == 17U && counts.faces == 6U);

    sandbox3d_authoring_object_destroy(object);
    henka_authoring_mesh_destroy(source);
    henka_scene_destroy(scene);
    henka_engine_destroy(engine);
}

static void henka_test_sandbox3d_object_authoring_edge_loop_slide(void)
{
    henka_engine_config config = {0};
    henka_engine* engine = NULL;
    henka_scene* scene = NULL;
    henka_authoring_mesh* source = NULL;
    sandbox3d_authoring_object* object = NULL;
    const henka_authoring_mesh_desc desc = {32U, 64U, 16U, 8U};
    henka_authoring_vertex_id vertices[12];
    henka_authoring_edge_id loop_edge;
    henka_authoring_vertex_id loop_vertices[4] = {2U, 5U, 8U, 11U};
    sandbox3d_modeling_operator_session operator_session = {0};
    float operator_baseline_x = 0.0f;
    henka_entity entity;
    size_t row;

    config.application_name = "Henka Edge Loop Slide Test";
    config.window_width = 320;
    config.window_height = 240;
    config.enable_vsync = false;
    HENKA_TEST_ASSERT(henka_engine_create(&config, &engine) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_create(&scene) == HENKA_SUCCESS);
    entity = henka_scene_create_entity_named(scene, "Edge Loop Slide Grid");
    HENKA_TEST_ASSERT(entity != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(henka_authoring_mesh_create(&desc, &source) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_test_make_quad_grid(source, 2U, 3U, vertices, 12U) ==
        HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_test_find_edge_between_vertices(
        source, vertices[1], vertices[4], &loop_edge) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_create_from_mesh(
        engine, scene, entity, source, 8U, &object) == HENKA_SUCCESS);
    sandbox3d_authoring_object_set_selection_mode(
        object, SANDBOX3D_AUTHORING_SELECTION_EDGE);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_select_component(
        object, loop_edge, false) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_select_edge_loop(object) ==
        HENKA_SUCCESS);
    {
        const henka_authoring_vertex* vertex = henka_authoring_mesh_get_vertex(
            sandbox3d_authoring_object_get_mesh(object), loop_vertices[0]);
        HENKA_TEST_ASSERT(vertex != NULL);
        operator_baseline_x = vertex->position.x;
    }
    HENKA_TEST_ASSERT(sandbox3d_modeling_operator_begin(
        &operator_session, object,
        SANDBOX3D_MODELING_OPERATOR_EDGE_SLIDE) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_modeling_operator_preview(
        &operator_session, 0.25f, false, false) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_has_preview(object));
    for (row = 0U; row < 4U; ++row)
    {
        const henka_authoring_vertex* vertex = henka_authoring_mesh_get_vertex(
            sandbox3d_authoring_object_get_mesh(object), loop_vertices[row]);
        HENKA_TEST_ASSERT(vertex != NULL);
        HENKA_TEST_ASSERT(fabsf(vertex->position.x - operator_baseline_x) < 0.0001f);
    }
    HENKA_TEST_ASSERT(sandbox3d_modeling_operator_cancel(&operator_session) ==
        HENKA_SUCCESS);
    HENKA_TEST_ASSERT(!sandbox3d_authoring_object_has_preview(object));
    HENKA_TEST_ASSERT(sandbox3d_modeling_operator_begin(
        &operator_session, object,
        SANDBOX3D_MODELING_OPERATOR_EDGE_SLIDE) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_modeling_operator_numeric_begin(
        &operator_session) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_modeling_operator_numeric_append(
        &operator_session, "-0.25", 5U) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_modeling_operator_numeric_commit(
        &operator_session) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(operator_session.amount, -0.25f, 0.0001f);
    HENKA_TEST_ASSERT(sandbox3d_modeling_operator_commit(&operator_session) ==
        HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_authoring_mesh_validate(
        sandbox3d_authoring_object_get_mesh(object)));
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_undo(object) == HENKA_SUCCESS);
    for (row = 0U; row < 4U; ++row)
    {
        const henka_authoring_vertex* vertex = henka_authoring_mesh_get_vertex(
            sandbox3d_authoring_object_get_mesh(object), loop_vertices[row]);
        HENKA_TEST_ASSERT(vertex != NULL);
        HENKA_TEST_ASSERT(fabsf(vertex->position.x - operator_baseline_x) < 0.0001f);
    }
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_slide_selected_edge_loop(
        object, 0.5f) == HENKA_SUCCESS);
    for (row = 0U; row < 4U; ++row)
    {
        const henka_authoring_vertex* vertex =
            henka_authoring_mesh_get_vertex(
                sandbox3d_authoring_object_get_mesh(object), loop_vertices[row]);
        HENKA_TEST_ASSERT(vertex != NULL);
        HENKA_TEST_ASSERT(fabsf(vertex->position.x - 1.5f) < 0.0001f);
    }
    HENKA_TEST_ASSERT(henka_authoring_mesh_validate(
        sandbox3d_authoring_object_get_mesh(object)));
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_undo(object) == HENKA_SUCCESS);
    for (row = 0U; row < 4U; ++row)
    {
        const henka_authoring_vertex* vertex =
            henka_authoring_mesh_get_vertex(
                sandbox3d_authoring_object_get_mesh(object), loop_vertices[row]);
        HENKA_TEST_ASSERT(vertex != NULL);
        HENKA_TEST_ASSERT(fabsf(vertex->position.x - 1.0f) < 0.0001f);
    }
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_redo(object) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_authoring_mesh_validate(
        sandbox3d_authoring_object_get_mesh(object)));
    sandbox3d_authoring_object_set_selection_mode(
        object, SANDBOX3D_AUTHORING_SELECTION_EDGE);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_select_component(
        object, loop_edge, false) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_select_edge_loop(object) ==
        HENKA_SUCCESS);
    sandbox3d_authoring_object_destroy(object);
    henka_authoring_mesh_destroy(source);
    henka_scene_destroy(scene);
    henka_engine_destroy(engine);
}

static void henka_test_sandbox3d_object_authoring_closed_quad_ring_loop_cut(void)
{
    henka_engine_config config = {0};
    henka_engine* engine = NULL;
    henka_scene* scene = NULL;
    henka_authoring_mesh* source = NULL;
    sandbox3d_authoring_object* object = NULL;
    const henka_authoring_mesh_desc desc = {32U, 64U, 16U, 8U};
    henka_authoring_vertex_id vertices[8];
    henka_authoring_edge_id closed_slide_edge = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_mesh_counts counts;
    henka_entity entity;
    size_t edge_slot;

    config.application_name = "Henka Closed Quad Ring Loop Cut Test";
    config.window_width = 320;
    config.window_height = 240;
    config.enable_vsync = false;
    HENKA_TEST_ASSERT(henka_engine_create(&config, &engine) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_create(&scene) == HENKA_SUCCESS);
    entity = henka_scene_create_entity_named(scene, "Closed Quad Ring Loop Cut");
    HENKA_TEST_ASSERT(entity != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(henka_authoring_mesh_create(&desc, &source) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_test_make_closed_quad_ring(source, vertices, 8U) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_create_from_mesh(
        engine, scene, entity, source, 8U, &object) == HENKA_SUCCESS);
    sandbox3d_authoring_object_set_selection_mode(object, SANDBOX3D_AUTHORING_SELECTION_FACE);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_select_component(object, 1U, false) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_loop_cut_selected_face(object) == HENKA_SUCCESS);
    counts = henka_authoring_mesh_get_counts(sandbox3d_authoring_object_get_mesh(object));
    HENKA_TEST_ASSERT(counts.vertices == 12U && counts.edges == 20U && counts.faces == 8U);
    HENKA_TEST_ASSERT(henka_authoring_mesh_validate(sandbox3d_authoring_object_get_mesh(object)));
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_undo(object) == HENKA_SUCCESS);
    counts = henka_authoring_mesh_get_counts(sandbox3d_authoring_object_get_mesh(object));
    HENKA_TEST_ASSERT(counts.vertices == 8U && counts.edges == 12U && counts.faces == 4U);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_redo(object) == HENKA_SUCCESS);
    counts = henka_authoring_mesh_get_counts(sandbox3d_authoring_object_get_mesh(object));
    HENKA_TEST_ASSERT(counts.vertices == 12U && counts.edges == 20U && counts.faces == 8U);

    for (edge_slot = 0U; edge_slot < desc.max_edges; ++edge_slot)
    {
        henka_authoring_edge_id edge_id;
        const henka_authoring_edge* edge;
        if (henka_authoring_mesh_get_edge_id_at(
                sandbox3d_authoring_object_get_mesh(object), edge_slot, &edge_id) != HENKA_SUCCESS)
        {
            continue;
        }
        edge = henka_authoring_mesh_get_edge(
            sandbox3d_authoring_object_get_mesh(object), edge_id);
        if (edge != NULL && edge->face_count == 2U &&
            edge->vertices[0] >= 9U && edge->vertices[0] <= 12U &&
            edge->vertices[1] >= 9U && edge->vertices[1] <= 12U)
        {
            closed_slide_edge = edge_id;
            break;
        }
    }
    HENKA_TEST_ASSERT(closed_slide_edge != HENKA_AUTHORING_INVALID_ID);
    sandbox3d_authoring_object_set_selection_mode(object, SANDBOX3D_AUTHORING_SELECTION_EDGE);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_select_component(
        object, closed_slide_edge, false) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_select_edge_loop(object) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_get_selected_component_count(object) == 4U);
    counts = henka_authoring_mesh_get_counts(sandbox3d_authoring_object_get_mesh(object));
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_slide_selected_edge_loop(object, 0.5f) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_authoring_mesh_get_counts(
        sandbox3d_authoring_object_get_mesh(object)).vertices == counts.vertices &&
        henka_authoring_mesh_get_counts(sandbox3d_authoring_object_get_mesh(object)).edges == counts.edges &&
        henka_authoring_mesh_get_counts(sandbox3d_authoring_object_get_mesh(object)).faces == counts.faces);
    HENKA_TEST_ASSERT(henka_authoring_mesh_validate(sandbox3d_authoring_object_get_mesh(object)));
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_undo(object) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_redo(object) == HENKA_SUCCESS);

    sandbox3d_authoring_object_destroy(object);
    henka_authoring_mesh_destroy(source);
    henka_scene_destroy(scene);
    henka_engine_destroy(engine);
}

static void henka_test_sandbox3d_object_authoring_interior_edge_bevel(void)
{
    henka_engine_config config = {0};
    henka_engine* engine = NULL;
    henka_scene* scene = NULL;
    henka_authoring_mesh* source = NULL;
    sandbox3d_authoring_object* object = NULL;
    const henka_authoring_mesh_desc desc = {32U, 64U, 16U, 8U};
    const henka_vec3 positions[6] = {
        {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f},
        {-1.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 0.0f}};
    const henka_authoring_vertex_id first_face[] = {1U, 2U, 3U, 4U};
    const henka_authoring_vertex_id second_face[] = {2U, 5U, 6U, 3U};
    henka_authoring_mesh_counts counts;
    henka_authoring_edge_id shared_edge = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_edge_id edge_id;
    sandbox3d_modeling_operator_session operator_session = {0};
    henka_entity entity;
    size_t index;

    config.application_name = "Henka Interior Edge Bevel Test";
    config.window_width = 320;
    config.window_height = 240;
    config.enable_vsync = false;
    HENKA_TEST_ASSERT(henka_engine_create(&config, &engine) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_create(&scene) == HENKA_SUCCESS);
    entity = henka_scene_create_entity_named(scene, "Interior Edge Bevel Patch");
    HENKA_TEST_ASSERT(entity != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(henka_authoring_mesh_create(&desc, &source) == HENKA_SUCCESS);
    for (index = 0U; index < 6U; ++index)
    {
        HENKA_TEST_ASSERT(henka_authoring_mesh_add_vertex(
            source, positions[index], (henka_vec2){0.0f, 0.0f}, 0U,
            &(henka_authoring_vertex_id){0U}) == HENKA_SUCCESS);
    }
    HENKA_TEST_ASSERT(henka_authoring_mesh_add_face(
        source, first_face, 4U, 0U, true, &(henka_authoring_face_id){0U}) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_authoring_mesh_add_face(
        source, second_face, 4U, 0U, true, &(henka_authoring_face_id){0U}) == HENKA_SUCCESS);
    for (index = 0U; index < henka_authoring_mesh_get_desc(source).max_edges; ++index)
    {
        if (henka_authoring_mesh_get_edge_id_at(source, index, &edge_id) != HENKA_SUCCESS)
        {
            continue;
        }
        {
            const henka_authoring_edge* edge = henka_authoring_mesh_get_edge(source, edge_id);
            if (edge != NULL && edge->face_count == 2U &&
                ((edge->vertices[0] == 2U && edge->vertices[1] == 3U) ||
                 (edge->vertices[0] == 3U && edge->vertices[1] == 2U)))
            {
                shared_edge = edge_id;
                break;
            }
        }
    }
    HENKA_TEST_ASSERT(shared_edge != HENKA_AUTHORING_INVALID_ID);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_create_from_mesh(
        engine, scene, entity, source, 8U, &object) == HENKA_SUCCESS);
    sandbox3d_authoring_object_set_selection_mode(object, SANDBOX3D_AUTHORING_SELECTION_EDGE);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_select_component(
        object, shared_edge, false) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_set_bevel_width(object, 0.2f) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_bevel_selected_edge(object) == HENKA_SUCCESS);
    counts = henka_authoring_mesh_get_counts(sandbox3d_authoring_object_get_mesh(object));
    HENKA_TEST_ASSERT(counts.vertices == 8U && counts.edges == 10U && counts.faces == 3U);
    HENKA_TEST_ASSERT(henka_authoring_mesh_validate(sandbox3d_authoring_object_get_mesh(object)));
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_undo(object) == HENKA_SUCCESS);
    counts = henka_authoring_mesh_get_counts(sandbox3d_authoring_object_get_mesh(object));
    HENKA_TEST_ASSERT(counts.vertices == 6U && counts.edges == 7U && counts.faces == 2U);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_redo(object) == HENKA_SUCCESS);
    counts = henka_authoring_mesh_get_counts(sandbox3d_authoring_object_get_mesh(object));
    HENKA_TEST_ASSERT(counts.vertices == 8U && counts.edges == 10U && counts.faces == 3U);

    HENKA_TEST_ASSERT(sandbox3d_authoring_object_undo(object) == HENKA_SUCCESS);
    sandbox3d_authoring_object_set_selection_mode(object, SANDBOX3D_AUTHORING_SELECTION_EDGE);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_select_component(
        object, shared_edge, false) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_modeling_operator_begin(
        &operator_session, object, SANDBOX3D_MODELING_OPERATOR_BEVEL) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_modeling_operator_preview(
        &operator_session, 0.2f, false, false) == HENKA_SUCCESS);
    counts = henka_authoring_mesh_get_counts(sandbox3d_authoring_object_get_mesh(object));
    HENKA_TEST_ASSERT(counts.vertices == 6U && counts.edges == 7U && counts.faces == 2U);
    HENKA_TEST_ASSERT(sandbox3d_modeling_operator_cancel(&operator_session) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_modeling_operator_begin(
        &operator_session, object, SANDBOX3D_MODELING_OPERATOR_BEVEL) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_modeling_operator_preview(
        &operator_session, 0.2f, false, false) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_modeling_operator_commit(&operator_session) == HENKA_SUCCESS);
    counts = henka_authoring_mesh_get_counts(sandbox3d_authoring_object_get_mesh(object));
    HENKA_TEST_ASSERT(counts.vertices == 8U && counts.edges == 10U && counts.faces == 3U);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_undo(object) == HENKA_SUCCESS);

    sandbox3d_authoring_object_destroy(object);
    henka_authoring_mesh_destroy(source);
    henka_scene_destroy(scene);
    henka_engine_destroy(engine);
}

static void henka_test_sandbox3d_object_authoring_vertex_extrude(void)
{
    henka_engine_config config = {0};
    henka_engine* engine = NULL;
    henka_scene* scene = NULL;
    henka_authoring_mesh* source = NULL;
    sandbox3d_authoring_object* object = NULL;
    const henka_authoring_mesh_desc desc = {16U, 32U, 16U, 8U};
    henka_authoring_mesh_counts counts;
    henka_entity entity;

    config.application_name = "Henka Vertex Extrude Test";
    config.window_width = 320;
    config.window_height = 240;
    config.enable_vsync = false;
    HENKA_TEST_ASSERT(henka_engine_create(&config, &engine) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_create(&scene) == HENKA_SUCCESS);
    entity = henka_scene_create_entity_named(scene, "Vertex Extrude Plane");
    HENKA_TEST_ASSERT(entity != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(henka_authoring_mesh_create_plane(&desc, 2.0f, 2.0f, &source) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_create_from_mesh(
        engine, scene, entity, source, 8U, &object) == HENKA_SUCCESS);
    sandbox3d_authoring_object_set_selection_mode(object, SANDBOX3D_AUTHORING_SELECTION_VERTEX);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_select_component(object, 1U, false) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_extrude_selected_vertex(object, 0.5f) == HENKA_SUCCESS);
    counts = henka_authoring_mesh_get_counts(sandbox3d_authoring_object_get_mesh(object));
    HENKA_TEST_ASSERT(counts.vertices == 5U && counts.edges == 7U && counts.faces == 3U);
    HENKA_TEST_ASSERT(henka_authoring_mesh_validate(sandbox3d_authoring_object_get_mesh(object)));
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_get_active_component_id(object) == 5U);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_undo(object) == HENKA_SUCCESS);
    counts = henka_authoring_mesh_get_counts(sandbox3d_authoring_object_get_mesh(object));
    HENKA_TEST_ASSERT(counts.vertices == 4U && counts.edges == 4U && counts.faces == 1U);
    HENKA_TEST_ASSERT(sandbox3d_authoring_object_redo(object) == HENKA_SUCCESS);
    counts = henka_authoring_mesh_get_counts(sandbox3d_authoring_object_get_mesh(object));
    HENKA_TEST_ASSERT(counts.vertices == 5U && counts.edges == 7U && counts.faces == 3U);

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
    henka_test_sandbox3d_modeling_operator_session();
    henka_test_sandbox3d_modeling_operator_loose_extrude();
    henka_test_sandbox3d_modeling_operator_surface_vertex_extrude();
    henka_test_sandbox3d_modeling_operator_boundary_edge_extrude();
    henka_test_sandbox3d_loose_renderer_bridge();
    henka_test_sandbox3d_loose_component_creation();
    henka_test_sandbox3d_object_authoring_quad_recovery_workflow();
    henka_test_sandbox3d_object_authoring_duplicate();
    henka_test_sandbox3d_object_authoring_source_persistence();
    henka_test_sandbox3d_object_authoring_clone_bridge();
    henka_test_sandbox3d_object_authoring_model_primitive_bridge();
    henka_test_sandbox3d_object_authoring_real_obj_import_bridge();
    henka_test_sandbox3d_object_authoring_component_selection();
    henka_test_sandbox3d_object_authoring_hover_query_preserves_selection();
    henka_test_sandbox3d_object_authoring_edge_ring_exact_grid();
    henka_test_sandbox3d_object_authoring_edge_loop_exact_grid();
    henka_test_sandbox3d_object_authoring_edge_dissolve();
    henka_test_sandbox3d_object_authoring_edge_delete();
    henka_test_sandbox3d_object_authoring_edge_bevel();
    henka_test_sandbox3d_object_authoring_multi_edge_bevel();
    henka_test_sandbox3d_object_authoring_loop_cut();
    henka_test_sandbox3d_object_authoring_quad_strip_loop_cut();
    henka_test_sandbox3d_object_authoring_closed_quad_ring_loop_cut();
    henka_test_sandbox3d_object_authoring_edge_loop_slide();
    henka_test_sandbox3d_object_authoring_interior_edge_bevel();
    henka_test_sandbox3d_object_authoring_vertex_extrude();
    henka_test_sandbox3d_object_authoring_scalable_selection();
    henka_test_sandbox3d_object_authoring_connected_scalable_selection();
}
