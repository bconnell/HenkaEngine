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
    session->source_snapshot = NULL;
    session->selection_ids = NULL;
    session->selection_count = 0U;
    session->selection_capacity = 0U;
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
         kind != SANDBOX3D_MODELING_OPERATOR_EDGE_SLIDE &&
         kind != SANDBOX3D_MODELING_OPERATOR_BEVEL &&
         kind != SANDBOX3D_MODELING_OPERATOR_EXTRUDE &&
         kind != SANDBOX3D_MODELING_OPERATOR_EDGE_EXTRUDE &&
         kind != SANDBOX3D_MODELING_OPERATOR_EDGE_BRIDGE &&
         kind != SANDBOX3D_MODELING_OPERATOR_UV_PROJECT &&
         kind != SANDBOX3D_MODELING_OPERATOR_UV_PACK &&
         kind != SANDBOX3D_MODELING_OPERATOR_UV_TRANSFORM &&
         kind != SANDBOX3D_MODELING_OPERATOR_UV_ISLAND_TRANSFORM &&
         kind != SANDBOX3D_MODELING_OPERATOR_UV_ISLAND_PACK &&
         kind != SANDBOX3D_MODELING_OPERATOR_UV_PACK_ALL &&
         kind != SANDBOX3D_MODELING_OPERATOR_UV_UNWRAP_PLANAR &&
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
    if (kind == SANDBOX3D_MODELING_OPERATOR_EDGE_SLIDE &&
        selection_mode != SANDBOX3D_AUTHORING_SELECTION_EDGE)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (kind == SANDBOX3D_MODELING_OPERATOR_BEVEL &&
        selection_mode == SANDBOX3D_AUTHORING_SELECTION_FACE &&
        selected_count != 1U)
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
        selection_mode == SANDBOX3D_AUTHORING_SELECTION_EDGE && selected_count != 1U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (kind == SANDBOX3D_MODELING_OPERATOR_EDGE_EXTRUDE &&
        (selection_mode != SANDBOX3D_AUTHORING_SELECTION_EDGE || selected_count == 0U))
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
          kind == SANDBOX3D_MODELING_OPERATOR_UV_UNWRAP_PLANAR) &&
        (selection_mode != SANDBOX3D_AUTHORING_SELECTION_FACE || selected_count != 1U))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (selection_limit == 0U || selected_count == 0U || selected_count > selection_limit ||
        selected_count > SIZE_MAX / sizeof(uint32_t))
    {
        return HENKA_ERROR_LIMIT;
    }
    sandbox3d_modeling_operator_reset(session);
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
    session->amount = 0.0f;
    session->preview_rebuild_count = 0U;
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
    henka_authoring_vertex_id* bevel_result_vertices = NULL;
    henka_authoring_mesh* candidate = NULL;
    henka_authoring_modeling_report report = {0};
    henka_authoring_face_id bevel_result_face = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_vertex_id extrude_result_vertex = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_edge_id extrude_result_edge = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_face_id extrude_result_face = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_face_id bridge_result_face = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_edge_id* boundary_chain_edges = NULL;
    henka_authoring_mesh_desc mesh_desc = {0};
    size_t affected_count = 0U;
    size_t bevel_result_count = 0U;
    size_t bevel_result_capacity = 0U;
    size_t index;
    float next_amount;
    float applied_amount;
    henka_vec3 offset = {0.0f, 0.0f, 0.0f};
    henka_result result = HENKA_SUCCESS;

    if (session == NULL || !session->active ||
        (session->kind != SANDBOX3D_MODELING_OPERATOR_MOVE &&
         session->kind != SANDBOX3D_MODELING_OPERATOR_EDGE_SLIDE &&
         session->kind != SANDBOX3D_MODELING_OPERATOR_BEVEL &&
         session->kind != SANDBOX3D_MODELING_OPERATOR_EXTRUDE &&
         session->kind != SANDBOX3D_MODELING_OPERATOR_EDGE_EXTRUDE &&
         session->kind != SANDBOX3D_MODELING_OPERATOR_EDGE_BRIDGE &&
         session->kind != SANDBOX3D_MODELING_OPERATOR_UV_PROJECT &&
         session->kind != SANDBOX3D_MODELING_OPERATOR_UV_PACK &&
         session->kind != SANDBOX3D_MODELING_OPERATOR_UV_TRANSFORM &&
         session->kind != SANDBOX3D_MODELING_OPERATOR_UV_ISLAND_TRANSFORM &&
         session->kind != SANDBOX3D_MODELING_OPERATOR_UV_ISLAND_PACK &&
         session->kind != SANDBOX3D_MODELING_OPERATOR_UV_PACK_ALL &&
         session->kind != SANDBOX3D_MODELING_OPERATOR_UV_UNWRAP_PLANAR &&
         session->kind != SANDBOX3D_MODELING_OPERATOR_UV_SEAM_TOGGLE) ||
        session->source_snapshot == NULL || session->object == NULL ||
        session->selection_ids == NULL || session->selection_count == 0U ||
        (session->kind == SANDBOX3D_MODELING_OPERATOR_MOVE &&
            session->axis == SANDBOX3D_MODELING_OPERATOR_AXIS_NONE) ||
        (session->kind == SANDBOX3D_MODELING_OPERATOR_EDGE_SLIDE &&
            session->selection_mode != SANDBOX3D_AUTHORING_SELECTION_EDGE) ||
        (session->kind == SANDBOX3D_MODELING_OPERATOR_BEVEL &&
            session->selection_mode == SANDBOX3D_AUTHORING_SELECTION_FACE &&
            session->selection_count != 1U) ||
        (session->kind == SANDBOX3D_MODELING_OPERATOR_EXTRUDE &&
            (session->selection_mode != SANDBOX3D_AUTHORING_SELECTION_VERTEX &&
             session->selection_mode != SANDBOX3D_AUTHORING_SELECTION_EDGE &&
             session->selection_mode != SANDBOX3D_AUTHORING_SELECTION_FACE)) ||
        (session->kind == SANDBOX3D_MODELING_OPERATOR_EXTRUDE &&
            session->selection_mode == SANDBOX3D_AUTHORING_SELECTION_EDGE &&
            session->selection_count != 1U) ||
        (session->kind == SANDBOX3D_MODELING_OPERATOR_EXTRUDE &&
            session->selection_mode != SANDBOX3D_AUTHORING_SELECTION_FACE &&
            session->axis == SANDBOX3D_MODELING_OPERATOR_AXIS_NONE) ||
        (session->kind == SANDBOX3D_MODELING_OPERATOR_EDGE_EXTRUDE &&
            (session->selection_mode != SANDBOX3D_AUTHORING_SELECTION_EDGE ||
             session->selection_count == 0U)) ||
        (session->kind == SANDBOX3D_MODELING_OPERATOR_EDGE_BRIDGE &&
            (session->selection_mode != SANDBOX3D_AUTHORING_SELECTION_EDGE ||
             session->selection_count < 2U ||
             (session->selection_count != 2U &&
              (session->selection_count & 1U) != 0U))) ||
        (session->kind == SANDBOX3D_MODELING_OPERATOR_UV_SEAM_TOGGLE &&
            (session->selection_mode != SANDBOX3D_AUTHORING_SELECTION_EDGE ||
             session->selection_count == 0U)) ||
        ((session->kind == SANDBOX3D_MODELING_OPERATOR_UV_PROJECT ||
          session->kind == SANDBOX3D_MODELING_OPERATOR_UV_PACK ||
          session->kind == SANDBOX3D_MODELING_OPERATOR_UV_TRANSFORM ||
          session->kind == SANDBOX3D_MODELING_OPERATOR_UV_ISLAND_TRANSFORM ||
          session->kind == SANDBOX3D_MODELING_OPERATOR_UV_ISLAND_PACK ||
          session->kind == SANDBOX3D_MODELING_OPERATOR_UV_PACK_ALL ||
          session->kind == SANDBOX3D_MODELING_OPERATOR_UV_UNWRAP_PLANAR) &&
            (session->selection_mode != SANDBOX3D_AUTHORING_SELECTION_FACE ||
             session->selection_count != 1U)) ||
        (session->kind == SANDBOX3D_MODELING_OPERATOR_UV_PROJECT &&
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
        (session->kind == SANDBOX3D_MODELING_OPERATOR_BEVEL &&
            (applied_amount <= 0.0f || applied_amount > 1000000.0f)) ||
        ((session->kind == SANDBOX3D_MODELING_OPERATOR_EXTRUDE ||
          session->kind == SANDBOX3D_MODELING_OPERATOR_EDGE_EXTRUDE) &&
            fabsf(applied_amount) <= 1.0e-7f) ||
        ((session->kind == SANDBOX3D_MODELING_OPERATOR_UV_PACK ||
          session->kind == SANDBOX3D_MODELING_OPERATOR_UV_ISLAND_PACK ||
          session->kind == SANDBOX3D_MODELING_OPERATOR_UV_PACK_ALL ||
          session->kind == SANDBOX3D_MODELING_OPERATOR_UV_UNWRAP_PLANAR) &&
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
        result = henka_authoring_mesh_slide_edge_loop(
            candidate,
            (const henka_authoring_edge_id*)session->selection_ids,
            session->selection_count,
            applied_amount,
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
            result = henka_authoring_mesh_bevel_face(
                candidate,
                (henka_authoring_face_id)session->selection_ids[0U],
                applied_amount,
                &bevel_result_face);
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
            else if (same_boundary_face)
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
                size_t selection_index;
                for (selection_index = 0U;
                     selection_index < session->selection_count;
                     ++selection_index)
                {
                    const henka_authoring_vertex_id vertex_id =
                        (henka_authoring_vertex_id)session->selection_ids[selection_index];
                    const size_t edge_count = henka_authoring_mesh_get_vertex_edge_count(
                        session->source_snapshot, vertex_id);
                    all_loose = all_loose && edge_count == 0U;
                    all_surface = all_surface && edge_count != 0U;
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
            result = henka_authoring_mesh_extrude_loose_edge(
                candidate,
                (henka_authoring_edge_id)session->selection_ids[0U],
                direction,
                applied_amount,
                &extrude_result_edge,
                &extrude_result_face,
                &report);
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
        henka_free(bevel_result_vertices);
        henka_free(boundary_chain_edges);
        return result;
    }
    session->amount = next_amount;
    ++session->preview_rebuild_count;
    session->state = SANDBOX3D_MODELING_OPERATOR_STATE_PREVIEW;
    henka_free(vertices);
    henka_free(bevel_result_vertices);
    henka_free(boundary_chain_edges);
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

henka_result sandbox3d_modeling_operator_commit(
    sandbox3d_modeling_operator_session* session)
{
    henka_result result;

    if (session == NULL || !session->active ||
        session->state != SANDBOX3D_MODELING_OPERATOR_STATE_PREVIEW ||
        session->object == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = sandbox3d_authoring_object_commit_preview(session->object);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
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
