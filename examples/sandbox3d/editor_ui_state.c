#include "editor_ui_state.h"

#include <float.h>
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
static const char* g_details_authoring_key =
    "ui.object_details.authoring.expanded";
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
    state->details_authoring_expanded = true;
    state->details_physics_expanded = false;
    state->details_interaction_expanded = false;
    state->details_actions_expanded = false;

    state->controls_scroll_offset = 0.0f;
    state->controls_content_height = 0.0f;
    state->details_scroll_offset = 0.0f;
    state->details_content_height = 0.0f;
    state->controls_scroll_dragging = false;
    state->details_scroll_dragging = false;
    state->controls_scroll_grab_offset = 0.0f;
    state->details_scroll_grab_offset = 0.0f;
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
            {g_details_authoring_key, &state->details_authoring_expanded},
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
            {g_details_authoring_key, &state->details_authoring_expanded},
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

void sandbox3d_editor_ui_scroll_state_set_content(
    sandbox3d_editor_scroll_state* state,
    float content_height,
    float viewport_height)
{
    if (state == NULL)
    {
        return;
    }

    state->content_height = isfinite(content_height) && content_height >= 0.0f
        ? content_height
        : 0.0f;
    state->viewport_height = isfinite(viewport_height) && viewport_height > 0.0f
        ? viewport_height
        : 0.0f;
    state->offset = sandbox3d_editor_ui_clamp_scroll(
        state->offset,
        state->content_height,
        state->viewport_height);
}

bool sandbox3d_editor_ui_scroll_state_apply_delta(
    sandbox3d_editor_scroll_state* state,
    float delta_pixels,
    float viewport_height)
{
    float requested_offset;

    if (state == NULL || !isfinite(delta_pixels) || delta_pixels == 0.0f)
    {
        if (state != NULL)
        {
            sandbox3d_editor_ui_scroll_state_set_content(
                state,
                state->content_height,
                viewport_height);
        }
        return false;
    }

    sandbox3d_editor_ui_scroll_state_set_content(
        state,
        state->content_height,
        viewport_height);
    if (state->content_height <= state->viewport_height)
    {
        state->offset = 0.0f;
        return false;
    }

    requested_offset = state->offset + delta_pixels;
    if (!isfinite(requested_offset))
    {
        requested_offset = delta_pixels < 0.0f ? 0.0f : FLT_MAX;
    }
    state->offset = sandbox3d_editor_ui_clamp_scroll(
        requested_offset,
        state->content_height,
        state->viewport_height);
    return true;
}

float sandbox3d_editor_ui_scrollbar_thumb_height(
    float content_height,
    float viewport_height,
    float track_height)
{
    float thumb_height;

    if (!isfinite(content_height) || !isfinite(viewport_height) ||
        !isfinite(track_height) || content_height <= viewport_height ||
        viewport_height <= 0.0f || track_height <= 0.0f)
    {
        return 0.0f;
    }

    thumb_height = track_height * viewport_height / content_height;
    if (!isfinite(thumb_height))
    {
        return 0.0f;
    }
    if (thumb_height < 24.0f)
    {
        thumb_height = 24.0f;
    }
    return thumb_height > track_height ? track_height : thumb_height;
}

float sandbox3d_editor_ui_scrollbar_thumb_offset(
    float scroll_offset,
    float content_height,
    float viewport_height,
    float track_height,
    float thumb_height)
{
    const float maximum_offset = content_height - viewport_height;
    const float travel = track_height - thumb_height;
    const float clamped_offset = sandbox3d_editor_ui_clamp_scroll(
        scroll_offset,
        content_height,
        viewport_height);

    if (maximum_offset <= 0.0f || travel <= 0.0f)
    {
        return 0.0f;
    }
    return travel * clamped_offset / maximum_offset;
}

bool sandbox3d_editor_ui_scroll_state_set_from_scrollbar(
    sandbox3d_editor_scroll_state* state,
    float pointer_y,
    float track_y,
    float track_height,
    float thumb_height,
    float grab_offset)
{
    float travel;
    float thumb_y;
    float normalized;

    if (state == NULL || !isfinite(pointer_y) || !isfinite(track_y) ||
        !isfinite(track_height) || !isfinite(thumb_height) ||
        !isfinite(grab_offset) || track_height <= 0.0f ||
        thumb_height <= 0.0f || thumb_height > track_height ||
        state->content_height <= state->viewport_height)
    {
        return false;
    }

    travel = track_height - thumb_height;
    if (travel <= 0.0f)
    {
        state->offset = 0.0f;
        return false;
    }
    thumb_y = pointer_y - grab_offset - track_y;
    if (thumb_y < 0.0f)
    {
        thumb_y = 0.0f;
    }
    if (thumb_y > travel)
    {
        thumb_y = travel;
    }
    normalized = thumb_y / travel;
    state->offset = sandbox3d_editor_ui_clamp_scroll(
        normalized * (state->content_height - state->viewport_height),
        state->content_height,
        state->viewport_height);
    return true;
}

static bool sandbox3d_editor_ui_scroll_by(
    float* offset,
    float content_height,
    float viewport_height,
    float delta_pixels)
{
    sandbox3d_editor_scroll_state state;

    if (offset == NULL)
    {
        return false;
    }
    state.offset = *offset;
    state.content_height = content_height;
    state.viewport_height = viewport_height;
    if (!sandbox3d_editor_ui_scroll_state_apply_delta(
            &state,
            delta_pixels,
            viewport_height))
    {
        *offset = state.offset;
        return false;
    }
    *offset = state.offset;
    return true;
}

bool sandbox3d_editor_ui_scroll_controls_by(
    sandbox3d_editor_ui_state* state,
    float viewport_height,
    float delta_pixels)
{
    return state != NULL && sandbox3d_editor_ui_scroll_by(
        &state->controls_scroll_offset,
        state->controls_content_height,
        viewport_height,
        delta_pixels);
}

bool sandbox3d_editor_ui_scroll_details_by(
    sandbox3d_editor_ui_state* state,
    float viewport_height,
    float delta_pixels)
{
    return state != NULL && sandbox3d_editor_ui_scroll_by(
        &state->details_scroll_offset,
        state->details_content_height,
        viewport_height,
        delta_pixels);
}

bool sandbox3d_editor_ui_scroll_controls(
    sandbox3d_editor_ui_state* state,
    float viewport_height,
    int direction)
{
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
    return sandbox3d_editor_ui_scroll_controls_by(
        state,
        viewport_height,
        (float)direction * 48.0f);
}

bool sandbox3d_editor_ui_scroll_details(
    sandbox3d_editor_ui_state* state,
    float viewport_height,
    int direction)
{
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
    return sandbox3d_editor_ui_scroll_details_by(
        state,
        viewport_height,
        (float)direction * 48.0f);
}
