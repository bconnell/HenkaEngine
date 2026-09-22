#include "modeling_operator.h"

#include <errno.h>
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <henka/memory.h>

static size_t sandbox3d_modeling_operator_selection_limit(
    sandbox3d_authoring_selection_mode mode)
{
    switch (mode)
    {
        case SANDBOX3D_AUTHORING_SELECTION_VERTEX:
            return HENKA_AUTHORING_MESH_HARD_MAX_VERTICES;
        case SANDBOX3D_AUTHORING_SELECTION_EDGE:
            return HENKA_AUTHORING_MESH_HARD_MAX_EDGES;
        case SANDBOX3D_AUTHORING_SELECTION_FACE:
            return HENKA_AUTHORING_MESH_HARD_MAX_FACES;
        default:
            return 0U;
    }
}

static bool sandbox3d_modeling_operator_selected_vertex_contains(
    const henka_authoring_vertex_id* vertex_ids,
    size_t vertex_count,
    henka_authoring_vertex_id vertex_id)
{
    size_t index;

    for (index = 0U; index < vertex_count; ++index)
    {
        if (vertex_ids[index] == vertex_id)
        {
            return true;
        }
    }
    return false;
}

static henka_result sandbox3d_modeling_operator_find_first_incident_face(
    const henka_authoring_mesh* mesh,
    henka_authoring_vertex_id vertex_id,
    henka_authoring_face_id* out_face_id)
{
    const henka_authoring_mesh_desc desc = mesh != NULL
        ? henka_authoring_mesh_get_desc(mesh)
        : (henka_authoring_mesh_desc){0};
    henka_authoring_face_id selected_face = HENKA_AUTHORING_INVALID_ID;
    size_t slot;

    if (mesh == NULL || out_face_id == NULL ||
        vertex_id == HENKA_AUTHORING_INVALID_ID)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_face_id = HENKA_AUTHORING_INVALID_ID;
    for (slot = 0U; slot < desc.max_faces; ++slot)
    {
        henka_authoring_face_id face_id = HENKA_AUTHORING_INVALID_ID;
        const henka_authoring_face* face;
        size_t corner;
        bool incident = false;

        if (henka_authoring_mesh_get_face_id_at(mesh, slot, &face_id) != HENKA_SUCCESS ||
            (face = henka_authoring_mesh_get_face(mesh, face_id)) == NULL)
        {
            continue;
        }
        for (corner = 0U; corner < face->corner_count; ++corner)
        {
            if (face->vertices[corner] == vertex_id)
            {
                incident = true;
                break;
            }
        }
        if (incident && (selected_face == HENKA_AUTHORING_INVALID_ID ||
                         face_id < selected_face))
        {
            selected_face = face_id;
        }
    }
    if (selected_face == HENKA_AUTHORING_INVALID_ID)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_face_id = selected_face;
    return HENKA_SUCCESS;
}

