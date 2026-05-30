#include "editor_controls.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <henka/engine.h>

static const henka_input_action g_profile_actions[] = {
    HENKA_INPUT_ACTION_SELECT_TOOL,
    HENKA_INPUT_ACTION_MOVE_TOOL,
    HENKA_INPUT_ACTION_ROTATE_TOOL,
    HENKA_INPUT_ACTION_SCALE_TOOL,
    HENKA_INPUT_ACTION_CONSTRAIN_X,
    HENKA_INPUT_ACTION_CONSTRAIN_Y,
    HENKA_INPUT_ACTION_CONSTRAIN_Z,
    HENKA_INPUT_ACTION_CONFIRM_TRANSFORM,
    HENKA_INPUT_ACTION_CANCEL_TRANSFORM,
    HENKA_INPUT_ACTION_SNAP_MODIFIER,
    HENKA_INPUT_ACTION_FINE_ADJUSTMENT_MODIFIER};

static const char* g_profile_action_keys[] = {
    "select_tool", "move_tool", "rotate_tool", "scale_tool", "constrain_x", "constrain_y", "constrain_z",
    "confirm_transform", "cancel_transform", "snap_modifier", "fine_adjustment_modifier"};

static bool sandbox3d_editor_valid_preset(sandbox3d_editor_preset preset)
{
    return preset >= SANDBOX3D_EDITOR_PRESET_HENKA_DEFAULT && preset < SANDBOX3D_EDITOR_PRESET_COUNT;
}

static bool sandbox3d_editor_key_is_supported(henka_key key)
{
    return key == HENKA_KEY_ESCAPE ||
        key == HENKA_KEY_ENTER ||
        key == HENKA_KEY_LEFT_CTRL ||
        key == HENKA_KEY_LEFT_SHIFT ||
        key == HENKA_KEY_M ||
        key == HENKA_KEY_G ||
        key == HENKA_KEY_R ||
        key == HENKA_KEY_S ||
        key == HENKA_KEY_X ||
        key == HENKA_KEY_Y ||
        key == HENKA_KEY_Z;
}

static bool sandbox3d_editor_custom_id_is_valid(const char* id)
{
    const unsigned char* cursor;
    if (id == NULL || id[0] == '\0' || strncmp(id, "preset-", 7U) == 0)
    {
        return false;
    }
    for (cursor = (const unsigned char*)id; *cursor != '\0'; ++cursor)
    {
        if (!isalnum(*cursor) && *cursor != '-' && *cursor != '_')
        {
            return false;
        }
    }
    return true;
}

static void sandbox3d_editor_copy_trimmed(char* destination, size_t capacity, const char* source)
{
    const char* end;
    const char* start;
    size_t length;
    if (destination == NULL || capacity == 0U)
    {
        return;
    }
    destination[0] = '\0';
    if (source == NULL)
    {
        return;
    }
    start = source;
    while (*start != '\0' && isspace((unsigned char)*start)) ++start;
    end = start + strlen(start);
    while (end > start && isspace((unsigned char)*(end - 1))) --end;
    length = (size_t)(end - start);
    if (length >= capacity) length = capacity - 1U;
    memcpy(destination, start, length);
    destination[length] = '\0';
}

static void sandbox3d_editor_add_key(sandbox3d_editor_binding* binding, henka_key key)
{
    if (binding != NULL && key > HENKA_KEY_UNKNOWN && binding->key_count < HENKA_MAX_ACTION_KEY_BINDINGS)
    {
        binding->keys[binding->key_count++] = key;
    }
}

static void sandbox3d_editor_add_mouse(sandbox3d_editor_binding* binding, henka_mouse_button button)
{
    if (binding != NULL && button > HENKA_MOUSE_BUTTON_UNKNOWN && binding->mouse_button_count < HENKA_MAX_ACTION_MOUSE_BINDINGS)
    {
        binding->mouse_buttons[binding->mouse_button_count++] = button;
    }
}

