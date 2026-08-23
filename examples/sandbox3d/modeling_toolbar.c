#include "modeling_toolbar.h"

#include <stdio.h>
#include <string.h>

static bool sandbox3d_modeling_toolbar_action_requires_selection(
    sandbox3d_modeling_toolbar_action action)
{
    return action == SANDBOX3D_MODELING_TOOLBAR_ACTION_MOVE ||
        action == SANDBOX3D_MODELING_TOOLBAR_ACTION_ROTATE ||
        action == SANDBOX3D_MODELING_TOOLBAR_ACTION_SCALE;
}

void sandbox3d_modeling_toolbar_state_reset(
    sandbox3d_modeling_toolbar_state* state)
{
    if (state == NULL)
    {
        return;
    }
    state->selection_mode = SANDBOX3D_AUTHORING_SELECTION_FACE;
    state->transform_tool = SANDBOX3D_TRANSFORM_TOOL_NONE;
    state->orientation_mode = SANDBOX3D_AUTHORING_ORIENTATION_WORLD;
    state->pivot_mode = SANDBOX3D_AUTHORING_PIVOT_MEDIAN;
    state->snap_enabled = false;
    state->xray_enabled = false;
    state->authoring_available = false;
    state->selected_component_count = 0U;
}

bool sandbox3d_modeling_toolbar_set_selection_mode(
    sandbox3d_modeling_toolbar_state* state,
    sandbox3d_authoring_selection_mode mode)
{
    if (state == NULL || mode < SANDBOX3D_AUTHORING_SELECTION_VERTEX ||
        mode > SANDBOX3D_AUTHORING_SELECTION_FACE)
    {
        return false;
    }
    state->selection_mode = mode;
    return true;
}

bool sandbox3d_modeling_toolbar_set_transform_tool(
    sandbox3d_modeling_toolbar_state* state,
    sandbox3d_transform_tool tool)
{
    if (state == NULL || tool < SANDBOX3D_TRANSFORM_TOOL_NONE ||
        tool > SANDBOX3D_TRANSFORM_TOOL_SCALE)
    {
        return false;
    }
    state->transform_tool = tool;
    return true;
}

bool sandbox3d_modeling_toolbar_set_orientation_mode(
    sandbox3d_modeling_toolbar_state* state,
    sandbox3d_authoring_orientation_mode mode)
{
    if (state == NULL || mode < SANDBOX3D_AUTHORING_ORIENTATION_WORLD ||
        mode > SANDBOX3D_AUTHORING_ORIENTATION_NORMAL)
    {
        return false;
    }
    state->orientation_mode = mode;
    return true;
}

bool sandbox3d_modeling_toolbar_set_pivot_mode(
    sandbox3d_modeling_toolbar_state* state,
    sandbox3d_authoring_pivot_mode mode)
{
    if (state == NULL || mode < SANDBOX3D_AUTHORING_PIVOT_MEDIAN ||
        mode > SANDBOX3D_AUTHORING_PIVOT_INDIVIDUAL)
    {
        return false;
    }
    state->pivot_mode = mode;
    return true;
}

const char* sandbox3d_modeling_toolbar_selection_label(
    sandbox3d_authoring_selection_mode mode)
{
    switch (mode)
    {
        case SANDBOX3D_AUTHORING_SELECTION_VERTEX:
            return "Vertex";
        case SANDBOX3D_AUTHORING_SELECTION_EDGE:
            return "Edge";
        case SANDBOX3D_AUTHORING_SELECTION_FACE:
            return "Face";
        default:
            return "Unknown";
    }
}

const char* sandbox3d_modeling_toolbar_transform_label(
    sandbox3d_transform_tool tool)
{
    switch (tool)
    {
        case SANDBOX3D_TRANSFORM_TOOL_MOVE:
            return "Move";
        case SANDBOX3D_TRANSFORM_TOOL_ROTATE:
            return "Rotate";
        case SANDBOX3D_TRANSFORM_TOOL_SCALE:
            return "Scale";
        case SANDBOX3D_TRANSFORM_TOOL_NONE:
        default:
            return "Select";
    }
}

