#ifndef SANDBOX3D_EDITOR_CONTROLS_H
#define SANDBOX3D_EDITOR_CONTROLS_H

#include <stdbool.h>
#include <stddef.h>

#include <henka/input.h>
#include <henka/math.h>
#include <henka/persistence.h>
#include <henka/scene.h>

#define SANDBOX3D_EDITOR_PRESET_COUNT 7
#define SANDBOX3D_EDITOR_MAX_CUSTOM_PROFILES 8
#define SANDBOX3D_EDITOR_PROFILE_ID_CAPACITY 48
#define SANDBOX3D_EDITOR_PROFILE_NAME_CAPACITY 64
#define SANDBOX3D_EDITOR_BINDING_TEXT_CAPACITY 64
#define SANDBOX3D_EDITOR_WARNING_CAPACITY 128

typedef enum sandbox3d_editor_preset
{
    SANDBOX3D_EDITOR_PRESET_HENKA_DEFAULT = 0,
    SANDBOX3D_EDITOR_PRESET_ALTERNATE_MOVE,
    SANDBOX3D_EDITOR_PRESET_DIRECT_TRANSFORM,
    SANDBOX3D_EDITOR_PRESET_COMPACT_TOOLS,
    SANDBOX3D_EDITOR_PRESET_AXIS_FOCUSED,
    SANDBOX3D_EDITOR_PRESET_PRECISION_LAYOUT,
    SANDBOX3D_EDITOR_PRESET_FAMILIAR_MODELING
} sandbox3d_editor_preset;

typedef enum sandbox3d_transform_tool
{
    SANDBOX3D_TRANSFORM_TOOL_NONE = 0,
    SANDBOX3D_TRANSFORM_TOOL_MOVE,
    SANDBOX3D_TRANSFORM_TOOL_ROTATE,
    SANDBOX3D_TRANSFORM_TOOL_SCALE
} sandbox3d_transform_tool;

typedef enum sandbox3d_transform_axis
{
    SANDBOX3D_TRANSFORM_AXIS_NONE = 0,
    SANDBOX3D_TRANSFORM_AXIS_X,
    SANDBOX3D_TRANSFORM_AXIS_Y,
    SANDBOX3D_TRANSFORM_AXIS_Z
} sandbox3d_transform_axis;

typedef struct sandbox3d_editor_binding
{
    henka_key keys[HENKA_MAX_ACTION_KEY_BINDINGS];
    size_t key_count;
    henka_mouse_button mouse_buttons[HENKA_MAX_ACTION_MOUSE_BINDINGS];
    size_t mouse_button_count;
} sandbox3d_editor_binding;

typedef struct sandbox3d_editor_profile
{
    char id[SANDBOX3D_EDITOR_PROFILE_ID_CAPACITY];
    char name[SANDBOX3D_EDITOR_PROFILE_NAME_CAPACITY];
    sandbox3d_editor_preset base_preset;
    sandbox3d_editor_binding bindings[HENKA_INPUT_ACTION_COUNT];
} sandbox3d_editor_profile;

typedef struct sandbox3d_editor_controls
{
    sandbox3d_editor_preset active_preset;
    int active_custom_index;
    sandbox3d_editor_profile custom_profiles[SANDBOX3D_EDITOR_MAX_CUSTOM_PROFILES];
    size_t custom_profile_count;
    char warning[SANDBOX3D_EDITOR_WARNING_CAPACITY];
} sandbox3d_editor_controls;

typedef struct sandbox3d_transform_session
{
    bool active;
    sandbox3d_transform_tool tool;
    sandbox3d_transform_axis axis;
    henka_entity entity;
    henka_transform original;
    henka_transform preview;
    float accumulated_amount;
} sandbox3d_transform_session;

void sandbox3d_editor_controls_initialize(sandbox3d_editor_controls* controls);
const char* sandbox3d_editor_preset_name(sandbox3d_editor_preset preset);
const sandbox3d_editor_profile* sandbox3d_editor_controls_get_active_profile(const sandbox3d_editor_controls* controls);
const char* sandbox3d_editor_controls_get_active_profile_name(const sandbox3d_editor_controls* controls);
henka_result sandbox3d_editor_controls_apply(const sandbox3d_editor_controls* controls, struct henka_engine* engine);
henka_result sandbox3d_editor_controls_load(sandbox3d_editor_controls* controls, const henka_settings* settings);
henka_result sandbox3d_editor_controls_save(const sandbox3d_editor_controls* controls, henka_settings* settings);
henka_result sandbox3d_editor_controls_create_custom(
    sandbox3d_editor_controls* controls,
    sandbox3d_editor_preset base_preset,
    const char* name,
    const char* id);
henka_result sandbox3d_editor_controls_rename_custom(
    sandbox3d_editor_controls* controls,
    const char* id,
    const char* name);
henka_result sandbox3d_editor_controls_delete_custom(sandbox3d_editor_controls* controls, const char* id);
henka_result sandbox3d_editor_controls_set_active(sandbox3d_editor_controls* controls, const char* id);
henka_result sandbox3d_editor_controls_set_binding_text(
    sandbox3d_editor_controls* controls,
    henka_input_action action,
    const char* binding_text);
henka_result sandbox3d_editor_controls_reset_current(sandbox3d_editor_controls* controls);
void sandbox3d_editor_controls_reset_all(sandbox3d_editor_controls* controls);
void sandbox3d_editor_controls_format_binding(
    const sandbox3d_editor_controls* controls,
    henka_input_action action,
    char* buffer,
    size_t buffer_size);

bool sandbox3d_transform_session_begin(
    sandbox3d_transform_session* session,
    sandbox3d_transform_tool tool,
    henka_entity entity,
    henka_transform original);
bool sandbox3d_transform_session_set_axis(sandbox3d_transform_session* session, sandbox3d_transform_axis axis);
bool sandbox3d_transform_session_preview(
    sandbox3d_transform_session* session,
    float delta,
    bool snap_active,
    bool fine_active);
bool sandbox3d_transform_session_confirm(sandbox3d_transform_session* session, henka_transform* out_transform);
bool sandbox3d_transform_session_cancel(sandbox3d_transform_session* session, henka_transform* out_transform);
const char* sandbox3d_transform_tool_name(sandbox3d_transform_tool tool);
const char* sandbox3d_transform_axis_name(sandbox3d_transform_axis axis);

#endif
