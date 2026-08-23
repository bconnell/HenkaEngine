#include "modeling_selection_scene.h"

#include <stdint.h>

#include <henka/authoring_mesh.h>
#include <henka/memory.h>

static henka_vec3 sandbox3d_modeling_selection_transform_point(
    henka_transform transform,
    henka_vec3 point)
{
    point.x *= transform.scale.x;
    point.y *= transform.scale.y;
    point.z *= transform.scale.z;
    return henka_vec3_add(
        transform.position,
        henka_quat_rotate_vec3(transform.rotation, point));
}

static bool sandbox3d_modeling_selection_project(
    const henka_camera* camera,
    henka_viewport viewport,
    henka_vec3 world_position,
    henka_vec2* out_screen_position,
    float* out_depth)
{
    return camera != NULL && out_screen_position != NULL && out_depth != NULL &&
        henka_camera_world_to_screen(
            camera,
            viewport.width,
            viewport.height,
            world_position,
            out_screen_position,
            out_depth) == HENKA_SUCCESS;
}

static bool sandbox3d_modeling_selection_face_sample(
    const henka_camera* camera,
    henka_transform transform,
    const henka_authoring_mesh* mesh,
    henka_authoring_face_id face_id,
    henka_vec3* out_center,
    bool* out_front_facing)
{
    const henka_authoring_face* face;
    henka_vec3 center = {0.0f, 0.0f, 0.0f};
    henka_vec3 first;
    henka_vec3 second;
    henka_vec3 third;
    henka_vec3 normal;
    size_t corner;

    if (camera == NULL || mesh == NULL || out_center == NULL ||
        out_front_facing == NULL ||
        (face = henka_authoring_mesh_get_face(mesh, face_id)) == NULL ||
        face->corner_count < 3U)
    {
        return false;
    }
    for (corner = 0U; corner < face->corner_count; ++corner)
    {
        const henka_authoring_vertex* vertex =
            henka_authoring_mesh_get_vertex(mesh, face->vertices[corner]);
        if (vertex == NULL)
        {
            return false;
        }
        center = henka_vec3_add(
            center,
            sandbox3d_modeling_selection_transform_point(
                transform, vertex->position));
    }
    center = henka_vec3_scale(center, 1.0f / (float)face->corner_count);
    first = sandbox3d_modeling_selection_transform_point(
        transform,
        henka_authoring_mesh_get_vertex(mesh, face->vertices[0])->position);
    second = sandbox3d_modeling_selection_transform_point(
        transform,
        henka_authoring_mesh_get_vertex(mesh, face->vertices[1])->position);
    third = sandbox3d_modeling_selection_transform_point(
        transform,
        henka_authoring_mesh_get_vertex(mesh, face->vertices[2])->position);
    normal = henka_vec3_cross(
        henka_vec3_subtract(second, first),
        henka_vec3_subtract(third, first));
    *out_center = center;
    *out_front_facing = henka_vec3_dot(
        normal,
        henka_vec3_subtract(camera->position, center)) > 0.0f;
    return true;
}

static bool sandbox3d_modeling_selection_component_sample(
    const henka_camera* camera,
    henka_transform transform,
    const henka_authoring_mesh* mesh,
    sandbox3d_authoring_selection_mode mode,
    uint32_t component_id,
    henka_vec3* out_position,
    bool* out_front_facing)
{
    if (mode == SANDBOX3D_AUTHORING_SELECTION_FACE)
    {
        return sandbox3d_modeling_selection_face_sample(
            camera, transform, mesh, (henka_authoring_face_id)component_id,
            out_position, out_front_facing);
    }
    if (mode == SANDBOX3D_AUTHORING_SELECTION_EDGE)
    {
        const henka_authoring_edge* edge = henka_authoring_mesh_get_edge(
            mesh, (henka_authoring_edge_id)component_id);
        const henka_authoring_vertex* first;
        const henka_authoring_vertex* second;
        size_t face;
        if (edge == NULL ||
            (first = henka_authoring_mesh_get_vertex(mesh, edge->vertices[0])) == NULL ||
            (second = henka_authoring_mesh_get_vertex(mesh, edge->vertices[1])) == NULL)
        {
            return false;
        }
        *out_position = henka_vec3_scale(
            henka_vec3_add(
                sandbox3d_modeling_selection_transform_point(transform, first->position),
                sandbox3d_modeling_selection_transform_point(transform, second->position)),
            0.5f);
        *out_front_facing = edge->face_count == 0U;
        for (face = 0U; face < edge->face_count && !*out_front_facing; ++face)
        {
            henka_vec3 unused_center;
            bool front_facing = false;
            if (sandbox3d_modeling_selection_face_sample(
                    camera, transform, mesh, edge->faces[face],
                    &unused_center, &front_facing) && front_facing)
            {
                *out_front_facing = true;
            }
        }
        return true;
    }
    {
        const henka_authoring_vertex* vertex = henka_authoring_mesh_get_vertex(
            mesh, (henka_authoring_vertex_id)component_id);
        const size_t edge_count = henka_authoring_mesh_get_vertex_edge_count(
            mesh, (henka_authoring_vertex_id)component_id);
        size_t edge_index;
        if (vertex == NULL)
        {
            return false;
        }
        *out_position = sandbox3d_modeling_selection_transform_point(
            transform, vertex->position);
        *out_front_facing = edge_count == 0U;
        for (edge_index = 0U; edge_index < edge_count && !*out_front_facing; ++edge_index)
        {
            henka_authoring_edge_id edge_id;
            const henka_authoring_edge* edge;
            size_t face;
            if (henka_authoring_mesh_get_vertex_edge_at(
                    mesh, (henka_authoring_vertex_id)component_id,
                    edge_index, &edge_id) != HENKA_SUCCESS ||
                (edge = henka_authoring_mesh_get_edge(mesh, edge_id)) == NULL)
            {
                continue;
            }
            for (face = 0U; face < edge->face_count; ++face)
            {
                henka_vec3 unused_center;
                bool front_facing = false;
                if (sandbox3d_modeling_selection_face_sample(
                        camera, transform, mesh, edge->faces[face],
                        &unused_center, &front_facing) && front_facing)
                {
                    *out_front_facing = true;
                    break;
                }
            }
        }
        return true;
    }
}