static henka_result sandbox3d_modeling_operator_apply_face_normal(
    const henka_authoring_mesh* source,
    henka_authoring_mesh* candidate,
    henka_authoring_face_id face_id,
    float distance)
{
    const henka_authoring_face* face;
    henka_authoring_vertex_id unique_vertices[
        HENKA_AUTHORING_MESH_HARD_MAX_FACE_CORNERS];
    henka_vec3 normal;
    size_t unique_count = 0U;
    size_t corner;

    if (source == NULL || candidate == NULL || !isfinite(distance) ||
        fabsf(distance) > 100.0f)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    face = henka_authoring_mesh_get_face(source, face_id);
    if (face == NULL || face->vertices == NULL || face->corner_count < 3U ||
        face->corner_count > HENKA_AUTHORING_MESH_HARD_MAX_FACE_CORNERS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    {
        const henka_authoring_vertex* first = henka_authoring_mesh_get_vertex(
            source, face->vertices[0U]);
        const henka_authoring_vertex* second = henka_authoring_mesh_get_vertex(
            source, face->vertices[1U]);
        const henka_authoring_vertex* third = henka_authoring_mesh_get_vertex(
            source, face->vertices[2U]);
        const henka_vec3 first_edge = first != NULL && second != NULL
            ? henka_vec3_subtract(second->position, first->position)
            : (henka_vec3){0.0f, 0.0f, 0.0f};
        const henka_vec3 second_edge = first != NULL && third != NULL
            ? henka_vec3_subtract(third->position, first->position)
            : (henka_vec3){0.0f, 0.0f, 0.0f};
        const henka_vec3 cross = henka_vec3_cross(first_edge, second_edge);
        if (first == NULL || second == NULL || third == NULL ||
            henka_vec3_length(cross) <= 0.0001f)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        normal = henka_vec3_normalize(cross);
    }
    for (corner = 0U; corner < face->corner_count; ++corner)
    {
        henka_authoring_vertex_id vertex_id = face->vertices[corner];
        const henka_authoring_vertex* candidate_vertex;
        size_t index;
        bool seen = false;
        for (index = 0U; index < unique_count; ++index)
        {
            if (unique_vertices[index] == vertex_id)
            {
                seen = true;
                break;
            }
        }
        if (seen)
        {
            continue;
        }
        if (unique_count >= sizeof(unique_vertices) / sizeof(unique_vertices[0]))
        {
            return HENKA_ERROR_LIMIT;
        }
        unique_vertices[unique_count++] = vertex_id;
        candidate_vertex = henka_authoring_mesh_get_vertex(candidate, vertex_id);
        if (candidate_vertex == NULL ||
            henka_authoring_mesh_set_vertex_position(
                candidate,
                vertex_id,
                henka_vec3_add(
                    candidate_vertex->position,
                    henka_vec3_scale(normal, distance))) != HENKA_SUCCESS)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
    }
    return HENKA_SUCCESS;
}

static bool sandbox3d_modeling_operator_faces_share_vertex(
    const henka_authoring_face* first,
    const henka_authoring_face* second)
{
    size_t first_corner;
    size_t second_corner;

    if (first == NULL || second == NULL)
    {
        return true;
    }
    for (first_corner = 0U; first_corner < first->corner_count; ++first_corner)
    {
        for (second_corner = 0U; second_corner < second->corner_count; ++second_corner)
        {
            if (first->vertices[first_corner] == second->vertices[second_corner])
            {
                return true;
            }
        }
    }
    return false;
}

static bool sandbox3d_modeling_operator_face_selection_contains(
    const henka_authoring_face_id* face_ids,
    size_t face_count,
    henka_authoring_face_id face_id)
{
    size_t index;

    for (index = 0U; index < face_count; ++index)
    {
        if (face_ids[index] == face_id)
        {
            return true;
        }
    }
    return false;
}

static bool sandbox3d_modeling_operator_face_region_touches_unselected_surface(
    const henka_authoring_mesh* mesh,
    const henka_authoring_face_id* face_ids,
    size_t face_count)
{
    size_t face_index;

    if (mesh == NULL || face_ids == NULL || face_count == 0U)
    {
        return true;
    }
    for (face_index = 0U; face_index < face_count; ++face_index)
    {
        const henka_authoring_face* face = henka_authoring_mesh_get_face(
            mesh, face_ids[face_index]);
        size_t corner;
        if (face == NULL)
        {
            return true;
        }
        for (corner = 0U; corner < face->corner_count; ++corner)
        {
            const henka_authoring_edge* edge = henka_authoring_mesh_get_edge(
                mesh, face->edges[corner]);
            size_t incident;
            if (edge == NULL)
            {
                return true;
            }
            for (incident = 0U; incident < edge->face_count; ++incident)
            {
                if (edge->faces[incident] != face->id &&
                    !sandbox3d_modeling_operator_face_selection_contains(
                        face_ids, face_count, edge->faces[incident]))
                {
                    return true;
                }
            }
        }
    }
    return false;
}

static henka_result sandbox3d_modeling_operator_apply_face_normals(
    const henka_authoring_mesh* source,
    henka_authoring_mesh* candidate,
    const henka_authoring_face_id* face_ids,
    size_t face_count,
    float distance)
{
    size_t index;
    size_t other_index;

    if (source == NULL || candidate == NULL || face_ids == NULL || face_count == 0U ||
        !isfinite(distance))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (face_count > HENKA_AUTHORING_MESH_HARD_MAX_FACES)
    {
        return HENKA_ERROR_LIMIT;
    }
    for (index = 0U; index < face_count; ++index)
    {
        const henka_authoring_face* face = henka_authoring_mesh_get_face(
            source, face_ids[index]);
        if (face == NULL || face->vertices == NULL || face->corner_count < 3U)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        for (other_index = 0U; other_index < index; ++other_index)
        {
            const henka_authoring_face* other = henka_authoring_mesh_get_face(
                source, face_ids[other_index]);
            if (face_ids[index] == face_ids[other_index] ||
                sandbox3d_modeling_operator_faces_share_vertex(face, other))
            {
                return HENKA_ERROR_INVALID_ARGUMENT;
            }
        }
    }
    for (index = 0U; index < face_count; ++index)
    {
        const henka_result result = sandbox3d_modeling_operator_apply_face_normal(
            source, candidate, face_ids[index], distance);
        if (result != HENKA_SUCCESS)
        {
            return result;
        }
    }
    return HENKA_SUCCESS;
}

static henka_result sandbox3d_modeling_operator_collect_boundary_vertex_chain(
    const henka_authoring_mesh* mesh,
    const henka_authoring_vertex_id* vertex_ids,
    size_t vertex_count,
    henka_authoring_edge_id* out_edge_ids,
    size_t edge_capacity,
    size_t* out_edge_count)
{
    const henka_authoring_mesh_desc desc = mesh != NULL
        ? henka_authoring_mesh_get_desc(mesh)
        : (henka_authoring_mesh_desc){0};
    size_t face_slot;

    if (mesh == NULL || vertex_ids == NULL || vertex_count < 2U ||
        out_edge_ids == NULL || out_edge_count == NULL ||
        edge_capacity < vertex_count || !henka_authoring_mesh_validate(mesh))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_edge_count = 0U;
    for (face_slot = 0U; face_slot < desc.max_faces; ++face_slot)
    {
        henka_authoring_face_id face_id;
        const henka_authoring_face* face;
        size_t selected_index;
        size_t corner;
        size_t edge_count = 0U;
        bool all_in_face = true;

        if (henka_authoring_mesh_get_face_id_at(mesh, face_slot, &face_id) != HENKA_SUCCESS ||
            (face = henka_authoring_mesh_get_face(mesh, face_id)) == NULL)
        {
            continue;
        }
        for (selected_index = 0U; selected_index < vertex_count; ++selected_index)
        {
            size_t other_index;
            bool in_face = false;
            if (vertex_ids[selected_index] == HENKA_AUTHORING_INVALID_ID)
            {
                return HENKA_ERROR_INVALID_ARGUMENT;
            }
            for (other_index = 0U; other_index < selected_index; ++other_index)
            {
                if (vertex_ids[other_index] == vertex_ids[selected_index])
                {
                    return HENKA_ERROR_INVALID_ARGUMENT;
                }
            }
            for (corner = 0U; corner < face->corner_count; ++corner)
            {
                if (face->vertices[corner] == vertex_ids[selected_index])
                {
                    in_face = true;
                    break;
                }
            }
            if (!in_face)
            {
                all_in_face = false;
                break;
            }
        }
        if (!all_in_face)
        {
            continue;
        }
        for (corner = 0U; corner < face->corner_count; ++corner)
        {
            const henka_authoring_edge* edge = henka_authoring_mesh_get_edge(
                mesh, face->edges[corner]);
            if (edge == NULL || edge->face_count != 1U ||
                !sandbox3d_modeling_operator_selected_vertex_contains(
                    vertex_ids, vertex_count, edge->vertices[0]) ||
                !sandbox3d_modeling_operator_selected_vertex_contains(
                    vertex_ids, vertex_count, edge->vertices[1]))
            {
                continue;
            }
            if (edge_count >= edge_capacity)
            {
                return HENKA_ERROR_LIMIT;
            }
            out_edge_ids[edge_count++] = edge->id;
        }
        if (edge_count == vertex_count - 1U || edge_count == vertex_count)
        {
            *out_edge_count = edge_count;
            return HENKA_SUCCESS;
        }
        *out_edge_count = 0U;
    }
    return HENKA_ERROR_INVALID_ARGUMENT;
}

static void sandbox3d_modeling_operator_release(
    sandbox3d_modeling_operator_session* session)
{
    if (session == NULL)
    {
        return;
    }
    henka_authoring_mesh_destroy(session->source_snapshot);
    henka_free(session->selection_ids);
    henka_free(session->split_vertex_ids);
    henka_free(session->split_first_edges);
    henka_free(session->split_second_edges);
    henka_free(session->inset_result_faces);
    henka_free(session->extrude_result_faces);
    henka_free(session->bevel_result_faces);
    henka_free(session->bevel_result_vertices);
    henka_free(session->subdivide_result_vertices);
    henka_free(session->fill_result_faces);
    session->source_snapshot = NULL;
    session->selection_ids = NULL;
    session->split_vertex_ids = NULL;
    session->split_first_edges = NULL;
    session->split_second_edges = NULL;
    session->inset_result_faces = NULL;
    session->extrude_result_faces = NULL;
    session->bevel_result_faces = NULL;
    session->bevel_result_vertices = NULL;
    session->subdivide_result_vertices = NULL;
    session->fill_result_faces = NULL;
    session->selection_count = 0U;
    session->selection_capacity = 0U;
    session->split_result_count = 0U;
    session->inset_result_count = 0U;
    session->extrude_result_face_count = 0U;
    session->bevel_result_count = 0U;
    session->bevel_result_vertex_count = 0U;
    session->subdivide_result_count = 0U;
    session->fill_result_count = 0U;
}

void sandbox3d_modeling_operator_reset(
    sandbox3d_modeling_operator_session* session)
{
    if (session == NULL)
    {
        return;
    }
    sandbox3d_modeling_operator_release(session);
    memset(session, 0, sizeof(*session));
    session->state = SANDBOX3D_MODELING_OPERATOR_STATE_IDLE;
    session->kind = SANDBOX3D_MODELING_OPERATOR_NONE;
    session->axis = SANDBOX3D_MODELING_OPERATOR_AXIS_NONE;
    session->active_component_id = HENKA_AUTHORING_INVALID_ID;
    session->transform_scale = (henka_vec3){1.0f, 1.0f, 1.0f};
    session->transform_axis = (henka_vec3){0.0f, 1.0f, 0.0f};
    session->transform_radians = 0.0f;
    session->transform_pivot_mode = SANDBOX3D_AUTHORING_PIVOT_MEDIAN;
    session->transform_orientation_mode = SANDBOX3D_AUTHORING_ORIENTATION_LOCAL;
    session->transform_configured = false;
    session->proportional_ring_count = 1U;
    session->loose_vertex_position = (henka_vec3){0.0f, 0.0f, 0.0f};
    session->loose_vertex_uv = (henka_vec2){0.0f, 0.0f};
    session->loose_vertex_material_region = 0U;
    session->loose_edge_first = HENKA_AUTHORING_INVALID_ID;
    session->loose_edge_second = HENKA_AUTHORING_INVALID_ID;
    session->loose_edge_hard = false;
    session->loose_configured = false;
    session->split_first_edge = HENKA_AUTHORING_INVALID_ID;
    session->split_second_edge = HENKA_AUTHORING_INVALID_ID;
    session->split_result_count = 0U;
    session->split_configured = false;
    session->created_component_id = HENKA_AUTHORING_INVALID_ID;
}

henka_result sandbox3d_modeling_operator_begin(
    sandbox3d_modeling_operator_session* session,
    sandbox3d_authoring_object* object,
    sandbox3d_modeling_operator_kind kind)
{
    const henka_authoring_mesh* source;
    sandbox3d_authoring_selection_mode selection_mode;
    size_t selected_count;
    size_t index;
    size_t selection_limit;
    henka_result result = HENKA_SUCCESS;

    if (session == NULL || object == NULL || session->active ||
        (kind != SANDBOX3D_MODELING_OPERATOR_MOVE &&
         kind != SANDBOX3D_MODELING_OPERATOR_PROPORTIONAL_MOVE &&
         kind != SANDBOX3D_MODELING_OPERATOR_SMOOTH_VERTICES &&
         kind != SANDBOX3D_MODELING_OPERATOR_ADD_LOOSE_VERTEX &&
         kind != SANDBOX3D_MODELING_OPERATOR_ADD_LOOSE_EDGE &&
         kind != SANDBOX3D_MODELING_OPERATOR_TRANSFORM &&
         kind != SANDBOX3D_MODELING_OPERATOR_EDGE_SLIDE &&
         kind != SANDBOX3D_MODELING_OPERATOR_SPLIT_EDGE &&
         kind != SANDBOX3D_MODELING_OPERATOR_BEVEL &&
         kind != SANDBOX3D_MODELING_OPERATOR_EXTRUDE &&
         kind != SANDBOX3D_MODELING_OPERATOR_EDGE_EXTRUDE &&
         kind != SANDBOX3D_MODELING_OPERATOR_CONNECT &&
         kind != SANDBOX3D_MODELING_OPERATOR_TRIANGULATE &&
         kind != SANDBOX3D_MODELING_OPERATOR_INSET &&
         kind != SANDBOX3D_MODELING_OPERATOR_FACE_NORMAL &&
         kind != SANDBOX3D_MODELING_OPERATOR_FLIP_FACE &&
         kind != SANDBOX3D_MODELING_OPERATOR_DELETE_FACES &&
         kind != SANDBOX3D_MODELING_OPERATOR_DELETE_EDGE &&
         kind != SANDBOX3D_MODELING_OPERATOR_DISSOLVE_EDGE &&
         kind != SANDBOX3D_MODELING_OPERATOR_DISSOLVE_VERTICES &&
         kind != SANDBOX3D_MODELING_OPERATOR_DELETE_VERTICES &&
         kind != SANDBOX3D_MODELING_OPERATOR_RIP_VERTEX_FACE &&
         kind != SANDBOX3D_MODELING_OPERATOR_MERGE_VERTICES_CENTER &&
         kind != SANDBOX3D_MODELING_OPERATOR_MERGE_VERTICES_ACTIVE &&
         kind != SANDBOX3D_MODELING_OPERATOR_MERGE_VERTICES_DISTANCE &&
         kind != SANDBOX3D_MODELING_OPERATOR_SUBDIVIDE &&
         kind != SANDBOX3D_MODELING_OPERATOR_EDGE_BRIDGE &&
         kind != SANDBOX3D_MODELING_OPERATOR_FILL_BOUNDARY_LOOP &&
         kind != SANDBOX3D_MODELING_OPERATOR_UV_PROJECT &&
         kind != SANDBOX3D_MODELING_OPERATOR_UV_PACK &&
         kind != SANDBOX3D_MODELING_OPERATOR_UV_TRANSFORM &&
         kind != SANDBOX3D_MODELING_OPERATOR_UV_ISLAND_TRANSFORM &&
         kind != SANDBOX3D_MODELING_OPERATOR_UV_ISLAND_PACK &&
         kind != SANDBOX3D_MODELING_OPERATOR_UV_PACK_ALL &&
         kind != SANDBOX3D_MODELING_OPERATOR_UV_UNWRAP_PLANAR &&
         kind != SANDBOX3D_MODELING_OPERATOR_UV_UNWRAP_CYLINDRICAL &&
         kind != SANDBOX3D_MODELING_OPERATOR_UV_UNWRAP_SPHERICAL &&
         kind != SANDBOX3D_MODELING_OPERATOR_UV_SEAM_TOGGLE))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    source = sandbox3d_authoring_object_get_mesh(object);
    if (source == NULL || !henka_authoring_mesh_validate(source))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    selection_mode = sandbox3d_authoring_object_get_selection_mode(object);
    selection_limit = sandbox3d_modeling_operator_selection_limit(selection_mode);
    selected_count = sandbox3d_authoring_object_get_selected_component_count(object);
    if (kind == SANDBOX3D_MODELING_OPERATOR_ADD_LOOSE_VERTEX ||
        kind == SANDBOX3D_MODELING_OPERATOR_ADD_LOOSE_EDGE)
    {
        selected_count = 0U;
    }
    if (kind == SANDBOX3D_MODELING_OPERATOR_EDGE_SLIDE &&
        selection_mode != SANDBOX3D_AUTHORING_SELECTION_EDGE)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (kind == SANDBOX3D_MODELING_OPERATOR_SMOOTH_VERTICES &&
        (selection_mode != SANDBOX3D_AUTHORING_SELECTION_VERTEX ||
         selected_count == 0U))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (kind == SANDBOX3D_MODELING_OPERATOR_SPLIT_EDGE &&
        (selection_mode != SANDBOX3D_AUTHORING_SELECTION_EDGE ||
         selected_count == 0U))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (kind == SANDBOX3D_MODELING_OPERATOR_BEVEL &&
        selection_mode == SANDBOX3D_AUTHORING_SELECTION_FACE &&
        selected_count == 0U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (kind == SANDBOX3D_MODELING_OPERATOR_BEVEL &&
        selection_mode != SANDBOX3D_AUTHORING_SELECTION_VERTEX &&
        selection_mode != SANDBOX3D_AUTHORING_SELECTION_EDGE &&
        selection_mode != SANDBOX3D_AUTHORING_SELECTION_FACE)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (kind == SANDBOX3D_MODELING_OPERATOR_EXTRUDE &&
        (selection_mode != SANDBOX3D_AUTHORING_SELECTION_VERTEX &&
         selection_mode != SANDBOX3D_AUTHORING_SELECTION_EDGE &&
         selection_mode != SANDBOX3D_AUTHORING_SELECTION_FACE))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (kind == SANDBOX3D_MODELING_OPERATOR_EXTRUDE &&
        selection_mode == SANDBOX3D_AUTHORING_SELECTION_EDGE && selected_count == 0U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (kind == SANDBOX3D_MODELING_OPERATOR_EDGE_EXTRUDE &&
        (selection_mode != SANDBOX3D_AUTHORING_SELECTION_EDGE || selected_count == 0U))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (kind == SANDBOX3D_MODELING_OPERATOR_CONNECT &&
        (selection_mode != SANDBOX3D_AUTHORING_SELECTION_VERTEX || selected_count != 2U))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (kind == SANDBOX3D_MODELING_OPERATOR_TRIANGULATE &&
        (selection_mode != SANDBOX3D_AUTHORING_SELECTION_FACE || selected_count == 0U))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (kind == SANDBOX3D_MODELING_OPERATOR_INSET &&
        (selection_mode != SANDBOX3D_AUTHORING_SELECTION_FACE || selected_count == 0U))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (kind == SANDBOX3D_MODELING_OPERATOR_FACE_NORMAL &&
        (selection_mode != SANDBOX3D_AUTHORING_SELECTION_FACE || selected_count == 0U))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (kind == SANDBOX3D_MODELING_OPERATOR_FLIP_FACE &&
        (selection_mode != SANDBOX3D_AUTHORING_SELECTION_FACE || selected_count == 0U))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (kind == SANDBOX3D_MODELING_OPERATOR_DELETE_FACES &&
        (selection_mode != SANDBOX3D_AUTHORING_SELECTION_FACE || selected_count == 0U))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (kind == SANDBOX3D_MODELING_OPERATOR_DELETE_EDGE &&
        (selection_mode != SANDBOX3D_AUTHORING_SELECTION_EDGE || selected_count == 0U))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (kind == SANDBOX3D_MODELING_OPERATOR_DISSOLVE_EDGE &&
        (selection_mode != SANDBOX3D_AUTHORING_SELECTION_EDGE || selected_count == 0U))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (kind == SANDBOX3D_MODELING_OPERATOR_DISSOLVE_VERTICES &&
        (selection_mode != SANDBOX3D_AUTHORING_SELECTION_VERTEX || selected_count == 0U))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (kind == SANDBOX3D_MODELING_OPERATOR_DELETE_VERTICES &&
        (selection_mode != SANDBOX3D_AUTHORING_SELECTION_VERTEX || selected_count == 0U))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (kind == SANDBOX3D_MODELING_OPERATOR_RIP_VERTEX_FACE &&
        (selection_mode != SANDBOX3D_AUTHORING_SELECTION_VERTEX || selected_count == 0U))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (kind == SANDBOX3D_MODELING_OPERATOR_MERGE_VERTICES_CENTER &&
        (selection_mode != SANDBOX3D_AUTHORING_SELECTION_VERTEX || selected_count < 2U))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (kind == SANDBOX3D_MODELING_OPERATOR_MERGE_VERTICES_ACTIVE &&
        (selection_mode != SANDBOX3D_AUTHORING_SELECTION_VERTEX || selected_count < 2U))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (kind == SANDBOX3D_MODELING_OPERATOR_MERGE_VERTICES_DISTANCE &&
        (selection_mode != SANDBOX3D_AUTHORING_SELECTION_VERTEX || selected_count < 2U))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (kind == SANDBOX3D_MODELING_OPERATOR_SUBDIVIDE &&
        (selection_mode != SANDBOX3D_AUTHORING_SELECTION_FACE || selected_count == 0U))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (kind == SANDBOX3D_MODELING_OPERATOR_EDGE_BRIDGE &&
        (selection_mode != SANDBOX3D_AUTHORING_SELECTION_EDGE ||
         selected_count < 2U ||
         (selected_count != 2U && (selected_count & 1U) != 0U)))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (kind == SANDBOX3D_MODELING_OPERATOR_FILL_BOUNDARY_LOOP &&
        (selection_mode != SANDBOX3D_AUTHORING_SELECTION_EDGE || selected_count < 3U))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (kind == SANDBOX3D_MODELING_OPERATOR_UV_SEAM_TOGGLE &&
        (selection_mode != SANDBOX3D_AUTHORING_SELECTION_EDGE || selected_count == 0U))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if ((kind == SANDBOX3D_MODELING_OPERATOR_UV_PROJECT ||
         kind == SANDBOX3D_MODELING_OPERATOR_UV_PACK ||
         kind == SANDBOX3D_MODELING_OPERATOR_UV_TRANSFORM ||
          kind == SANDBOX3D_MODELING_OPERATOR_UV_ISLAND_TRANSFORM ||
          kind == SANDBOX3D_MODELING_OPERATOR_UV_ISLAND_PACK ||
         kind == SANDBOX3D_MODELING_OPERATOR_UV_PACK_ALL ||
         kind == SANDBOX3D_MODELING_OPERATOR_UV_UNWRAP_PLANAR ||
         kind == SANDBOX3D_MODELING_OPERATOR_UV_UNWRAP_CYLINDRICAL ||
         kind == SANDBOX3D_MODELING_OPERATOR_UV_UNWRAP_SPHERICAL) &&
        (selection_mode != SANDBOX3D_AUTHORING_SELECTION_FACE || selected_count != 1U))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (selection_limit == 0U ||
        ((kind != SANDBOX3D_MODELING_OPERATOR_ADD_LOOSE_VERTEX &&
          kind != SANDBOX3D_MODELING_OPERATOR_ADD_LOOSE_EDGE) && selected_count == 0U) ||
        selected_count > selection_limit ||
        selected_count > SIZE_MAX / sizeof(uint32_t))
    {
        return HENKA_ERROR_LIMIT;
    }
    sandbox3d_modeling_operator_reset(session);
    if (selected_count > 0U)
    {
        session->selection_ids = henka_calloc(selected_count, sizeof(*session->selection_ids));
        if (session->selection_ids == NULL)
        {
            sandbox3d_modeling_operator_reset(session);
            return HENKA_ERROR_OUT_OF_MEMORY;
        }
        for (index = 0U; index < selected_count; ++index)
        {
            if (sandbox3d_authoring_object_get_selected_component_at(
                    object, index, &session->selection_ids[index]) != HENKA_SUCCESS ||
                session->selection_ids[index] == HENKA_AUTHORING_INVALID_ID)
            {
                sandbox3d_modeling_operator_reset(session);
                return HENKA_ERROR_INVALID_ARGUMENT;
            }
        }
    }
    result = henka_authoring_mesh_clone(source, &session->source_snapshot);
    if (result != HENKA_SUCCESS)
    {
        sandbox3d_modeling_operator_reset(session);
        return result;
    }
    session->active = true;
    session->state = SANDBOX3D_MODELING_OPERATOR_STATE_BEGIN;
    session->kind = kind;
    session->axis = SANDBOX3D_MODELING_OPERATOR_AXIS_NONE;
    session->object = object;
    session->selection_mode = selection_mode;
    session->active_component_id =
        sandbox3d_authoring_object_get_active_component_id(object);
    session->selection_count = selected_count;
    session->selection_capacity = selected_count;
    session->amount = kind == SANDBOX3D_MODELING_OPERATOR_SPLIT_EDGE
        ? 0.5f
        : 0.0f;
    session->preview_rebuild_count = 0U;
    session->transform_scale = (henka_vec3){1.0f, 1.0f, 1.0f};
    session->transform_axis = (henka_vec3){0.0f, 1.0f, 0.0f};
    session->transform_radians = 0.0f;
    session->transform_pivot_mode = SANDBOX3D_AUTHORING_PIVOT_MEDIAN;
    session->transform_orientation_mode = SANDBOX3D_AUTHORING_ORIENTATION_LOCAL;
    session->transform_configured = false;
    session->proportional_ring_count = 1U;
    session->loose_vertex_position = (henka_vec3){0.0f, 0.0f, 0.0f};
    session->loose_vertex_uv = (henka_vec2){0.0f, 0.0f};
    session->loose_vertex_material_region = 0U;
    session->loose_edge_first = HENKA_AUTHORING_INVALID_ID;
    session->loose_edge_second = HENKA_AUTHORING_INVALID_ID;
    session->loose_edge_hard = false;
    session->loose_configured = false;
    session->created_component_id = HENKA_AUTHORING_INVALID_ID;
    if (kind == SANDBOX3D_MODELING_OPERATOR_SPLIT_EDGE)
    {
        if (selected_count > SIZE_MAX / 2U)
        {
            sandbox3d_modeling_operator_reset(session);
            return HENKA_ERROR_LIMIT;
        }
        result = sandbox3d_authoring_object_reserve_component_selection_capacity(
            object, selected_count * 2U);
        if (result != HENKA_SUCCESS)
        {
            sandbox3d_modeling_operator_reset(session);
            return result;
        }
    }
    if (kind == SANDBOX3D_MODELING_OPERATOR_INSET ||
        kind == SANDBOX3D_MODELING_OPERATOR_BEVEL ||
        kind == SANDBOX3D_MODELING_OPERATOR_SUBDIVIDE)
    {
        result = sandbox3d_authoring_object_reserve_component_selection_capacity(
            object, selected_count);
        if (result != HENKA_SUCCESS)
        {
            sandbox3d_modeling_operator_reset(session);
            return result;
        }
    }
    return HENKA_SUCCESS;
}

henka_result sandbox3d_modeling_operator_set_axis(
    sandbox3d_modeling_operator_session* session,
    sandbox3d_modeling_operator_axis axis)
{
    if (session == NULL || !session->active ||
        axis < SANDBOX3D_MODELING_OPERATOR_AXIS_NONE ||
        axis > SANDBOX3D_MODELING_OPERATOR_AXIS_Z)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    session->axis = axis;
    return HENKA_SUCCESS;
}

henka_result sandbox3d_modeling_operator_set_proportional_ring_count(
    sandbox3d_modeling_operator_session* session,
    size_t ring_count)
{
    if (session == NULL || !session->active ||
        session->kind != SANDBOX3D_MODELING_OPERATOR_PROPORTIONAL_MOVE ||
        session->state != SANDBOX3D_MODELING_OPERATOR_STATE_BEGIN ||
        ring_count > 8U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    session->proportional_ring_count = ring_count;
    return HENKA_SUCCESS;
}

henka_result sandbox3d_modeling_operator_set_loose_vertex(
    sandbox3d_modeling_operator_session* session,
    henka_vec3 position,
    henka_vec2 uv,
    uint32_t material_region)
{
    if (session == NULL || !session->active ||
        session->kind != SANDBOX3D_MODELING_OPERATOR_ADD_LOOSE_VERTEX ||
        session->state != SANDBOX3D_MODELING_OPERATOR_STATE_BEGIN ||
        !isfinite(position.x) || !isfinite(position.y) || !isfinite(position.z) ||
        !isfinite(uv.x) || !isfinite(uv.y))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    session->loose_vertex_position = position;
    session->loose_vertex_uv = uv;
    session->loose_vertex_material_region = material_region;
    session->loose_configured = true;
    return HENKA_SUCCESS;
}

henka_result sandbox3d_modeling_operator_set_loose_edge(
    sandbox3d_modeling_operator_session* session,
    henka_authoring_vertex_id first,
    henka_authoring_vertex_id second,
    bool hard)
{
    if (session == NULL || !session->active ||
        session->kind != SANDBOX3D_MODELING_OPERATOR_ADD_LOOSE_EDGE ||
        session->state != SANDBOX3D_MODELING_OPERATOR_STATE_BEGIN ||
        first == HENKA_AUTHORING_INVALID_ID ||
        second == HENKA_AUTHORING_INVALID_ID || first == second)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    session->loose_edge_first = first;
    session->loose_edge_second = second;
    session->loose_edge_hard = hard;
    session->loose_configured = true;
    return HENKA_SUCCESS;
}

henka_result sandbox3d_modeling_operator_set_transform(
    sandbox3d_modeling_operator_session* session,
    henka_vec3 scale,
    henka_vec3 axis,
    float radians,
    sandbox3d_authoring_pivot_mode pivot_mode,
    sandbox3d_authoring_orientation_mode orientation_mode)
{
    if (session == NULL || !session->active ||
        session->kind != SANDBOX3D_MODELING_OPERATOR_TRANSFORM ||
        session->state != SANDBOX3D_MODELING_OPERATOR_STATE_BEGIN ||
        !isfinite(scale.x) || !isfinite(scale.y) || !isfinite(scale.z) ||
        scale.x <= 0.0f || scale.y <= 0.0f || scale.z <= 0.0f ||
        scale.x > 4.0f || scale.y > 4.0f || scale.z > 4.0f ||
        !isfinite(axis.x) || !isfinite(axis.y) || !isfinite(axis.z) ||
        henka_vec3_length(axis) <= 0.000001f || !isfinite(radians) ||
        pivot_mode < SANDBOX3D_AUTHORING_PIVOT_MEDIAN ||
        pivot_mode > SANDBOX3D_AUTHORING_PIVOT_INDIVIDUAL ||
        orientation_mode < SANDBOX3D_AUTHORING_ORIENTATION_WORLD ||
        orientation_mode > SANDBOX3D_AUTHORING_ORIENTATION_NORMAL ||
        (pivot_mode == SANDBOX3D_AUTHORING_PIVOT_INDIVIDUAL &&
            session->selection_mode != SANDBOX3D_AUTHORING_SELECTION_FACE))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    session->transform_scale = scale;
    session->transform_axis = axis;
    session->transform_radians = radians;
    session->transform_pivot_mode = pivot_mode;
    session->transform_orientation_mode = orientation_mode;
    session->transform_configured = true;
    return HENKA_SUCCESS;
}

static bool sandbox3d_modeling_operator_numeric_character_valid(char character)
{
    return (character >= '0' && character <= '9') ||
        character == '+' || character == '-' || character == '.';
}

henka_result sandbox3d_modeling_operator_numeric_begin(
    sandbox3d_modeling_operator_session* session)
{
    if (session == NULL || !session->active)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    session->numeric_active = true;
    session->numeric_length = 0U;
    session->numeric_text[0] = '\0';
    return HENKA_SUCCESS;
}

henka_result sandbox3d_modeling_operator_numeric_append(
    sandbox3d_modeling_operator_session* session,
    const char* text,
    size_t text_size)
{
    size_t index;

    if (session == NULL || !session->active || !session->numeric_active ||
        (text == NULL && text_size != 0U))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (text_size > sizeof(session->numeric_text) - 1U - session->numeric_length)
    {
        return HENKA_ERROR_LIMIT;
    }
    for (index = 0U; index < text_size; ++index)
    {
        if (!sandbox3d_modeling_operator_numeric_character_valid(text[index]))
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
    }
    memcpy(session->numeric_text + session->numeric_length, text, text_size);
    session->numeric_length += text_size;
    session->numeric_text[session->numeric_length] = '\0';
    return HENKA_SUCCESS;
}

henka_result sandbox3d_modeling_operator_numeric_backspace(
    sandbox3d_modeling_operator_session* session)
{
    if (session == NULL || !session->active || !session->numeric_active)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (session->numeric_length > 0U)
    {
        --session->numeric_length;
        session->numeric_text[session->numeric_length] = '\0';
    }
    return HENKA_SUCCESS;
}

const char* sandbox3d_modeling_operator_get_numeric_text(
    const sandbox3d_modeling_operator_session* session)
{
    return session == NULL || !session->numeric_active
        ? ""
        : session->numeric_text;
}

static bool sandbox3d_modeling_operator_append_vertex(
    henka_authoring_vertex_id* vertices,
    size_t* inout_count,
    size_t capacity,
    henka_authoring_vertex_id vertex_id)
{
    size_t index;

    if (vertices == NULL || inout_count == NULL || vertex_id == HENKA_AUTHORING_INVALID_ID)
    {
        return false;
    }
    for (index = 0U; index < *inout_count; ++index)
    {
        if (vertices[index] == vertex_id)
        {
            return true;
        }
    }
    if (*inout_count >= capacity)
    {
        return false;
    }
    vertices[*inout_count] = vertex_id;
    ++*inout_count;
    return true;
}

static henka_result sandbox3d_modeling_operator_collect_vertices(
    const sandbox3d_modeling_operator_session* session,
    henka_authoring_vertex_id* out_vertices,
    size_t vertex_capacity,
    size_t* out_count)
{
    size_t index;
    size_t vertex_count = 0U;

    if (session == NULL || session->source_snapshot == NULL || out_vertices == NULL ||
        out_count == NULL || vertex_capacity == 0U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    for (index = 0U; index < session->selection_count; ++index)
    {
        const uint32_t selected_id = session->selection_ids[index];
        if (session->selection_mode == SANDBOX3D_AUTHORING_SELECTION_VERTEX)
        {
            if (henka_authoring_mesh_get_vertex(
                    session->source_snapshot, (henka_authoring_vertex_id)selected_id) == NULL ||
                !sandbox3d_modeling_operator_append_vertex(
                    out_vertices, &vertex_count, vertex_capacity,
                    (henka_authoring_vertex_id)selected_id))
            {
                return HENKA_ERROR_INVALID_ARGUMENT;
            }
        }
        else if (session->selection_mode == SANDBOX3D_AUTHORING_SELECTION_EDGE)
        {
            const henka_authoring_edge* edge = henka_authoring_mesh_get_edge(
                session->source_snapshot, (henka_authoring_edge_id)selected_id);
            if (edge == NULL ||
                !sandbox3d_modeling_operator_append_vertex(
                    out_vertices, &vertex_count, vertex_capacity, edge->vertices[0]) ||
                !sandbox3d_modeling_operator_append_vertex(
                    out_vertices, &vertex_count, vertex_capacity, edge->vertices[1]))
            {
                return HENKA_ERROR_INVALID_ARGUMENT;
            }
        }
        else if (session->selection_mode == SANDBOX3D_AUTHORING_SELECTION_FACE)
        {
            const henka_authoring_face* face = henka_authoring_mesh_get_face(
                session->source_snapshot, (henka_authoring_face_id)selected_id);
            size_t corner;
            if (face == NULL || face->vertices == NULL ||
                face->corner_count < 3U ||
                face->corner_count > HENKA_AUTHORING_MESH_HARD_MAX_FACE_CORNERS)
            {
                return HENKA_ERROR_INVALID_ARGUMENT;
            }
            for (corner = 0U; corner < face->corner_count; ++corner)
            {
                if (henka_authoring_mesh_get_vertex(
                        session->source_snapshot, face->vertices[corner]) == NULL ||
                    !sandbox3d_modeling_operator_append_vertex(
                        out_vertices, &vertex_count, vertex_capacity,
                        face->vertices[corner]))
                {
                    return HENKA_ERROR_INVALID_ARGUMENT;
                }
            }
        }
        else
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
    }
    *out_count = vertex_count;
    return vertex_count > 0U ? HENKA_SUCCESS : HENKA_ERROR_INVALID_ARGUMENT;
}

static float sandbox3d_modeling_operator_snap_amount(float amount)
{
    return roundf(amount / 0.25f) * 0.25f;
}

henka_result sandbox3d_modeling_operator_preview(
    sandbox3d_modeling_operator_session* session,
    float delta,
    bool snap_active,
    bool fine_active)
{
    const henka_authoring_mesh_counts counts = session != NULL && session->source_snapshot != NULL
        ? henka_authoring_mesh_get_counts(session->source_snapshot)
        : (henka_authoring_mesh_counts){0};
    henka_authoring_vertex_id* vertices = NULL;
    henka_authoring_vertex_id* merge_survivors = NULL;
    henka_authoring_vertex_id* bevel_result_vertices = NULL;
    henka_authoring_mesh* candidate = NULL;
    henka_authoring_modeling_report report = {0};
    henka_authoring_vertex_id extrude_result_vertex = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_edge_id extrude_result_edge = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_face_id extrude_result_face = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_face_id bridge_result_face = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_vertex_id* split_vertex_ids = NULL;
    henka_authoring_edge_id* split_first_edges = NULL;
    henka_authoring_edge_id* split_second_edges = NULL;
    henka_authoring_edge_id* boundary_chain_edges = NULL;
    henka_authoring_face_id* inset_result_faces = NULL;
    henka_authoring_face_id* extrude_result_faces = NULL;
    henka_authoring_face_id* bevel_result_faces = NULL;
    henka_authoring_vertex_id* subdivide_result_vertices = NULL;
    henka_authoring_face_id* fill_result_faces = NULL;
    henka_authoring_face_id* rip_face_ids = NULL;
    henka_authoring_mesh_desc mesh_desc = {0};
    size_t affected_count = 0U;
    size_t merge_survivor_count = 0U;
    size_t bevel_result_count = 0U;
    size_t bevel_result_capacity = 0U;
    size_t extrude_result_face_count = 0U;
    size_t fill_result_count = 0U;
    size_t previous_face_max_id = 0U;
    size_t index;
    float next_amount;
    float applied_amount;
    bool use_loose_edge_batch = false;
    bool use_face_edge_batch = false;
    bool extrude_face_region_touches_unselected = false;
    henka_vec3 offset = {0.0f, 0.0f, 0.0f};
    henka_result result = HENKA_SUCCESS;

    if (session == NULL || !session->active ||
        (session->kind != SANDBOX3D_MODELING_OPERATOR_MOVE &&
         session->kind != SANDBOX3D_MODELING_OPERATOR_PROPORTIONAL_MOVE &&
         session->kind != SANDBOX3D_MODELING_OPERATOR_SMOOTH_VERTICES &&
         session->kind != SANDBOX3D_MODELING_OPERATOR_TRANSFORM &&
         session->kind != SANDBOX3D_MODELING_OPERATOR_ADD_LOOSE_VERTEX &&
         session->kind != SANDBOX3D_MODELING_OPERATOR_ADD_LOOSE_EDGE &&
         session->kind != SANDBOX3D_MODELING_OPERATOR_EDGE_SLIDE &&
         session->kind != SANDBOX3D_MODELING_OPERATOR_SPLIT_EDGE &&
         session->kind != SANDBOX3D_MODELING_OPERATOR_BEVEL &&
         session->kind != SANDBOX3D_MODELING_OPERATOR_EXTRUDE &&
         session->kind != SANDBOX3D_MODELING_OPERATOR_EDGE_EXTRUDE &&
         session->kind != SANDBOX3D_MODELING_OPERATOR_CONNECT &&
         session->kind != SANDBOX3D_MODELING_OPERATOR_TRIANGULATE &&
         session->kind != SANDBOX3D_MODELING_OPERATOR_INSET &&
         session->kind != SANDBOX3D_MODELING_OPERATOR_FACE_NORMAL &&
         session->kind != SANDBOX3D_MODELING_OPERATOR_FLIP_FACE &&
         session->kind != SANDBOX3D_MODELING_OPERATOR_DELETE_FACES &&
         session->kind != SANDBOX3D_MODELING_OPERATOR_DELETE_EDGE &&
         session->kind != SANDBOX3D_MODELING_OPERATOR_DISSOLVE_EDGE &&
         session->kind != SANDBOX3D_MODELING_OPERATOR_DISSOLVE_VERTICES &&
         session->kind != SANDBOX3D_MODELING_OPERATOR_DELETE_VERTICES &&
         session->kind != SANDBOX3D_MODELING_OPERATOR_RIP_VERTEX_FACE &&
         session->kind != SANDBOX3D_MODELING_OPERATOR_MERGE_VERTICES_CENTER &&
         session->kind != SANDBOX3D_MODELING_OPERATOR_MERGE_VERTICES_ACTIVE &&
         session->kind != SANDBOX3D_MODELING_OPERATOR_MERGE_VERTICES_DISTANCE &&
         session->kind != SANDBOX3D_MODELING_OPERATOR_SUBDIVIDE &&
         session->kind != SANDBOX3D_MODELING_OPERATOR_EDGE_BRIDGE &&
         session->kind != SANDBOX3D_MODELING_OPERATOR_FILL_BOUNDARY_LOOP &&
         session->kind != SANDBOX3D_MODELING_OPERATOR_UV_PROJECT &&
         session->kind != SANDBOX3D_MODELING_OPERATOR_UV_PACK &&
         session->kind != SANDBOX3D_MODELING_OPERATOR_UV_TRANSFORM &&
         session->kind != SANDBOX3D_MODELING_OPERATOR_UV_ISLAND_TRANSFORM &&
         session->kind != SANDBOX3D_MODELING_OPERATOR_UV_ISLAND_PACK &&
         session->kind != SANDBOX3D_MODELING_OPERATOR_UV_PACK_ALL &&
         session->kind != SANDBOX3D_MODELING_OPERATOR_UV_UNWRAP_PLANAR &&
         session->kind != SANDBOX3D_MODELING_OPERATOR_UV_UNWRAP_CYLINDRICAL &&
         session->kind != SANDBOX3D_MODELING_OPERATOR_UV_UNWRAP_SPHERICAL &&
         session->kind != SANDBOX3D_MODELING_OPERATOR_UV_SEAM_TOGGLE) ||
         session->source_snapshot == NULL || session->object == NULL ||
         (((session->kind != SANDBOX3D_MODELING_OPERATOR_ADD_LOOSE_VERTEX &&
            session->kind != SANDBOX3D_MODELING_OPERATOR_ADD_LOOSE_EDGE) &&
           (session->selection_ids == NULL || session->selection_count == 0U))) ||
         ((session->kind == SANDBOX3D_MODELING_OPERATOR_ADD_LOOSE_VERTEX ||
           session->kind == SANDBOX3D_MODELING_OPERATOR_ADD_LOOSE_EDGE) &&
          !session->loose_configured) ||
        ((session->kind == SANDBOX3D_MODELING_OPERATOR_MOVE ||
          session->kind == SANDBOX3D_MODELING_OPERATOR_PROPORTIONAL_MOVE) &&
            session->axis == SANDBOX3D_MODELING_OPERATOR_AXIS_NONE) ||
        (session->kind == SANDBOX3D_MODELING_OPERATOR_SMOOTH_VERTICES &&
            session->selection_mode != SANDBOX3D_AUTHORING_SELECTION_VERTEX) ||
        (session->kind == SANDBOX3D_MODELING_OPERATOR_TRANSFORM &&
            !session->transform_configured) ||
        (session->kind == SANDBOX3D_MODELING_OPERATOR_EDGE_SLIDE &&
            session->selection_mode != SANDBOX3D_AUTHORING_SELECTION_EDGE) ||
        (session->kind == SANDBOX3D_MODELING_OPERATOR_SPLIT_EDGE &&
            (session->selection_mode != SANDBOX3D_AUTHORING_SELECTION_EDGE ||
             session->selection_count == 0U)) ||
        (session->kind == SANDBOX3D_MODELING_OPERATOR_BEVEL &&
            session->selection_mode == SANDBOX3D_AUTHORING_SELECTION_FACE &&
            session->selection_count == 0U) ||
        (session->kind == SANDBOX3D_MODELING_OPERATOR_EXTRUDE &&
            (session->selection_mode != SANDBOX3D_AUTHORING_SELECTION_VERTEX &&
             session->selection_mode != SANDBOX3D_AUTHORING_SELECTION_EDGE &&
             session->selection_mode != SANDBOX3D_AUTHORING_SELECTION_FACE)) ||
        (session->kind == SANDBOX3D_MODELING_OPERATOR_EXTRUDE &&
            session->selection_mode == SANDBOX3D_AUTHORING_SELECTION_EDGE &&
            session->selection_count == 0U) ||
        (session->kind == SANDBOX3D_MODELING_OPERATOR_EXTRUDE &&
            session->selection_mode != SANDBOX3D_AUTHORING_SELECTION_FACE &&
            session->axis == SANDBOX3D_MODELING_OPERATOR_AXIS_NONE) ||
        (session->kind == SANDBOX3D_MODELING_OPERATOR_EDGE_EXTRUDE &&
            (session->selection_mode != SANDBOX3D_AUTHORING_SELECTION_EDGE ||
             session->selection_count == 0U)) ||
        (session->kind == SANDBOX3D_MODELING_OPERATOR_CONNECT &&
            (session->selection_mode != SANDBOX3D_AUTHORING_SELECTION_VERTEX ||
             session->selection_count != 2U)) ||
        (session->kind == SANDBOX3D_MODELING_OPERATOR_TRIANGULATE &&
            (session->selection_mode != SANDBOX3D_AUTHORING_SELECTION_FACE ||
             session->selection_count == 0U)) ||
        (session->kind == SANDBOX3D_MODELING_OPERATOR_INSET &&
            (session->selection_mode != SANDBOX3D_AUTHORING_SELECTION_FACE ||
             session->selection_count == 0U)) ||
        (session->kind == SANDBOX3D_MODELING_OPERATOR_FACE_NORMAL &&
            (session->selection_mode != SANDBOX3D_AUTHORING_SELECTION_FACE ||
             session->selection_count == 0U)) ||
        (session->kind == SANDBOX3D_MODELING_OPERATOR_FLIP_FACE &&
            (session->selection_mode != SANDBOX3D_AUTHORING_SELECTION_FACE ||
             session->selection_count == 0U)) ||
        (session->kind == SANDBOX3D_MODELING_OPERATOR_DELETE_FACES &&
            (session->selection_mode != SANDBOX3D_AUTHORING_SELECTION_FACE ||
             session->selection_count == 0U)) ||
        (session->kind == SANDBOX3D_MODELING_OPERATOR_DELETE_EDGE &&
            (session->selection_mode != SANDBOX3D_AUTHORING_SELECTION_EDGE ||
             session->selection_count == 0U)) ||
        (session->kind == SANDBOX3D_MODELING_OPERATOR_DISSOLVE_EDGE &&
            (session->selection_mode != SANDBOX3D_AUTHORING_SELECTION_EDGE ||
             session->selection_count == 0U)) ||
        (session->kind == SANDBOX3D_MODELING_OPERATOR_DISSOLVE_VERTICES &&
            (session->selection_mode != SANDBOX3D_AUTHORING_SELECTION_VERTEX ||
             session->selection_count == 0U)) ||
        (session->kind == SANDBOX3D_MODELING_OPERATOR_DELETE_VERTICES &&
            (session->selection_mode != SANDBOX3D_AUTHORING_SELECTION_VERTEX ||
             session->selection_count == 0U)) ||
        (session->kind == SANDBOX3D_MODELING_OPERATOR_RIP_VERTEX_FACE &&
            (session->selection_mode != SANDBOX3D_AUTHORING_SELECTION_VERTEX ||
             session->selection_count == 0U)) ||
        (session->kind == SANDBOX3D_MODELING_OPERATOR_MERGE_VERTICES_CENTER &&
            (session->selection_mode != SANDBOX3D_AUTHORING_SELECTION_VERTEX ||
             session->selection_count < 2U)) ||
        (session->kind == SANDBOX3D_MODELING_OPERATOR_MERGE_VERTICES_ACTIVE &&
            (session->selection_mode != SANDBOX3D_AUTHORING_SELECTION_VERTEX ||
             session->selection_count < 2U)) ||
        (session->kind == SANDBOX3D_MODELING_OPERATOR_MERGE_VERTICES_DISTANCE &&
            (session->selection_mode != SANDBOX3D_AUTHORING_SELECTION_VERTEX ||
             session->selection_count < 2U)) ||
        (session->kind == SANDBOX3D_MODELING_OPERATOR_SUBDIVIDE &&
            (session->selection_mode != SANDBOX3D_AUTHORING_SELECTION_FACE ||
             session->selection_count == 0U)) ||
        (session->kind == SANDBOX3D_MODELING_OPERATOR_EDGE_BRIDGE &&
            (session->selection_mode != SANDBOX3D_AUTHORING_SELECTION_EDGE ||
             session->selection_count < 2U ||
             (session->selection_count != 2U &&
              (session->selection_count & 1U) != 0U))) ||
        (session->kind == SANDBOX3D_MODELING_OPERATOR_FILL_BOUNDARY_LOOP &&
            (session->selection_mode != SANDBOX3D_AUTHORING_SELECTION_EDGE ||
             session->selection_count < 3U)) ||
        (session->kind == SANDBOX3D_MODELING_OPERATOR_UV_SEAM_TOGGLE &&
            (session->selection_mode != SANDBOX3D_AUTHORING_SELECTION_EDGE ||
             session->selection_count == 0U)) ||
        ((session->kind == SANDBOX3D_MODELING_OPERATOR_UV_PROJECT ||
          session->kind == SANDBOX3D_MODELING_OPERATOR_UV_PACK ||
          session->kind == SANDBOX3D_MODELING_OPERATOR_UV_TRANSFORM ||
          session->kind == SANDBOX3D_MODELING_OPERATOR_UV_ISLAND_TRANSFORM ||
          session->kind == SANDBOX3D_MODELING_OPERATOR_UV_ISLAND_PACK ||
          session->kind == SANDBOX3D_MODELING_OPERATOR_UV_PACK_ALL ||
          session->kind == SANDBOX3D_MODELING_OPERATOR_UV_UNWRAP_PLANAR ||
          session->kind == SANDBOX3D_MODELING_OPERATOR_UV_UNWRAP_CYLINDRICAL ||
          session->kind == SANDBOX3D_MODELING_OPERATOR_UV_UNWRAP_SPHERICAL) &&
            (session->selection_mode != SANDBOX3D_AUTHORING_SELECTION_FACE ||
             session->selection_count != 1U)) ||
        ((session->kind == SANDBOX3D_MODELING_OPERATOR_UV_PROJECT ||
          session->kind == SANDBOX3D_MODELING_OPERATOR_UV_UNWRAP_CYLINDRICAL ||
          session->kind == SANDBOX3D_MODELING_OPERATOR_UV_UNWRAP_SPHERICAL) &&
            session->axis == SANDBOX3D_MODELING_OPERATOR_AXIS_NONE) ||
        !isfinite(delta) ||
        !isfinite(session->amount))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (session->preview_rebuild_count == SIZE_MAX)
    {
        return HENKA_ERROR_LIMIT;
    }
    next_amount = session->amount + delta * (fine_active ? 0.2f : 1.0f);
    if (!isfinite(next_amount))
    {
        return HENKA_ERROR_LIMIT;
    }
    applied_amount = snap_active
        ? sandbox3d_modeling_operator_snap_amount(next_amount)
        : next_amount;
    if (!isfinite(applied_amount) ||
        (session->kind == SANDBOX3D_MODELING_OPERATOR_EDGE_SLIDE &&
            (applied_amount <= -1.0f || applied_amount >= 1.0f)) ||
        (session->kind == SANDBOX3D_MODELING_OPERATOR_SPLIT_EDGE &&
            (applied_amount <= 0.0f || applied_amount >= 1.0f)) ||
        (session->kind == SANDBOX3D_MODELING_OPERATOR_SMOOTH_VERTICES &&
            (applied_amount < 0.0f || applied_amount > 1.0f)) ||
        (session->kind == SANDBOX3D_MODELING_OPERATOR_BEVEL &&
            (applied_amount <= 0.0f || applied_amount > 1000000.0f)) ||
        ((session->kind == SANDBOX3D_MODELING_OPERATOR_EXTRUDE ||
          session->kind == SANDBOX3D_MODELING_OPERATOR_EDGE_EXTRUDE) &&
            fabsf(applied_amount) <= 1.0e-7f) ||
        (session->kind == SANDBOX3D_MODELING_OPERATOR_INSET &&
            (applied_amount <= 0.0f || applied_amount >= 1.0f)) ||
        (session->kind == SANDBOX3D_MODELING_OPERATOR_FACE_NORMAL &&
            fabsf(applied_amount) > 100.0f) ||
        ((session->kind == SANDBOX3D_MODELING_OPERATOR_UV_PACK ||
          session->kind == SANDBOX3D_MODELING_OPERATOR_UV_ISLAND_PACK ||
          session->kind == SANDBOX3D_MODELING_OPERATOR_UV_PACK_ALL ||
          session->kind == SANDBOX3D_MODELING_OPERATOR_UV_UNWRAP_PLANAR ||
          session->kind == SANDBOX3D_MODELING_OPERATOR_UV_UNWRAP_CYLINDRICAL ||
          session->kind == SANDBOX3D_MODELING_OPERATOR_UV_UNWRAP_SPHERICAL) &&
            (applied_amount < 0.0f || applied_amount >= 0.5f)) ||
        ((session->kind == SANDBOX3D_MODELING_OPERATOR_UV_TRANSFORM ||
          session->kind == SANDBOX3D_MODELING_OPERATOR_UV_ISLAND_TRANSFORM) &&
            (applied_amount <= -1.0f || applied_amount > 1000000.0f)) ||
        (session->kind == SANDBOX3D_MODELING_OPERATOR_MOVE &&
            (counts.vertices == 0U || counts.vertices > SIZE_MAX / sizeof(*vertices))))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (session->kind == SANDBOX3D_MODELING_OPERATOR_MOVE)
    {
        vertices = henka_calloc(counts.vertices, sizeof(*vertices));
        if (vertices == NULL)
        {
            return HENKA_ERROR_OUT_OF_MEMORY;
        }
    }
    if (session->kind == SANDBOX3D_MODELING_OPERATOR_INSET)
    {
        if (session->selection_count > SIZE_MAX / sizeof(*inset_result_faces))
        {
            result = HENKA_ERROR_LIMIT;
        }
        else
        {
            inset_result_faces = henka_malloc(
                session->selection_count * sizeof(*inset_result_faces));
            if (inset_result_faces == NULL)
            {
                result = HENKA_ERROR_OUT_OF_MEMORY;
            }
        }
    }
    if (result == HENKA_SUCCESS && session->kind == SANDBOX3D_MODELING_OPERATOR_EXTRUDE &&
        session->selection_mode == SANDBOX3D_AUTHORING_SELECTION_FACE)
    {
        const henka_authoring_mesh_counts source_counts =
            henka_authoring_mesh_get_counts(session->source_snapshot);
        if (session->selection_count > SIZE_MAX / sizeof(*extrude_result_faces))
        {
            result = HENKA_ERROR_LIMIT;
        }
        else
        {
            extrude_result_faces = henka_malloc(
                session->selection_count * sizeof(*extrude_result_faces));
            if (extrude_result_faces == NULL)
            {
                result = HENKA_ERROR_OUT_OF_MEMORY;
            }
        }
        for (index = 0U; result == HENKA_SUCCESS && index < source_counts.faces; ++index)
        {
            henka_authoring_face_id face_id = HENKA_AUTHORING_INVALID_ID;
            if (henka_authoring_mesh_get_face_id_at(
                    session->source_snapshot, index, &face_id) != HENKA_SUCCESS)
            {
                result = HENKA_ERROR_INVALID_ARGUMENT;
            }
            else if ((size_t)face_id > previous_face_max_id)
            {
                previous_face_max_id = face_id;
            }
        }
        if (result == HENKA_SUCCESS && session->selection_count > 1U)
        {
            extrude_face_region_touches_unselected =
                sandbox3d_modeling_operator_face_region_touches_unselected_surface(
                    session->source_snapshot,
                    (const henka_authoring_face_id*)session->selection_ids,
                    session->selection_count);
        }
    }
    if (result == HENKA_SUCCESS &&
        (session->kind == SANDBOX3D_MODELING_OPERATOR_SUBDIVIDE ||
         session->kind == SANDBOX3D_MODELING_OPERATOR_RIP_VERTEX_FACE))
    {
        if (session->selection_count > SIZE_MAX / sizeof(*subdivide_result_vertices))
        {
            result = HENKA_ERROR_LIMIT;
        }
        else
        {
            subdivide_result_vertices = henka_malloc(
                session->selection_count * sizeof(*subdivide_result_vertices));
            if (subdivide_result_vertices == NULL)
            {
                result = HENKA_ERROR_OUT_OF_MEMORY;
            }
        }
    }
    if (result == HENKA_SUCCESS &&
        session->kind == SANDBOX3D_MODELING_OPERATOR_RIP_VERTEX_FACE)
    {
        if (session->selection_count > SIZE_MAX / sizeof(*rip_face_ids))
        {
            result = HENKA_ERROR_LIMIT;
        }
        else
        {
            rip_face_ids = henka_malloc(
                session->selection_count * sizeof(*rip_face_ids));
            if (rip_face_ids == NULL)
            {
                result = HENKA_ERROR_OUT_OF_MEMORY;
            }
        }
        for (index = 0U; result == HENKA_SUCCESS && index < session->selection_count; ++index)
        {
            result = sandbox3d_modeling_operator_find_first_incident_face(
                session->source_snapshot,
                (henka_authoring_vertex_id)session->selection_ids[index],
                &rip_face_ids[index]);
        }
    }
    if (result == HENKA_SUCCESS &&
        session->kind == SANDBOX3D_MODELING_OPERATOR_FILL_BOUNDARY_LOOP)
    {
        if (session->selection_count > SIZE_MAX / sizeof(*fill_result_faces))
        {
            result = HENKA_ERROR_LIMIT;
        }
        else
        {
            fill_result_faces = henka_malloc(
                session->selection_count * sizeof(*fill_result_faces));
            if (fill_result_faces == NULL)
            {
                result = HENKA_ERROR_OUT_OF_MEMORY;
            }
        }
    }
    if (result == HENKA_SUCCESS && session->kind == SANDBOX3D_MODELING_OPERATOR_BEVEL &&
        session->selection_mode == SANDBOX3D_AUTHORING_SELECTION_FACE)
    {
        if (session->selection_count > SIZE_MAX / sizeof(*bevel_result_faces))
        {
            result = HENKA_ERROR_LIMIT;
        }
        else
        {
            bevel_result_faces = henka_malloc(
                session->selection_count * sizeof(*bevel_result_faces));
            if (bevel_result_faces == NULL)
            {
                result = HENKA_ERROR_OUT_OF_MEMORY;
            }
        }
    }
    if (result == HENKA_SUCCESS && session->kind == SANDBOX3D_MODELING_OPERATOR_BEVEL &&
        session->selection_mode == SANDBOX3D_AUTHORING_SELECTION_VERTEX)
    {
        mesh_desc = henka_authoring_mesh_get_desc(session->source_snapshot);
        bevel_result_capacity = mesh_desc.max_vertices;
        if (bevel_result_capacity == 0U ||
            bevel_result_capacity > SIZE_MAX / sizeof(*bevel_result_vertices))
        {
            result = HENKA_ERROR_LIMIT;
        }
        else
        {
            bevel_result_vertices = henka_malloc(
                bevel_result_capacity * sizeof(*bevel_result_vertices));
            if (bevel_result_vertices == NULL)
            {
                result = HENKA_ERROR_OUT_OF_MEMORY;
            }
        }
    }
    if (result == HENKA_SUCCESS &&
        session->kind == SANDBOX3D_MODELING_OPERATOR_EDGE_BRIDGE)
    {
        result = sandbox3d_authoring_object_build_selected_boundary_bridge_candidate(
            session->object, &candidate, &bridge_result_face, &report);
    }
    else if (result == HENKA_SUCCESS)
    {
        result = henka_authoring_mesh_clone(
            session->source_snapshot, &candidate);
    }
    if (result == HENKA_SUCCESS &&
        session->kind == SANDBOX3D_MODELING_OPERATOR_SPLIT_EDGE)
    {
        if (session->selection_count > SIZE_MAX / sizeof(*split_vertex_ids))
        {
            result = HENKA_ERROR_LIMIT;
        }
        else
        {
            split_vertex_ids = henka_malloc(
                session->selection_count * sizeof(*split_vertex_ids));
            split_first_edges = henka_malloc(
                session->selection_count * sizeof(*split_first_edges));
            split_second_edges = henka_malloc(
                session->selection_count * sizeof(*split_second_edges));
            if (split_vertex_ids == NULL || split_first_edges == NULL ||
                split_second_edges == NULL)
            {
                result = HENKA_ERROR_OUT_OF_MEMORY;
            }
        }
        session->split_first_edge = HENKA_AUTHORING_INVALID_ID;
        session->split_second_edge = HENKA_AUTHORING_INVALID_ID;
        session->split_configured = false;
        if (result == HENKA_SUCCESS)
        {
            const henka_result chain_result =
                henka_authoring_mesh_split_boundary_edge_chains(
                    candidate,
                    (const henka_authoring_edge_id*)session->selection_ids,
                    session->selection_count,
                    applied_amount,
                    split_vertex_ids,
                    split_first_edges,
                    split_second_edges,
                    &report);
            if (chain_result == HENKA_SUCCESS)
            {
                result = HENKA_SUCCESS;
            }
            else if (chain_result == HENKA_ERROR_INVALID_ARGUMENT ||
                     chain_result == HENKA_ERROR_LIMIT)
            {
                const henka_result single_chain_result =
                    henka_authoring_mesh_split_boundary_edge_chain(
                        candidate,
                        (const henka_authoring_edge_id*)session->selection_ids,
                        session->selection_count,
                        applied_amount,
                        split_vertex_ids,
                        split_first_edges,
                        split_second_edges,
                        &report);
                if (single_chain_result == HENKA_SUCCESS)
                {
                    result = HENKA_SUCCESS;
                }
                else if (single_chain_result == HENKA_ERROR_INVALID_ARGUMENT ||
                         single_chain_result == HENKA_ERROR_LIMIT)
                {
                    result = henka_authoring_mesh_split_edges(
                        candidate,
                        (const henka_authoring_edge_id*)session->selection_ids,
                        session->selection_count,
                        applied_amount,
                        split_vertex_ids,
                        split_first_edges,
                        split_second_edges,
                        &report);
                    if (result == HENKA_ERROR_INVALID_ARGUMENT &&
                        chain_result == HENKA_ERROR_LIMIT)
                    {
                        result = chain_result;
                    }
                }
                else
                {
                    result = single_chain_result;
                }
            }
            else
            {
                result = chain_result;
            }
            if (result == HENKA_SUCCESS)
            {
                session->split_first_edge = split_first_edges[0U];
                session->split_second_edge = split_second_edges[0U];
                session->split_configured = true;
            }
        }
    }
    if (result == HENKA_SUCCESS &&
        (session->kind == SANDBOX3D_MODELING_OPERATOR_MERGE_VERTICES_CENTER ||
         session->kind == SANDBOX3D_MODELING_OPERATOR_MERGE_VERTICES_ACTIVE ||
         session->kind == SANDBOX3D_MODELING_OPERATOR_MERGE_VERTICES_DISTANCE))
    {
        if (session->selection_count > SIZE_MAX / sizeof(*merge_survivors))
        {
            result = HENKA_ERROR_LIMIT;
        }
        else
        {
            merge_survivors = henka_malloc(
                session->selection_count * sizeof(*merge_survivors));
            if (merge_survivors == NULL)
            {
                result = HENKA_ERROR_OUT_OF_MEMORY;
            }
        }
    }
    if (result == HENKA_SUCCESS &&
        session->kind == SANDBOX3D_MODELING_OPERATOR_TRANSFORM)
    {
        result = sandbox3d_authoring_object_apply_component_transform_candidate(
            session->object,
            candidate,
            session->transform_scale,
            session->transform_axis,
            session->transform_radians,
            session->transform_pivot_mode,
            session->transform_orientation_mode);
    }
    if (result == HENKA_SUCCESS &&
        session->kind == SANDBOX3D_MODELING_OPERATOR_ADD_LOOSE_VERTEX)
    {
        henka_authoring_vertex_id created_id = HENKA_AUTHORING_INVALID_ID;
        result = sandbox3d_authoring_object_apply_add_loose_vertex_candidate(
            candidate,
            session->loose_vertex_position,
            session->loose_vertex_uv,
            session->loose_vertex_material_region,
            &created_id);
        session->created_component_id = (uint32_t)created_id;
    }
    if (result == HENKA_SUCCESS &&
        session->kind == SANDBOX3D_MODELING_OPERATOR_ADD_LOOSE_EDGE)
    {
        henka_authoring_edge_id created_id = HENKA_AUTHORING_INVALID_ID;
        result = sandbox3d_authoring_object_apply_add_loose_edge_candidate(
            candidate,
            session->loose_edge_first,
            session->loose_edge_second,
            session->loose_edge_hard,
            &created_id);
        session->created_component_id = (uint32_t)created_id;
    }
    if (result == HENKA_SUCCESS &&
        session->kind == SANDBOX3D_MODELING_OPERATOR_RIP_VERTEX_FACE)
    {
        size_t rip_result_count = 0U;
        result = henka_authoring_mesh_rip_vertex_faces(
            candidate,
            (const henka_authoring_vertex_id*)session->selection_ids,
            rip_face_ids,
            session->selection_count,
            subdivide_result_vertices,
            session->selection_count,
            &rip_result_count,
            &report);
        if (result == HENKA_SUCCESS)
        {
            session->subdivide_result_count = rip_result_count;
            session->created_component_id = (uint32_t)subdivide_result_vertices[0U];
        }
    }
    if (result == HENKA_SUCCESS &&
        session->kind == SANDBOX3D_MODELING_OPERATOR_PROPORTIONAL_MOVE)
    {
        henka_vec3 proportional_offset = {0.0f, 0.0f, 0.0f};
        if (session->axis == SANDBOX3D_MODELING_OPERATOR_AXIS_X)
        {
            proportional_offset.x = applied_amount;
        }
        else if (session->axis == SANDBOX3D_MODELING_OPERATOR_AXIS_Y)
        {
            proportional_offset.y = applied_amount;
        }
        else
        {
            proportional_offset.z = applied_amount;
        }
        result = sandbox3d_authoring_object_apply_proportional_move_candidate(
            session->source_snapshot,
            candidate,
            session->selection_mode,
            session->selection_ids,
            session->selection_count,
            proportional_offset,
            session->proportional_ring_count);
    }
    if (result == HENKA_SUCCESS &&
        session->kind == SANDBOX3D_MODELING_OPERATOR_SMOOTH_VERTICES)
    {
        result = henka_authoring_mesh_smooth_vertices(
            candidate,
            (const henka_authoring_vertex_id*)session->selection_ids,
            session->selection_count,
            applied_amount,
            &report);
    }
    if (result == HENKA_SUCCESS && session->kind == SANDBOX3D_MODELING_OPERATOR_MOVE)
    {
        result = sandbox3d_modeling_operator_collect_vertices(
            session, vertices, counts.vertices, &affected_count);
    }
    if (result == HENKA_SUCCESS && session->kind == SANDBOX3D_MODELING_OPERATOR_MOVE)
    {
        if (session->axis == SANDBOX3D_MODELING_OPERATOR_AXIS_X)
        {
            offset.x = applied_amount;
        }
        else if (session->axis == SANDBOX3D_MODELING_OPERATOR_AXIS_Y)
        {
            offset.y = applied_amount;
        }
        else
        {
            offset.z = applied_amount;
        }
        for (index = 0U; index < affected_count && result == HENKA_SUCCESS; ++index)
        {
            const henka_authoring_vertex* vertex = henka_authoring_mesh_get_vertex(
                candidate, vertices[index]);
            result = vertex == NULL
                ? HENKA_ERROR_INVALID_ARGUMENT
                : henka_authoring_mesh_set_vertex_position(
                    candidate, vertices[index], henka_vec3_add(vertex->position, offset));
        }
    }
    if (result == HENKA_SUCCESS && session->kind == SANDBOX3D_MODELING_OPERATOR_EDGE_SLIDE)
    {
        result = henka_authoring_mesh_slide_edge_loops(
            candidate,
            (const henka_authoring_edge_id*)session->selection_ids,
            session->selection_count,
            applied_amount,
            &report);
    }
    if (result == HENKA_SUCCESS &&
        session->kind == SANDBOX3D_MODELING_OPERATOR_CONNECT)
    {
        henka_authoring_face_id connected_face_id = HENKA_AUTHORING_INVALID_ID;
        result = henka_authoring_mesh_connect_vertices(
            candidate,
            (henka_authoring_vertex_id)session->selection_ids[0U],
            (henka_authoring_vertex_id)session->selection_ids[1U],
            &connected_face_id,
            &report);
    }
    if (result == HENKA_SUCCESS &&
        session->kind == SANDBOX3D_MODELING_OPERATOR_TRIANGULATE)
    {
        result = henka_authoring_mesh_triangulate_faces(
            candidate,
            (const henka_authoring_face_id*)session->selection_ids,
            session->selection_count,
            &report);
    }
    if (result == HENKA_SUCCESS &&
        session->kind == SANDBOX3D_MODELING_OPERATOR_INSET)
    {
        result = henka_authoring_mesh_inset_faces(
            candidate,
            (const henka_authoring_face_id*)session->selection_ids,
            session->selection_count,
            applied_amount,
            inset_result_faces,
            session->selection_count,
            &session->inset_result_count,
            &report);
    }
    if (result == HENKA_SUCCESS &&
        session->kind == SANDBOX3D_MODELING_OPERATOR_FACE_NORMAL)
    {
        result = sandbox3d_modeling_operator_apply_face_normals(
            session->source_snapshot,
            candidate,
            (const henka_authoring_face_id*)session->selection_ids,
            session->selection_count,
            applied_amount);
    }
    if (result == HENKA_SUCCESS &&
        session->kind == SANDBOX3D_MODELING_OPERATOR_FLIP_FACE)
    {
        result = henka_authoring_mesh_flip_faces(
            candidate,
            (const henka_authoring_face_id*)session->selection_ids,
            session->selection_count,
            &report);
    }
    if (result == HENKA_SUCCESS &&
        session->kind == SANDBOX3D_MODELING_OPERATOR_DELETE_FACES)
    {
        result = henka_authoring_mesh_delete_faces(
            candidate,
            (const henka_authoring_face_id*)session->selection_ids,
            session->selection_count,
            &report);
    }
    if (result == HENKA_SUCCESS &&
        session->kind == SANDBOX3D_MODELING_OPERATOR_DELETE_EDGE)
    {
        const henka_authoring_edge* selected_edge =
            henka_authoring_mesh_get_edge(
                candidate,
                (henka_authoring_edge_id)session->selection_ids[0U]);
        if (selected_edge == NULL)
        {
            result = HENKA_ERROR_INVALID_ARGUMENT;
        }
        else if (session->selection_count == 1U && selected_edge->face_count != 0U)
        {
            result = henka_authoring_mesh_delete_edge(
                candidate,
                (henka_authoring_edge_id)session->selection_ids[0U],
                &report);
        }
        else if (session->selection_count == 1U)
        {
            result = henka_authoring_mesh_delete_loose_edges(
                candidate,
                (const henka_authoring_edge_id*)session->selection_ids,
                session->selection_count,
                &report);
        }
        else
        {
            use_loose_edge_batch = selected_edge->face_count == 0U;
            use_face_edge_batch = !use_loose_edge_batch;
            for (index = 1U; index < session->selection_count; ++index)
            {
                const henka_authoring_edge* edge = henka_authoring_mesh_get_edge(
                    candidate,
                    (henka_authoring_edge_id)session->selection_ids[index]);
                if (edge == NULL || (edge->face_count == 0U) != use_loose_edge_batch ||
                    (edge->face_count != 0U && edge->face_count > 2U))
                {
                    result = HENKA_ERROR_INVALID_ARGUMENT;
                    break;
                }
            }
            if (result == HENKA_SUCCESS)
            {
                result = use_face_edge_batch
                    ? henka_authoring_mesh_delete_face_edges(
                          candidate,
                          (const henka_authoring_edge_id*)session->selection_ids,
                          session->selection_count,
                          &report)
                    : henka_authoring_mesh_delete_loose_edges(
                          candidate,
                          (const henka_authoring_edge_id*)session->selection_ids,
                          session->selection_count,
                          &report);
            }
        }
    }
    if (result == HENKA_SUCCESS &&
        session->kind == SANDBOX3D_MODELING_OPERATOR_DISSOLVE_EDGE)
    {
        result = session->selection_count == 1U
            ? henka_authoring_mesh_dissolve_edge(
                  candidate,
                  (henka_authoring_edge_id)session->selection_ids[0U],
                  &report)
            : henka_authoring_mesh_dissolve_edges(
                  candidate,
                  (const henka_authoring_edge_id*)session->selection_ids,
                  session->selection_count,
                  &report);
    }
    if (result == HENKA_SUCCESS &&
        session->kind == SANDBOX3D_MODELING_OPERATOR_DISSOLVE_VERTICES)
    {
        result = henka_authoring_mesh_dissolve_vertices(
            candidate,
            (const henka_authoring_vertex_id*)session->selection_ids,
            session->selection_count,
            &report);
    }
    if (result == HENKA_SUCCESS &&
        session->kind == SANDBOX3D_MODELING_OPERATOR_DELETE_VERTICES)
    {
        result = henka_authoring_mesh_delete_vertices(
            candidate,
            (const henka_authoring_vertex_id*)session->selection_ids,
            session->selection_count,
            &report);
    }
    if (result == HENKA_SUCCESS &&
        session->kind == SANDBOX3D_MODELING_OPERATOR_MERGE_VERTICES_CENTER)
    {
        result = henka_authoring_mesh_merge_vertices(
            candidate,
            (const henka_authoring_vertex_id*)session->selection_ids,
            session->selection_count,
            HENKA_AUTHORING_VERTEX_MERGE_CENTER,
            HENKA_AUTHORING_INVALID_ID,
            merge_survivors,
            session->selection_count,
            &merge_survivor_count,
            &report);
    }
    if (result == HENKA_SUCCESS &&
        session->kind == SANDBOX3D_MODELING_OPERATOR_MERGE_VERTICES_ACTIVE)
    {
        result = henka_authoring_mesh_merge_vertices(
            candidate,
            (const henka_authoring_vertex_id*)session->selection_ids,
            session->selection_count,
            HENKA_AUTHORING_VERTEX_MERGE_ACTIVE,
            (henka_authoring_vertex_id)session->active_component_id,
            merge_survivors,
            session->selection_count,
            &merge_survivor_count,
            &report);
    }
    if (result == HENKA_SUCCESS &&
        session->kind == SANDBOX3D_MODELING_OPERATOR_MERGE_VERTICES_DISTANCE)
    {
        result = henka_authoring_mesh_merge_vertices_by_distance(
            candidate,
            (const henka_authoring_vertex_id*)session->selection_ids,
            session->selection_count,
            sandbox3d_authoring_object_get_merge_distance(session->object),
            merge_survivors,
            session->selection_count,
            &merge_survivor_count,
            &report);
    }
    if (result == HENKA_SUCCESS &&
        session->kind == SANDBOX3D_MODELING_OPERATOR_SUBDIVIDE)
    {
        result = henka_authoring_mesh_subdivide_faces(
            candidate,
            (const henka_authoring_face_id*)session->selection_ids,
            session->selection_count,
            subdivide_result_vertices,
            session->selection_count,
            &session->subdivide_result_count,
            &report);
    }
    if (result == HENKA_SUCCESS && session->kind == SANDBOX3D_MODELING_OPERATOR_BEVEL)
    {
        if (session->selection_mode == SANDBOX3D_AUTHORING_SELECTION_VERTEX)
        {
            result = henka_authoring_mesh_bevel_vertices(
                candidate,
                (const henka_authoring_vertex_id*)session->selection_ids,
                session->selection_count,
                applied_amount,
                bevel_result_vertices,
                bevel_result_capacity,
                &bevel_result_count,
                &report);
        }
        else if (session->selection_mode == SANDBOX3D_AUTHORING_SELECTION_EDGE)
        {
            result = henka_authoring_mesh_bevel_edges(
                candidate,
                (const henka_authoring_edge_id*)session->selection_ids,
                session->selection_count,
                applied_amount,
                &report);
        }
        else
        {
            result = henka_authoring_mesh_bevel_faces(
                candidate,
                (const henka_authoring_face_id*)session->selection_ids,
                session->selection_count,
                applied_amount,
                bevel_result_faces,
                session->selection_count,
                &session->bevel_result_count,
                &report);
        }
    }
    if (result == HENKA_SUCCESS &&
        session->kind == SANDBOX3D_MODELING_OPERATOR_EDGE_EXTRUDE)
    {
        if (session->selection_count == 1U)
        {
            result = henka_authoring_mesh_extrude_edge(
                candidate,
                (henka_authoring_edge_id)session->selection_ids[0U],
                applied_amount,
                &extrude_result_edge,
                &extrude_result_face,
                &report);
        }
        else
        {
            bool same_boundary_face = true;
            bool all_interior = true;
            henka_authoring_face_id boundary_face_id = HENKA_AUTHORING_INVALID_ID;
            size_t selection_index;
            for (selection_index = 0U;
                 selection_index < session->selection_count;
                 ++selection_index)
            {
                const henka_authoring_edge* edge = henka_authoring_mesh_get_edge(
                    session->source_snapshot,
                    (henka_authoring_edge_id)session->selection_ids[selection_index]);
                if (edge == NULL || edge->face_count != 1U)
                {
                    same_boundary_face = false;
                }
                if (edge == NULL || edge->face_count != 2U)
                {
                    all_interior = false;
                }
                if (edge == NULL ||
                    (edge->face_count != 1U && edge->face_count != 2U))
                {
                    break;
                }
                if (edge->face_count == 1U && boundary_face_id == HENKA_AUTHORING_INVALID_ID)
                {
                    boundary_face_id = edge->faces[0];
                }
                else if (edge->face_count == 1U && boundary_face_id != edge->faces[0])
                {
                    same_boundary_face = false;
                }
            }
            if (all_interior)
            {
                result = henka_authoring_mesh_extrude_interior_edges(
                    candidate,
                    (const henka_authoring_edge_id*)session->selection_ids,
                    session->selection_count,
                    applied_amount,
                    &report);
            }
            else
            {
                result = henka_authoring_mesh_extrude_boundary_edge_chains(
                    candidate,
                    (const henka_authoring_edge_id*)session->selection_ids,
                    session->selection_count,
                    applied_amount,
                    &report);
                if (result == HENKA_ERROR_INVALID_ARGUMENT ||
                    result == HENKA_ERROR_LIMIT)
                {
                    if (same_boundary_face)
                    {
                        result = henka_authoring_mesh_extrude_boundary_edge_chain(
                            candidate,
                            (const henka_authoring_edge_id*)session->selection_ids,
                            session->selection_count,
                            applied_amount,
                            &report);
                    }
                    else
                    {
                        result = henka_authoring_mesh_extrude_boundary_edges(
                            candidate,
                            (const henka_authoring_edge_id*)session->selection_ids,
                            session->selection_count,
                            applied_amount,
                            &report);
                    }
                }
            }
        }
    }
    if (result == HENKA_SUCCESS &&
        session->kind == SANDBOX3D_MODELING_OPERATOR_FILL_BOUNDARY_LOOP)
    {
        result = henka_authoring_mesh_fill_boundary_loops(
            candidate,
            (const henka_authoring_edge_id*)session->selection_ids,
            session->selection_count,
            fill_result_faces,
            session->selection_count,
            &fill_result_count,
            &report);
    }
    if (result == HENKA_SUCCESS &&
        session->kind == SANDBOX3D_MODELING_OPERATOR_UV_PROJECT)
    {
        result = henka_authoring_mesh_project_face_uv(
            candidate,
            (henka_authoring_face_id)session->selection_ids[0U],
            (henka_authoring_uv_projection_axis)(session->axis -
                SANDBOX3D_MODELING_OPERATOR_AXIS_X));
    }
    if (result == HENKA_SUCCESS &&
        session->kind == SANDBOX3D_MODELING_OPERATOR_UV_PACK)
    {
        result = henka_authoring_mesh_pack_face_uv(
            candidate,
            (henka_authoring_face_id)session->selection_ids[0U],
            applied_amount);
    }
    if (result == HENKA_SUCCESS &&
        session->kind == SANDBOX3D_MODELING_OPERATOR_UV_TRANSFORM)
    {
        const float uniform_scale = 1.0f + applied_amount;
        result = henka_authoring_mesh_transform_face_uv(
            candidate,
            (henka_authoring_face_id)session->selection_ids[0U],
            (henka_vec2){uniform_scale, uniform_scale},
            (henka_vec2){0.0f, 0.0f});
    }
    if (result == HENKA_SUCCESS &&
        session->kind == SANDBOX3D_MODELING_OPERATOR_UV_ISLAND_TRANSFORM)
    {
        const float uniform_scale = 1.0f + applied_amount;
        result = henka_authoring_mesh_transform_uv_island(
            candidate,
            (henka_authoring_face_id)session->selection_ids[0U],
            (henka_vec2){uniform_scale, uniform_scale},
            (henka_vec2){0.0f, 0.0f});
    }
    if (result == HENKA_SUCCESS &&
        session->kind == SANDBOX3D_MODELING_OPERATOR_UV_ISLAND_PACK)
    {
        result = henka_authoring_mesh_pack_uv_island(
            candidate,
            (henka_authoring_face_id)session->selection_ids[0U],
            applied_amount);
    }
    if (result == HENKA_SUCCESS &&
        session->kind == SANDBOX3D_MODELING_OPERATOR_UV_PACK_ALL)
    {
        result = henka_authoring_mesh_pack_uv_islands(candidate, applied_amount);
    }
    if (result == HENKA_SUCCESS &&
        session->kind == SANDBOX3D_MODELING_OPERATOR_UV_UNWRAP_PLANAR)
    {
        result = henka_authoring_mesh_unwrap_planar_faces(candidate, applied_amount);
    }
    if (result == HENKA_SUCCESS &&
        session->kind == SANDBOX3D_MODELING_OPERATOR_UV_UNWRAP_CYLINDRICAL)
    {
        result = henka_authoring_mesh_unwrap_cylindrical_faces(
            candidate,
            (henka_authoring_uv_projection_axis)(session->axis -
                SANDBOX3D_MODELING_OPERATOR_AXIS_X),
            applied_amount);
    }
    if (result == HENKA_SUCCESS &&
        session->kind == SANDBOX3D_MODELING_OPERATOR_UV_UNWRAP_SPHERICAL)
    {
        result = henka_authoring_mesh_unwrap_spherical_faces(
            candidate,
            (henka_authoring_uv_projection_axis)(session->axis -
                SANDBOX3D_MODELING_OPERATOR_AXIS_X),
            applied_amount);
    }
    if (result == HENKA_SUCCESS &&
        session->kind == SANDBOX3D_MODELING_OPERATOR_UV_SEAM_TOGGLE)
    {
        for (index = 0U; index < session->selection_count && result == HENKA_SUCCESS; ++index)
        {
            const henka_authoring_edge_id edge_id =
                (henka_authoring_edge_id)session->selection_ids[index];
            const bool seam = henka_authoring_mesh_edge_is_seam(candidate, edge_id);
            result = henka_authoring_mesh_set_edge_seam(candidate, edge_id, !seam);
        }
    }
    if (result == HENKA_SUCCESS && session->kind == SANDBOX3D_MODELING_OPERATOR_EXTRUDE)
    {
        henka_vec3 direction = {0.0f, 0.0f, 0.0f};
        if (session->axis == SANDBOX3D_MODELING_OPERATOR_AXIS_X)
        {
            direction.x = 1.0f;
        }
        else if (session->axis == SANDBOX3D_MODELING_OPERATOR_AXIS_Y)
        {
            direction.y = 1.0f;
        }
        else
        {
            direction.z = 1.0f;
        }
        if (session->selection_mode == SANDBOX3D_AUTHORING_SELECTION_VERTEX)
        {
            if (session->selection_count > 1U)
            {
                bool all_loose = true;
                bool all_surface = true;
                bool all_closed = true;
                size_t selection_index;
                for (selection_index = 0U;
                     selection_index < session->selection_count;
                     ++selection_index)
                {
                    const henka_authoring_vertex_id vertex_id =
                        (henka_authoring_vertex_id)session->selection_ids[selection_index];
                    const size_t edge_count = henka_authoring_mesh_get_vertex_edge_count(
                        session->source_snapshot, vertex_id);
                    bool vertex_closed = edge_count > 0U;
                    size_t edge_index;
                    all_loose = all_loose && edge_count == 0U;
                    all_surface = all_surface && edge_count != 0U;
                    for (edge_index = 0U; edge_index < edge_count; ++edge_index)
                    {
                        henka_authoring_edge_id edge_id;
                        const henka_authoring_edge* edge;
                        if (henka_authoring_mesh_get_vertex_edge_at(
                                session->source_snapshot, vertex_id, edge_index, &edge_id) != HENKA_SUCCESS ||
                            (edge = henka_authoring_mesh_get_edge(
                                session->source_snapshot, edge_id)) == NULL ||
                            edge->face_count != 2U)
                        {
                            vertex_closed = false;
                            break;
                        }
                    }
                    all_closed = all_closed && vertex_closed;
                }
                if (all_loose)
                {
                    result = henka_authoring_mesh_extrude_loose_vertices(
                        candidate,
                        (const henka_authoring_vertex_id*)session->selection_ids,
                        session->selection_count,
                        direction,
                        applied_amount,
                        &report);
                }
                else if (all_surface)
                {
                    if (all_closed)
                    {
                        result = henka_authoring_mesh_extrude_interior_vertices(
                            candidate,
                            (const henka_authoring_vertex_id*)session->selection_ids,
                            session->selection_count,
                            applied_amount,
                            &report);
                    }
                    else
                    {
                        size_t boundary_chain_edge_count = 0U;
                        if (session->selection_count > SIZE_MAX / sizeof(*boundary_chain_edges))
                        {
                            result = HENKA_ERROR_LIMIT;
                        }
                        else
                        {
                            boundary_chain_edges = henka_malloc(
                                session->selection_count * sizeof(*boundary_chain_edges));
                            if (boundary_chain_edges == NULL)
                            {
                                result = HENKA_ERROR_OUT_OF_MEMORY;
                            }
                            else if (sandbox3d_modeling_operator_collect_boundary_vertex_chain(
                                session->source_snapshot,
                                (const henka_authoring_vertex_id*)session->selection_ids,
                                session->selection_count,
                                boundary_chain_edges,
                                session->selection_count,
                                &boundary_chain_edge_count) == HENKA_SUCCESS)
                            {
                                result = henka_authoring_mesh_extrude_boundary_edge_chain(
                                    candidate,
                                    boundary_chain_edges,
                                    boundary_chain_edge_count,
                                    applied_amount,
                                    &report);
                            }
                            else
                            {
                                result = henka_authoring_mesh_extrude_boundary_vertices(
                                    candidate,
                                    (const henka_authoring_vertex_id*)session->selection_ids,
                                    session->selection_count,
                                    applied_amount,
                                    &report);
                            }
                        }
                    }
                }
                else
                {
                    result = HENKA_ERROR_INVALID_ARGUMENT;
                }
            }
            else
            {
                const henka_authoring_vertex_id vertex_id =
                    (henka_authoring_vertex_id)session->selection_ids[0U];

                if (henka_authoring_mesh_get_vertex_edge_count(
                        session->source_snapshot, vertex_id) == 0U)
                {
                    result = henka_authoring_mesh_extrude_loose_vertex(
                        candidate,
                        vertex_id,
                        direction,
                        applied_amount,
                        &extrude_result_vertex,
                        &extrude_result_edge,
                        &report);
                }
                else
                {
                    result = henka_authoring_mesh_extrude_vertex(
                        candidate,
                        vertex_id,
                        applied_amount,
                        &extrude_result_vertex,
                        &report);
                }
            }
        }
        else if (session->selection_mode == SANDBOX3D_AUTHORING_SELECTION_EDGE)
        {
            if (session->selection_count == 1U)
            {
                result = henka_authoring_mesh_extrude_loose_edge(
                    candidate,
                    (henka_authoring_edge_id)session->selection_ids[0U],
                    direction,
                    applied_amount,
                    &extrude_result_edge,
                    &extrude_result_face,
                    &report);
            }
            else
            {
                result = henka_authoring_mesh_extrude_loose_edges(
                    candidate,
                    (const henka_authoring_edge_id*)session->selection_ids,
                    session->selection_count,
                    direction,
                    applied_amount,
                    &report);
            }
        }
        else if (session->selection_count == 1U)
        {
            result = henka_authoring_mesh_extrude_face(
                candidate,
                (henka_authoring_face_id)session->selection_ids[0U],
                applied_amount,
                &extrude_result_face);
        }
        else
        {
            result = henka_authoring_mesh_extrude_face_region(
                candidate,
                (const henka_authoring_face_id*)session->selection_ids,
                session->selection_count,
                applied_amount,
                &report);
        }
    }
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_authoring_object_preview_candidate(session->object, candidate);
    }
    if (result != HENKA_SUCCESS)
    {
        henka_authoring_mesh_destroy(candidate);
        henka_free(vertices);
        henka_free(merge_survivors);
        henka_free(bevel_result_vertices);
        henka_free(split_vertex_ids);
        henka_free(split_first_edges);
        henka_free(split_second_edges);
        henka_free(boundary_chain_edges);
        henka_free(inset_result_faces);
        henka_free(extrude_result_faces);
        henka_free(bevel_result_faces);
        henka_free(subdivide_result_vertices);
        henka_free(fill_result_faces);
        henka_free(rip_face_ids);
        return result;
    }
    if (session->kind == SANDBOX3D_MODELING_OPERATOR_SPLIT_EDGE)
    {
        henka_free(session->split_vertex_ids);
        henka_free(session->split_first_edges);
        henka_free(session->split_second_edges);
        session->split_vertex_ids = split_vertex_ids;
        session->split_first_edges = split_first_edges;
        session->split_second_edges = split_second_edges;
        session->split_result_count = session->selection_count;
        split_vertex_ids = NULL;
        split_first_edges = NULL;
        split_second_edges = NULL;
    }
    if (session->kind == SANDBOX3D_MODELING_OPERATOR_INSET)
    {
        henka_free(session->inset_result_faces);
        session->inset_result_faces = inset_result_faces;
        inset_result_faces = NULL;
    }
    if (session->kind == SANDBOX3D_MODELING_OPERATOR_EXTRUDE &&
        session->selection_mode == SANDBOX3D_AUTHORING_SELECTION_FACE)
    {
        if (session->selection_count == 1U)
        {
            extrude_result_faces[0U] = extrude_result_face;
            extrude_result_face_count = 1U;
        }
        else if (extrude_face_region_touches_unselected)
        {
            memcpy(
                extrude_result_faces,
                session->selection_ids,
                session->selection_count * sizeof(*extrude_result_faces));
            extrude_result_face_count = session->selection_count;
        }
        else
        {
            const henka_authoring_mesh_counts candidate_counts =
                henka_authoring_mesh_get_counts(candidate);
            for (index = 0U;
                 index < candidate_counts.faces &&
                     extrude_result_face_count < session->selection_count;
                 ++index)
            {
                henka_authoring_face_id face_id = HENKA_AUTHORING_INVALID_ID;
                if (henka_authoring_mesh_get_face_id_at(
                        candidate, index, &face_id) != HENKA_SUCCESS)
                {
                    result = HENKA_ERROR_INVALID_ARGUMENT;
                    break;
                }
                if ((size_t)face_id > previous_face_max_id)
                {
                    extrude_result_faces[extrude_result_face_count++] = face_id;
                }
            }
            if (result == HENKA_SUCCESS &&
                extrude_result_face_count != session->selection_count)
            {
                result = HENKA_ERROR_INVALID_ARGUMENT;
            }
        }
        if (result == HENKA_SUCCESS)
        {
            henka_free(session->extrude_result_faces);
            session->extrude_result_faces = extrude_result_faces;
            session->extrude_result_face_count = extrude_result_face_count;
            extrude_result_faces = NULL;
        }
    }
    if (result != HENKA_SUCCESS)
    {
        (void)sandbox3d_authoring_object_cancel_preview(session->object);
        henka_free(vertices);
        henka_free(merge_survivors);
        henka_free(bevel_result_vertices);
        henka_free(split_vertex_ids);
        henka_free(split_first_edges);
        henka_free(split_second_edges);
        henka_free(boundary_chain_edges);
        henka_free(inset_result_faces);
        henka_free(extrude_result_faces);
        henka_free(bevel_result_faces);
        henka_free(subdivide_result_vertices);
        henka_free(fill_result_faces);
        henka_free(rip_face_ids);
        return result;
    }
    if (session->kind == SANDBOX3D_MODELING_OPERATOR_SUBDIVIDE)
    {
        henka_free(session->subdivide_result_vertices);
        session->subdivide_result_vertices = subdivide_result_vertices;
        subdivide_result_vertices = NULL;
    }
    if (session->kind == SANDBOX3D_MODELING_OPERATOR_RIP_VERTEX_FACE)
    {
        henka_free(session->subdivide_result_vertices);
        session->subdivide_result_vertices = subdivide_result_vertices;
        subdivide_result_vertices = NULL;
    }
    if (session->kind == SANDBOX3D_MODELING_OPERATOR_FILL_BOUNDARY_LOOP)
    {
        henka_free(session->fill_result_faces);
        session->fill_result_faces = fill_result_faces;
        session->fill_result_count = fill_result_count;
        fill_result_faces = NULL;
    }
    if (session->kind == SANDBOX3D_MODELING_OPERATOR_BEVEL &&
        session->selection_mode == SANDBOX3D_AUTHORING_SELECTION_FACE)
    {
        henka_free(session->bevel_result_faces);
        session->bevel_result_faces = bevel_result_faces;
        bevel_result_faces = NULL;
    }
    if (session->kind == SANDBOX3D_MODELING_OPERATOR_BEVEL &&
        session->selection_mode == SANDBOX3D_AUTHORING_SELECTION_VERTEX)
    {
        henka_free(session->bevel_result_vertices);
        session->bevel_result_vertices = bevel_result_vertices;
        session->bevel_result_vertex_count = bevel_result_count;
        bevel_result_vertices = NULL;
    }
    session->amount = next_amount;
    ++session->preview_rebuild_count;
    session->state = SANDBOX3D_MODELING_OPERATOR_STATE_PREVIEW;
    henka_free(vertices);
    henka_free(merge_survivors);
    henka_free(bevel_result_vertices);
    henka_free(split_vertex_ids);
    henka_free(split_first_edges);
    henka_free(split_second_edges);
    henka_free(boundary_chain_edges);
    henka_free(inset_result_faces);
    henka_free(extrude_result_faces);
    henka_free(bevel_result_faces);
    henka_free(rip_face_ids);
    henka_free(fill_result_faces);
    return HENKA_SUCCESS;
}

henka_result sandbox3d_modeling_operator_numeric_commit(
    sandbox3d_modeling_operator_session* session)
{
    char* end = NULL;
    float target_amount;
    float delta;
    henka_result result;

    if (session == NULL || !session->active || !session->numeric_active ||
        session->numeric_length == 0U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    errno = 0;
    target_amount = strtof(session->numeric_text, &end);
    if (errno == ERANGE || end == NULL ||
        (size_t)(end - session->numeric_text) != session->numeric_length ||
        !isfinite(target_amount) || !isfinite(session->amount))
    {
        return HENKA_ERROR_NUMERIC_RANGE;
    }
    delta = target_amount - session->amount;
    if (!isfinite(delta))
    {
        return HENKA_ERROR_NUMERIC_RANGE;
    }
    result = sandbox3d_modeling_operator_preview(session, delta, false, false);
    if (result == HENKA_SUCCESS)
    {
        session->numeric_active = false;
        session->numeric_length = 0U;
        session->numeric_text[0] = '\0';
    }
    return result;
}

uint32_t sandbox3d_modeling_operator_get_created_component_id(
    const sandbox3d_modeling_operator_session* session)
{
    return session == NULL ? HENKA_AUTHORING_INVALID_ID : session->created_component_id;
}

henka_result sandbox3d_modeling_operator_commit(
    sandbox3d_modeling_operator_session* session)
{
    henka_result result;
    uint32_t* replacement_ids = NULL;
    size_t replacement_count = 0U;
    size_t index;

    if (session == NULL || !session->active ||
        session->state != SANDBOX3D_MODELING_OPERATOR_STATE_PREVIEW ||
        session->object == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (session->kind == SANDBOX3D_MODELING_OPERATOR_SPLIT_EDGE)
    {
        if (!session->split_configured || session->split_result_count == 0U ||
            session->split_first_edges == NULL || session->split_second_edges == NULL ||
            session->split_result_count > SIZE_MAX / 2U)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        replacement_count = session->split_result_count * 2U;
        if (replacement_count > SIZE_MAX / sizeof(*replacement_ids))
        {
            return HENKA_ERROR_LIMIT;
        }
        replacement_ids = henka_malloc(replacement_count * sizeof(*replacement_ids));
        if (replacement_ids == NULL)
        {
            return HENKA_ERROR_OUT_OF_MEMORY;
        }
        for (index = 0U; index < session->split_result_count; ++index)
        {
            replacement_ids[index * 2U] = session->split_first_edges[index];
            replacement_ids[index * 2U + 1U] = session->split_second_edges[index];
        }
    }
    if (session->kind == SANDBOX3D_MODELING_OPERATOR_INSET &&
        (session->inset_result_faces == NULL ||
         session->inset_result_count != session->selection_count ||
         session->inset_result_count == 0U))
    {
        henka_free(replacement_ids);
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (session->kind == SANDBOX3D_MODELING_OPERATOR_SUBDIVIDE &&
        (session->subdivide_result_vertices == NULL ||
         session->subdivide_result_count != session->selection_count ||
         session->subdivide_result_count == 0U))
    {
        henka_free(replacement_ids);
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (session->kind == SANDBOX3D_MODELING_OPERATOR_BEVEL &&
        session->selection_mode == SANDBOX3D_AUTHORING_SELECTION_FACE &&
        (session->bevel_result_faces == NULL ||
         session->bevel_result_count != session->selection_count ||
         session->bevel_result_count == 0U))
    {
        henka_free(replacement_ids);
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (session->kind == SANDBOX3D_MODELING_OPERATOR_BEVEL &&
        session->selection_mode == SANDBOX3D_AUTHORING_SELECTION_VERTEX &&
        (session->bevel_result_vertices == NULL ||
         session->bevel_result_vertex_count == 0U))
    {
        henka_free(replacement_ids);
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (session->kind == SANDBOX3D_MODELING_OPERATOR_EXTRUDE &&
        session->selection_mode == SANDBOX3D_AUTHORING_SELECTION_FACE &&
        (session->extrude_result_faces == NULL ||
         session->extrude_result_face_count != session->selection_count ||
         session->extrude_result_face_count == 0U))
    {
        henka_free(replacement_ids);
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (session->kind == SANDBOX3D_MODELING_OPERATOR_FILL_BOUNDARY_LOOP &&
        (session->fill_result_faces == NULL || session->fill_result_count == 0U))
    {
        henka_free(replacement_ids);
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = sandbox3d_authoring_object_commit_preview(session->object);
    if (result != HENKA_SUCCESS)
    {
        henka_free(replacement_ids);
        return result;
    }
    if (session->kind == SANDBOX3D_MODELING_OPERATOR_RIP_VERTEX_FACE)
    {
        if (session->subdivide_result_vertices == NULL ||
            session->subdivide_result_count != session->selection_count ||
            session->subdivide_result_count == 0U)
        {
            henka_free(replacement_ids);
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        sandbox3d_authoring_object_set_selection_mode(
            session->object, SANDBOX3D_AUTHORING_SELECTION_VERTEX);
        result = sandbox3d_authoring_object_replace_component_selection(
            session->object,
            (const uint32_t*)session->subdivide_result_vertices,
            session->subdivide_result_count,
            session->subdivide_result_vertices[0U]);
        if (result != HENKA_SUCCESS)
        {
            henka_free(replacement_ids);
            return result;
        }
    }
    if (session->kind == SANDBOX3D_MODELING_OPERATOR_SPLIT_EDGE)
    {
        result = sandbox3d_authoring_object_replace_component_selection(
            session->object, replacement_ids, replacement_count, replacement_ids[0U]);
        if (result != HENKA_SUCCESS)
        {
            henka_free(replacement_ids);
            return result;
        }
    }
    if (session->kind == SANDBOX3D_MODELING_OPERATOR_INSET)
    {
        result = sandbox3d_authoring_object_replace_component_selection(
            session->object,
            (const uint32_t*)session->inset_result_faces,
            session->inset_result_count,
            session->inset_result_faces[0U]);
        if (result != HENKA_SUCCESS)
        {
            return result;
        }
    }
    if (session->kind == SANDBOX3D_MODELING_OPERATOR_SUBDIVIDE)
    {
        sandbox3d_authoring_object_set_selection_mode(
            session->object, SANDBOX3D_AUTHORING_SELECTION_VERTEX);
        result = sandbox3d_authoring_object_replace_component_selection(
            session->object,
            (const uint32_t*)session->subdivide_result_vertices,
            session->subdivide_result_count,
            session->subdivide_result_vertices[0U]);
        if (result != HENKA_SUCCESS)
        {
            henka_free(replacement_ids);
            return result;
        }
    }
    if (session->kind == SANDBOX3D_MODELING_OPERATOR_BEVEL &&
        session->selection_mode == SANDBOX3D_AUTHORING_SELECTION_FACE)
    {
        result = sandbox3d_authoring_object_replace_component_selection(
            session->object,
            (const uint32_t*)session->bevel_result_faces,
            session->bevel_result_count,
            session->bevel_result_faces[0U]);
        if (result != HENKA_SUCCESS)
        {
            henka_free(replacement_ids);
            return result;
        }
    }
    if (session->kind == SANDBOX3D_MODELING_OPERATOR_BEVEL &&
        session->selection_mode == SANDBOX3D_AUTHORING_SELECTION_VERTEX)
    {
        result = sandbox3d_authoring_object_replace_component_selection(
            session->object,
            (const uint32_t*)session->bevel_result_vertices,
            session->bevel_result_vertex_count,
            session->bevel_result_vertices[0U]);
        if (result != HENKA_SUCCESS)
        {
            henka_free(replacement_ids);
            return result;
        }
    }
    if (session->kind == SANDBOX3D_MODELING_OPERATOR_EXTRUDE &&
        session->selection_mode == SANDBOX3D_AUTHORING_SELECTION_FACE)
    {
        result = sandbox3d_authoring_object_replace_component_selection(
            session->object,
            (const uint32_t*)session->extrude_result_faces,
            session->extrude_result_face_count,
            session->extrude_result_faces[0U]);
        if (result != HENKA_SUCCESS)
        {
            henka_free(replacement_ids);
            return result;
        }
    }
    if (session->kind == SANDBOX3D_MODELING_OPERATOR_FILL_BOUNDARY_LOOP)
    {
        sandbox3d_authoring_object_set_selection_mode(
            session->object, SANDBOX3D_AUTHORING_SELECTION_FACE);
        result = sandbox3d_authoring_object_replace_component_selection(
            session->object,
            (const uint32_t*)session->fill_result_faces,
            session->fill_result_count,
            session->fill_result_faces[0U]);
        if (result != HENKA_SUCCESS)
        {
            henka_free(replacement_ids);
            return result;
        }
    }
    if (session->kind == SANDBOX3D_MODELING_OPERATOR_SPLIT_EDGE ||
        session->kind == SANDBOX3D_MODELING_OPERATOR_INSET ||
        session->kind == SANDBOX3D_MODELING_OPERATOR_SUBDIVIDE ||
        (session->kind == SANDBOX3D_MODELING_OPERATOR_EXTRUDE &&
         session->selection_mode == SANDBOX3D_AUTHORING_SELECTION_FACE) ||
        session->kind == SANDBOX3D_MODELING_OPERATOR_FILL_BOUNDARY_LOOP ||
        (session->kind == SANDBOX3D_MODELING_OPERATOR_BEVEL &&
         (session->selection_mode == SANDBOX3D_AUTHORING_SELECTION_VERTEX ||
          session->selection_mode == SANDBOX3D_AUTHORING_SELECTION_FACE)) ||
        session->kind == SANDBOX3D_MODELING_OPERATOR_RIP_VERTEX_FACE)
    {
        result = sandbox3d_authoring_object_record_current_selection(session->object);
        if (result != HENKA_SUCCESS)
        {
            henka_free(replacement_ids);
            return result;
        }
    }
    henka_free(replacement_ids);
    sandbox3d_modeling_operator_reset(session);
    return HENKA_SUCCESS;
}

henka_result sandbox3d_modeling_operator_cancel(
    sandbox3d_modeling_operator_session* session)
{
    henka_result result = HENKA_SUCCESS;

    if (session == NULL || !session->active)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (session->state == SANDBOX3D_MODELING_OPERATOR_STATE_PREVIEW)
    {
        result = sandbox3d_authoring_object_cancel_preview(session->object);
        if (result != HENKA_SUCCESS)
        {
            return result;
        }
    }
    sandbox3d_modeling_operator_reset(session);
    return HENKA_SUCCESS;
}
