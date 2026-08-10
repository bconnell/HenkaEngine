#include "object_authoring_tools.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

#include <henka/memory.h>
#include <henka/model.h>

struct sandbox3d_authoring_object
{
    henka_engine* engine;
    henka_scene* scene;
    henka_entity entity;
    henka_authoring_mesh* mesh;
    henka_authoring_mesh_history* history;
    size_t history_steps;
    henka_mesh* render_mesh;
    henka_authoring_face_id selected_face;
};

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

    for (face_id = 0U; face_id < HENKA_AUTHORING_MESH_HARD_MAX_FACES; ++face_id)
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
    if (object == NULL || henka_authoring_mesh_get_face(object->mesh, object->selected_face) != NULL)
    {
        return;
    }
    object->selected_face = HENKA_AUTHORING_INVALID_ID;
    for (face_id = 0U; face_id < HENKA_AUTHORING_MESH_HARD_MAX_FACES; ++face_id)
    {
        if (henka_authoring_mesh_get_face(object->mesh, (henka_authoring_face_id)face_id) != NULL)
        {
            object->selected_face = (henka_authoring_face_id)face_id;
            return;
        }
    }
}

static henka_result sandbox3d_authoring_publish_candidate(
    sandbox3d_authoring_object* object,
    henka_authoring_mesh* candidate,
    bool checkpoint_history)
{
    henka_mesh* candidate_render = NULL;
    henka_mesh* old_scene_mesh = NULL;
    henka_bounds candidate_bounds = {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}};
    henka_bounds old_bounds;
    bool had_old_bounds;
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
    result = henka_scene_set_entity_mesh(object->scene, object->entity, candidate_render);
    if (result == HENKA_SUCCESS)
    {
        result = henka_scene_set_entity_local_bounds(object->scene, object->entity, candidate_bounds);
    }
    if (result != HENKA_SUCCESS)
    {
        (void)henka_scene_set_entity_mesh(object->scene, object->entity, old_scene_mesh);
        if (had_old_bounds) (void)henka_scene_set_entity_local_bounds(object->scene, object->entity, old_bounds);
        else (void)henka_scene_clear_entity_local_bounds(object->scene, object->entity);
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
            henka_mesh_destroy(candidate_render);
            return result;
        }
    }
    henka_authoring_mesh_destroy(object->mesh);
    henka_mesh_destroy(object->render_mesh);
    object->mesh = candidate;
    object->render_mesh = candidate_render;
    sandbox3d_authoring_repair_selection(object);
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
    bool had_old_bounds;
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
    if (result == HENKA_SUCCESS)
    {
        result = henka_scene_set_entity_mesh(object->scene, object->entity, candidate_render);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_scene_set_entity_local_bounds(object->scene, object->entity, candidate_bounds);
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
    sandbox3d_authoring_object* object;
    henka_authoring_mesh_desc default_desc;
    henka_authoring_mesh* mesh = NULL;
    henka_mesh* old_mesh = NULL;
    henka_bounds bounds = {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}};
    henka_result result;

    if (out_object == NULL || engine == NULL || scene == NULL ||
        entity == HENKA_INVALID_ENTITY || !henka_scene_is_entity_valid(scene, entity) ||
        history_steps == 0U)
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
        sandbox3d_authoring_object_destroy(object);
        return result;
    }
    *out_object = object;
    return HENKA_SUCCESS;
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
            (void)henka_scene_clear_entity_mesh(object->scene, object->entity);
        }
    }
    henka_mesh_destroy(object->render_mesh);
    henka_authoring_mesh_history_destroy(object->history);
    henka_authoring_mesh_destroy(object->mesh);
    henka_free(object);
}

henka_entity sandbox3d_authoring_object_get_entity(const sandbox3d_authoring_object* object)
{
    return object == NULL ? HENKA_INVALID_ENTITY : object->entity;
}

const henka_authoring_mesh* sandbox3d_authoring_object_get_mesh(const sandbox3d_authoring_object* object)
{
    return object == NULL ? NULL : object->mesh;
}

henka_authoring_face_id sandbox3d_authoring_object_get_selected_face(const sandbox3d_authoring_object* object)
{
    return object == NULL ? HENKA_AUTHORING_INVALID_ID : object->selected_face;
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
    for (face_id = 0U; face_id < HENKA_AUTHORING_MESH_HARD_MAX_FACES; ++face_id)
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
    object->selected_face = face_id;
    return HENKA_SUCCESS;
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
        result = sandbox3d_authoring_publish_candidate(object, candidate, true);
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
        result = sandbox3d_authoring_publish_candidate(object, candidate, true);
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

henka_result sandbox3d_authoring_object_save_source(
    const sandbox3d_authoring_object* object,
    const char* path)
{
    if (object == NULL || path == NULL || path[0] == '\0')
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return henka_authoring_mesh_save_file(object->mesh, path);
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
        result = sandbox3d_authoring_publish_candidate(object, candidate, false);
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