static void sandbox3d_editor_build_preset(sandbox3d_editor_preset preset, sandbox3d_editor_profile* profile)
{
    if (profile == NULL) return;
    memset(profile, 0, sizeof(*profile));
    snprintf(profile->id, sizeof(profile->id), "preset-%d", (int)preset);
    snprintf(profile->name, sizeof(profile->name), "%s", sandbox3d_editor_preset_name(preset));
    profile->base_preset = preset;
    sandbox3d_editor_add_key(&profile->bindings[HENKA_INPUT_ACTION_MOVE_TOOL], preset == SANDBOX3D_EDITOR_PRESET_ALTERNATE_MOVE ? HENKA_KEY_G : HENKA_KEY_M);
    sandbox3d_editor_add_key(&profile->bindings[HENKA_INPUT_ACTION_MOVE_TOOL], preset == SANDBOX3D_EDITOR_PRESET_ALTERNATE_MOVE ? HENKA_KEY_M : HENKA_KEY_G);
    sandbox3d_editor_add_key(&profile->bindings[HENKA_INPUT_ACTION_ROTATE_TOOL], HENKA_KEY_R);
    sandbox3d_editor_add_key(&profile->bindings[HENKA_INPUT_ACTION_SCALE_TOOL], HENKA_KEY_S);
    sandbox3d_editor_add_key(&profile->bindings[HENKA_INPUT_ACTION_CONSTRAIN_X], HENKA_KEY_X);
    sandbox3d_editor_add_key(&profile->bindings[HENKA_INPUT_ACTION_CONSTRAIN_Y], HENKA_KEY_Y);
    sandbox3d_editor_add_key(&profile->bindings[HENKA_INPUT_ACTION_CONSTRAIN_Z], HENKA_KEY_Z);
    sandbox3d_editor_add_key(&profile->bindings[HENKA_INPUT_ACTION_CONFIRM_TRANSFORM], HENKA_KEY_ENTER);
    sandbox3d_editor_add_mouse(&profile->bindings[HENKA_INPUT_ACTION_CONFIRM_TRANSFORM], HENKA_MOUSE_BUTTON_LEFT);
    sandbox3d_editor_add_key(&profile->bindings[HENKA_INPUT_ACTION_CANCEL_TRANSFORM], HENKA_KEY_ESCAPE);
    sandbox3d_editor_add_mouse(&profile->bindings[HENKA_INPUT_ACTION_CANCEL_TRANSFORM], HENKA_MOUSE_BUTTON_RIGHT);
    sandbox3d_editor_add_key(&profile->bindings[HENKA_INPUT_ACTION_SNAP_MODIFIER], HENKA_KEY_LEFT_CTRL);
    sandbox3d_editor_add_key(&profile->bindings[HENKA_INPUT_ACTION_FINE_ADJUSTMENT_MODIFIER], HENKA_KEY_LEFT_SHIFT);
}

const char* sandbox3d_editor_preset_name(sandbox3d_editor_preset preset)
{
    static const char* names[SANDBOX3D_EDITOR_PRESET_COUNT] = {
        "Henka Default", "Alternate Move", "Direct Transform", "Compact Tools", "Axis Focused", "Precision Layout", "Familiar Modeling"};
    return sandbox3d_editor_valid_preset(preset) ? names[preset] : "Henka Default";
}

void sandbox3d_editor_controls_initialize(sandbox3d_editor_controls* controls)
{
    if (controls != NULL)
    {
        memset(controls, 0, sizeof(*controls));
        controls->active_preset = SANDBOX3D_EDITOR_PRESET_HENKA_DEFAULT;
        controls->active_custom_index = -1;
    }
}

const sandbox3d_editor_profile* sandbox3d_editor_controls_get_active_profile(const sandbox3d_editor_controls* controls)
{
    static sandbox3d_editor_profile preset;
    if (controls != NULL && controls->active_custom_index >= 0 && (size_t)controls->active_custom_index < controls->custom_profile_count)
    {
        return &controls->custom_profiles[controls->active_custom_index];
    }
    sandbox3d_editor_build_preset(controls != NULL ? controls->active_preset : SANDBOX3D_EDITOR_PRESET_HENKA_DEFAULT, &preset);
    return &preset;
}

const char* sandbox3d_editor_controls_get_active_profile_name(const sandbox3d_editor_controls* controls)
{
    return sandbox3d_editor_controls_get_active_profile(controls)->name;
}

