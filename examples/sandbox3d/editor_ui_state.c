#include "editor_ui_state.h"

#include <float.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

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
static const char* g_details_audio_key =
    "ui.object_details.audio.expanded";

static const char* g_details_group_order_prefix =
    "ui.object_details.group_order.";
static const char* g_controls_scroll_offset_key =
    "ui.controls.scroll.offset";
static const char* g_details_scroll_offset_key =
    "ui.object_details.scroll.offset";

static float sandbox3d_editor_ui_sanitize_scroll_offset(float value)
{
    return isfinite(value) && value > 0.0f ? value : 0.0f;
}

static void sandbox3d_editor_ui_details_group_order_reset(
    unsigned char* order)
{
    size_t index;

    if (order == NULL)
    {
        return;
    }
    for (index = 0U; index < SANDBOX3D_EDITOR_DETAILS_GROUP_COUNT; ++index)
    {
        order[index] = (unsigned char)index;
    }
}

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
    state->details_audio_expanded = false;
    sandbox3d_editor_ui_details_group_order_reset(state->details_group_order);

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
            {g_details_actions_key, &state->details_actions_expanded},
            {g_details_audio_key, &state->details_audio_expanded}
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

        state->controls_scroll_offset =
            sandbox3d_editor_ui_sanitize_scroll_offset(
                henka_settings_get_float(
                    settings,
                    g_controls_scroll_offset_key,
                    0.0f));
        state->details_scroll_offset =
            sandbox3d_editor_ui_sanitize_scroll_offset(
                henka_settings_get_float(
                    settings,
                    g_details_scroll_offset_key,
                    0.0f));

        {
            unsigned char loaded_order[SANDBOX3D_EDITOR_DETAILS_GROUP_COUNT];
            char key[96];
            bool has_complete_order = true;

            sandbox3d_editor_ui_details_group_order_reset(loaded_order);
            for (index = 0U;
                 index < SANDBOX3D_EDITOR_DETAILS_GROUP_COUNT;
                 ++index)
            {
                (void)snprintf(
                    key,
                    sizeof(key),
                    "%s%zu",
                    g_details_group_order_prefix,
                    index);
                if (!henka_settings_has_key(settings, key))
                {
                    has_complete_order = false;
                    break;
                }
                loaded_order[index] = (unsigned char)henka_settings_get_int(
                    settings,
                    key,
                    (int)index);
            }
            if (has_complete_order &&
                sandbox3d_editor_ui_details_group_order_is_valid(loaded_order))
            {
                memcpy(
                    state->details_group_order,
                    loaded_order,
                    sizeof(state->details_group_order));
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
    if (!sandbox3d_editor_ui_details_group_order_is_valid(
            state->details_group_order))
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
            {g_details_actions_key, &state->details_actions_expanded},
            {g_details_audio_key, &state->details_audio_expanded}
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

        {
            const henka_result controls_result = henka_settings_set_float(
                settings,
                g_controls_scroll_offset_key,
                sandbox3d_editor_ui_sanitize_scroll_offset(
                    state->controls_scroll_offset));
            if (controls_result != HENKA_SUCCESS)
            {
                return controls_result;
            }
        }
        {
            const henka_result details_result = henka_settings_set_float(
                settings,
                g_details_scroll_offset_key,
                sandbox3d_editor_ui_sanitize_scroll_offset(
                    state->details_scroll_offset));
            if (details_result != HENKA_SUCCESS)
            {
                return details_result;
            }
        }

        for (index = 0U;
             index < SANDBOX3D_EDITOR_DETAILS_GROUP_COUNT;
             ++index)
        {
            char key[96];
            (void)snprintf(
                key,
                sizeof(key),
                "%s%zu",
                g_details_group_order_prefix,
                index);
            {
                const henka_result result = henka_settings_set_int(
                    settings,
                    key,
                    (int)state->details_group_order[index]);
                if (result != HENKA_SUCCESS)
                {
                    return result;
                }
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
    sandbox3d_editor_scroll_state state;

    state.offset = requested_offset;
    state.content_height = content_height;
    state.viewport_height = viewport_height;
    if (!isfinite(content_height) || content_height < 0.0f ||
        !isfinite(viewport_height) || viewport_height <= 0.0f)
    {
        return 0.0f;
    }
    if (henka_ui_scroll_state_set_content(
            &state,
            content_height,
            viewport_height) != HENKA_SUCCESS)
    {
        return 0.0f;
    }
    return state.offset;
}

float sandbox3d_editor_ui_details_footer_reserve(
    bool sticky_footer_visible)
{
    /* Keep the final scrollable row clear of the 24-pixel sticky footer and
     * leave a bounded 10-pixel separation so the two surfaces do not merge
     * visually or in hit testing. */
    return sticky_footer_visible ? 34.0f : 0.0f;
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

    state->content_height = isfinite(content_height) && content_height >= 0.0f ? content_height : 0.0f;
    state->viewport_height = isfinite(viewport_height) && viewport_height > 0.0f ? viewport_height : 1.0f;
    (void)henka_ui_scroll_state_set_content(
        state,
        state->content_height,
        state->viewport_height);
}

bool sandbox3d_editor_ui_scroll_state_apply_delta(
    sandbox3d_editor_scroll_state* state,
    float delta_pixels,
    float viewport_height)
{
    if (state == NULL || !isfinite(delta_pixels))
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

    sandbox3d_editor_ui_scroll_state_set_content(state, state->content_height, viewport_height);
    if (henka_ui_scroll_state_apply_delta(state, delta_pixels) != HENKA_SUCCESS)
    {
        return false;
    }
    return state->content_height > state->viewport_height && delta_pixels != 0.0f;
}

float sandbox3d_editor_ui_scrollbar_thumb_height(
    float content_height,
    float viewport_height,
    float track_height)
{
    float thumb_height = 0.0f;
    (void)henka_ui_scrollbar_thumb_height(
        content_height,
        viewport_height,
        track_height,
        &thumb_height);
    return thumb_height;
}

float sandbox3d_editor_ui_scrollbar_thumb_offset(
    float scroll_offset,
    float content_height,
    float viewport_height,
    float track_height,
    float thumb_height)
{
    float thumb_offset = 0.0f;
    (void)henka_ui_scrollbar_thumb_offset(
        scroll_offset,
        content_height,
        viewport_height,
        track_height,
        thumb_height,
        &thumb_offset);
    return thumb_offset;
}

bool sandbox3d_editor_ui_scroll_state_set_from_scrollbar(
    sandbox3d_editor_scroll_state* state,
    float pointer_y,
    float track_y,
    float track_height,
    float thumb_height,
    float grab_offset)
{
    return henka_ui_scroll_state_set_from_scrollbar(
        state,
        pointer_y,
        track_y,
        track_height,
        thumb_height,
        grab_offset) == HENKA_SUCCESS;
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

bool sandbox3d_editor_ui_details_group_order_is_valid(
    const unsigned char* order)
{
    size_t index;

    if (order == NULL)
    {
        return false;
    }
    for (index = 0U; index < SANDBOX3D_EDITOR_DETAILS_GROUP_COUNT; ++index)
    {
        size_t other;
        if (order[index] >= SANDBOX3D_EDITOR_DETAILS_GROUP_COUNT)
        {
            return false;
        }
        for (other = 0U; other < index; ++other)
        {
            if (order[other] == order[index])
            {
                return false;
            }
        }
    }
    return true;
}

bool sandbox3d_editor_ui_reorder_details_group(
    sandbox3d_editor_ui_state* state,
    size_t from_position,
    size_t to_position)
{
    unsigned char moving;
    size_t index;

    if (state == NULL ||
        !sandbox3d_editor_ui_details_group_order_is_valid(
            state->details_group_order) ||
        from_position >= SANDBOX3D_EDITOR_DETAILS_GROUP_COUNT ||
        to_position >= SANDBOX3D_EDITOR_DETAILS_GROUP_COUNT)
    {
        return false;
    }
    if (from_position == to_position)
    {
        return true;
    }
    moving = state->details_group_order[from_position];
    if (from_position < to_position)
    {
        for (index = from_position; index < to_position; ++index)
        {
            state->details_group_order[index] =
                state->details_group_order[index + 1U];
        }
    }
    else
    {
        for (index = from_position; index > to_position; --index)
        {
            state->details_group_order[index] =
                state->details_group_order[index - 1U];
        }
    }
    state->details_group_order[to_position] = moving;
    return sandbox3d_editor_ui_details_group_order_is_valid(
        state->details_group_order);
}
