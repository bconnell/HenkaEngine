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
         kind != SANDBOX3D_MODELING_OPERATOR_EXTRUDE))
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
         selection_mode != SANDBOX3D_AUTHORING_SELECTION_EDGE))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (kind == SANDBOX3D_MODELING_OPERATOR_EXTRUDE && selected_count != 1U)
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
         session->kind != SANDBOX3D_MODELING_OPERATOR_EXTRUDE) ||
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
             session->selection_mode != SANDBOX3D_AUTHORING_SELECTION_EDGE)) ||
        (session->kind == SANDBOX3D_MODELING_OPERATOR_EXTRUDE &&
            session->selection_count != 1U) ||
        (session->kind == SANDBOX3D_MODELING_OPERATOR_EXTRUDE &&
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
        (session->kind == SANDBOX3D_MODELING_OPERATOR_EXTRUDE &&
            fabsf(applied_amount) <= 1.0e-7f) ||
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
    if (result == HENKA_SUCCESS)
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
            result = henka_authoring_mesh_extrude_loose_vertex(
                candidate,
                (henka_authoring_vertex_id)session->selection_ids[0U],
                direction,
                applied_amount,
                &extrude_result_vertex,
                &extrude_result_edge,
                &report);
        }
        else
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
        return result;
    }
    session->amount = next_amount;
    ++session->preview_rebuild_count;
    session->state = SANDBOX3D_MODELING_OPERATOR_STATE_PREVIEW;
    henka_free(vertices);
    henka_free(bevel_result_vertices);
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
