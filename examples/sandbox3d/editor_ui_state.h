#ifndef SANDBOX3D_EDITOR_UI_STATE_H
#define SANDBOX3D_EDITOR_UI_STATE_H

#include <stdbool.h>
#include <stddef.h>

#include <henka/persistence.h>
#include <henka/result.h>
#include <henka/ui.h>

/* Presentation-only scroll state. It deliberately does not carry any engine
 * or scene data, so panel resizing and input ownership can be handled without
 * mutating the scene. */
typedef henka_ui_scroll_state sandbox3d_editor_scroll_state;

typedef enum sandbox3d_editor_details_group_id
{
    SANDBOX3D_EDITOR_DETAILS_GROUP_OVERVIEW = 0,
    SANDBOX3D_EDITOR_DETAILS_GROUP_TRANSFORM,
    SANDBOX3D_EDITOR_DETAILS_GROUP_MATERIALS,
    SANDBOX3D_EDITOR_DETAILS_GROUP_AUTHORING,
    SANDBOX3D_EDITOR_DETAILS_GROUP_PHYSICS,
    SANDBOX3D_EDITOR_DETAILS_GROUP_INTERACTION,
    SANDBOX3D_EDITOR_DETAILS_GROUP_ACTIONS,
    SANDBOX3D_EDITOR_DETAILS_GROUP_COUNT
} sandbox3d_editor_details_group_id;

typedef struct sandbox3d_editor_ui_state
{
    bool controls_workspace_expanded;
    bool controls_viewer_expanded;
    bool controls_viewport_expanded;
    bool controls_viewport_tool_expanded;

    bool details_overview_expanded;
    bool details_transform_expanded;
    bool details_materials_expanded;
    bool details_authoring_expanded;
    bool details_physics_expanded;
    bool details_interaction_expanded;
    bool details_actions_expanded;
    unsigned char details_group_order[SANDBOX3D_EDITOR_DETAILS_GROUP_COUNT];

    float controls_scroll_offset;
    float controls_content_height;
    float details_scroll_offset;
    float details_content_height;
    bool controls_scroll_dragging;
    bool details_scroll_dragging;
    float controls_scroll_grab_offset;
    float details_scroll_grab_offset;
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

void sandbox3d_editor_ui_scroll_state_set_content(
    sandbox3d_editor_scroll_state* state,
    float content_height,
    float viewport_height);

bool sandbox3d_editor_ui_scroll_state_apply_delta(
    sandbox3d_editor_scroll_state* state,
    float delta_pixels,
    float viewport_height);

float sandbox3d_editor_ui_scrollbar_thumb_height(
    float content_height,
    float viewport_height,
    float track_height);

float sandbox3d_editor_ui_scrollbar_thumb_offset(
    float scroll_offset,
    float content_height,
    float viewport_height,
    float track_height,
    float thumb_height);

bool sandbox3d_editor_ui_scroll_state_set_from_scrollbar(
    sandbox3d_editor_scroll_state* state,
    float pointer_y,
    float track_y,
    float track_height,
    float thumb_height,
    float grab_offset);

bool sandbox3d_editor_ui_scroll_controls_by(
    sandbox3d_editor_ui_state* state,
    float viewport_height,
    float delta_pixels);

bool sandbox3d_editor_ui_scroll_details_by(
    sandbox3d_editor_ui_state* state,
    float viewport_height,
    float delta_pixels);

bool sandbox3d_editor_ui_scroll_controls(
    sandbox3d_editor_ui_state* state,
    float viewport_height,
    int direction);


bool sandbox3d_editor_ui_scroll_details(
    sandbox3d_editor_ui_state* state,
    float viewport_height,
    int direction);

bool sandbox3d_editor_ui_details_group_order_is_valid(
    const unsigned char* order);

bool sandbox3d_editor_ui_reorder_details_group(
    sandbox3d_editor_ui_state* state,
    size_t from_position,
    size_t to_position);
#endif