henka_result sandbox3d_editor_controls_apply(const sandbox3d_editor_controls* controls, struct henka_engine* engine)
{
    const sandbox3d_editor_profile* profile = sandbox3d_editor_controls_get_active_profile(controls);
    size_t action_index;
    if (engine == NULL) return HENKA_ERROR_INVALID_ARGUMENT;
    for (action_index = 0U; action_index < sizeof(g_profile_actions) / sizeof(g_profile_actions[0]); ++action_index)
    {
        const henka_input_action action = g_profile_actions[action_index];
        const sandbox3d_editor_binding* binding = &profile->bindings[action];
        size_t index;
        henka_input_clear_action_bindings(engine, action);
        for (index = 0U; index < binding->key_count; ++index) henka_input_add_action_key_binding(engine, action, binding->keys[index]);
        for (index = 0U; index < binding->mouse_button_count; ++index) henka_input_add_action_mouse_button_binding(engine, action, binding->mouse_buttons[index]);
    }
    return HENKA_SUCCESS;
}

static int sandbox3d_editor_find_custom(const sandbox3d_editor_controls* controls, const char* id)
{
    size_t index;
    if (controls == NULL || id == NULL) return -1;
    for (index = 0U; index < controls->custom_profile_count; ++index)
    {
        if (strcmp(controls->custom_profiles[index].id, id) == 0) return (int)index;
    }
    return -1;
}

henka_result sandbox3d_editor_controls_create_custom(sandbox3d_editor_controls* controls, sandbox3d_editor_preset base_preset, const char* name, const char* id)
{
    char trimmed_name[SANDBOX3D_EDITOR_PROFILE_NAME_CAPACITY];
    char trimmed_id[SANDBOX3D_EDITOR_PROFILE_ID_CAPACITY];
    size_t index;
    sandbox3d_editor_profile* profile;
    if (controls == NULL || !sandbox3d_editor_valid_preset(base_preset) || controls->custom_profile_count >= SANDBOX3D_EDITOR_MAX_CUSTOM_PROFILES) return HENKA_ERROR_INVALID_ARGUMENT;
    sandbox3d_editor_copy_trimmed(trimmed_name, sizeof(trimmed_name), name);
    sandbox3d_editor_copy_trimmed(trimmed_id, sizeof(trimmed_id), id);
    if (trimmed_name[0] == '\0' || !sandbox3d_editor_custom_id_is_valid(trimmed_id) || sandbox3d_editor_find_custom(controls, trimmed_id) >= 0) return HENKA_ERROR_INVALID_ARGUMENT;
    for (index = 0U; index < controls->custom_profile_count; ++index) if (strcmp(controls->custom_profiles[index].name, trimmed_name) == 0) return HENKA_ERROR_INVALID_ARGUMENT;
    profile = &controls->custom_profiles[controls->custom_profile_count++];
    sandbox3d_editor_build_preset(base_preset, profile);
    snprintf(profile->id, sizeof(profile->id), "%s", trimmed_id);
    snprintf(profile->name, sizeof(profile->name), "%s", trimmed_name);
    controls->active_custom_index = (int)(controls->custom_profile_count - 1U);
    return HENKA_SUCCESS;
}

henka_result sandbox3d_editor_controls_rename_custom(sandbox3d_editor_controls* controls, const char* id, const char* name)
{
    char trimmed[SANDBOX3D_EDITOR_PROFILE_NAME_CAPACITY];
    int index = sandbox3d_editor_find_custom(controls, id);
    size_t other;
    if (index < 0) return HENKA_ERROR_INVALID_ARGUMENT;
    sandbox3d_editor_copy_trimmed(trimmed, sizeof(trimmed), name);
    if (trimmed[0] == '\0') return HENKA_ERROR_INVALID_ARGUMENT;
    for (other = 0U; other < controls->custom_profile_count; ++other) if ((int)other != index && strcmp(controls->custom_profiles[other].name, trimmed) == 0) return HENKA_ERROR_INVALID_ARGUMENT;
    snprintf(controls->custom_profiles[index].name, sizeof(controls->custom_profiles[index].name), "%s", trimmed);
    return HENKA_SUCCESS;
}

