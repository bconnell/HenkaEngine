#include "object_authoring_tools.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

#include <henka/memory.h>
#include <henka/model.h>
#include <henka/persistence.h>

#define SANDBOX3D_AUTHORING_MAX_HISTORY_STEPS 64U
#define SANDBOX3D_AUTHORING_MAX_SELECTED_COMPONENTS 64U

struct sandbox3d_authoring_object
{
    henka_engine* engine;
    henka_scene* scene;
    henka_entity entity;
    henka_authoring_mesh* mesh;
    henka_authoring_mesh_history* history;
    size_t history_steps;
    henka_mesh* render_mesh;
    henka_mesh* previous_scene_mesh;
    bool had_previous_bounds;
    henka_bounds previous_bounds;
    henka_physics_world* physics_world;
    henka_physics_body_id physics_body;
    henka_authoring_face_id selected_face;
    henka_authoring_face_id* selection_history;
    size_t selection_history_capacity;
    size_t selection_history_count;
    size_t selection_history_cursor;
    sandbox3d_authoring_selection_mode selection_mode;
    uint32_t selected_vertices[SANDBOX3D_AUTHORING_MAX_SELECTED_COMPONENTS];
    uint32_t selected_edges[SANDBOX3D_AUTHORING_MAX_SELECTED_COMPONENTS];
    uint32_t selected_faces[SANDBOX3D_AUTHORING_MAX_SELECTED_COMPONENTS];
    size_t selected_vertex_count;
    size_t selected_edge_count;
    size_t selected_face_count;
    char* source_path;
};

