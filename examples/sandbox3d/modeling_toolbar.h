#ifndef SANDBOX3D_MODELING_TOOLBAR_H
#define SANDBOX3D_MODELING_TOOLBAR_H

#include <stdbool.h>
#include <stddef.h>

#include <henka/result.h>

#include "editor_controls.h"
#include "object_authoring_tools.h"

typedef enum sandbox3d_modeling_toolbar_action
{
    SANDBOX3D_MODELING_TOOLBAR_ACTION_SELECT = 0,
    SANDBOX3D_MODELING_TOOLBAR_ACTION_MOVE,
    SANDBOX3D_MODELING_TOOLBAR_ACTION_ROTATE,
    SANDBOX3D_MODELING_TOOLBAR_ACTION_SCALE,
    SANDBOX3D_MODELING_TOOLBAR_ACTION_SNAP,
    SANDBOX3D_MODELING_TOOLBAR_ACTION_XRAY,
    SANDBOX3D_MODELING_TOOLBAR_ACTION_COUNT
} sandbox3d_modeling_toolbar_action;

typedef struct sandbox3d_modeling_toolbar_state
{
    sandbox3d_authoring_selection_mode selection_mode;
    sandbox3d_transform_tool transform_tool;
    sandbox3d_authoring_orientation_mode orientation_mode;
    sandbox3d_authoring_pivot_mode pivot_mode;
    bool snap_enabled;
    bool xray_enabled;
    bool authoring_available;
    size_t selected_component_count;
} sandbox3d_modeling_toolbar_state;

void sandbox3d_modeling_toolbar_state_reset(
    sandbox3d_modeling_toolbar_state* state);
bool sandbox3d_modeling_toolbar_set_selection_mode(
    sandbox3d_modeling_toolbar_state* state,
    sandbox3d_authoring_selection_mode mode);
bool sandbox3d_modeling_toolbar_set_transform_tool(
    sandbox3d_modeling_toolbar_state* state,
    sandbox3d_transform_tool tool);
bool sandbox3d_modeling_toolbar_set_orientation_mode(
    sandbox3d_modeling_toolbar_state* state,
    sandbox3d_authoring_orientation_mode mode);
bool sandbox3d_modeling_toolbar_set_pivot_mode(
    sandbox3d_modeling_toolbar_state* state,
    sandbox3d_authoring_pivot_mode mode);
const char* sandbox3d_modeling_toolbar_selection_label(
    sandbox3d_authoring_selection_mode mode);
const char* sandbox3d_modeling_toolbar_transform_label(
    sandbox3d_transform_tool tool);
const char* sandbox3d_modeling_toolbar_orientation_label(
    sandbox3d_authoring_orientation_mode mode);
const char* sandbox3d_modeling_toolbar_pivot_label(
    sandbox3d_authoring_pivot_mode mode);
const char* sandbox3d_modeling_toolbar_action_tooltip(
    sandbox3d_modeling_toolbar_action action);
const char* sandbox3d_modeling_toolbar_disabled_reason(
    sandbox3d_modeling_toolbar_action action,
    const sandbox3d_modeling_toolbar_state* state);
henka_result sandbox3d_modeling_toolbar_format_summary(
    const sandbox3d_modeling_toolbar_state* state,
    char* buffer,
    size_t buffer_size);

#endif