henka_result sandbox3d_editor_controls_delete_custom(sandbox3d_editor_controls* controls, const char* id)
{
    int index = sandbox3d_editor_find_custom(controls, id);
    if (index < 0) return HENKA_ERROR_INVALID_ARGUMENT;
    if ((size_t)index + 1U < controls->custom_profile_count) memmove(&controls->custom_profiles[index], &controls->custom_profiles[index + 1], (controls->custom_profile_count - (size_t)index - 1U) * sizeof(controls->custom_profiles[0]));
    controls->custom_profile_count -= 1U;
    if (controls->active_custom_index == index)
    {
        controls->active_custom_index = -1;
        controls->active_preset = SANDBOX3D_EDITOR_PRESET_HENKA_DEFAULT;
    }
    else if (controls->active_custom_index > index)
    {
        controls->active_custom_index -= 1;
    }
    return HENKA_SUCCESS;
}

henka_result sandbox3d_editor_controls_set_active(sandbox3d_editor_controls* controls, const char* id)
{
    int index;
    int preset;
    if (controls == NULL || id == NULL) return HENKA_ERROR_INVALID_ARGUMENT;
    index = sandbox3d_editor_find_custom(controls, id);
    if (index >= 0) { controls->active_custom_index = index; return HENKA_SUCCESS; }
    for (preset = 0; preset < SANDBOX3D_EDITOR_PRESET_COUNT; ++preset)
    {
        char expected[24];
        snprintf(expected, sizeof(expected), "preset-%d", preset);
        if (strcmp(id, expected) == 0)
        {
            controls->active_custom_index = -1;
            controls->active_preset = (sandbox3d_editor_preset)preset;
            return HENKA_SUCCESS;
        }
    }
    return HENKA_ERROR_INVALID_ARGUMENT;
}

static int sandbox3d_editor_action_setting_index(henka_input_action action)
{
    size_t index;
    for (index = 0U; index < sizeof(g_profile_actions) / sizeof(g_profile_actions[0]); ++index) if (g_profile_actions[index] == action) return (int)index;
    return -1;
}

static henka_result sandbox3d_editor_parse_binding(const char* text, sandbox3d_editor_binding* binding)
{
    char copy[SANDBOX3D_EDITOR_BINDING_TEXT_CAPACITY];
    char* context = NULL;
    char* token;
    if (text == NULL || binding == NULL || strlen(text) >= sizeof(copy)) return HENKA_ERROR_INVALID_ARGUMENT;
    memset(binding, 0, sizeof(*binding));
    snprintf(copy, sizeof(copy), "%s", text);
    token = strtok_s(copy, ",", &context);
    while (token != NULL)
    {
        char trimmed[24];
        henka_key key;
        henka_mouse_button button;
        sandbox3d_editor_copy_trimmed(trimmed, sizeof(trimmed), token);
        key = henka_input_key_find_by_name(trimmed);
        button = henka_input_mouse_button_find_by_name(trimmed);
        if (key > HENKA_KEY_UNKNOWN && sandbox3d_editor_key_is_supported(key))
        {
            size_t index;
            for (index = 0U; index < binding->key_count; ++index) if (binding->keys[index] == key) return HENKA_ERROR_INVALID_ARGUMENT;
            if (binding->key_count >= HENKA_MAX_ACTION_KEY_BINDINGS) return HENKA_ERROR_INVALID_ARGUMENT;
            sandbox3d_editor_add_key(binding, key);
        }
        else if (button > HENKA_MOUSE_BUTTON_UNKNOWN)
        {
            size_t index;
            for (index = 0U; index < binding->mouse_button_count; ++index) if (binding->mouse_buttons[index] == button) return HENKA_ERROR_INVALID_ARGUMENT;
            if (binding->mouse_button_count >= HENKA_MAX_ACTION_MOUSE_BINDINGS) return HENKA_ERROR_INVALID_ARGUMENT;
            sandbox3d_editor_add_mouse(binding, button);
        }
        else return HENKA_ERROR_INVALID_ARGUMENT;
        token = strtok_s(NULL, ",", &context);
    }
    return binding->key_count + binding->mouse_button_count > 0U ? HENKA_SUCCESS : HENKA_ERROR_INVALID_ARGUMENT;
}

