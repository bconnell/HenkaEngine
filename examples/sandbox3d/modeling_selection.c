#include "modeling_selection.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <henka/authoring_mesh.h>

static bool sandbox3d_modeling_selection_finite_point(henka_vec2 point)
{
    return isfinite(point.x) && isfinite(point.y);
}

static int sandbox3d_modeling_selection_compare_ids(
    const void* left_pointer,
    const void* right_pointer)
{
    const uint32_t left = *(const uint32_t*)left_pointer;
    const uint32_t right = *(const uint32_t*)right_pointer;
    return left < right ? -1 : left > right ? 1 : 0;
}

static size_t sandbox3d_modeling_selection_sort_unique(
    uint32_t* ids,
    size_t count)
{
    size_t read_index;
    size_t write_count;

    if (ids == NULL || count == 0U)
    {
        return 0U;
    }
    qsort(ids, count, sizeof(ids[0]), sandbox3d_modeling_selection_compare_ids);
    write_count = 1U;
    for (read_index = 1U; read_index < count; ++read_index)
    {
        if (ids[read_index] != ids[write_count - 1U])
        {
            ids[write_count++] = ids[read_index];
        }
    }
    return write_count;
}

static bool sandbox3d_modeling_selection_contains_sorted(
    const uint32_t* ids,
    size_t count,
    uint32_t id)
{
    size_t low = 0U;
    size_t high = count;

    while (low < high)
    {
        const size_t middle = low + (high - low) / 2U;
        if (ids[middle] < id)
        {
            low = middle + 1U;
        }
        else
        {
            high = middle;
        }
    }
    return low < count && ids[low] == id;
}

static bool sandbox3d_modeling_selection_candidate_is_eligible(
    const sandbox3d_modeling_selection_candidate* candidate,
    sandbox3d_modeling_selection_rect rect,
    bool xray_enabled,
    bool front_facing_only)
{
    return candidate != NULL &&
        candidate->screen_position.x >= rect.minimum_x &&
        candidate->screen_position.x <= rect.maximum_x &&
        candidate->screen_position.y >= rect.minimum_y &&
        candidate->screen_position.y <= rect.maximum_y &&
        (!front_facing_only || candidate->front_facing) &&
        (xray_enabled || candidate->visible);
}

void sandbox3d_modeling_selection_reset(
    sandbox3d_modeling_selection_session* session)
{
    if (session == NULL)
    {
        return;
    }
    memset(session, 0, sizeof(*session));
    session->operation = SANDBOX3D_MODELING_SELECTION_REPLACE;
}

sandbox3d_modeling_selection_operation
sandbox3d_modeling_selection_operation_from_modifiers(
    bool control_down,
    bool shift_down)
{
    if (shift_down)
    {
        return SANDBOX3D_MODELING_SELECTION_SUBTRACT;
    }
    return control_down
        ? SANDBOX3D_MODELING_SELECTION_ADD
        : SANDBOX3D_MODELING_SELECTION_REPLACE;
}

