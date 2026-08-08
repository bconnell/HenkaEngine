#include "editor_ui_state.h"

#include <math.h>
#include <stddef.h>

typedef struct sandbox3d_editor_ui_bool_binding
{
    const char* key;
    bool* value;
} sandbox3d_editor_ui_bool_binding;

typedef struct sandbox3d_editor_ui_const_bool_binding
{
    const char* key;
    const bool* value;
} sandbox3d_editor_ui_const_bool_binding;

static const char* g_controls_workspace_key =
    "ui.controls.main.workspace.expanded";
static const char* g_controls_viewer_key =
    "ui.controls.main.viewer.expanded";
static const char* g_controls_viewport_key =
    "ui.controls.main.viewport.expanded";
static const char* g_controls_viewport_tool_key =
    "ui.controls.main.viewport_tool.expanded";

static const char* g_details_overview_key =
    "ui.object_details.overview.expanded";
static const char* g_details_transform_key =
    "ui.object_details.transform.expanded";
static const char* g_details_materials_key =
    "ui.object_details.materials.expanded";
static const char* g_details_physics_key =
    "ui.object_details.physics.expanded";
static const char* g_details_interaction_key =
    "ui.object_details.interaction.expanded";
static const char* g_details_actions_key =
    "ui.object_details.actions.expanded";

void sandbox3d_editor_ui_state_reset(
    sandbox3d_editor_ui_state* state)
{
    if (state == NULL)
    {
        return;
    }

    state->controls_workspace_expanded = false;
    state->controls_viewer_expanded = true;
    state->controls_viewport_expanded = true;
    state->controls_viewport_tool_expanded = false;

    state->details_overview_expanded = true;
    state->details_transform_expanded = true;
    state->details_materials_expanded = true;
    state->details_physics_expanded = false;
    state->details_interaction_expanded = false;
    state->details_actions_expanded = false;

    state->controls_scroll_offset = 0.0f;
    state->controls_content_height = 0.0f;
    state->details_scroll_offset = 0.0f;
    state->details_content_height = 0.0f;
}

void sandbox3d_editor_ui_state_load(
    const henka_settings* settings,
    sandbox3d_editor_ui_state* state)
{
    size_t index;

    if (state == NULL)
    {
        return;
    }

    sandbox3d_editor_ui_state_reset(state);

    if (settings == NULL)
    {
        return;
    }

    {
        sandbox3d_editor_ui_bool_binding bindings[] =
        {
            {g_controls_workspace_key, &state->controls_workspace_expanded},
            {g_controls_viewer_key, &state->controls_viewer_expanded},
            {g_controls_viewport_key, &state->controls_viewport_expanded},
            {g_controls_viewport_tool_key, &state->controls_viewport_tool_expanded},
            {g_details_overview_key, &state->details_overview_expanded},
            {g_details_transform_key, &state->details_transform_expanded},
            {g_details_materials_key, &state->details_materials_expanded},
            {g_details_physics_key, &state->details_physics_expanded},
            {g_details_interaction_key, &state->details_interaction_expanded},
            {g_details_actions_key, &state->details_actions_expanded}
        };

        for (index = 0U;
             index < sizeof(bindings) / sizeof(bindings[0]);
             ++index)
        {
            if (henka_settings_has_key(settings, bindings[index].key))
            {
                *bindings[index].value =
                    henka_settings_get_bool(
                        settings,
                        bindings[index].key,
                        *bindings[index].value);
            }
        }
    }
}

henka_result sandbox3d_editor_ui_state_store(
    henka_settings* settings,
    const sandbox3d_editor_ui_state* state)
{
    size_t index;

    if (settings == NULL || state == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    {
        const sandbox3d_editor_ui_const_bool_binding bindings[] =
        {
            {g_controls_workspace_key, &state->controls_workspace_expanded},
            {g_controls_viewer_key, &state->controls_viewer_expanded},
            {g_controls_viewport_key, &state->controls_viewport_expanded},
            {g_controls_viewport_tool_key, &state->controls_viewport_tool_expanded},
            {g_details_overview_key, &state->details_overview_expanded},
            {g_details_transform_key, &state->details_transform_expanded},
            {g_details_materials_key, &state->details_materials_expanded},
            {g_details_physics_key, &state->details_physics_expanded},
            {g_details_interaction_key, &state->details_interaction_expanded},
            {g_details_actions_key, &state->details_actions_expanded}
        };

        for (index = 0U;
             index < sizeof(bindings) / sizeof(bindings[0]);
             ++index)
        {
            const henka_result result =
                henka_settings_set_bool(
                    settings,
                    bindings[index].key,
                    *bindings[index].value);
            if (result != HENKA_SUCCESS)
            {
                return result;
            }
        }
    }

    return HENKA_SUCCESS;
}

float sandbox3d_editor_ui_clamp_scroll(
    float requested_offset,
    float content_height,
    float viewport_height)
{
    float maximum_offset;

    if (!isfinite(requested_offset) ||
        !isfinite(content_height) ||
        !isfinite(viewport_height) ||
        requested_offset < 0.0f ||
        content_height < 0.0f ||
        viewport_height <= 0.0f ||
        content_height <= viewport_height)
    {
        return 0.0f;
    }

    maximum_offset = content_height - viewport_height;
    return requested_offset > maximum_offset
        ? maximum_offset
        : requested_offset;
}

bool sandbox3d_editor_ui_scroll_controls(
    sandbox3d_editor_ui_state* state,
    float viewport_height,
    int direction)
{
    float requested_offset;
    float step_direction;

    if (state == NULL ||
        !isfinite(viewport_height) ||
        viewport_height <= 0.0f ||
        direction == 0)
    {
        if (state != NULL)
        {
            state->controls_scroll_offset = 0.0f;
        }
        return false;
    }

    state->controls_scroll_offset =
        sandbox3d_editor_ui_clamp_scroll(
            state->controls_scroll_offset,
            state->controls_content_height,
            viewport_height);

    if (!isfinite(state->controls_content_height) ||
        state->controls_content_height <= viewport_height)
    {
        state->controls_scroll_offset = 0.0f;
        return false;
    }

    step_direction = (float)direction;
    requested_offset =
        state->controls_scroll_offset +
        step_direction * 48.0f;
    if (requested_offset < 0.0f)
    {
        requested_offset = 0.0f;
    }

    state->controls_scroll_offset =
        sandbox3d_editor_ui_clamp_scroll(
            requested_offset,
            state->controls_content_height,
            viewport_height);
    return true;
}

bool sandbox3d_editor_ui_scroll_details(
    sandbox3d_editor_ui_state* state,
    float viewport_height,
    int direction)
{
    float requested_offset;

    if (state == NULL ||
        !isfinite(viewport_height) ||
        viewport_height <= 0.0f ||
        direction == 0)
    {
        if (state != NULL)
        {
            state->details_scroll_offset = 0.0f;
        }
        return false;
    }

    state->details_scroll_offset =
        sandbox3d_editor_ui_clamp_scroll(
            state->details_scroll_offset,
            state->details_content_height,
            viewport_height);

    if (!isfinite(state->details_content_height) ||
        state->details_content_height <= viewport_height)
    {
        state->details_scroll_offset = 0.0f;
        return false;
    }

    requested_offset =
        state->details_scroll_offset +
        (float)direction * 48.0f;
    if (requested_offset < 0.0f)
    {
        requested_offset = 0.0f;
    }

    state->details_scroll_offset =
        sandbox3d_editor_ui_clamp_scroll(
            requested_offset,
            state->details_content_height,
            viewport_height);
    return true;
}