henka_result sandbox3d_editor_controls_set_binding_text(sandbox3d_editor_controls* controls, henka_input_action action, const char* binding_text)
{
    sandbox3d_editor_binding parsed;
    sandbox3d_editor_profile* profile;
    size_t action_index;
    size_t index;
    if (controls == NULL || controls->active_custom_index < 0 || (size_t)controls->active_custom_index >= controls->custom_profile_count ||
        sandbox3d_editor_action_setting_index(action) < 0 ||
        sandbox3d_editor_parse_binding(binding_text, &parsed) != HENKA_SUCCESS) return HENKA_ERROR_INVALID_ARGUMENT;
    profile = &controls->custom_profiles[controls->active_custom_index];
    for (action_index = 0U; action_index < sizeof(g_profile_actions) / sizeof(g_profile_actions[0]); ++action_index)
    {
        const henka_input_action other_action = g_profile_actions[action_index];
        const sandbox3d_editor_binding* other = &profile->bindings[other_action];
        size_t other_index;
        if (other_action == action) continue;
        for (index = 0U; index < parsed.key_count; ++index) for (other_index = 0U; other_index < other->key_count; ++other_index) if (parsed.keys[index] == other->keys[other_index]) return HENKA_ERROR_INVALID_ARGUMENT;
        for (index = 0U; index < parsed.mouse_button_count; ++index) for (other_index = 0U; other_index < other->mouse_button_count; ++other_index) if (parsed.mouse_buttons[index] == other->mouse_buttons[other_index]) return HENKA_ERROR_INVALID_ARGUMENT;
    }
    profile->bindings[action] = parsed;
    return HENKA_SUCCESS;
}

henka_result sandbox3d_editor_controls_reset_current(sandbox3d_editor_controls* controls)
{
    sandbox3d_editor_profile* profile;
    char id[SANDBOX3D_EDITOR_PROFILE_ID_CAPACITY];
    char name[SANDBOX3D_EDITOR_PROFILE_NAME_CAPACITY];
    sandbox3d_editor_preset preset;
    if (controls == NULL) return HENKA_ERROR_INVALID_ARGUMENT;
    if (controls->active_custom_index < 0) return HENKA_SUCCESS;
    profile = &controls->custom_profiles[controls->active_custom_index];
    snprintf(id, sizeof(id), "%s", profile->id);
    snprintf(name, sizeof(name), "%s", profile->name);
    preset = profile->base_preset;
    sandbox3d_editor_build_preset(preset, profile);
    snprintf(profile->id, sizeof(profile->id), "%s", id);
    snprintf(profile->name, sizeof(profile->name), "%s", name);
    return HENKA_SUCCESS;
}

void sandbox3d_editor_controls_reset_all(sandbox3d_editor_controls* controls)
{
    sandbox3d_editor_controls_initialize(controls);
}

void sandbox3d_editor_controls_format_binding(const sandbox3d_editor_controls* controls, henka_input_action action, char* buffer, size_t buffer_size)
{
    const sandbox3d_editor_binding* binding;
    size_t index;
    if (buffer == NULL || buffer_size == 0U) return;
    buffer[0] = '\0';
    if (action <= HENKA_INPUT_ACTION_UNKNOWN || action >= HENKA_INPUT_ACTION_COUNT)
    {
        snprintf(buffer, buffer_size, "(unbound)");
        return;
    }
    binding = &sandbox3d_editor_controls_get_active_profile(controls)->bindings[action];
    for (index = 0U; index < binding->key_count; ++index) snprintf(buffer + strlen(buffer), buffer_size - strlen(buffer), "%s%s", strlen(buffer) > 0U ? ", " : "", henka_input_key_get_name(binding->keys[index]));
    for (index = 0U; index < binding->mouse_button_count; ++index) snprintf(buffer + strlen(buffer), buffer_size - strlen(buffer), "%s%s", strlen(buffer) > 0U ? ", " : "", henka_input_mouse_button_get_name(binding->mouse_buttons[index]));
    if (buffer[0] == '\0') snprintf(buffer, buffer_size, "(unbound)");
}