henka_result sandbox3d_modeling_selection_begin(
    sandbox3d_modeling_selection_session* session,
    henka_vec2 start,
    float viewport_width,
    float viewport_height,
    sandbox3d_modeling_selection_operation operation)
{
    if (session == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    sandbox3d_modeling_selection_reset(session);
    if (!sandbox3d_modeling_selection_finite_point(start) ||
        !isfinite(viewport_width) || !isfinite(viewport_height) ||
        viewport_width <= 0.0f || viewport_height <= 0.0f ||
        start.x < 0.0f || start.y < 0.0f ||
        start.x >= viewport_width || start.y >= viewport_height ||
        operation < SANDBOX3D_MODELING_SELECTION_REPLACE ||
        operation > SANDBOX3D_MODELING_SELECTION_SUBTRACT)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    session->active = true;
    session->operation = operation;
    session->start = start;
    session->current = start;
    session->viewport_width = viewport_width;
    session->viewport_height = viewport_height;
    return HENKA_SUCCESS;
}

henka_result sandbox3d_modeling_selection_update(
    sandbox3d_modeling_selection_session* session,
    henka_vec2 current,
    float drag_threshold)
{
    float delta_x;
    float delta_y;

    if (session == NULL || !session->active ||
        !sandbox3d_modeling_selection_finite_point(current) ||
        !isfinite(drag_threshold) || drag_threshold < 0.0f)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    session->current.x = fminf(session->viewport_width, fmaxf(0.0f, current.x));
    session->current.y = fminf(session->viewport_height, fmaxf(0.0f, current.y));
    delta_x = session->current.x - session->start.x;
    delta_y = session->current.y - session->start.y;
    if (delta_x * delta_x + delta_y * delta_y >= drag_threshold * drag_threshold)
    {
        session->dragging = true;
    }
    return HENKA_SUCCESS;
}

henka_result sandbox3d_modeling_selection_get_rect(
    const sandbox3d_modeling_selection_session* session,
    sandbox3d_modeling_selection_rect* out_rect)
{
    if (session == NULL || out_rect == NULL || !session->active)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    out_rect->minimum_x = fminf(session->start.x, session->current.x);
    out_rect->minimum_y = fminf(session->start.y, session->current.y);
    out_rect->maximum_x = fmaxf(session->start.x, session->current.x);
    out_rect->maximum_y = fmaxf(session->start.y, session->current.y);
    return HENKA_SUCCESS;
}

henka_result sandbox3d_modeling_selection_build_result(
    const sandbox3d_modeling_selection_session* session,
    const sandbox3d_modeling_selection_candidate* candidates,
    size_t candidate_count,
    bool xray_enabled,
    bool front_facing_only,
    const uint32_t* prior_ids,
    size_t prior_count,
    uint32_t* out_ids,
    size_t output_capacity,
    size_t* out_count,
    uint32_t* out_active_id)
{
    sandbox3d_modeling_selection_rect rect;
    size_t required_capacity;
    size_t candidate_write_count = 0U;
    size_t index;

    if (out_count == NULL || out_active_id == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_count = 0U;
    *out_active_id = HENKA_AUTHORING_INVALID_ID;
    if (session == NULL || !session->active || !session->dragging ||
        (candidate_count > 0U && candidates == NULL) ||
        (prior_count > 0U && prior_ids == NULL) || out_ids == NULL ||
        session->operation < SANDBOX3D_MODELING_SELECTION_REPLACE ||
        session->operation > SANDBOX3D_MODELING_SELECTION_SUBTRACT ||
        sandbox3d_modeling_selection_get_rect(session, &rect) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (candidate_count > SIZE_MAX - prior_count)
    {
        return HENKA_ERROR_LIMIT;
    }
    required_capacity = candidate_count + prior_count;
    if (required_capacity > output_capacity)
    {
        return HENKA_ERROR_LIMIT;
    }
    for (index = 0U; index < prior_count; ++index)
    {
        if (prior_ids[index] == HENKA_AUTHORING_INVALID_ID)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
    }
    for (index = 0U; index < candidate_count; ++index)
    {
        if (candidates[index].component_id == HENKA_AUTHORING_INVALID_ID ||
            !sandbox3d_modeling_selection_finite_point(candidates[index].screen_position) ||
            !isfinite(candidates[index].depth))
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
    }

    if (session->operation == SANDBOX3D_MODELING_SELECTION_SUBTRACT)
    {
        size_t result_count = 0U;
        for (index = 0U; index < prior_count; ++index)
        {
            out_ids[index] = prior_ids[index];
        }
        for (index = 0U; index < candidate_count; ++index)
        {
            if (sandbox3d_modeling_selection_candidate_is_eligible(
                    &candidates[index], rect, xray_enabled, front_facing_only))
            {
                out_ids[prior_count + candidate_write_count++] =
                    candidates[index].component_id;
            }
        }
        candidate_write_count = sandbox3d_modeling_selection_sort_unique(
            out_ids + prior_count, candidate_write_count);
        for (index = 0U; index < prior_count; ++index)
        {
            if (!sandbox3d_modeling_selection_contains_sorted(
                    out_ids + prior_count, candidate_write_count, prior_ids[index]))
            {
                out_ids[result_count++] = prior_ids[index];
            }
        }
        result_count = sandbox3d_modeling_selection_sort_unique(out_ids, result_count);
        *out_count = result_count;
        *out_active_id = result_count > 0U
            ? out_ids[result_count - 1U]
            : HENKA_AUTHORING_INVALID_ID;
        return HENKA_SUCCESS;
    }

    if (session->operation == SANDBOX3D_MODELING_SELECTION_ADD)
    {
        for (index = 0U; index < prior_count; ++index)
        {
            out_ids[candidate_write_count++] = prior_ids[index];
        }
    }
    {
        uint32_t newest_candidate = HENKA_AUTHORING_INVALID_ID;
        for (index = 0U; index < candidate_count; ++index)
        {
            if (sandbox3d_modeling_selection_candidate_is_eligible(
                    &candidates[index], rect, xray_enabled, front_facing_only))
            {
                out_ids[candidate_write_count++] = candidates[index].component_id;
                if (newest_candidate == HENKA_AUTHORING_INVALID_ID ||
                    candidates[index].component_id > newest_candidate)
                {
                    newest_candidate = candidates[index].component_id;
                }
            }
        }
        candidate_write_count = sandbox3d_modeling_selection_sort_unique(
            out_ids, candidate_write_count);
        *out_count = candidate_write_count;
        *out_active_id = newest_candidate != HENKA_AUTHORING_INVALID_ID
            ? newest_candidate
            : candidate_write_count > 0U
                ? out_ids[candidate_write_count - 1U]
                : HENKA_AUTHORING_INVALID_ID;
    }
    return HENKA_SUCCESS;
}