static henka_result sandbox3d_authoring_set_source_path(
    sandbox3d_authoring_object* object,
    const char* path)
{
    char* copy;
    size_t length;

    if (object == NULL || path == NULL || path[0] == '\0')
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    length = strlen(path);
    if (length > 767U)
    {
        return HENKA_ERROR_LIMIT;
    }
    copy = henka_calloc(length + 1U, sizeof(*copy));
    if (copy == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    memcpy(copy, path, length + 1U);
    henka_free(object->source_path);
    object->source_path = copy;
    return HENKA_SUCCESS;
}

static henka_result sandbox3d_authoring_set_physics_bounds(
    sandbox3d_authoring_object* object,
    const henka_bounds* bounds)
{
    henka_physics_body_state body_state;

    if (object == NULL || bounds == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (object->physics_world == NULL || object->physics_body == HENKA_INVALID_PHYSICS_BODY_ID)
    {
        return HENKA_SUCCESS;
    }
    if (henka_physics_body_get_state(
            object->physics_world, object->physics_body, &body_state) != HENKA_SUCCESS ||
        body_state.collider.shape != HENKA_PHYSICS_SHAPE_BOX)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    body_state.collider.offset = bounds->center;
    body_state.collider.data.box.half_extents = bounds->extents;
    return henka_physics_body_set_collider(
        object->physics_world, object->physics_body, body_state.collider);
}

static henka_result sandbox3d_authoring_get_physics_collider(
    const sandbox3d_authoring_object* object,
    henka_physics_collider_desc* out_collider)
{
    henka_physics_body_state body_state;

    if (object == NULL || out_collider == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (object->physics_world == NULL || object->physics_body == HENKA_INVALID_PHYSICS_BODY_ID)
    {
        *out_collider = (henka_physics_collider_desc){0};
        return HENKA_SUCCESS;
    }
    if (henka_physics_body_get_state(
            object->physics_world, object->physics_body, &body_state) != HENKA_SUCCESS ||
        body_state.collider.shape != HENKA_PHYSICS_SHAPE_BOX)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_collider = body_state.collider;
    return HENKA_SUCCESS;
}

static void sandbox3d_authoring_restore_physics_collider(
    const sandbox3d_authoring_object* object,
    const henka_physics_collider_desc* collider)
{
    if (object != NULL && collider != NULL && object->physics_world != NULL &&
        object->physics_body != HENKA_INVALID_PHYSICS_BODY_ID)
    {
        (void)henka_physics_body_set_collider(
            object->physics_world, object->physics_body, *collider);
    }
}

static bool sandbox3d_authoring_finite_vec3(henka_vec3 value)
{
    return isfinite(value.x) && isfinite(value.y) && isfinite(value.z);
}

static henka_result sandbox3d_authoring_evaluate_render(
    sandbox3d_authoring_object* object,
    const henka_authoring_mesh* source,
    henka_mesh** out_render_mesh,
    henka_bounds* out_bounds)
{
    henka_authoring_render_vertex* render_vertices = NULL;
    uint32_t* render_indices = NULL;
    henka_model_vertex* model_vertices = NULL;
    uint32_t* model_indices = NULL;
    henka_authoring_render_data render;
    henka_model_data model;
    size_t vertex_count = 0U;
    size_t index_count = 0U;
    size_t face_id;
    size_t vertex_index;
    size_t index;
    henka_vec3 minimum = {0.0f, 0.0f, 0.0f};
    henka_vec3 maximum = {0.0f, 0.0f, 0.0f};
    henka_result result;

    if (object == NULL || source == NULL || out_render_mesh == NULL || out_bounds == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    memset(&model, 0, sizeof(model));
    *out_render_mesh = NULL;
    if (!henka_authoring_mesh_validate(source))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    for (face_id = 1U; face_id <= HENKA_AUTHORING_MESH_HARD_MAX_FACES; ++face_id)
    {
        const henka_authoring_face* face = henka_authoring_mesh_get_face(
            source, (henka_authoring_face_id)face_id);
        if (face == NULL)
        {
            continue;
        }
        if (face->corner_count < 3U || face->corner_count > HENKA_AUTHORING_MESH_HARD_MAX_FACE_CORNERS)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        vertex_count += face->corner_count;
        index_count += (face->corner_count - 2U) * 3U;
    }
    if (vertex_count == 0U ||
        vertex_count > (size_t)HENKA_AUTHORING_MESH_HARD_MAX_FACES * HENKA_AUTHORING_MESH_HARD_MAX_FACE_CORNERS ||
        index_count == 0U ||
        index_count > (size_t)HENKA_AUTHORING_MESH_HARD_MAX_FACES *
            (HENKA_AUTHORING_MESH_HARD_MAX_FACE_CORNERS - 2U) * 3U ||
        vertex_count > UINT32_MAX || index_count > UINT32_MAX)
    {
        return HENKA_ERROR_LIMIT;
    }

    render_vertices = henka_calloc(vertex_count, sizeof(*render_vertices));
    render_indices = henka_calloc(index_count, sizeof(*render_indices));
    model_vertices = henka_calloc(vertex_count, sizeof(*model_vertices));
    model_indices = henka_calloc(index_count, sizeof(*model_indices));
    if (render_vertices == NULL || render_indices == NULL ||
        model_vertices == NULL || model_indices == NULL)
    {
        result = HENKA_ERROR_OUT_OF_MEMORY;
        goto cleanup;
    }

    render = (henka_authoring_render_data){
        render_vertices, vertex_count, 0U, render_indices, index_count, 0U};
    result = henka_authoring_mesh_evaluate(source, &render);
    if (result != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    for (vertex_index = 0U; vertex_index < render.vertex_count; ++vertex_index)
    {
        const henka_authoring_render_vertex* vertex = &render.vertices[vertex_index];
        if (!sandbox3d_authoring_finite_vec3(vertex->position) ||
            !sandbox3d_authoring_finite_vec3(vertex->normal) ||
            !isfinite(vertex->uv.x) || !isfinite(vertex->uv.y))
        {
            result = HENKA_ERROR_INVALID_ARGUMENT;
            goto cleanup;
        }
        if (vertex_index == 0U)
        {
            minimum = vertex->position;
            maximum = vertex->position;
        }
        else
        {
            minimum.x = fminf(minimum.x, vertex->position.x);
            minimum.y = fminf(minimum.y, vertex->position.y);
            minimum.z = fminf(minimum.z, vertex->position.z);
            maximum.x = fmaxf(maximum.x, vertex->position.x);
            maximum.y = fmaxf(maximum.y, vertex->position.y);
            maximum.z = fmaxf(maximum.z, vertex->position.z);
        }
        model_vertices[vertex_index].position = vertex->position;
        model_vertices[vertex_index].normal = vertex->normal;
        model_vertices[vertex_index].uv = vertex->uv;
        model_vertices[vertex_index].color = (henka_vec4){1.0f, 1.0f, 1.0f, 1.0f};
        model_vertices[vertex_index].tangent = vertex->tangent;
        model_vertices[vertex_index].tangent_valid = false;
        model_vertices[vertex_index].material_region = vertex->material_region;
    }
    for (index = 0U; index < render.index_count; ++index)
    {
        if (render.indices[index] >= render.vertex_count)
        {
            result = HENKA_ERROR_INVALID_ARGUMENT;
            goto cleanup;
        }
        model_indices[index] = render.indices[index];
    }
    model.vertices = model_vertices;
    model.vertex_count = (uint32_t)render.vertex_count;
    model.indices = model_indices;
    model.index_count = (uint32_t)render.index_count;
    result = henka_mesh_create_from_model_data(object->engine, &model, out_render_mesh);
    if (result == HENKA_SUCCESS)
    {
        out_bounds->center = (henka_vec3){
            (minimum.x + maximum.x) * 0.5f,
            (minimum.y + maximum.y) * 0.5f,
            (minimum.z + maximum.z) * 0.5f};
        out_bounds->extents = (henka_vec3){
            (maximum.x - minimum.x) * 0.5f,
            (maximum.y - minimum.y) * 0.5f,
            (maximum.z - minimum.z) * 0.5f};
    }

cleanup:
    henka_free(model_indices);
    henka_free(model_vertices);
    henka_free(render_indices);
    henka_free(render_vertices);
    return result;
}

static void sandbox3d_authoring_repair_selection(sandbox3d_authoring_object* object)
{
    size_t face_id;
    size_t selected_index;
    if (object == NULL)
    {
        return;
    }
    if (henka_authoring_mesh_get_face(object->mesh, object->selected_face) == NULL)
    {
        object->selected_face = HENKA_AUTHORING_INVALID_ID;
        for (face_id = 1U; face_id <= HENKA_AUTHORING_MESH_HARD_MAX_FACES; ++face_id)
        {
            if (henka_authoring_mesh_get_face(object->mesh, (henka_authoring_face_id)face_id) != NULL)
            {
                object->selected_face = (henka_authoring_face_id)face_id;
                break;
            }
        }
    }
    if (object->selection_mode != SANDBOX3D_AUTHORING_SELECTION_FACE ||
        object->selected_face == HENKA_AUTHORING_INVALID_ID)
    {
        return;
    }
    for (selected_index = 0U; selected_index < object->selected_face_count; ++selected_index)
    {
        if (object->selected_faces[selected_index] == object->selected_face)
        {
            return;
        }
    }
    if (object->selected_face_count < SANDBOX3D_AUTHORING_MAX_SELECTED_COMPONENTS)
    {
        object->selected_faces[object->selected_face_count++] = object->selected_face;
    }
    else
    {
        object->selected_faces[object->selected_face_count - 1U] = object->selected_face;
    }
}

static uint32_t* sandbox3d_authoring_selected_ids(
    sandbox3d_authoring_object* object,
    size_t* out_count)
{
    if (out_count != NULL) *out_count = 0U;
    if (object == NULL || out_count == NULL)
    {
        return NULL;
    }
    switch (object->selection_mode)
    {
        case SANDBOX3D_AUTHORING_SELECTION_VERTEX:
            *out_count = object->selected_vertex_count;
            return object->selected_vertices;
        case SANDBOX3D_AUTHORING_SELECTION_EDGE:
            *out_count = object->selected_edge_count;
            return object->selected_edges;
        case SANDBOX3D_AUTHORING_SELECTION_FACE:
        default:
            *out_count = object->selected_face_count;
            return object->selected_faces;
    }
}

static const uint32_t* sandbox3d_authoring_selected_ids_const(
    const sandbox3d_authoring_object* object,
    size_t* out_count)
{
    return sandbox3d_authoring_selected_ids((sandbox3d_authoring_object*)object, out_count);
}

static bool sandbox3d_authoring_component_selected(
    const sandbox3d_authoring_object* object,
    uint32_t id)
{
    size_t count;
    size_t index;
    const uint32_t* ids = sandbox3d_authoring_selected_ids_const(object, &count);
    for (index = 0U; ids != NULL && index < count; ++index)
    {
        if (ids[index] == id) return true;
    }
    return false;
}

static void sandbox3d_authoring_remove_invalid_component_selection(
    sandbox3d_authoring_object* object)
{
    uint32_t* ids;
    size_t* count;
    size_t index;
    size_t write_index = 0U;
    if (object == NULL) return;
    ids = sandbox3d_authoring_selected_ids(object, &index);
    if (object->selection_mode == SANDBOX3D_AUTHORING_SELECTION_VERTEX)
        count = &object->selected_vertex_count;
    else if (object->selection_mode == SANDBOX3D_AUTHORING_SELECTION_EDGE)
        count = &object->selected_edge_count;
    else
        count = &object->selected_face_count;
    for (index = 0U; ids != NULL && index < *count; ++index)
    {
        const bool valid = object->selection_mode == SANDBOX3D_AUTHORING_SELECTION_VERTEX
            ? henka_authoring_mesh_get_vertex(object->mesh, ids[index]) != NULL
            : object->selection_mode == SANDBOX3D_AUTHORING_SELECTION_EDGE
                ? henka_authoring_mesh_get_edge(object->mesh, ids[index]) != NULL
                : henka_authoring_mesh_get_face(object->mesh, ids[index]) != NULL;
        if (valid) ids[write_index++] = ids[index];
    }
    *count = write_index;
    if (object->selection_mode == SANDBOX3D_AUTHORING_SELECTION_FACE)
    {
        object->selected_face = write_index > 0U
            ? (henka_authoring_face_id)ids[write_index - 1U]
            : HENKA_AUTHORING_INVALID_ID;
    }
}

static void sandbox3d_authoring_reset_selection_history(sandbox3d_authoring_object* object)
{
    if (object == NULL || object->selection_history == NULL || object->selection_history_capacity == 0U)
    {
        return;
    }
    object->selection_history_count = 1U;
    object->selection_history_cursor = 0U;
    object->selection_history[0] = object->selected_face;
}

static void sandbox3d_authoring_commit_selection_history(
    sandbox3d_authoring_object* object,
    henka_authoring_face_id selected_before,
    henka_authoring_face_id selected_after)
{
    if (object == NULL || object->selection_history == NULL || object->selection_history_capacity == 0U)
    {
        return;
    }
    if (object->selection_history_capacity == 1U)
    {
        object->selection_history[0] = selected_after;
        object->selection_history_count = 1U;
        object->selection_history_cursor = 0U;
        return;
    }
    object->selection_history[object->selection_history_cursor] = selected_before;
    object->selection_history_count = object->selection_history_cursor + 1U;
    if (object->selection_history_count >= object->selection_history_capacity)
    {
        memmove(
            object->selection_history,
            object->selection_history + 1U,
            (object->selection_history_count - 1U) * sizeof(*object->selection_history));
        --object->selection_history_count;
        if (object->selection_history_cursor > 0U)
        {
            --object->selection_history_cursor;
        }
    }
    ++object->selection_history_cursor;
    object->selection_history[object->selection_history_cursor] = selected_after;
    object->selection_history_count = object->selection_history_cursor + 1U;
}

static void sandbox3d_authoring_restore_selection_history(sandbox3d_authoring_object* object)
{
    if (object == NULL || object->selection_history == NULL ||
        object->selection_history_cursor >= object->selection_history_count)
    {
        return;
    }
    object->selected_face = object->selection_history[object->selection_history_cursor];
    sandbox3d_authoring_repair_selection(object);
    object->selection_history[object->selection_history_cursor] = object->selected_face;
}

static henka_result sandbox3d_authoring_publish_candidate(
    sandbox3d_authoring_object* object,
    henka_authoring_mesh* candidate,
    bool checkpoint_history,
    henka_authoring_face_id selected_after)
{
    henka_mesh* candidate_render = NULL;
    henka_mesh* old_scene_mesh = NULL;
    henka_bounds candidate_bounds = {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}};
    henka_bounds old_bounds;
    henka_physics_collider_desc old_collider = {0};
    bool had_old_bounds;
    bool had_old_collider;
    const henka_authoring_face_id selected_before = object != NULL
        ? object->selected_face
        : HENKA_AUTHORING_INVALID_ID;
    henka_result result;

    if (object == NULL || candidate == NULL ||
        !henka_scene_is_entity_valid(object->scene, object->entity))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = sandbox3d_authoring_evaluate_render(object, candidate,
        &candidate_render, &candidate_bounds);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    if (henka_scene_get_entity_mesh(object->scene, object->entity, &old_scene_mesh) != HENKA_SUCCESS)
    {
        henka_mesh_destroy(candidate_render);
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    had_old_bounds = henka_scene_get_entity_local_bounds(
        object->scene, object->entity, &old_bounds) == HENKA_SUCCESS;
    had_old_collider = sandbox3d_authoring_get_physics_collider(object, &old_collider) == HENKA_SUCCESS &&
        object->physics_world != NULL && object->physics_body != HENKA_INVALID_PHYSICS_BODY_ID;
    result = henka_scene_set_entity_mesh(object->scene, object->entity, candidate_render);
    if (result == HENKA_SUCCESS)
    {
        result = henka_scene_set_entity_local_bounds(object->scene, object->entity, candidate_bounds);
    }
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_authoring_set_physics_bounds(object, &candidate_bounds);
    }
    if (result != HENKA_SUCCESS)
    {
        (void)henka_scene_set_entity_mesh(object->scene, object->entity, old_scene_mesh);
        if (had_old_bounds) (void)henka_scene_set_entity_local_bounds(object->scene, object->entity, old_bounds);
        else (void)henka_scene_clear_entity_local_bounds(object->scene, object->entity);
        if (had_old_collider) sandbox3d_authoring_restore_physics_collider(object, &old_collider);
        henka_mesh_destroy(candidate_render);
        return result;
    }
    if (checkpoint_history)
    {
        result = henka_authoring_mesh_history_checkpoint(object->history, candidate);
        if (result != HENKA_SUCCESS)
        {
            (void)henka_scene_set_entity_mesh(object->scene, object->entity, old_scene_mesh);
            if (had_old_bounds) (void)henka_scene_set_entity_local_bounds(object->scene, object->entity, old_bounds);
            else (void)henka_scene_clear_entity_local_bounds(object->scene, object->entity);
            if (had_old_collider) sandbox3d_authoring_restore_physics_collider(object, &old_collider);
            henka_mesh_destroy(candidate_render);
            return result;
        }
    }
    henka_authoring_mesh_destroy(object->mesh);
    henka_mesh_destroy(object->render_mesh);
    object->mesh = candidate;
    object->render_mesh = candidate_render;
    object->selected_face = selected_after;
    sandbox3d_authoring_repair_selection(object);
    sandbox3d_authoring_remove_invalid_component_selection(object);
    if (checkpoint_history)
    {
        sandbox3d_authoring_commit_selection_history(
            object, selected_before, object->selected_face);
    }
    return HENKA_SUCCESS;
}

static henka_result sandbox3d_authoring_replace_loaded_source(
    sandbox3d_authoring_object* object,
    henka_authoring_mesh* candidate)
{
    henka_authoring_mesh_history* candidate_history = NULL;
    henka_mesh* candidate_render = NULL;
    henka_mesh* old_scene_mesh = NULL;
    henka_bounds candidate_bounds = {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}};
    henka_bounds old_bounds;
    henka_physics_collider_desc old_collider = {0};
    bool had_old_bounds;
    bool had_old_collider;
    henka_result result;

    if (object == NULL || candidate == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_authoring_mesh_history_create(
        candidate, object->history_steps, &candidate_history);
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_authoring_evaluate_render(
            object, candidate, &candidate_render, &candidate_bounds);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_scene_get_entity_mesh(object->scene, object->entity, &old_scene_mesh);
    }
    had_old_bounds = henka_scene_get_entity_local_bounds(
        object->scene, object->entity, &old_bounds) == HENKA_SUCCESS;
    had_old_collider = sandbox3d_authoring_get_physics_collider(object, &old_collider) == HENKA_SUCCESS &&
        object->physics_world != NULL && object->physics_body != HENKA_INVALID_PHYSICS_BODY_ID;
    if (result == HENKA_SUCCESS)
    {
        result = henka_scene_set_entity_mesh(object->scene, object->entity, candidate_render);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_scene_set_entity_local_bounds(object->scene, object->entity, candidate_bounds);
    }
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_authoring_set_physics_bounds(object, &candidate_bounds);
    }
    if (result != HENKA_SUCCESS)
    {
        if (old_scene_mesh != NULL)
        {
            (void)henka_scene_set_entity_mesh(object->scene, object->entity, old_scene_mesh);
        }
        if (had_old_bounds)
        {
            (void)henka_scene_set_entity_local_bounds(object->scene, object->entity, old_bounds);
        }
        else
        {
            (void)henka_scene_clear_entity_local_bounds(object->scene, object->entity);
        }
        if (had_old_collider) sandbox3d_authoring_restore_physics_collider(object, &old_collider);
        henka_mesh_destroy(candidate_render);
        henka_authoring_mesh_history_destroy(candidate_history);
        return result;
    }
    henka_authoring_mesh_destroy(object->mesh);
    henka_authoring_mesh_history_destroy(object->history);
    henka_mesh_destroy(object->render_mesh);
    object->mesh = candidate;
    object->history = candidate_history;
    object->render_mesh = candidate_render;
    sandbox3d_authoring_repair_selection(object);
    sandbox3d_authoring_reset_selection_history(object);
    return HENKA_SUCCESS;
}

size_t sandbox3d_object_authoring_collect_user_entities(
    const henka_scene* scene,
    henka_entity* out_entities,
    size_t capacity)
{
    size_t count;
    size_t index;

    if (scene == NULL)
    {
        return 0U;
    }

    count = 0U;
    for (index = 0U; index < henka_scene_get_entity_count(scene); ++index)
    {
        henka_entity entity = henka_scene_get_entity_at_index(scene, index);
        if (entity == HENKA_INVALID_ENTITY || henka_scene_is_entity_helper(scene, entity))
        {
            continue;
        }

        if (out_entities != NULL && count < capacity)
        {
            out_entities[count] = entity;
        }
        ++count;
    }

    return count;
}

bool sandbox3d_object_authoring_can_edit_entity(
    const henka_scene* scene,
    henka_entity entity)
{
    return scene != NULL &&
        entity != HENKA_INVALID_ENTITY &&
        henka_scene_is_entity_valid(scene, entity) &&
        !henka_scene_is_entity_helper(scene, entity);
}

henka_result sandbox3d_object_authoring_duplicate_entity(
    henka_scene* scene,
    henka_entity source,
    const char* duplicate_name,
    henka_entity* out_duplicate)
{
    henka_entity duplicate;
    henka_transform transform;
    henka_mesh* mesh;
    henka_material material;
    const henka_material_asset* material_asset;
    henka_bounds bounds = {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}};
    henka_interaction_desc interaction;
    uint32_t flags;
    const char* tag;
    henka_result result;
    bool has_bounds;

    if (out_duplicate == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_duplicate = HENKA_INVALID_ENTITY;

    if (!sandbox3d_object_authoring_can_edit_entity(scene, source) ||
        duplicate_name == NULL || duplicate_name[0] == '\0')
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    if (henka_scene_get_entity_transform(scene, source, &transform) != HENKA_SUCCESS ||
        henka_scene_get_entity_mesh(scene, source, &mesh) != HENKA_SUCCESS ||
        henka_scene_get_entity_material(scene, source, &material) != HENKA_SUCCESS ||
        henka_scene_get_entity_material_asset(scene, source, &material_asset) != HENKA_SUCCESS ||
        henka_scene_get_entity_interaction(scene, source, &interaction) != HENKA_SUCCESS ||
        henka_scene_get_entity_flags(scene, source, &flags) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    tag = henka_scene_get_entity_tag(scene, source);
    has_bounds = henka_scene_get_entity_local_bounds(scene, source, &bounds) == HENKA_SUCCESS;
    duplicate = henka_scene_create_entity_named(scene, duplicate_name);
    if (duplicate == HENKA_INVALID_ENTITY)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }

    result = henka_scene_set_entity_transform(scene, duplicate, transform);
    if (result == HENKA_SUCCESS && mesh != NULL)
    {
        result = henka_scene_set_entity_mesh(scene, duplicate, mesh);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_scene_set_entity_material(scene, duplicate, material);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_scene_set_entity_material_asset(scene, duplicate, material_asset);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_scene_set_entity_tag(scene, duplicate, tag);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_scene_set_entity_visible(
            scene,
            duplicate,
            henka_scene_is_entity_visible(scene, source));
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_scene_set_entity_flags(scene, duplicate, flags);
    }
    if (result == HENKA_SUCCESS)
    {
        result = has_bounds
            ? henka_scene_set_entity_local_bounds(scene, duplicate, bounds)
            : henka_scene_clear_entity_local_bounds(scene, duplicate);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_scene_set_entity_interaction(scene, duplicate, &interaction);
    }

    if (result != HENKA_SUCCESS)
    {
        henka_scene_destroy_entity(scene, duplicate);
        return result;
    }

    *out_duplicate = duplicate;
    return HENKA_SUCCESS;
}

static henka_result sandbox3d_authoring_object_create_from_owned_mesh(
    henka_engine* engine,
    henka_scene* scene,
    henka_entity entity,
    henka_authoring_mesh* mesh,
    size_t history_steps,
    sandbox3d_authoring_object** out_object)
{
    sandbox3d_authoring_object* object;
    henka_mesh* old_mesh = NULL;
    henka_bounds bounds = {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}};
    henka_result result;

    if (out_object == NULL || engine == NULL || scene == NULL || mesh == NULL ||
        entity == HENKA_INVALID_ENTITY || !henka_scene_is_entity_valid(scene, entity) ||
        history_steps == 0U || history_steps > SANDBOX3D_AUTHORING_MAX_HISTORY_STEPS)
    {
        henka_authoring_mesh_destroy(mesh);
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_object = NULL;
    object = henka_calloc(1U, sizeof(*object));
    if (object == NULL)
    {
        henka_authoring_mesh_destroy(mesh);
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    object->engine = engine;
    object->scene = scene;
    object->entity = entity;
    object->mesh = mesh;
    object->history_steps = history_steps;
    object->selected_face = 1U;
    object->selection_mode = SANDBOX3D_AUTHORING_SELECTION_FACE;
    object->selected_faces[0] = object->selected_face;
    object->selected_face_count = 1U;
    object->physics_body = HENKA_INVALID_PHYSICS_BODY_ID;
    object->selection_history_capacity = history_steps;
    object->selection_history = henka_calloc(
        object->selection_history_capacity, sizeof(*object->selection_history));
    if (object->selection_history == NULL)
    {
        sandbox3d_authoring_object_destroy(object);
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    sandbox3d_authoring_reset_selection_history(object);
    result = henka_authoring_mesh_history_create(mesh, history_steps, &object->history);
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_authoring_evaluate_render(object, mesh, &object->render_mesh, &bounds);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_scene_get_entity_mesh(scene, entity, &old_mesh);
    }
    if (result == HENKA_SUCCESS)
    {
        object->previous_scene_mesh = old_mesh;
        object->had_previous_bounds = henka_scene_get_entity_local_bounds(
            scene, entity, &object->previous_bounds) == HENKA_SUCCESS;
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_scene_set_entity_mesh(scene, entity, object->render_mesh);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_scene_set_entity_local_bounds(scene, entity, bounds);
    }
    if (result != HENKA_SUCCESS)
    {
        if (object->render_mesh != NULL)
        {
            (void)henka_scene_set_entity_mesh(scene, entity, old_mesh);
        }
        if (object->had_previous_bounds)
        {
            (void)henka_scene_set_entity_local_bounds(scene, entity, object->previous_bounds);
        }
        else
        {
            (void)henka_scene_clear_entity_local_bounds(scene, entity);
        }
        sandbox3d_authoring_object_destroy(object);
        return result;
    }
    *out_object = object;
    return HENKA_SUCCESS;
}

henka_result sandbox3d_authoring_object_create_box(
    henka_engine* engine,
    henka_scene* scene,
    henka_entity entity,
    float width,
    float height,
    float depth,
    const henka_authoring_mesh_desc* mesh_desc,
    size_t history_steps,
    sandbox3d_authoring_object** out_object)
{
    henka_authoring_mesh_desc default_desc;
    henka_authoring_mesh* mesh = NULL;
    henka_result result;

    if (out_object == NULL || engine == NULL || scene == NULL ||
        entity == HENKA_INVALID_ENTITY || !henka_scene_is_entity_valid(scene, entity) ||
        history_steps == 0U || history_steps > SANDBOX3D_AUTHORING_MAX_HISTORY_STEPS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_object = NULL;
    default_desc = henka_authoring_mesh_desc_default();
    result = henka_authoring_mesh_create_box(
        mesh_desc == NULL ? &default_desc : mesh_desc, width, height, depth, &mesh);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    return sandbox3d_authoring_object_create_from_owned_mesh(
        engine, scene, entity, mesh, history_steps, out_object);
}

henka_result sandbox3d_authoring_object_create_from_mesh(
    henka_engine* engine,
    henka_scene* scene,
    henka_entity entity,
    const henka_authoring_mesh* source,
    size_t history_steps,
    sandbox3d_authoring_object** out_object)
{
    henka_authoring_mesh* mesh = NULL;
    henka_result result;

    if (out_object == NULL || source == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_object = NULL;
    result = henka_authoring_mesh_clone(source, &mesh);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    result = sandbox3d_authoring_object_create_from_owned_mesh(
        engine, scene, entity, mesh, history_steps, out_object);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    return HENKA_SUCCESS;
}

henka_result sandbox3d_authoring_object_create_from_model_primitive(
    henka_engine* engine,
    henka_scene* scene,
    henka_entity entity,
    const henka_model_scene_primitive* primitive,
    size_t history_steps,
    sandbox3d_authoring_object** out_object)
{
    henka_authoring_mesh_desc desc;
    henka_authoring_mesh* mesh = NULL;
    henka_authoring_vertex_id* vertex_ids = NULL;
    size_t face_count;
    size_t vertex_index;
    size_t index_offset;
    henka_result result;

    if (out_object != NULL) *out_object = NULL;
    if (primitive == NULL || primitive->vertices == NULL || primitive->indices == NULL ||
        primitive->vertex_count < 3U || primitive->index_count < 3U ||
        primitive->index_count % 3U != 0U ||
        primitive->vertex_count > HENKA_AUTHORING_MESH_HARD_MAX_VERTICES)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    face_count = primitive->index_count / 3U;
    if (face_count > HENKA_AUTHORING_MESH_HARD_MAX_FACES ||
        face_count > HENKA_AUTHORING_MESH_HARD_MAX_EDGES / 3U)
    {
        return HENKA_ERROR_LIMIT;
    }
    desc = (henka_authoring_mesh_desc){
        primitive->vertex_count,
        face_count * 3U,
        face_count,
        3U};
    result = henka_authoring_mesh_create(&desc, &mesh);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    vertex_ids = henka_calloc(primitive->vertex_count, sizeof(*vertex_ids));
    if (vertex_ids == NULL)
    {
        henka_authoring_mesh_destroy(mesh);
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    for (vertex_index = 0U; vertex_index < primitive->vertex_count; ++vertex_index)
    {
        const henka_model_vertex* source = &primitive->vertices[vertex_index];
        result = henka_authoring_mesh_add_vertex(
            mesh,
            source->position,
            source->uv,
            source->material_region,
            &vertex_ids[vertex_index]);
        if (result != HENKA_SUCCESS)
        {
            henka_free(vertex_ids);
            henka_authoring_mesh_destroy(mesh);
            return result;
        }
    }
    for (index_offset = 0U; index_offset < primitive->index_count; index_offset += 3U)
    {
        const uint32_t first = primitive->indices[index_offset];
        const uint32_t second = primitive->indices[index_offset + 1U];
        const uint32_t third = primitive->indices[index_offset + 2U];
        henka_authoring_vertex_id face_vertices[3];
        henka_authoring_face_id face_id;

        if (first >= primitive->vertex_count || second >= primitive->vertex_count ||
            third >= primitive->vertex_count || first == second || first == third || second == third)
        {
            henka_free(vertex_ids);
            henka_authoring_mesh_destroy(mesh);
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        face_vertices[0] = vertex_ids[first];
        face_vertices[1] = vertex_ids[second];
        face_vertices[2] = vertex_ids[third];
        result = henka_authoring_mesh_add_face(
            mesh,
            face_vertices,
            3U,
            primitive->vertices[first].material_region,
            true,
            &face_id);
        if (result != HENKA_SUCCESS)
        {
            henka_free(vertex_ids);
            henka_authoring_mesh_destroy(mesh);
            return result;
        }
    }
    henka_free(vertex_ids);
    if (!henka_authoring_mesh_validate(mesh))
    {
        henka_authoring_mesh_destroy(mesh);
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return sandbox3d_authoring_object_create_from_owned_mesh(
        engine, scene, entity, mesh, history_steps, out_object);
}

void sandbox3d_authoring_object_destroy(sandbox3d_authoring_object* object)
{
    if (object == NULL)
    {
        return;
    }
    if (object->scene != NULL &&
        henka_scene_is_entity_valid(object->scene, object->entity))
    {
        henka_mesh* scene_mesh = NULL;
        if (henka_scene_get_entity_mesh(object->scene, object->entity, &scene_mesh) == HENKA_SUCCESS &&
            scene_mesh == object->render_mesh)
        {
            if (object->previous_scene_mesh != NULL)
            {
                (void)henka_scene_set_entity_mesh(
                    object->scene, object->entity, object->previous_scene_mesh);
            }
            else
            {
                (void)henka_scene_clear_entity_mesh(object->scene, object->entity);
            }
            if (object->had_previous_bounds)
            {
                (void)henka_scene_set_entity_local_bounds(
                    object->scene, object->entity, object->previous_bounds);
            }
            else
            {
                (void)henka_scene_clear_entity_local_bounds(object->scene, object->entity);
            }
        }
    }
    henka_mesh_destroy(object->render_mesh);
    henka_authoring_mesh_history_destroy(object->history);
    henka_authoring_mesh_destroy(object->mesh);
    henka_free(object->selection_history);
    henka_free(object->source_path);
    henka_free(object);
}

henka_result sandbox3d_authoring_object_bind_physics(
    sandbox3d_authoring_object* object,
    henka_physics_world* world,
    henka_physics_body_id body)
{
    henka_physics_body_state body_state;
    henka_bounds bounds;

    if (object == NULL || world == NULL || body == HENKA_INVALID_PHYSICS_BODY_ID ||
        henka_scene_get_entity_local_bounds(object->scene, object->entity, &bounds) != HENKA_SUCCESS ||
        henka_physics_body_get_state(world, body, &body_state) != HENKA_SUCCESS ||
        body_state.collider.shape != HENKA_PHYSICS_SHAPE_BOX)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    body_state.collider.offset = bounds.center;
    body_state.collider.data.box.half_extents = bounds.extents;
    if (henka_physics_body_set_collider(world, body, body_state.collider) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    object->physics_world = world;
    object->physics_body = body;
    return HENKA_SUCCESS;
}

void sandbox3d_authoring_object_unbind_physics(sandbox3d_authoring_object* object)
{
    if (object != NULL)
    {
        object->physics_world = NULL;
        object->physics_body = HENKA_INVALID_PHYSICS_BODY_ID;
    }
}

henka_entity sandbox3d_authoring_object_get_entity(const sandbox3d_authoring_object* object)
{
    return object == NULL ? HENKA_INVALID_ENTITY : object->entity;
}

const henka_authoring_mesh* sandbox3d_authoring_object_get_mesh(const sandbox3d_authoring_object* object)
{
    return object == NULL ? NULL : object->mesh;
}

henka_result sandbox3d_authoring_object_get_render_material_region_range(
    const sandbox3d_authoring_object* object,
    uint32_t* out_min_region,
    uint32_t* out_max_region)
{
    if (object == NULL || object->render_mesh == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return henka_mesh_get_material_region_range(
        object->render_mesh, out_min_region, out_max_region);
}

henka_authoring_face_id sandbox3d_authoring_object_get_selected_face(const sandbox3d_authoring_object* object)
{
    return object == NULL ? HENKA_AUTHORING_INVALID_ID : object->selected_face;
}

void sandbox3d_authoring_object_set_selection_mode(
    sandbox3d_authoring_object* object,
    sandbox3d_authoring_selection_mode mode)
{
    if (object == NULL || mode < SANDBOX3D_AUTHORING_SELECTION_VERTEX ||
        mode > SANDBOX3D_AUTHORING_SELECTION_FACE)
    {
        return;
    }
    object->selection_mode = mode;
}

sandbox3d_authoring_selection_mode sandbox3d_authoring_object_get_selection_mode(
    const sandbox3d_authoring_object* object)
{
    return object == NULL ? SANDBOX3D_AUTHORING_SELECTION_FACE : object->selection_mode;
}

void sandbox3d_authoring_object_clear_component_selection(
    sandbox3d_authoring_object* object)
{
    uint32_t* ids = sandbox3d_authoring_selected_ids(object, &(size_t){0U});
    if (ids == NULL || object == NULL) return;
    if (object->selection_mode == SANDBOX3D_AUTHORING_SELECTION_VERTEX)
        object->selected_vertex_count = 0U;
    else if (object->selection_mode == SANDBOX3D_AUTHORING_SELECTION_EDGE)
        object->selected_edge_count = 0U;
    else
    {
        object->selected_face_count = 0U;
        object->selected_face = HENKA_AUTHORING_INVALID_ID;
    }
}

size_t sandbox3d_authoring_object_get_selected_component_count(
    const sandbox3d_authoring_object* object)
{
    size_t count = 0U;
    (void)sandbox3d_authoring_selected_ids_const(object, &count);
    return count;
}

henka_result sandbox3d_authoring_object_get_selected_component_at(
    const sandbox3d_authoring_object* object,
    size_t ordinal,
    uint32_t* out_id)
{
    size_t count;
    const uint32_t* ids;
    if (out_id != NULL) *out_id = HENKA_AUTHORING_INVALID_ID;
    ids = sandbox3d_authoring_selected_ids_const(object, &count);
    if (ids == NULL || out_id == NULL || ordinal >= count)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_id = ids[ordinal];
    return HENKA_SUCCESS;
}

henka_result sandbox3d_authoring_object_select_component(
    sandbox3d_authoring_object* object,
    uint32_t component_id,
    bool additive)
{
    uint32_t* ids;
    size_t* count;
    if (object == NULL || component_id == HENKA_AUTHORING_INVALID_ID)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (object->selection_mode == SANDBOX3D_AUTHORING_SELECTION_VERTEX)
    {
        if (henka_authoring_mesh_get_vertex(object->mesh, component_id) == NULL) return HENKA_ERROR_INVALID_ARGUMENT;
        ids = object->selected_vertices; count = &object->selected_vertex_count;
    }
    else if (object->selection_mode == SANDBOX3D_AUTHORING_SELECTION_EDGE)
    {
        if (henka_authoring_mesh_get_edge(object->mesh, component_id) == NULL) return HENKA_ERROR_INVALID_ARGUMENT;
        ids = object->selected_edges; count = &object->selected_edge_count;
    }
    else
    {
        if (henka_authoring_mesh_get_face(object->mesh, component_id) == NULL) return HENKA_ERROR_INVALID_ARGUMENT;
        ids = object->selected_faces; count = &object->selected_face_count;
    }
    if (!additive) *count = 0U;
    if (!sandbox3d_authoring_component_selected(object, component_id))
    {
        if (*count >= SANDBOX3D_AUTHORING_MAX_SELECTED_COMPONENTS) return HENKA_ERROR_LIMIT;
        ids[(*count)++] = component_id;
    }
    if (object->selection_mode == SANDBOX3D_AUTHORING_SELECTION_FACE)
    {
        object->selected_face = (henka_authoring_face_id)component_id;
    }
    return HENKA_SUCCESS;
}

static henka_vec3 sandbox3d_authoring_transform_point(
    henka_transform transform,
    henka_vec3 point)
{
    point.x *= transform.scale.x;
    point.y *= transform.scale.y;
    point.z *= transform.scale.z;
    return henka_vec3_add(transform.position, henka_quat_rotate_vec3(transform.rotation, point));
}

static bool sandbox3d_authoring_ray_triangle(
    henka_ray ray,
    henka_vec3 first,
    henka_vec3 second,
    henka_vec3 third,
    float maximum_distance,
    float* out_distance)
{
    const henka_vec3 edge_one = henka_vec3_subtract(second, first);
    const henka_vec3 edge_two = henka_vec3_subtract(third, first);
    const henka_vec3 cross_direction = henka_vec3_cross(ray.direction, edge_two);
    const float determinant = henka_vec3_dot(edge_one, cross_direction);
    const float inverse_determinant = 1.0f / determinant;
    const henka_vec3 origin_offset = henka_vec3_subtract(ray.origin, first);
    const float barycentric_u = henka_vec3_dot(origin_offset, cross_direction) * inverse_determinant;
    const henka_vec3 cross_offset = henka_vec3_cross(origin_offset, edge_one);
    const float barycentric_v = henka_vec3_dot(ray.direction, cross_offset) * inverse_determinant;
    const float distance = henka_vec3_dot(edge_two, cross_offset) * inverse_determinant;

    if (!isfinite(determinant) || fabsf(determinant) <= 0.000001f ||
        !isfinite(barycentric_u) || !isfinite(barycentric_v) || !isfinite(distance) ||
        barycentric_u < 0.0f || barycentric_u > 1.0f || barycentric_v < 0.0f ||
        barycentric_u + barycentric_v > 1.0f || distance < 0.0f || distance > maximum_distance)
    {
        return false;
    }
    if (out_distance != NULL)
    {
        *out_distance = distance;
    }
    return true;
}

static bool sandbox3d_authoring_ray_point_distance(
    henka_ray ray,
    henka_vec3 point,
    float maximum_distance,
    float* out_distance)
{
    const henka_vec3 offset = henka_vec3_subtract(point, ray.origin);
    const float distance = henka_vec3_dot(offset, ray.direction);
    const henka_vec3 closest = henka_vec3_add(ray.origin, henka_vec3_scale(ray.direction, distance));
    const float error = henka_vec3_length(henka_vec3_subtract(point, closest));
    const float tolerance = 0.18f + distance * 0.018f;
    if (!isfinite(distance) || !isfinite(error) || distance < 0.0f || distance > maximum_distance || error > tolerance)
    {
        return false;
    }
    if (out_distance != NULL) *out_distance = distance;
    return true;
}

static bool sandbox3d_authoring_ray_segment_distance(
    henka_ray ray,
    henka_vec3 first,
    henka_vec3 second,
    float maximum_distance,
    float* out_distance)
{
    const henka_vec3 segment = henka_vec3_subtract(second, first);
    const henka_vec3 origin_to_first = henka_vec3_subtract(ray.origin, first);
    const float segment_length_squared = henka_vec3_dot(segment, segment);
    const float ray_segment_dot = henka_vec3_dot(ray.direction, segment);
    const float ray_first_dot = henka_vec3_dot(ray.direction, origin_to_first);
    const float segment_first_dot = henka_vec3_dot(segment, origin_to_first);
    float ray_distance;
    float segment_factor;
    henka_vec3 ray_point;
    henka_vec3 segment_point;
    float tolerance;
    if (!isfinite(segment_length_squared) || segment_length_squared <= 0.000001f)
    {
        return sandbox3d_authoring_ray_point_distance(ray, first, maximum_distance, out_distance);
    }
    {
        const float denominator = 1.0f * segment_length_squared - ray_segment_dot * ray_segment_dot;
        if (fabsf(denominator) <= 0.000001f)
        {
            segment_factor = fminf(1.0f, fmaxf(0.0f, segment_first_dot / segment_length_squared));
            ray_distance = ray_first_dot + ray_segment_dot * segment_factor;
        }
        else
        {
            ray_distance = (ray_segment_dot * segment_first_dot - segment_length_squared * ray_first_dot) / denominator;
            segment_factor = (ray_segment_dot * ray_distance + segment_first_dot) / segment_length_squared;
            segment_factor = fminf(1.0f, fmaxf(0.0f, segment_factor));
        }
    }
    if (ray_distance < 0.0f || ray_distance > maximum_distance)
    {
        return false;
    }
    ray_point = henka_vec3_add(ray.origin, henka_vec3_scale(ray.direction, ray_distance));
    segment_point = henka_vec3_add(first, henka_vec3_scale(segment, segment_factor));
    tolerance = 0.14f + ray_distance * 0.014f;
    if (henka_vec3_length(henka_vec3_subtract(ray_point, segment_point)) > tolerance)
    {
        return false;
    }
    if (out_distance != NULL) *out_distance = ray_distance;
    return true;
}

henka_result sandbox3d_authoring_object_pick_component(
    sandbox3d_authoring_object* object,
    henka_ray ray,
    float maximum_distance,
    bool additive)
{
    henka_transform transform;
    uint32_t nearest_id = HENKA_AUTHORING_INVALID_ID;
    float nearest_distance = maximum_distance;
    size_t id;
    if (object == NULL || !isfinite(maximum_distance) || maximum_distance <= 0.0f ||
        !sandbox3d_authoring_finite_vec3(ray.origin) || !sandbox3d_authoring_finite_vec3(ray.direction) ||
        henka_vec3_length(ray.direction) <= 0.000001f ||
        henka_scene_get_entity_transform(object->scene, object->entity, &transform) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    ray.direction = henka_vec3_normalize(ray.direction);
    if (object->selection_mode == SANDBOX3D_AUTHORING_SELECTION_FACE)
    {
        uint32_t prior_faces[SANDBOX3D_AUTHORING_MAX_SELECTED_COMPONENTS];
        size_t prior_face_count = 0U;
        henka_result result;
        if (additive)
        {
            prior_face_count = object->selected_face_count;
            memcpy(prior_faces, object->selected_faces, prior_face_count * sizeof(prior_faces[0]));
        }
        result = sandbox3d_authoring_object_pick_face(object, ray, maximum_distance);
        if (result == HENKA_SUCCESS && additive)
        {
            object->selected_face_count = prior_face_count;
            memcpy(object->selected_faces, prior_faces, prior_face_count * sizeof(prior_faces[0]));
            object->selection_mode = SANDBOX3D_AUTHORING_SELECTION_FACE;
            result = sandbox3d_authoring_object_select_component(
                object, object->selected_face, true);
        }
        if (result == HENKA_SUCCESS)
        {
            return result;
        }
        return result;
    }
    for (id = 1U; id <= HENKA_AUTHORING_MESH_HARD_MAX_VERTICES &&
        object->selection_mode == SANDBOX3D_AUTHORING_SELECTION_VERTEX; ++id)
    {
        const henka_authoring_vertex* vertex = henka_authoring_mesh_get_vertex(object->mesh, (henka_authoring_vertex_id)id);
        float distance;
        if (vertex != NULL && sandbox3d_authoring_ray_point_distance(
                ray, sandbox3d_authoring_transform_point(transform, vertex->position), nearest_distance, &distance))
        {
            nearest_distance = distance;
            nearest_id = (uint32_t)id;
        }
    }
    if (object->selection_mode == SANDBOX3D_AUTHORING_SELECTION_EDGE)
    {
        for (id = 1U; id <= HENKA_AUTHORING_MESH_HARD_MAX_EDGES; ++id)
        {
            const henka_authoring_edge* edge = henka_authoring_mesh_get_edge(object->mesh, (henka_authoring_edge_id)id);
            const henka_authoring_vertex* first;
            const henka_authoring_vertex* second;
            float distance;
            if (edge == NULL ||
                (first = henka_authoring_mesh_get_vertex(object->mesh, edge->vertices[0])) == NULL ||
                (second = henka_authoring_mesh_get_vertex(object->mesh, edge->vertices[1])) == NULL)
            {
                continue;
            }
            if (sandbox3d_authoring_ray_segment_distance(
                    ray,
                    sandbox3d_authoring_transform_point(transform, first->position),
                    sandbox3d_authoring_transform_point(transform, second->position),
                    nearest_distance,
                    &distance))
            {
                nearest_distance = distance;
                nearest_id = (uint32_t)id;
            }
        }
    }
    return nearest_id == HENKA_AUTHORING_INVALID_ID
        ? HENKA_ERROR_UNKNOWN
        : sandbox3d_authoring_object_select_component(object, nearest_id, additive);
}

henka_result sandbox3d_authoring_object_move_selected_components(
    sandbox3d_authoring_object* object,
    henka_vec3 offset)
{
    henka_authoring_mesh* candidate = NULL;
    henka_authoring_vertex_id vertex_ids[SANDBOX3D_AUTHORING_MAX_SELECTED_COMPONENTS * 2U];
    size_t vertex_count = 0U;
    size_t selected_count;
    const uint32_t* selected_ids;
    size_t index;
    henka_result result;
    if (object == NULL || !sandbox3d_authoring_finite_vec3(offset)) return HENKA_ERROR_INVALID_ARGUMENT;
    selected_ids = sandbox3d_authoring_selected_ids_const(object, &selected_count);
    if (selected_ids == NULL || selected_count == 0U) return HENKA_ERROR_INVALID_ARGUMENT;
    for (index = 0U; index < selected_count; ++index)
    {
        const henka_authoring_vertex_id* candidates = NULL;
        size_t candidate_count = 0U;
        if (object->selection_mode == SANDBOX3D_AUTHORING_SELECTION_VERTEX)
        {
            candidates = &selected_ids[index]; candidate_count = 1U;
        }
        else if (object->selection_mode == SANDBOX3D_AUTHORING_SELECTION_EDGE)
        {
            const henka_authoring_edge* edge = henka_authoring_mesh_get_edge(object->mesh, selected_ids[index]);
            if (edge != NULL) { candidates = edge->vertices; candidate_count = 2U; }
        }
        else
        {
            const henka_authoring_face* face = henka_authoring_mesh_get_face(object->mesh, selected_ids[index]);
            if (face != NULL) { candidates = face->vertices; candidate_count = face->corner_count; }
        }
        for (size_t candidate_index = 0U; candidate_index < candidate_count; ++candidate_index)
        {
            size_t existing;
            bool duplicate = false;
            for (existing = 0U; existing < vertex_count; ++existing)
                if (vertex_ids[existing] == candidates[candidate_index]) duplicate = true;
            if (!duplicate && vertex_count < sizeof(vertex_ids) / sizeof(vertex_ids[0]))
                vertex_ids[vertex_count++] = candidates[candidate_index];
        }
    }
    result = henka_authoring_mesh_clone(object->mesh, &candidate);
    if (result == HENKA_SUCCESS)
    {
        for (index = 0U; index < vertex_count && result == HENKA_SUCCESS; ++index)
        {
            const henka_authoring_vertex* vertex = henka_authoring_mesh_get_vertex(candidate, vertex_ids[index]);
            result = vertex == NULL ? HENKA_ERROR_INVALID_ARGUMENT :
                henka_authoring_mesh_set_vertex_position(candidate, vertex->id, henka_vec3_add(vertex->position, offset));
        }
    }
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_authoring_publish_candidate(object, candidate, true, object->selected_face);
    }
    if (result != HENKA_SUCCESS) henka_authoring_mesh_destroy(candidate);
    return result;
}

henka_result sandbox3d_authoring_object_pick_face(
    sandbox3d_authoring_object* object,
    henka_ray ray,
    float maximum_distance)
{
    henka_transform transform;
    henka_authoring_face_id nearest_face = HENKA_AUTHORING_INVALID_ID;
    float nearest_distance = maximum_distance;
    size_t face_id;

    if (object == NULL || !isfinite(maximum_distance) || maximum_distance <= 0.0f ||
        !sandbox3d_authoring_finite_vec3(ray.origin) || !sandbox3d_authoring_finite_vec3(ray.direction) ||
        henka_vec3_length(ray.direction) <= 0.000001f ||
        henka_scene_get_entity_transform(object->scene, object->entity, &transform) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    ray.direction = henka_vec3_normalize(ray.direction);
    for (face_id = 1U; face_id <= HENKA_AUTHORING_MESH_HARD_MAX_FACES; ++face_id)
    {
        const henka_authoring_face* face = henka_authoring_mesh_get_face(
            object->mesh, (henka_authoring_face_id)face_id);
        size_t corner;
        if (face == NULL)
        {
            continue;
        }
        for (corner = 1U; corner + 1U < face->corner_count; ++corner)
        {
            const henka_authoring_vertex* first = henka_authoring_mesh_get_vertex(
                object->mesh, face->vertices[0]);
            const henka_authoring_vertex* second = henka_authoring_mesh_get_vertex(
                object->mesh, face->vertices[corner]);
            const henka_authoring_vertex* third = henka_authoring_mesh_get_vertex(
                object->mesh, face->vertices[corner + 1U]);
            float distance;
            if (first != NULL && second != NULL && third != NULL &&
                sandbox3d_authoring_ray_triangle(
                    ray,
                    sandbox3d_authoring_transform_point(transform, first->position),
                    sandbox3d_authoring_transform_point(transform, second->position),
                    sandbox3d_authoring_transform_point(transform, third->position),
                    nearest_distance,
                    &distance))
            {
                nearest_distance = distance;
                nearest_face = face->id;
            }
        }
    }
    if (nearest_face == HENKA_AUTHORING_INVALID_ID)
    {
        return HENKA_ERROR_UNKNOWN;
    }
    object->selected_face = nearest_face;
    object->selection_mode = SANDBOX3D_AUTHORING_SELECTION_FACE;
    (void)sandbox3d_authoring_object_select_component(object, nearest_face, false);
    return HENKA_SUCCESS;
}

henka_result sandbox3d_authoring_object_select_face(
    sandbox3d_authoring_object* object,
    henka_authoring_face_id face_id)
{
    if (object == NULL || henka_authoring_mesh_get_face(object->mesh, face_id) == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    object->selection_mode = SANDBOX3D_AUTHORING_SELECTION_FACE;
    return sandbox3d_authoring_object_select_component(object, face_id, false);
}

henka_result sandbox3d_authoring_object_set_selected_face_material_region(
    sandbox3d_authoring_object* object,
    uint32_t material_region)
{
    henka_authoring_mesh* candidate = NULL;
    henka_result result;

    if (object == NULL || henka_authoring_mesh_get_face(object->mesh, object->selected_face) == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_authoring_mesh_clone(object->mesh, &candidate);
    if (result == HENKA_SUCCESS)
    {
        result = henka_authoring_mesh_set_face_material_region(
            candidate, object->selected_face, material_region);
    }
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_authoring_publish_candidate(
            object, candidate, true, object->selected_face);
        if (result != HENKA_SUCCESS)
        {
            henka_authoring_mesh_destroy(candidate);
        }
    }
    else
    {
        henka_authoring_mesh_destroy(candidate);
    }
    return result;
}

henka_result sandbox3d_authoring_object_extrude_selected_face(
    sandbox3d_authoring_object* object,
    float distance)
{
    henka_authoring_mesh* candidate = NULL;
    henka_authoring_face_id new_face_id = HENKA_AUTHORING_INVALID_ID;
    henka_result result;
    if (object == NULL || !isfinite(distance))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_authoring_mesh_clone(object->mesh, &candidate);
    if (result == HENKA_SUCCESS)
    {
        result = henka_authoring_mesh_extrude_face(
            candidate, object->selected_face, distance, &new_face_id);
    }
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_authoring_publish_candidate(
            object, candidate, true, new_face_id);
        if (result == HENKA_SUCCESS)
        {
            object->selected_face = new_face_id;
        }
        else
        {
            henka_authoring_mesh_destroy(candidate);
        }
    }
    else
    {
        henka_authoring_mesh_destroy(candidate);
    }
    return result;
}

henka_result sandbox3d_authoring_object_inset_selected_face(
    sandbox3d_authoring_object* object,
    float factor)
{
    henka_authoring_mesh* candidate = NULL;
    henka_authoring_face_id new_face_id = HENKA_AUTHORING_INVALID_ID;
    henka_result result;
    if (object == NULL || !isfinite(factor))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_authoring_mesh_clone(object->mesh, &candidate);
    if (result == HENKA_SUCCESS)
    {
        result = henka_authoring_mesh_inset_face(
            candidate, object->selected_face, factor, &new_face_id);
    }
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_authoring_publish_candidate(
            object, candidate, true, new_face_id);
        if (result == HENKA_SUCCESS)
        {
            object->selected_face = new_face_id;
        }
        else
        {
            henka_authoring_mesh_destroy(candidate);
        }
    }
    else
    {
        henka_authoring_mesh_destroy(candidate);
    }
    return result;
}

henka_result sandbox3d_authoring_object_bevel_selected_face(
    sandbox3d_authoring_object* object,
    float width)
{
    henka_authoring_mesh* candidate = NULL;
    henka_authoring_face_id new_face_id = HENKA_AUTHORING_INVALID_ID;
    henka_result result;
    if (object == NULL || !isfinite(width))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_authoring_mesh_clone(object->mesh, &candidate);
    if (result == HENKA_SUCCESS)
    {
        result = henka_authoring_mesh_bevel_face(
            candidate, object->selected_face, width, &new_face_id);
    }
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_authoring_publish_candidate(
            object, candidate, true, new_face_id);
        if (result == HENKA_SUCCESS)
        {
            object->selected_face = new_face_id;
        }
        else
        {
            henka_authoring_mesh_destroy(candidate);
        }
    }
    else
    {
        henka_authoring_mesh_destroy(candidate);
    }
    return result;
}

henka_result sandbox3d_authoring_object_subdivide_selected_face(
    sandbox3d_authoring_object* object)
{
    henka_authoring_mesh* candidate = NULL;
    henka_authoring_vertex_id center_vertex_id = HENKA_AUTHORING_INVALID_ID;
    henka_result result;
    if (object == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_authoring_mesh_clone(object->mesh, &candidate);
    if (result == HENKA_SUCCESS)
    {
        result = henka_authoring_mesh_subdivide_face(
            candidate, object->selected_face, &center_vertex_id);
    }
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_authoring_publish_candidate(
            object, candidate, true, object->selected_face);
        if (result == HENKA_SUCCESS)
        {
            (void)center_vertex_id;
            sandbox3d_authoring_repair_selection(object);
        }
        else
        {
            henka_authoring_mesh_destroy(candidate);
        }
    }
    else
    {
        henka_authoring_mesh_destroy(candidate);
    }
    return result;
}

henka_result sandbox3d_authoring_object_project_selected_face_uv(
    sandbox3d_authoring_object* object,
    henka_authoring_uv_projection_axis axis)
{
    henka_authoring_mesh* candidate = NULL;
    henka_result result;
    if (object == NULL || axis > HENKA_AUTHORING_UV_PROJECT_Z)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_authoring_mesh_clone(object->mesh, &candidate);
    if (result == HENKA_SUCCESS)
    {
        result = henka_authoring_mesh_project_face_uv(
            candidate, object->selected_face, axis);
    }
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_authoring_publish_candidate(
            object, candidate, true, object->selected_face);
        if (result != HENKA_SUCCESS)
        {
            henka_authoring_mesh_destroy(candidate);
        }
    }
    else
    {
        henka_authoring_mesh_destroy(candidate);
    }
    return result;
}

henka_result sandbox3d_authoring_object_pack_selected_face_uv(
    sandbox3d_authoring_object* object,
    float padding)
{
    henka_authoring_mesh* candidate = NULL;
    henka_result result;
    if (object == NULL || !isfinite(padding))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_authoring_mesh_clone(object->mesh, &candidate);
    if (result == HENKA_SUCCESS)
    {
        result = henka_authoring_mesh_pack_face_uv(
            candidate, object->selected_face, padding);
    }
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_authoring_publish_candidate(
            object, candidate, true, object->selected_face);
        if (result != HENKA_SUCCESS)
        {
            henka_authoring_mesh_destroy(candidate);
        }
    }
    else
    {
        henka_authoring_mesh_destroy(candidate);
    }
    return result;
}

henka_result sandbox3d_authoring_object_save_source(
    const sandbox3d_authoring_object* object,
    const char* path)
{
    if (object == NULL || path == NULL || path[0] == '\0')
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    {
        const henka_result result = henka_authoring_mesh_save_file(object->mesh, path);
        if (result == HENKA_SUCCESS)
        {
            (void)sandbox3d_authoring_set_source_path((sandbox3d_authoring_object*)object, path);
        }
        return result;
    }
}

henka_result sandbox3d_authoring_object_reload_source(
    sandbox3d_authoring_object* object,
    const char* path)
{
    henka_authoring_mesh* candidate = NULL;
    henka_result result;
    if (object == NULL || path == NULL || path[0] == '\0')
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_authoring_mesh_clone(object->mesh, &candidate);
    if (result == HENKA_SUCCESS)
    {
        result = henka_authoring_mesh_load_file(candidate, path);
    }
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_authoring_replace_loaded_source(object, candidate);
    }
    if (result != HENKA_SUCCESS)
    {
        henka_authoring_mesh_destroy(candidate);
    }
    return result;
}

henka_result sandbox3d_authoring_object_save_project(
    const sandbox3d_authoring_object* object,
    const char* project_path,
    const char* source_path)
{
    henka_settings* settings = NULL;
    henka_transform transform;
    henka_result result;

    if (object == NULL || project_path == NULL || project_path[0] == '\0' ||
        source_path == NULL || source_path[0] == '\0' ||
        henka_scene_get_entity_transform(object->scene, object->entity, &transform) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_authoring_mesh_save_file(object->mesh, source_path);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    result = henka_settings_create(&settings);
    if (result == HENKA_SUCCESS) result = henka_settings_set_int(settings, "project.version", 1);
    if (result == HENKA_SUCCESS) result = henka_settings_set_string(settings, "project.source_path", source_path);
    if (result == HENKA_SUCCESS) result = henka_settings_set_float(settings, "transform.position.x", transform.position.x);
    if (result == HENKA_SUCCESS) result = henka_settings_set_float(settings, "transform.position.y", transform.position.y);
    if (result == HENKA_SUCCESS) result = henka_settings_set_float(settings, "transform.position.z", transform.position.z);
    if (result == HENKA_SUCCESS) result = henka_settings_set_float(settings, "transform.rotation.x", transform.rotation.x);
    if (result == HENKA_SUCCESS) result = henka_settings_set_float(settings, "transform.rotation.y", transform.rotation.y);
    if (result == HENKA_SUCCESS) result = henka_settings_set_float(settings, "transform.rotation.z", transform.rotation.z);
    if (result == HENKA_SUCCESS) result = henka_settings_set_float(settings, "transform.rotation.w", transform.rotation.w);
    if (result == HENKA_SUCCESS) result = henka_settings_set_float(settings, "transform.scale.x", transform.scale.x);
    if (result == HENKA_SUCCESS) result = henka_settings_set_float(settings, "transform.scale.y", transform.scale.y);
    if (result == HENKA_SUCCESS) result = henka_settings_set_float(settings, "transform.scale.z", transform.scale.z);
    if (result == HENKA_SUCCESS) result = henka_settings_set_bool(
        settings, "entity.visible", henka_scene_is_entity_visible(object->scene, object->entity));
    if (result == HENKA_SUCCESS) result = henka_settings_save_file(settings, project_path);
    henka_settings_destroy(settings);
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_authoring_set_source_path((sandbox3d_authoring_object*)object, source_path);
    }
    return result;
}

henka_result sandbox3d_authoring_object_load_project(
    sandbox3d_authoring_object* object,
    const char* project_path)
{
    henka_settings* settings = NULL;
    henka_authoring_mesh* candidate = NULL;
    henka_authoring_mesh* rollback_mesh = NULL;
    henka_transform transform;
    henka_transform previous_transform = henka_transform_identity();
    const char* source_path;
    bool visible = false;
    bool previous_visible;
    bool have_previous_scene_state = false;
    int version;
    henka_result result;

    if (object == NULL || project_path == NULL || project_path[0] == '\0')
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_settings_create(&settings);
    if (result == HENKA_SUCCESS) result = henka_settings_load_file(settings, project_path);
    version = result == HENKA_SUCCESS ? henka_settings_get_int(settings, "project.version", 0) : 0;
    source_path = result == HENKA_SUCCESS ? henka_settings_get_string(settings, "project.source_path", NULL) : NULL;
    if (result == HENKA_SUCCESS && (version != 1 || source_path == NULL || source_path[0] == '\0'))
        result = HENKA_ERROR_UNKNOWN;
    transform = henka_transform_identity();
    if (result == HENKA_SUCCESS)
    {
        transform.position.x = henka_settings_get_float(settings, "transform.position.x", NAN);
        transform.position.y = henka_settings_get_float(settings, "transform.position.y", NAN);
        transform.position.z = henka_settings_get_float(settings, "transform.position.z", NAN);
        transform.rotation.x = henka_settings_get_float(settings, "transform.rotation.x", NAN);
        transform.rotation.y = henka_settings_get_float(settings, "transform.rotation.y", NAN);
        transform.rotation.z = henka_settings_get_float(settings, "transform.rotation.z", NAN);
        transform.rotation.w = henka_settings_get_float(settings, "transform.rotation.w", NAN);
        transform.scale.x = henka_settings_get_float(settings, "transform.scale.x", NAN);
        transform.scale.y = henka_settings_get_float(settings, "transform.scale.y", NAN);
        transform.scale.z = henka_settings_get_float(settings, "transform.scale.z", NAN);
        visible = henka_settings_get_bool(settings, "entity.visible", false);
        if (!sandbox3d_authoring_finite_vec3(transform.position) ||
            !sandbox3d_authoring_finite_vec3(transform.scale) ||
            !isfinite(transform.rotation.x) || !isfinite(transform.rotation.y) ||
            !isfinite(transform.rotation.z) || !isfinite(transform.rotation.w) ||
            transform.scale.x <= 0.0f || transform.scale.y <= 0.0f || transform.scale.z <= 0.0f)
        {
            result = HENKA_ERROR_INVALID_ARGUMENT;
        }
    }
    if (result == HENKA_SUCCESS) result = henka_authoring_mesh_clone(object->mesh, &candidate);
    if (result == HENKA_SUCCESS) result = henka_authoring_mesh_clone(object->mesh, &rollback_mesh);
    if (result == HENKA_SUCCESS) result = henka_authoring_mesh_load_file(candidate, source_path);
    if (result == HENKA_SUCCESS) result = henka_scene_get_entity_transform(object->scene, object->entity, &previous_transform);
    if (result == HENKA_SUCCESS) have_previous_scene_state = true;
    previous_visible = result == HENKA_SUCCESS ? henka_scene_is_entity_visible(object->scene, object->entity) : false;
    if (result == HENKA_SUCCESS) result = sandbox3d_authoring_replace_loaded_source(object, candidate);
    if (result == HENKA_SUCCESS) result = henka_scene_set_entity_transform(object->scene, object->entity, transform);
    if (result == HENKA_SUCCESS) result = henka_scene_set_entity_visible(object->scene, object->entity, visible);
    if (result == HENKA_SUCCESS) result = sandbox3d_authoring_set_source_path(object, source_path);
    if (result != HENKA_SUCCESS)
    {
        if (candidate != NULL && candidate != object->mesh) henka_authoring_mesh_destroy(candidate);
        if (rollback_mesh != NULL)
        {
            (void)sandbox3d_authoring_replace_loaded_source(object, rollback_mesh);
            rollback_mesh = NULL;
        }
        if (have_previous_scene_state)
        {
            (void)henka_scene_set_entity_transform(object->scene, object->entity, previous_transform);
            (void)henka_scene_set_entity_visible(object->scene, object->entity, previous_visible);
        }
    }
    henka_authoring_mesh_destroy(rollback_mesh);
    henka_settings_destroy(settings);
    return result;
}

const char* sandbox3d_authoring_object_get_source_path(
    const sandbox3d_authoring_object* object)
{
    return object != NULL && object->source_path != NULL ? object->source_path : "";
}

static henka_result sandbox3d_authoring_history_move(
    sandbox3d_authoring_object* object,
    bool undo)
{
    henka_authoring_mesh* candidate = NULL;
    henka_result result;
    if (object == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_authoring_mesh_clone(object->mesh, &candidate);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    result = undo
        ? henka_authoring_mesh_history_undo(object->history, candidate)
        : henka_authoring_mesh_history_redo(object->history, candidate);
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_authoring_publish_candidate(
            object, candidate, false, object->selected_face);
        if (result == HENKA_SUCCESS && object->selection_history_count > 0U)
        {
            if (undo)
            {
                if (object->selection_history_cursor > 0U)
                {
                    --object->selection_history_cursor;
                }
            }
            else if (object->selection_history_cursor + 1U < object->selection_history_count)
            {
                ++object->selection_history_cursor;
            }
            sandbox3d_authoring_restore_selection_history(object);
        }
        if (result != HENKA_SUCCESS)
        {
            (void)(undo
                ? henka_authoring_mesh_history_redo(object->history, object->mesh)
                : henka_authoring_mesh_history_undo(object->history, object->mesh));
            henka_authoring_mesh_destroy(candidate);
        }
    }
    else
    {
        henka_authoring_mesh_destroy(candidate);
    }
    return result;
}

henka_result sandbox3d_authoring_object_undo(sandbox3d_authoring_object* object)
{
    return sandbox3d_authoring_history_move(object, true);
}

henka_result sandbox3d_authoring_object_redo(sandbox3d_authoring_object* object)
{
    return sandbox3d_authoring_history_move(object, false);
}