henka_result sandbox3d_editor_controls_save(const sandbox3d_editor_controls* controls, henka_settings* settings)
{
    size_t profile_index;
    if (controls == NULL || settings == NULL) return HENKA_ERROR_INVALID_ARGUMENT;
    henka_settings_set_int(settings, "controls.version", 1);
    henka_settings_set_string(settings, "controls.active_profile", controls->active_custom_index >= 0 ? controls->custom_profiles[controls->active_custom_index].id : sandbox3d_editor_controls_get_active_profile(controls)->id);
    henka_settings_set_int(settings, "controls.custom_count", (int)controls->custom_profile_count);
    for (profile_index = 0U; profile_index < controls->custom_profile_count; ++profile_index)
    {
        char key[96];
        size_t action_index;
        const sandbox3d_editor_profile* profile = &controls->custom_profiles[profile_index];
        snprintf(key, sizeof(key), "controls.custom.%zu.id", profile_index); henka_settings_set_string(settings, key, profile->id);
        snprintf(key, sizeof(key), "controls.custom.%zu.name", profile_index); henka_settings_set_string(settings, key, profile->name);
        snprintf(key, sizeof(key), "controls.custom.%zu.base", profile_index); henka_settings_set_int(settings, key, (int)profile->base_preset);
        for (action_index = 0U; action_index < sizeof(g_profile_actions) / sizeof(g_profile_actions[0]); ++action_index)
        {
            char value[SANDBOX3D_EDITOR_BINDING_TEXT_CAPACITY];
            sandbox3d_editor_controls temporary;
            const sandbox3d_editor_binding* binding = &profile->bindings[g_profile_actions[action_index]];
            if (binding->key_count == 0U && binding->mouse_button_count == 0U) continue;
            memset(&temporary, 0, sizeof(temporary));
            temporary.custom_profiles[0] = *profile;
            temporary.custom_profile_count = 1U;
            temporary.active_custom_index = 0;
            sandbox3d_editor_controls_format_binding(&temporary, g_profile_actions[action_index], value, sizeof(value));
            snprintf(key, sizeof(key), "controls.custom.%zu.%s", profile_index, g_profile_action_keys[action_index]);
            henka_settings_set_string(settings, key, value);
        }
    }
    return HENKA_SUCCESS;
}

henka_result sandbox3d_editor_controls_load(sandbox3d_editor_controls* controls, const henka_settings* settings)
{
    int count;
    int index;
    const char* active;
    if (controls == NULL || settings == NULL) return HENKA_ERROR_INVALID_ARGUMENT;
    sandbox3d_editor_controls_initialize(controls);
    if (henka_settings_has_key(settings, "controls.version") && henka_settings_get_int(settings, "controls.version", 0) != 1) goto invalid;
    count = henka_settings_get_int(settings, "controls.custom_count", 0);
    if (count < 0 || count > SANDBOX3D_EDITOR_MAX_CUSTOM_PROFILES) { snprintf(controls->warning, sizeof(controls->warning), "Control profile settings were invalid. Henka Default loaded."); return HENKA_ERROR_INVALID_ARGUMENT; }
    for (index = 0; index < count; ++index)
    {
        char key[96];
        const char* id;
        const char* name;
        int base;
        size_t action_index;
        snprintf(key, sizeof(key), "controls.custom.%d.id", index); id = henka_settings_get_string(settings, key, "");
        snprintf(key, sizeof(key), "controls.custom.%d.name", index); name = henka_settings_get_string(settings, key, "");
        snprintf(key, sizeof(key), "controls.custom.%d.base", index); base = henka_settings_get_int(settings, key, -1);
        if (sandbox3d_editor_controls_create_custom(controls, (sandbox3d_editor_preset)base, name, id) != HENKA_SUCCESS) goto invalid;
        for (action_index = 0U; action_index < sizeof(g_profile_actions) / sizeof(g_profile_actions[0]); ++action_index)
        {
            snprintf(key, sizeof(key), "controls.custom.%d.%s", index, g_profile_action_keys[action_index]);
            if (henka_settings_has_key(settings, key) && sandbox3d_editor_controls_set_binding_text(controls, g_profile_actions[action_index], henka_settings_get_string(settings, key, "")) != HENKA_SUCCESS) goto invalid;
        }
    }
    active = henka_settings_get_string(settings, "controls.active_profile", "preset-0");
    if (sandbox3d_editor_controls_set_active(controls, active) != HENKA_SUCCESS) goto invalid;
    return HENKA_SUCCESS;
invalid:
    sandbox3d_editor_controls_initialize(controls);
    snprintf(controls->warning, sizeof(controls->warning), "Control profile settings were invalid. Henka Default loaded.");
    return HENKA_ERROR_INVALID_ARGUMENT;
}

