#ifndef SANDBOX3D_EDITOR_UI_STATE_H
#define SANDBOX3D_EDITOR_UI_STATE_H

#include <stdbool.h>

#include <henka/persistence.h>
#include <henka/result.h>

typedef struct sandbox3d_editor_ui_state
{
    bool controls_workspace_expanded;
    bool controls_viewer_expanded;
    bool controls_viewport_expanded;
    bool controls_viewport_tool_expanded;

    bool details_overview_expanded;
    bool details_transform_expanded;
    bool details_materials_expanded;
    bool details_physics_expanded;
    bool details_interaction_expanded;
    bool details_actions_expanded;

    float controls_scroll_offset;
    float controls_content_height;
    float details_scroll_offset;
    float details_content_height;
} sandbox3d_editor_ui_state;

void sandbox3d_editor_ui_state_reset(
    sandbox3d_editor_ui_state* state);

void sandbox3d_editor_ui_state_load(
    const henka_settings* settings,
    sandbox3d_editor_ui_state* state);

henka_result sandbox3d_editor_ui_state_store(
    henka_settings* settings,
    const sandbox3d_editor_ui_state* state);

float sandbox3d_editor_ui_clamp_scroll(
    float requested_offset,
    float content_height,
    float viewport_height);

bool sandbox3d_editor_ui_scroll_controls(
    sandbox3d_editor_ui_state* state,
    float viewport_height,
    int direction);


bool sandbox3d_editor_ui_scroll_details(
    sandbox3d_editor_ui_state* state,
    float viewport_height,
    int direction);
#endif