henka_result sandbox3d_modeling_selection_apply_scene(
    const sandbox3d_modeling_selection_session* session,
    bool xray_enabled,
    const henka_camera* camera,
    henka_scene* scene,
    henka_viewport viewport,
    sandbox3d_authoring_object* object,
    size_t* out_selected_count)
{
    const henka_authoring_mesh* mesh;
    henka_authoring_mesh_desc desc;
    sandbox3d_authoring_selection_mode mode;
    sandbox3d_modeling_selection_candidate* candidates = NULL;
    uint32_t* prior_ids = NULL;
    uint32_t* result_ids = NULL;
    henka_transform transform;
    size_t slot_capacity;
    size_t candidate_count = 0U;
    size_t prior_count;
    size_t result_capacity;
    size_t result_count = 0U;
    uint32_t active_id = HENKA_AUTHORING_INVALID_ID;
    size_t slot;
    henka_result result = HENKA_ERROR_INVALID_ARGUMENT;

    if (out_selected_count != NULL)
    {
        *out_selected_count = 0U;
    }
    if (session == NULL || camera == NULL || scene == NULL || object == NULL ||
        out_selected_count == NULL || !session->active || !session->dragging ||
        !henka_viewport_is_valid(viewport) ||
        (float)viewport.width != session->viewport_width ||
        (float)viewport.height != session->viewport_height ||
        (mesh = sandbox3d_authoring_object_get_mesh(object)) == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    mode = sandbox3d_authoring_object_get_selection_mode(object);
    desc = henka_authoring_mesh_get_desc(mesh);
    slot_capacity = mode == SANDBOX3D_AUTHORING_SELECTION_VERTEX
        ? desc.max_vertices
        : mode == SANDBOX3D_AUTHORING_SELECTION_EDGE
            ? desc.max_edges
            : desc.max_faces;
    prior_count = sandbox3d_authoring_object_get_selected_component_count(object);
    if (slot_capacity == 0U || slot_capacity > SIZE_MAX - prior_count ||
        henka_scene_get_entity_transform(
            scene, sandbox3d_authoring_object_get_entity(object),
            &transform) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result_capacity = slot_capacity + prior_count;
    candidates = henka_calloc(slot_capacity, sizeof(*candidates));
    result_ids = henka_calloc(result_capacity, sizeof(*result_ids));
    if (prior_count > 0U)
    {
        prior_ids = henka_calloc(prior_count, sizeof(*prior_ids));
    }
    if (candidates == NULL || result_ids == NULL ||
        (prior_count > 0U && prior_ids == NULL))
    {
        result = HENKA_ERROR_OUT_OF_MEMORY;
        goto cleanup;
    }
    for (slot = 0U; slot < prior_count; ++slot)
    {
        if (sandbox3d_authoring_object_get_selected_component_at(
                object, slot, &prior_ids[slot]) != HENKA_SUCCESS)
        {
            goto cleanup;
        }
    }
    for (slot = 0U; slot < slot_capacity; ++slot)
    {
        uint32_t id = HENKA_AUTHORING_INVALID_ID;
        henka_vec3 world_position;
        henka_ray ray;
        uint32_t visible_id = HENKA_AUTHORING_INVALID_ID;
        bool front_facing;
        const henka_result id_result = mode == SANDBOX3D_AUTHORING_SELECTION_VERTEX
            ? henka_authoring_mesh_get_vertex_id_at(mesh, slot, &id)
            : mode == SANDBOX3D_AUTHORING_SELECTION_EDGE
                ? henka_authoring_mesh_get_edge_id_at(mesh, slot, &id)
                : henka_authoring_mesh_get_face_id_at(mesh, slot, &id);
        if (id_result != HENKA_SUCCESS ||
            !sandbox3d_modeling_selection_component_sample(
                camera, transform, mesh, mode, id,
                &world_position, &front_facing) ||
            !sandbox3d_modeling_selection_project(
                camera, viewport, world_position,
                &candidates[candidate_count].screen_position,
                &candidates[candidate_count].depth))
        {
            continue;
        }
        candidates[candidate_count].component_id = id;
        candidates[candidate_count].front_facing = front_facing;
        candidates[candidate_count].visible =
            henka_camera_screen_point_to_ray(
                camera, viewport.width, viewport.height,
                candidates[candidate_count].screen_position, &ray) == HENKA_SUCCESS &&
            sandbox3d_authoring_object_find_component(
                object, ray, 1000000.0f, &visible_id) == HENKA_SUCCESS &&
            visible_id == id;
        ++candidate_count;
    }
    result = sandbox3d_modeling_selection_build_result(
        session, candidates, candidate_count, xray_enabled, true,
        prior_ids, prior_count, result_ids, result_capacity,
        &result_count, &active_id);
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_authoring_object_replace_component_selection(
            object, result_ids, result_count, active_id);
    }
    if (result == HENKA_SUCCESS)
    {
        *out_selected_count = result_count;
    }

cleanup:
    henka_free(prior_ids);
    henka_free(result_ids);
    henka_free(candidates);
    return result;
}