bool sandbox3d_transform_session_begin(sandbox3d_transform_session* session, sandbox3d_transform_tool tool, henka_entity entity, henka_transform original)
{
    if (session == NULL || tool <= SANDBOX3D_TRANSFORM_TOOL_NONE || tool > SANDBOX3D_TRANSFORM_TOOL_SCALE || entity == HENKA_INVALID_ENTITY) return false;
    memset(session, 0, sizeof(*session));
    session->active = true; session->tool = tool; session->entity = entity; session->original = original; session->preview = original;
    return true;
}

bool sandbox3d_transform_session_set_axis(sandbox3d_transform_session* session, sandbox3d_transform_axis axis)
{
    if (session == NULL || !session->active || axis < SANDBOX3D_TRANSFORM_AXIS_NONE || axis > SANDBOX3D_TRANSFORM_AXIS_Z) return false;
    session->axis = axis; return true;
}

static float sandbox3d_editor_snap(float value, float increment) { return increment > 0.0f ? roundf(value / increment) * increment : value; }

bool sandbox3d_transform_session_preview(sandbox3d_transform_session* session, float delta, bool snap_active, bool fine_active)
{
    float amount;
    if (session == NULL || !session->active || !isfinite(delta)) return false;
    session->accumulated_amount += delta * (fine_active ? 0.2f : 1.0f);
    amount = session->accumulated_amount;
    if (snap_active) amount = sandbox3d_editor_snap(amount, session->tool == SANDBOX3D_TRANSFORM_TOOL_ROTATE ? 15.0f * HENKA_DEG_TO_RAD : (session->tool == SANDBOX3D_TRANSFORM_TOOL_SCALE ? 0.1f : 0.25f));
    session->preview = session->original;
    if (session->tool == SANDBOX3D_TRANSFORM_TOOL_MOVE)
    {
        if (session->axis == SANDBOX3D_TRANSFORM_AXIS_Y) session->preview.position.y += amount;
        else if (session->axis == SANDBOX3D_TRANSFORM_AXIS_Z) session->preview.position.z += amount;
        else session->preview.position.x += amount;
    }
    else if (session->tool == SANDBOX3D_TRANSFORM_TOOL_ROTATE)
    {
        henka_vec3 axis = session->axis == SANDBOX3D_TRANSFORM_AXIS_X ? (henka_vec3){1,0,0} : (session->axis == SANDBOX3D_TRANSFORM_AXIS_Z ? (henka_vec3){0,0,1} : (henka_vec3){0,1,0});
        session->preview.rotation = henka_quat_multiply(session->original.rotation, henka_quat_from_axis_angle(axis, amount));
    }
    else
    {
        float multiplier = fmaxf(0.01f, 1.0f + amount);
        if (session->axis == SANDBOX3D_TRANSFORM_AXIS_X) session->preview.scale.x *= multiplier;
        else if (session->axis == SANDBOX3D_TRANSFORM_AXIS_Y) session->preview.scale.y *= multiplier;
        else if (session->axis == SANDBOX3D_TRANSFORM_AXIS_Z) session->preview.scale.z *= multiplier;
        else session->preview.scale = henka_vec3_scale(session->preview.scale, multiplier);
    }
    return true;
}

bool sandbox3d_transform_session_confirm(sandbox3d_transform_session* session, henka_transform* out_transform)
{
    if (session == NULL || !session->active) return false;
    if (out_transform != NULL) *out_transform = session->preview;
    session->active = false; return true;
}

bool sandbox3d_transform_session_cancel(sandbox3d_transform_session* session, henka_transform* out_transform)
{
    if (session == NULL || !session->active) return false;
    if (out_transform != NULL) *out_transform = session->original;
    session->active = false; return true;
}

const char* sandbox3d_transform_tool_name(sandbox3d_transform_tool tool)
{
    return tool == SANDBOX3D_TRANSFORM_TOOL_MOVE ? "Move" : (tool == SANDBOX3D_TRANSFORM_TOOL_ROTATE ? "Rotate" : (tool == SANDBOX3D_TRANSFORM_TOOL_SCALE ? "Scale" : "None"));
}

const char* sandbox3d_transform_axis_name(sandbox3d_transform_axis axis)
{
    return axis == SANDBOX3D_TRANSFORM_AXIS_X ? "X" : (axis == SANDBOX3D_TRANSFORM_AXIS_Y ? "Y" : (axis == SANDBOX3D_TRANSFORM_AXIS_Z ? "Z" : "Free"));
}