const char* sandbox3d_modeling_toolbar_orientation_label(
    sandbox3d_authoring_orientation_mode mode)
{
    switch (mode)
    {
        case SANDBOX3D_AUTHORING_ORIENTATION_WORLD:
            return "World";
        case SANDBOX3D_AUTHORING_ORIENTATION_LOCAL:
            return "Local";
        case SANDBOX3D_AUTHORING_ORIENTATION_NORMAL:
            return "Normal";
        default:
            return "Unknown";
    }
}

const char* sandbox3d_modeling_toolbar_pivot_label(
    sandbox3d_authoring_pivot_mode mode)
{
    switch (mode)
    {
        case SANDBOX3D_AUTHORING_PIVOT_MEDIAN:
            return "Median";
        case SANDBOX3D_AUTHORING_PIVOT_ACTIVE:
            return "Active";
        case SANDBOX3D_AUTHORING_PIVOT_INDIVIDUAL:
            return "Individual";
        default:
            return "Unknown";
    }
}

const char* sandbox3d_modeling_toolbar_action_tooltip(
    sandbox3d_modeling_toolbar_action action)
{
    switch (action)
    {
        case SANDBOX3D_MODELING_TOOLBAR_ACTION_SELECT:
            return "Select components in the active mesh mode.";
        case SANDBOX3D_MODELING_TOOLBAR_ACTION_MOVE:
            return "Move selected components with a preview, numeric entry, and undo.";
        case SANDBOX3D_MODELING_TOOLBAR_ACTION_ROTATE:
            return "Rotate selected components using the chosen orientation and pivot.";
        case SANDBOX3D_MODELING_TOOLBAR_ACTION_SCALE:
            return "Scale selected components using the chosen pivot.";
        case SANDBOX3D_MODELING_TOOLBAR_ACTION_SNAP:
            return "Toggle the current transform snap policy.";
        case SANDBOX3D_MODELING_TOOLBAR_ACTION_XRAY:
            return "Show components through surfaces when the depth-overlay path is available.";
        case SANDBOX3D_MODELING_TOOLBAR_ACTION_COUNT:
        default:
            return "Unavailable modeling action.";
    }
}

const char* sandbox3d_modeling_toolbar_disabled_reason(
    sandbox3d_modeling_toolbar_action action,
    const sandbox3d_modeling_toolbar_state* state)
{
    if (action == SANDBOX3D_MODELING_TOOLBAR_ACTION_XRAY)
    {
        return "X-Ray is unavailable until the depth-overlay path is implemented.";
    }
    if (state == NULL)
    {
        return "Toolbar state is unavailable.";
    }
    if (sandbox3d_modeling_toolbar_action_requires_selection(action))
    {
        if (!state->authoring_available)
        {
            return "Make the selected asset editable first.";
        }
        if (state->selected_component_count == 0U)
        {
            return "Select at least one component first.";
        }
    }
    return "";
}

henka_result sandbox3d_modeling_toolbar_format_summary(
    const sandbox3d_modeling_toolbar_state* state,
    char* buffer,
    size_t buffer_size)
{
    char formatted[128];
    int required;

    if (state == NULL || buffer == NULL || buffer_size == 0U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    required = snprintf(
        formatted,
        sizeof(formatted),
        "%s | %s | %s | %s | %zu selected",
        sandbox3d_modeling_toolbar_selection_label(state->selection_mode),
        sandbox3d_modeling_toolbar_transform_label(state->transform_tool),
        sandbox3d_modeling_toolbar_orientation_label(state->orientation_mode),
        sandbox3d_modeling_toolbar_pivot_label(state->pivot_mode),
        state->selected_component_count);
    if (required < 0 || (size_t)required >= sizeof(formatted))
    {
        return HENKA_ERROR_LIMIT;
    }
    if ((size_t)required >= buffer_size)
    {
        return HENKA_ERROR_LIMIT;
    }
    memcpy(buffer, formatted, (size_t)required + 1U);
    return HENKA_SUCCESS;
}
