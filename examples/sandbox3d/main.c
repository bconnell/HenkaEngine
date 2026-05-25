#include <math.h>
#include <stdio.h>
#include <string.h>

#include <henka/henka.h>

typedef enum sandbox3d_object_kind
{
    SANDBOX3D_OBJECT_GROUND = 0,
    SANDBOX3D_OBJECT_TEXTURED_CUBE,
    SANDBOX3D_OBJECT_COLORED_CUBE,
    SANDBOX3D_OBJECT_OBJ_MARKER,
    SANDBOX3D_OBJECT_MISSING_TEXTURE,
    SANDBOX3D_OBJECT_MISSING_MODEL,
    SANDBOX3D_OBJECT_DEBUG_GRID,
    SANDBOX3D_OBJECT_COUNT
} sandbox3d_object_kind;

typedef struct sandbox3d_object_descriptor
{
    sandbox3d_object_kind kind;
    henka_entity entity;
    const char* display_name;
    const char* location_hint;
    const char* short_explanation;
    const char* developer_detail;
    const char* mesh_summary;
    const char* material_summary;
    const char* texture_summary;
    bool can_hide;
    bool can_reset;
    henka_transform default_transform;
} sandbox3d_object_descriptor;

typedef struct sandbox3d_workspace_state
{
    bool scene_objects_panel_visible;
    bool object_details_panel_visible;
} sandbox3d_workspace_state;

typedef struct sandbox3d_workspace_layout
{
    henka_ui_rect controls_panel;
    henka_ui_rect scene_objects_panel;
    henka_ui_rect object_details_panel;
} sandbox3d_workspace_layout;

typedef struct sandbox3d_state
{
    henka_scene* scene;
    henka_settings* settings;
    henka_ui_context* ui;
    henka_camera camera;
    henka_mesh* cube_mesh;
    henka_mesh* ground_mesh;
    henka_mesh* grid_mesh;
    henka_mesh* marker_mesh;
    henka_mesh* missing_model_mesh;
    henka_shader* basic_shader;
    henka_shader* grid_shader;
    henka_texture* cube_texture;
    henka_texture* ground_texture;
    henka_texture* missing_texture;
    henka_entity cube_entity;
    henka_entity ground_entity;
    henka_entity grid_entity;
    henka_entity colored_cube_entity;
    henka_entity fallback_cube_entity;
    henka_entity marker_entity;
    henka_entity fallback_model_entity;
    sandbox3d_object_descriptor descriptors[SANDBOX3D_OBJECT_COUNT];
    sandbox3d_workspace_state workspace;
    henka_entity selected_entity;
    bool settings_file_found;
    bool startup_panels_auto_opened;
    bool ui_visibility_report_pending;
    bool ui_visible_last_frame;
} sandbox3d_state;

static const float g_default_mouse_look_sensitivity = 0.0025f;
static const float g_default_camera_movement_speed = 4.0f;
static const henka_vec3 g_camera_start_position = {0.0f, 2.4f, 8.6f};
static const float g_camera_start_yaw = -HENKA_PI * 0.5f;
static const float g_camera_start_pitch = -0.22f;
static const henka_vec3 g_textured_cube_position = {0.0f, 0.5f, 0.0f};
static const henka_vec3 g_colored_cube_position = {-2.6f, 0.5f, 1.4f};
static const henka_vec3 g_missing_texture_position = {2.6f, 0.5f, 1.4f};
static const henka_vec3 g_marker_position = {-4.4f, 0.0f, -0.9f};
static const henka_vec3 g_missing_model_position = {4.4f, 0.5f, -0.9f};

static const float g_ui_panel_margin = 20.0f;
static const float g_ui_panel_gap = 12.0f;
static const float g_ui_controls_width = 360.0f;
static const float g_ui_scene_width = 292.0f;
static const float g_ui_details_width = 576.0f;
static const float g_ui_panel_height = 420.0f;

static const char* g_setting_key_grid_visible = "grid_visible";
static const char* g_setting_key_wireframe_enabled = "wireframe_enabled";
static const char* g_setting_key_mouse_sensitivity = "mouse_sensitivity";
static const char* g_setting_key_camera_speed = "camera_movement_speed";
static const char* g_setting_key_camera_position_x = "camera_position_x";
static const char* g_setting_key_camera_position_y = "camera_position_y";
static const char* g_setting_key_camera_position_z = "camera_position_z";
static const char* g_setting_key_camera_yaw = "camera_yaw_radians";
static const char* g_setting_key_camera_pitch = "camera_pitch_radians";
static const char* g_setting_key_selected_object_name = "ui.selected_object_name";
static const char* g_setting_key_scene_panel_visible = "ui.scene_objects_panel_visible";
static const char* g_setting_key_details_panel_visible = "ui.object_details_panel_visible";

static float sandbox3d_get_mouse_sensitivity(const sandbox3d_state* state);

static const char* sandbox3d_get_build_configuration_label(void)
{
#if defined(_DEBUG)
    return "Debug";
#else
    return "Release";
#endif
}

static henka_result sandbox3d_get_settings_path(const henka_engine* engine, char** out_path)
{
    return henka_path_resolve(henka_engine_get_user_data_base_path(engine), "sandbox3d.settings", out_path);
}

static henka_transform sandbox3d_make_transform(henka_vec3 position, henka_vec3 scale)
{
    henka_transform transform;

    transform = henka_transform_identity();
    transform.position = position;
    transform.scale = scale;
    return transform;
}

static void sandbox3d_reset_workspace_layout(sandbox3d_state* state)
{
    if (state == NULL)
    {
        return;
    }

    state->workspace.scene_objects_panel_visible = true;
    state->workspace.object_details_panel_visible = true;
}

static void sandbox3d_truncate_text(const char* source, char* buffer, size_t buffer_size, size_t max_visible_characters)
{
    size_t length;

    if (buffer == NULL || buffer_size == 0U)
    {
        return;
    }

    if (source == NULL || source[0] == '\0')
    {
        snprintf(buffer, buffer_size, "(unavailable)");
        return;
    }

    length = strlen(source);
    if (length <= max_visible_characters || max_visible_characters < 4U)
    {
        snprintf(buffer, buffer_size, "%s", source);
        return;
    }

    snprintf(
        buffer,
        buffer_size,
        "%.*s...",
        (int)(max_visible_characters - 3U),
        source);
}

static void sandbox3d_format_display_path(const char* label, const char* path, char* buffer, size_t buffer_size)
{
    char truncated_value[80];
    const char* value;
    size_t path_length;

    if (buffer == NULL || buffer_size == 0U)
    {
        return;
    }

    value = (path != NULL && path[0] != '\0') ? path : "(unavailable)";
    path_length = strlen(value);
    if (path_length > 58U)
    {
        snprintf(truncated_value, sizeof(truncated_value), "...%s", value + path_length - 55U);
        value = truncated_value;
    }

    snprintf(buffer, buffer_size, "%s: %s", label, value);
}

static const sandbox3d_object_descriptor* sandbox3d_get_descriptor_by_entity(
    const sandbox3d_state* state,
    henka_entity entity)
{
    size_t index;

    if (state == NULL || entity == HENKA_INVALID_ENTITY)
    {
        return NULL;
    }

    for (index = 0U; index < SANDBOX3D_OBJECT_COUNT; ++index)
    {
        if (state->descriptors[index].entity == entity)
        {
            return &state->descriptors[index];
        }
    }

    return NULL;
}

static henka_entity sandbox3d_get_first_selectable_entity(const sandbox3d_state* state)
{
    size_t index;

    if (state == NULL)
    {
        return HENKA_INVALID_ENTITY;
    }

    for (index = 0U; index < SANDBOX3D_OBJECT_COUNT; ++index)
    {
        if (state->descriptors[index].entity != HENKA_INVALID_ENTITY)
        {
            return state->descriptors[index].entity;
        }
    }

    return HENKA_INVALID_ENTITY;
}

static const char* sandbox3d_safe_entity_name(const sandbox3d_state* state, henka_entity entity, const char* fallback_name)
{
    const char* name;

    if (state == NULL || state->scene == NULL)
    {
        return fallback_name;
    }

    name = henka_scene_get_entity_name(state->scene, entity);
    return name != NULL ? name : fallback_name;
}

static void sandbox3d_register_object_descriptor(
    sandbox3d_state* state,
    sandbox3d_object_kind kind,
    henka_entity entity,
    const char* display_name,
    const char* location_hint,
    const char* short_explanation,
    const char* developer_detail,
    const char* mesh_summary,
    const char* material_summary,
    const char* texture_summary,
    bool can_hide,
    bool can_reset,
    henka_transform default_transform)
{
    if (state == NULL || kind < 0 || kind >= SANDBOX3D_OBJECT_COUNT)
    {
        return;
    }

    state->descriptors[kind].kind = kind;
    state->descriptors[kind].entity = entity;
    state->descriptors[kind].display_name = display_name;
    state->descriptors[kind].location_hint = location_hint;
    state->descriptors[kind].short_explanation = short_explanation;
    state->descriptors[kind].developer_detail = developer_detail;
    state->descriptors[kind].mesh_summary = mesh_summary;
    state->descriptors[kind].material_summary = material_summary;
    state->descriptors[kind].texture_summary = texture_summary;
    state->descriptors[kind].can_hide = can_hide;
    state->descriptors[kind].can_reset = can_reset;
    state->descriptors[kind].default_transform = default_transform;
}

static void sandbox3d_print_scene_legend(const sandbox3d_state* state)
{
    size_t index;

    printf("Scene examples:\n");
    for (index = 0U; index < SANDBOX3D_OBJECT_COUNT; ++index)
    {
        const sandbox3d_object_descriptor* descriptor;

        descriptor = &state->descriptors[index];
        printf(
            "  %s: %s %s\n",
            sandbox3d_safe_entity_name(state, descriptor->entity, descriptor->display_name),
            descriptor->location_hint,
            descriptor->short_explanation);
    }
}

static void sandbox3d_print_capture_state(henka_engine* engine, const char* trigger)
{
    printf("Mouse look: %s", henka_engine_is_mouse_captured(engine) ? "captured" : "released");
    if (trigger != NULL)
    {
        printf(" with %s", trigger);
    }
    printf("\n");
    fflush(stdout);
}

static void sandbox3d_print_ui_state(bool visible)
{
    printf("Sandbox panel: %s\n", visible ? "shown" : "hidden");
    fflush(stdout);
}

static void sandbox3d_print_startup_ui_cue(const sandbox3d_state* state)
{
    if (state == NULL)
    {
        return;
    }

    if (state->startup_panels_auto_opened)
    {
        printf("Startup UI: the in-window panels start open on first run so the scene tools are immediately visible.\n");
        printf("Startup UI: press F4 to hide the panels and press F4 again to bring them back.\n");
    }
    else
    {
        printf("Startup UI: press F4 to open the in-window panels.\n");
    }

    fflush(stdout);
}

static void sandbox3d_print_help(const sandbox3d_state* state)
{
    printf("Henka Engine Sandbox 3D\n");
    printf("Build: local %s %s %s\n", sandbox3d_get_build_configuration_label(), __DATE__, __TIME__);
    printf("This scene shows texture rendering, untextured material color, early OBJ loading, visible fallback behavior, and the first developer-facing scene inspection panels.\n");
    printf("Controls:\n");
    printf("  W A S D          Move across the scene\n");
    printf("  Q / E            Move down / up\n");
    printf("  Shift            Move faster\n");
    printf("  Mouse            Look around while mouse capture is active\n");
    printf("  Right Mouse / Tab Toggle mouse capture\n");
    printf("  F1               Toggle wireframe\n");
    printf("  F2               Print the scene legend again\n");
    printf("  F3               Show or hide the debug grid\n");
    printf("  F4               Show or hide the sandbox panels\n");
    printf("  H                Print controls and the scene legend again\n");
    printf("  Escape           Close the panels first. Then release the mouse. Then exit.\n");
    printf("Panel shortcuts:\n");
    printf("  Press F4 to open the in-window panels.\n");
    printf("  Use the panels to inspect named scene objects, focus the camera, reset object transforms, toggle visibility, and save or reset local sandbox settings.\n");
    printf("  Mouse look and camera movement pause while the UI is open.\n");
    sandbox3d_print_scene_legend(state);
    printf("Manual QA focus:\n");
    printf("  Confirm each scene example is visible, object selection updates the details panel, and camera focus and reset actions stay predictable.\n");
    printf("Current limitations:\n");
    printf("  OBJ loading is still early and currently limited to a small, documented subset.\n");
    printf("  OBJ material libraries, negative indices, animation, hierarchy tools, scene saving, and broader 2D or 2.5D workflows are not available yet.\n");
    printf("  The UI overlay is intentionally small and is not a full editor.\n");
    printf("  Sandbox settings are saved locally beside the executable in the user folder.\n");
    fflush(stdout);
}

static henka_result sandbox3d_configure_entity(
    henka_scene* scene,
    henka_entity entity,
    henka_mesh* mesh,
    henka_material material,
    henka_transform transform)
{
    henka_result result;

    result = henka_scene_set_entity_transform(scene, entity, transform);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    result = henka_scene_set_entity_mesh(scene, entity, mesh);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    return henka_scene_set_entity_material(scene, entity, material);
}

static void sandbox3d_release_owned_resources(sandbox3d_state* state)
{
    if (state == NULL)
    {
        return;
    }

    henka_mesh_destroy(state->grid_mesh);
    henka_mesh_destroy(state->ground_mesh);
    henka_mesh_destroy(state->cube_mesh);
    henka_scene_destroy(state->scene);
    henka_settings_destroy(state->settings);
    henka_ui_destroy(state->ui);

    state->grid_mesh = NULL;
    state->ground_mesh = NULL;
    state->cube_mesh = NULL;
    state->marker_mesh = NULL;
    state->missing_model_mesh = NULL;
    state->scene = NULL;
    state->settings = NULL;
    state->ui = NULL;
}

static void sandbox3d_reset_camera_defaults(sandbox3d_state* state)
{
    if (state == NULL)
    {
        return;
    }

    state->camera.position = g_camera_start_position;
    state->camera.yaw_radians = g_camera_start_yaw;
    state->camera.pitch_radians = g_camera_start_pitch;
    state->camera.movement_speed = g_default_camera_movement_speed;
    state->camera.fast_movement_multiplier = 2.5f;
}

static void sandbox3d_adjust_mouse_sensitivity(sandbox3d_state* state, float delta)
{
    float next_value;

    if (state == NULL || state->settings == NULL)
    {
        return;
    }

    next_value = sandbox3d_get_mouse_sensitivity(state) + delta;
    if (next_value < 0.0005f)
    {
        next_value = 0.0005f;
    }
    if (next_value > 0.02f)
    {
        next_value = 0.02f;
    }

    henka_settings_set_float(state->settings, g_setting_key_mouse_sensitivity, next_value);
}

static void sandbox3d_adjust_camera_speed(sandbox3d_state* state, float delta)
{
    float next_value;

    if (state == NULL)
    {
        return;
    }

    next_value = state->camera.movement_speed + delta;
    if (next_value < 1.0f)
    {
        next_value = 1.0f;
    }
    if (next_value > 16.0f)
    {
        next_value = 16.0f;
    }

    state->camera.movement_speed = next_value;
}

static float sandbox3d_get_mouse_sensitivity(const sandbox3d_state* state)
{
    float value;

    if (state == NULL || state->settings == NULL)
    {
        return g_default_mouse_look_sensitivity;
    }

    value = henka_settings_get_float(state->settings, g_setting_key_mouse_sensitivity, g_default_mouse_look_sensitivity);
    return value > 0.0f ? value : g_default_mouse_look_sensitivity;
}

static void sandbox3d_select_entity(sandbox3d_state* state, henka_entity entity)
{
    if (state == NULL || state->scene == NULL)
    {
        return;
    }

    if (henka_scene_is_entity_valid(state->scene, entity))
    {
        state->selected_entity = entity;
    }
    else
    {
        state->selected_entity = HENKA_INVALID_ENTITY;
    }
}

static void sandbox3d_restore_selected_object(sandbox3d_state* state)
{
    const char* selected_name;
    henka_entity selected_entity;

    if (state == NULL || state->settings == NULL || state->scene == NULL)
    {
        return;
    }

    selected_entity = HENKA_INVALID_ENTITY;
    selected_name = henka_settings_get_string(state->settings, g_setting_key_selected_object_name, "");
    if (selected_name[0] != '\0')
    {
        if (henka_scene_find_entity_by_name(state->scene, selected_name, &selected_entity) != HENKA_SUCCESS)
        {
            selected_entity = HENKA_INVALID_ENTITY;
        }
    }

    if (selected_entity == HENKA_INVALID_ENTITY)
    {
        selected_entity = sandbox3d_get_first_selectable_entity(state);
    }

    sandbox3d_select_entity(state, selected_entity);
}

static void sandbox3d_apply_loaded_settings(henka_engine* engine, sandbox3d_state* state)
{
    bool grid_visible;
    bool wireframe_enabled;
    float movement_speed;
    henka_result result;

    if (engine == NULL || state == NULL || state->settings == NULL || state->scene == NULL)
    {
        return;
    }

    grid_visible = henka_settings_get_bool(state->settings, g_setting_key_grid_visible, true);
    wireframe_enabled = henka_settings_get_bool(state->settings, g_setting_key_wireframe_enabled, false);
    movement_speed = henka_settings_get_float(state->settings, g_setting_key_camera_speed, g_default_camera_movement_speed);

    state->camera.position.x = henka_settings_get_float(state->settings, g_setting_key_camera_position_x, g_camera_start_position.x);
    state->camera.position.y = henka_settings_get_float(state->settings, g_setting_key_camera_position_y, g_camera_start_position.y);
    state->camera.position.z = henka_settings_get_float(state->settings, g_setting_key_camera_position_z, g_camera_start_position.z);
    state->camera.yaw_radians = henka_settings_get_float(state->settings, g_setting_key_camera_yaw, g_camera_start_yaw);
    state->camera.pitch_radians = henka_settings_get_float(state->settings, g_setting_key_camera_pitch, g_camera_start_pitch);
    state->camera.movement_speed = movement_speed > 0.0f ? movement_speed : g_default_camera_movement_speed;
    state->camera.fast_movement_multiplier = 2.5f;

    result = henka_scene_set_entity_visible(state->scene, state->grid_entity, grid_visible);
    if (result != HENKA_SUCCESS)
    {
        HENKA_LOG_WARN("Debug grid visibility could not be restored from sandbox settings.");
    }

    result = henka_engine_set_wireframe(engine, wireframe_enabled);
    if (result != HENKA_SUCCESS)
    {
        HENKA_LOG_WARN("Wireframe state could not be restored from sandbox settings.");
    }

    if (henka_settings_get_float(state->settings, g_setting_key_mouse_sensitivity, g_default_mouse_look_sensitivity) <= 0.0f)
    {
        henka_settings_set_float(state->settings, g_setting_key_mouse_sensitivity, g_default_mouse_look_sensitivity);
    }

    state->workspace.scene_objects_panel_visible = henka_settings_get_bool(state->settings, g_setting_key_scene_panel_visible, true);
    state->workspace.object_details_panel_visible = henka_settings_get_bool(state->settings, g_setting_key_details_panel_visible, true);
    sandbox3d_restore_selected_object(state);
}

static henka_result sandbox3d_save_settings(henka_engine* engine, sandbox3d_state* state)
{
    const sandbox3d_object_descriptor* descriptor;
    char* settings_path;
    henka_result result;

    if (engine == NULL || state == NULL || state->settings == NULL || state->scene == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    henka_settings_set_bool(state->settings, g_setting_key_grid_visible, henka_scene_is_entity_visible(state->scene, state->grid_entity));
    henka_settings_set_bool(state->settings, g_setting_key_wireframe_enabled, henka_engine_is_wireframe_enabled(engine));
    henka_settings_set_float(state->settings, g_setting_key_mouse_sensitivity, sandbox3d_get_mouse_sensitivity(state));
    henka_settings_set_float(state->settings, g_setting_key_camera_speed, state->camera.movement_speed);
    henka_settings_set_float(state->settings, g_setting_key_camera_position_x, state->camera.position.x);
    henka_settings_set_float(state->settings, g_setting_key_camera_position_y, state->camera.position.y);
    henka_settings_set_float(state->settings, g_setting_key_camera_position_z, state->camera.position.z);
    henka_settings_set_float(state->settings, g_setting_key_camera_yaw, state->camera.yaw_radians);
    henka_settings_set_float(state->settings, g_setting_key_camera_pitch, state->camera.pitch_radians);
    henka_settings_set_bool(state->settings, g_setting_key_scene_panel_visible, state->workspace.scene_objects_panel_visible);
    henka_settings_set_bool(state->settings, g_setting_key_details_panel_visible, state->workspace.object_details_panel_visible);

    descriptor = sandbox3d_get_descriptor_by_entity(state, state->selected_entity);
    if (descriptor != NULL && descriptor->display_name != NULL)
    {
        henka_settings_set_string(state->settings, g_setting_key_selected_object_name, descriptor->display_name);
    }
    else
    {
        henka_settings_remove(state->settings, g_setting_key_selected_object_name);
    }

    result = sandbox3d_get_settings_path(engine, &settings_path);
    if (result != HENKA_SUCCESS)
    {
        HENKA_LOG_WARN("Sandbox settings could not be saved because the local settings path could not be resolved.");
        return result;
    }

    result = henka_settings_save_file(state->settings, settings_path);
    if (result != HENKA_SUCCESS)
    {
        HENKA_LOG_WARN("Sandbox settings could not be saved to '%s'.", settings_path);
    }

    henka_free(settings_path);
    return result;
}

static henka_result sandbox3d_reset_settings_object(sandbox3d_state* state)
{
    henka_result result;

    if (state == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    henka_settings_destroy(state->settings);
    state->settings = NULL;
    result = henka_settings_create(&state->settings);
    return result;
}

static henka_result sandbox3d_reset_settings(henka_engine* engine, sandbox3d_state* state)
{
    char* settings_path;
    henka_result result;

    if (engine == NULL || state == NULL || state->scene == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    result = sandbox3d_reset_settings_object(state);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    sandbox3d_reset_camera_defaults(state);
    sandbox3d_reset_workspace_layout(state);
    state->selected_entity = sandbox3d_get_first_selectable_entity(state);

    result = henka_scene_set_entity_visible(state->scene, state->grid_entity, true);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    result = henka_engine_set_wireframe(engine, false);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    result = sandbox3d_get_settings_path(engine, &settings_path);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    remove(settings_path);
    henka_free(settings_path);
    return sandbox3d_save_settings(engine, state);
}

static bool sandbox3d_toggle_grid_visibility(sandbox3d_state* state, bool visible, bool print_status)
{
    if (state == NULL || state->scene == NULL)
    {
        return false;
    }

    if (henka_scene_set_entity_visible(state->scene, state->grid_entity, visible) != HENKA_SUCCESS)
    {
        return false;
    }

    if (print_status)
    {
        printf("Debug grid: %s\n", visible ? "shown" : "hidden");
        fflush(stdout);
    }

    return true;
}

static bool sandbox3d_toggle_wireframe(henka_engine* engine, bool enabled, bool print_status)
{
    if (engine == NULL)
    {
        return false;
    }

    if (henka_engine_set_wireframe(engine, enabled) != HENKA_SUCCESS)
    {
        return false;
    }

    if (print_status)
    {
        printf("Wireframe: %s\n", enabled ? "on" : "off");
        fflush(stdout);
    }

    return true;
}

static bool sandbox3d_reset_selected_entity_transform(sandbox3d_state* state)
{
    const sandbox3d_object_descriptor* descriptor;

    descriptor = sandbox3d_get_descriptor_by_entity(state, state != NULL ? state->selected_entity : HENKA_INVALID_ENTITY);
    if (state == NULL || state->scene == NULL || descriptor == NULL || !descriptor->can_reset)
    {
        return false;
    }

    return henka_scene_set_entity_transform(state->scene, descriptor->entity, descriptor->default_transform) == HENKA_SUCCESS;
}

static bool sandbox3d_toggle_selected_entity_visibility(sandbox3d_state* state)
{
    bool currently_visible;
    const sandbox3d_object_descriptor* descriptor;

    descriptor = sandbox3d_get_descriptor_by_entity(state, state != NULL ? state->selected_entity : HENKA_INVALID_ENTITY);
    if (state == NULL || state->scene == NULL || descriptor == NULL || !descriptor->can_hide)
    {
        return false;
    }

    currently_visible = henka_scene_is_entity_visible(state->scene, descriptor->entity);
    return henka_scene_set_entity_visible(state->scene, descriptor->entity, !currently_visible) == HENKA_SUCCESS;
}

static bool sandbox3d_focus_camera_on_selected(sandbox3d_state* state)
{
    const sandbox3d_object_descriptor* descriptor;
    float distance;
    float focus_height;
    float max_scale;
    float pitch;
    float yaw;
    henka_result result;
    henka_transform transform;
    henka_vec3 direction;
    henka_vec3 focus_offset;
    henka_vec3 focus_target;

    if (state == NULL || state->scene == NULL)
    {
        return false;
    }

    descriptor = sandbox3d_get_descriptor_by_entity(state, state->selected_entity);
    if (descriptor == NULL)
    {
        return false;
    }

    result = henka_scene_get_entity_transform(state->scene, descriptor->entity, &transform);
    if (result != HENKA_SUCCESS)
    {
        return false;
    }

    max_scale = transform.scale.x;
    if (transform.scale.y > max_scale)
    {
        max_scale = transform.scale.y;
    }
    if (transform.scale.z > max_scale)
    {
        max_scale = transform.scale.z;
    }
    if (max_scale < 1.0f)
    {
        max_scale = 1.0f;
    }

    focus_height = max_scale * 0.45f;
    distance = 2.4f + max_scale * 2.4f;

    switch (descriptor->kind)
    {
        case SANDBOX3D_OBJECT_GROUND:
            focus_height = 0.2f;
            distance = 7.2f;
            break;
        case SANDBOX3D_OBJECT_DEBUG_GRID:
            focus_height = 0.2f;
            distance = 8.0f;
            break;
        case SANDBOX3D_OBJECT_OBJ_MARKER:
            distance = 4.4f;
            break;
        default:
            break;
    }

    focus_target = transform.position;
    focus_target.y += focus_height;
    focus_offset = (henka_vec3){distance * 0.45f, 1.1f + max_scale * 0.7f, distance};
    state->camera.position = henka_vec3_add(focus_target, focus_offset);

    direction = henka_vec3_subtract(focus_target, state->camera.position);
    direction = henka_vec3_normalize(direction);
    yaw = atan2f(direction.z, direction.x);
    pitch = asinf(direction.y);

    if (pitch > 1.55334306f)
    {
        pitch = 1.55334306f;
    }
    if (pitch < -1.55334306f)
    {
        pitch = -1.55334306f;
    }

    state->camera.yaw_radians = yaw;
    state->camera.pitch_radians = pitch;
    return true;
}

static void sandbox3d_print_selected_object_info(const sandbox3d_state* state)
{
    bool visible;
    const sandbox3d_object_descriptor* descriptor;
    henka_transform transform;

    if (state == NULL || state->scene == NULL)
    {
        return;
    }

    descriptor = sandbox3d_get_descriptor_by_entity(state, state->selected_entity);
    if (descriptor == NULL)
    {
        printf("No sandbox object is currently selected.\n");
        fflush(stdout);
        return;
    }

    if (henka_scene_get_entity_transform(state->scene, descriptor->entity, &transform) != HENKA_SUCCESS)
    {
        printf("Selected object info could not be read.\n");
        fflush(stdout);
        return;
    }

    visible = henka_scene_is_entity_visible(state->scene, descriptor->entity);

    printf("Object Info\n");
    printf("  Name: %s\n", descriptor->display_name);
    printf("  Entity: %u\n", (unsigned int)descriptor->entity);
    printf("  Visible: %s\n", visible ? "Yes" : "No");
    printf("  Position: %.2f %.2f %.2f\n", transform.position.x, transform.position.y, transform.position.z);
    printf("  Rotation: %.2f %.2f %.2f %.2f\n", transform.rotation.x, transform.rotation.y, transform.rotation.z, transform.rotation.w);
    printf("  Scale: %.2f %.2f %.2f\n", transform.scale.x, transform.scale.y, transform.scale.z);
    printf("  Demonstrates: %s\n", descriptor->short_explanation);
    printf("  Detail: %s\n", descriptor->developer_detail);
    printf("  Mesh: %s\n", descriptor->mesh_summary);
    printf("  Material: %s\n", descriptor->material_summary);
    printf("  Texture: %s\n", descriptor->texture_summary);
    fflush(stdout);
}

static sandbox3d_workspace_layout sandbox3d_get_workspace_layout(int framebuffer_width, int framebuffer_height)
{
    float available_height;
    float full_width;
    sandbox3d_workspace_layout layout;

    layout.controls_panel = (henka_ui_rect){g_ui_panel_margin, g_ui_panel_margin, g_ui_controls_width, g_ui_panel_height};
    layout.scene_objects_panel = (henka_ui_rect){g_ui_panel_margin, g_ui_panel_margin, g_ui_scene_width, g_ui_panel_height};
    layout.object_details_panel = (henka_ui_rect){g_ui_panel_margin, g_ui_panel_margin, g_ui_details_width, g_ui_panel_height};

    if (framebuffer_width <= 0 || framebuffer_height <= 0)
    {
        return layout;
    }

    available_height = (float)framebuffer_height - g_ui_panel_margin * 2.0f;
    if (available_height < 360.0f)
    {
        available_height = 360.0f;
    }

    if (framebuffer_width >= 1220)
    {
        layout.controls_panel.x = g_ui_panel_margin;
        layout.controls_panel.y = g_ui_panel_margin;
        layout.controls_panel.width = g_ui_controls_width;
        layout.controls_panel.height = available_height;

        layout.scene_objects_panel.x = layout.controls_panel.x + layout.controls_panel.width + g_ui_panel_gap;
        layout.scene_objects_panel.y = g_ui_panel_margin;
        layout.scene_objects_panel.width = g_ui_scene_width;
        layout.scene_objects_panel.height = available_height;

        layout.object_details_panel.x = layout.scene_objects_panel.x + layout.scene_objects_panel.width + g_ui_panel_gap;
        layout.object_details_panel.y = g_ui_panel_margin;
        layout.object_details_panel.width = (float)framebuffer_width - layout.object_details_panel.x - g_ui_panel_margin;
        layout.object_details_panel.height = available_height;
    }
    else if (framebuffer_width >= 900)
    {
        layout.controls_panel.x = g_ui_panel_margin;
        layout.controls_panel.y = g_ui_panel_margin;
        layout.controls_panel.width = g_ui_controls_width;
        layout.controls_panel.height = available_height;

        layout.scene_objects_panel.x = layout.controls_panel.x + layout.controls_panel.width + g_ui_panel_gap;
        layout.scene_objects_panel.y = g_ui_panel_margin;
        layout.scene_objects_panel.width = (float)framebuffer_width - layout.scene_objects_panel.x - g_ui_panel_margin;
        layout.scene_objects_panel.height = 208.0f;

        layout.object_details_panel.x = layout.scene_objects_panel.x;
        layout.object_details_panel.y = layout.scene_objects_panel.y + layout.scene_objects_panel.height + g_ui_panel_gap;
        layout.object_details_panel.width = layout.scene_objects_panel.width;
        layout.object_details_panel.height = available_height - layout.scene_objects_panel.height - g_ui_panel_gap;
    }
    else
    {
        full_width = (float)framebuffer_width - g_ui_panel_margin * 2.0f;
        if (full_width < 340.0f)
        {
            full_width = 340.0f;
        }

        layout.controls_panel.x = g_ui_panel_margin;
        layout.controls_panel.y = g_ui_panel_margin;
        layout.controls_panel.width = full_width;
        layout.controls_panel.height = 250.0f;

        layout.scene_objects_panel.x = g_ui_panel_margin;
        layout.scene_objects_panel.y = layout.controls_panel.y + layout.controls_panel.height + g_ui_panel_gap;
        layout.scene_objects_panel.width = full_width;
        layout.scene_objects_panel.height = 170.0f;

        layout.object_details_panel.x = g_ui_panel_margin;
        layout.object_details_panel.y = layout.scene_objects_panel.y + layout.scene_objects_panel.height + g_ui_panel_gap;
        layout.object_details_panel.width = full_width;
        layout.object_details_panel.height = available_height - layout.controls_panel.height - layout.scene_objects_panel.height - g_ui_panel_gap * 2.0f;
        if (layout.object_details_panel.height < 150.0f)
        {
            layout.object_details_panel.height = 150.0f;
        }
    }

    return layout;
}

static void sandbox3d_draw_controls_panel(
    henka_engine* engine,
    sandbox3d_state* state,
    const sandbox3d_workspace_layout* layout,
    const char* asset_path_text,
    const char* user_path_text,
    const char* settings_path_text,
    const char* capture_text,
    const char* fps_text,
    const char* camera_text,
    const char* mouse_text,
    const char* speed_text)
{
    bool scene_panel_visible;
    bool details_panel_visible;
    bool grid_visible;
    bool wireframe_enabled;
    float button_width;
    float right_column_x;
    henka_ui_rect panel_bounds;

    if (engine == NULL || state == NULL || layout == NULL)
    {
        return;
    }

    panel_bounds = layout->controls_panel;
    button_width = (panel_bounds.width - 44.0f) * 0.5f;
    right_column_x = panel_bounds.x + 22.0f + button_width;
    scene_panel_visible = state->workspace.scene_objects_panel_visible;
    details_panel_visible = state->workspace.object_details_panel_visible;
    grid_visible = henka_scene_is_entity_visible(state->scene, state->grid_entity);
    wireframe_enabled = henka_engine_is_wireframe_enabled(engine);

    henka_ui_panel(state->ui, panel_bounds, "Controls");
    henka_ui_label(state->ui, panel_bounds.x + 14.0f, panel_bounds.y + 36.0f, 1.0f, "Current Scene");
        if (henka_ui_toggle(state->ui, "grid", (henka_ui_rect){panel_bounds.x + 14.0f, panel_bounds.y + 54.0f, button_width, 32.0f}, "Debug Grid", &grid_visible))
        {
            sandbox3d_toggle_grid_visibility(state, grid_visible, true);
        }
        else
        {
            sandbox3d_toggle_grid_visibility(state, grid_visible, false);
        }

    if (henka_ui_toggle(state->ui, "wireframe", (henka_ui_rect){right_column_x, panel_bounds.y + 54.0f, button_width, 32.0f}, "Wireframe", &wireframe_enabled))
    {
        sandbox3d_toggle_wireframe(engine, wireframe_enabled, true);
    }
    else
    {
        sandbox3d_toggle_wireframe(engine, wireframe_enabled, false);
    }

    if (henka_ui_button(state->ui, "reset_camera", (henka_ui_rect){panel_bounds.x + 14.0f, panel_bounds.y + 98.0f, button_width, 32.0f}, "Reset Camera"))
    {
        sandbox3d_reset_camera_defaults(state);
        printf("Camera reset to the default sandbox view.\n");
        fflush(stdout);
    }

    if (henka_ui_button(state->ui, "save_settings", (henka_ui_rect){right_column_x, panel_bounds.y + 98.0f, button_width, 32.0f}, "Save Settings"))
    {
        if (sandbox3d_save_settings(engine, state) == HENKA_SUCCESS)
        {
            printf("Sandbox settings saved.\n");
        }
        else
        {
            printf("Sandbox settings could not be saved.\n");
        }
        fflush(stdout);
    }

    if (henka_ui_button(state->ui, "reset_settings", (henka_ui_rect){panel_bounds.x + 14.0f, panel_bounds.y + 142.0f, button_width, 32.0f}, "Reset Settings"))
    {
        if (sandbox3d_reset_settings(engine, state) == HENKA_SUCCESS)
        {
            printf("Sandbox settings reset to defaults.\n");
        }
        else
        {
            printf("Sandbox settings could not be reset.\n");
        }
        fflush(stdout);
    }

    if (henka_ui_button(state->ui, "print_help", (henka_ui_rect){right_column_x, panel_bounds.y + 142.0f, button_width, 32.0f}, "Print Help"))
    {
        sandbox3d_print_help(state);
    }

    if (henka_ui_button(state->ui, "print_legend", (henka_ui_rect){panel_bounds.x + 14.0f, panel_bounds.y + 186.0f, button_width, 32.0f}, "Scene Legend"))
    {
        sandbox3d_print_scene_legend(state);
        fflush(stdout);
    }

    if (henka_ui_button(state->ui, "reset_layout", (henka_ui_rect){right_column_x, panel_bounds.y + 186.0f, button_width, 32.0f}, "Reset Layout"))
    {
        sandbox3d_reset_workspace_layout(state);
        printf("Sandbox panel layout reset.\n");
        fflush(stdout);
    }

    henka_ui_label(state->ui, panel_bounds.x + 14.0f, panel_bounds.y + 228.0f, 1.0f, "Panels");
    if (henka_ui_toggle(state->ui, "scene_panel_visible", (henka_ui_rect){panel_bounds.x + 14.0f, panel_bounds.y + 246.0f, button_width, 32.0f}, "Scene Objects", &scene_panel_visible))
    {
        state->workspace.scene_objects_panel_visible = scene_panel_visible;
    }
    else
    {
        state->workspace.scene_objects_panel_visible = scene_panel_visible;
    }

    if (henka_ui_toggle(state->ui, "details_panel_visible", (henka_ui_rect){right_column_x, panel_bounds.y + 246.0f, button_width, 32.0f}, "Object Details", &details_panel_visible))
    {
        state->workspace.object_details_panel_visible = details_panel_visible;
    }
    else
    {
        state->workspace.object_details_panel_visible = details_panel_visible;
    }

    henka_ui_label(state->ui, panel_bounds.x + 14.0f, panel_bounds.y + 288.0f, 1.0f, "Adjust");
    if (henka_ui_button(state->ui, "mouse_less", (henka_ui_rect){panel_bounds.x + 14.0f, panel_bounds.y + 306.0f, 72.0f, 28.0f}, "Less"))
    {
        sandbox3d_adjust_mouse_sensitivity(state, -0.0005f);
    }
    if (henka_ui_button(state->ui, "mouse_more", (henka_ui_rect){panel_bounds.x + 94.0f, panel_bounds.y + 306.0f, 72.0f, 28.0f}, "More"))
    {
        sandbox3d_adjust_mouse_sensitivity(state, 0.0005f);
    }
    if (henka_ui_button(state->ui, "speed_less", (henka_ui_rect){right_column_x, panel_bounds.y + 306.0f, 72.0f, 28.0f}, "Less"))
    {
        sandbox3d_adjust_camera_speed(state, -0.5f);
    }
    if (henka_ui_button(state->ui, "speed_more", (henka_ui_rect){right_column_x + 80.0f, panel_bounds.y + 306.0f, 72.0f, 28.0f}, "More"))
    {
        sandbox3d_adjust_camera_speed(state, 0.5f);
    }

    henka_ui_label(state->ui, panel_bounds.x + 14.0f, panel_bounds.y + 346.0f, 1.0f, capture_text);
    henka_ui_label(state->ui, panel_bounds.x + 14.0f, panel_bounds.y + 362.0f, 1.0f, grid_visible ? "Grid: Visible" : "Grid: Hidden");
    henka_ui_label(state->ui, panel_bounds.x + 14.0f, panel_bounds.y + 378.0f, 1.0f, wireframe_enabled ? "Wireframe: On" : "Wireframe: Off");
    henka_ui_label(state->ui, right_column_x, panel_bounds.y + 346.0f, 1.0f, mouse_text);
    henka_ui_label(state->ui, right_column_x, panel_bounds.y + 362.0f, 1.0f, speed_text);
    henka_ui_label(state->ui, right_column_x, panel_bounds.y + 378.0f, 1.0f, fps_text);

    henka_ui_label(state->ui, panel_bounds.x + 14.0f, panel_bounds.y + panel_bounds.height - 66.0f, 1.0f, camera_text);
    henka_ui_label(state->ui, panel_bounds.x + 14.0f, panel_bounds.y + panel_bounds.height - 50.0f, 1.0f, asset_path_text);
    henka_ui_label(state->ui, panel_bounds.x + 14.0f, panel_bounds.y + panel_bounds.height - 34.0f, 1.0f, user_path_text);
    henka_ui_label(state->ui, panel_bounds.x + 14.0f, panel_bounds.y + panel_bounds.height - 18.0f, 1.0f, settings_path_text);
}

static void sandbox3d_draw_scene_objects_panel(
    sandbox3d_state* state,
    const sandbox3d_workspace_layout* layout)
{
    char row_label[96];
    const sandbox3d_object_descriptor* descriptor;
    const char* entity_name;
    float row_y;
    henka_entity entity;
    henka_ui_rect panel_bounds;
    size_t scene_index;

    if (state == NULL || layout == NULL || state->scene == NULL || !state->workspace.scene_objects_panel_visible)
    {
        return;
    }

    panel_bounds = layout->scene_objects_panel;
    henka_ui_panel(state->ui, panel_bounds, "Scene Objects");
    henka_ui_label(state->ui, panel_bounds.x + 14.0f, panel_bounds.y + 36.0f, 1.0f, "Current scene examples");

    row_y = panel_bounds.y + 58.0f;
    for (scene_index = 0U; scene_index < henka_scene_get_entity_count(state->scene); ++scene_index)
    {
        entity = henka_scene_get_entity_at_index(state->scene, scene_index);
        if (entity == HENKA_INVALID_ENTITY)
        {
            continue;
        }

        descriptor = sandbox3d_get_descriptor_by_entity(state, entity);
        entity_name = henka_scene_get_entity_name(state->scene, entity);
        if (descriptor == NULL && entity_name == NULL)
        {
            continue;
        }

        if (row_y + 30.0f > panel_bounds.y + panel_bounds.height - 18.0f)
        {
            henka_ui_label(state->ui, panel_bounds.x + 14.0f, row_y + 4.0f, 1.0f, "More objects exist than this panel can show right now.");
            break;
        }

        sandbox3d_truncate_text(
            entity_name != NULL ? entity_name : descriptor->display_name,
            row_label,
            sizeof(row_label),
            28U);

        if (!henka_scene_is_entity_visible(state->scene, entity))
        {
            size_t label_length;

            label_length = strlen(row_label);
            if (label_length + 9U < sizeof(row_label))
            {
                memcpy(row_label + label_length, " [Hidden]", 10U);
            }
        }

        if (henka_ui_selectable(
            state->ui,
            entity_name != NULL ? entity_name : row_label,
            (henka_ui_rect){panel_bounds.x + 14.0f, row_y, panel_bounds.width - 28.0f, 28.0f},
            row_label,
            state->selected_entity == entity))
        {
            sandbox3d_select_entity(state, entity);
        }

        row_y += 34.0f;
    }
}

static void sandbox3d_draw_object_details_panel(
    sandbox3d_state* state,
    const sandbox3d_workspace_layout* layout)
{
    bool visible;
    char action_label[32];
    char detail_text[92];
    char developer_text[96];
    char material_text[96];
    char mesh_text[96];
    char position_text[64];
    char rotation_text[72];
    char texture_text[96];
    char visibility_text[48];
    char scale_text[64];
    const sandbox3d_object_descriptor* descriptor;
    henka_result result;
    henka_transform transform;
    henka_ui_rect panel_bounds;

    if (state == NULL || layout == NULL || !state->workspace.object_details_panel_visible)
    {
        return;
    }

    panel_bounds = layout->object_details_panel;
    henka_ui_panel(state->ui, panel_bounds, "Object Details");

    descriptor = sandbox3d_get_descriptor_by_entity(state, state->selected_entity);
    if (state->scene == NULL || descriptor == NULL)
    {
        henka_ui_label(state->ui, panel_bounds.x + 14.0f, panel_bounds.y + 38.0f, 1.0f, "Select an object from Scene Objects to inspect it.");
        henka_ui_label(state->ui, panel_bounds.x + 14.0f, panel_bounds.y + 56.0f, 1.0f, "The details panel will show visibility, transform, and object purpose.");
        return;
    }

    result = henka_scene_get_entity_transform(state->scene, descriptor->entity, &transform);
    if (result != HENKA_SUCCESS)
    {
        henka_ui_label(state->ui, panel_bounds.x + 14.0f, panel_bounds.y + 38.0f, 1.0f, "Selected object details could not be read.");
        return;
    }

    visible = henka_scene_is_entity_visible(state->scene, descriptor->entity);
    snprintf(visibility_text, sizeof(visibility_text), "Visible: %s", visible ? "Yes" : "No");
    snprintf(position_text, sizeof(position_text), "Position: %.2f %.2f %.2f", transform.position.x, transform.position.y, transform.position.z);
    snprintf(rotation_text, sizeof(rotation_text), "Rotation: %.2f %.2f %.2f %.2f", transform.rotation.x, transform.rotation.y, transform.rotation.z, transform.rotation.w);
    snprintf(scale_text, sizeof(scale_text), "Scale: %.2f %.2f %.2f", transform.scale.x, transform.scale.y, transform.scale.z);

    sandbox3d_truncate_text(descriptor->short_explanation, detail_text, sizeof(detail_text), 62U);
    sandbox3d_truncate_text(descriptor->developer_detail, developer_text, sizeof(developer_text), 60U);
    sandbox3d_truncate_text(descriptor->mesh_summary, mesh_text, sizeof(mesh_text), 60U);
    sandbox3d_truncate_text(descriptor->material_summary, material_text, sizeof(material_text), 58U);
    sandbox3d_truncate_text(descriptor->texture_summary, texture_text, sizeof(texture_text), 58U);

    henka_ui_label(state->ui, panel_bounds.x + 14.0f, panel_bounds.y + 36.0f, 1.0f, descriptor->display_name);
    henka_ui_label(state->ui, panel_bounds.x + 14.0f, panel_bounds.y + 54.0f, 1.0f, visibility_text);
    henka_ui_label(state->ui, panel_bounds.x + 14.0f, panel_bounds.y + 70.0f, 1.0f, position_text);
    henka_ui_label(state->ui, panel_bounds.x + 14.0f, panel_bounds.y + 86.0f, 1.0f, rotation_text);
    henka_ui_label(state->ui, panel_bounds.x + 14.0f, panel_bounds.y + 102.0f, 1.0f, scale_text);
    snprintf(detail_text, sizeof(detail_text), "Demonstrates: %s", descriptor->short_explanation);
    sandbox3d_truncate_text(detail_text, developer_text, sizeof(developer_text), 64U);
    henka_ui_label(state->ui, panel_bounds.x + 14.0f, panel_bounds.y + 126.0f, 1.0f, developer_text);
    snprintf(detail_text, sizeof(detail_text), "Detail: %s", descriptor->developer_detail);
    sandbox3d_truncate_text(detail_text, developer_text, sizeof(developer_text), 64U);
    henka_ui_label(state->ui, panel_bounds.x + 14.0f, panel_bounds.y + 142.0f, 1.0f, developer_text);
    snprintf(mesh_text, sizeof(mesh_text), "Mesh: %s", descriptor->mesh_summary);
    sandbox3d_truncate_text(mesh_text, developer_text, sizeof(developer_text), 64U);
    henka_ui_label(state->ui, panel_bounds.x + 14.0f, panel_bounds.y + 166.0f, 1.0f, developer_text);
    snprintf(material_text, sizeof(material_text), "Material: %s", descriptor->material_summary);
    sandbox3d_truncate_text(material_text, developer_text, sizeof(developer_text), 64U);
    henka_ui_label(state->ui, panel_bounds.x + 14.0f, panel_bounds.y + 182.0f, 1.0f, developer_text);
    snprintf(texture_text, sizeof(texture_text), "Texture: %s", descriptor->texture_summary);
    sandbox3d_truncate_text(texture_text, developer_text, sizeof(developer_text), 64U);
    henka_ui_label(state->ui, panel_bounds.x + 14.0f, panel_bounds.y + 198.0f, 1.0f, developer_text);
    snprintf(detail_text, sizeof(detail_text), "Entity: %u", (unsigned int)descriptor->entity);
    henka_ui_label(state->ui, panel_bounds.x + 14.0f, panel_bounds.y + 222.0f, 1.0f, detail_text);

    snprintf(action_label, sizeof(action_label), "%s", visible ? "Hide Object" : "Show Object");
    if (henka_ui_button(state->ui, "toggle_selected_visibility", (henka_ui_rect){panel_bounds.x + 14.0f, panel_bounds.y + 252.0f, 136.0f, 32.0f}, action_label))
    {
        if (sandbox3d_toggle_selected_entity_visibility(state))
        {
            printf("%s visibility toggled.\n", descriptor->display_name);
        }
        else
        {
            printf("%s visibility could not be changed.\n", descriptor->display_name);
        }
        fflush(stdout);
    }

    if (henka_ui_button(state->ui, "focus_selected_camera", (henka_ui_rect){panel_bounds.x + 162.0f, panel_bounds.y + 252.0f, 136.0f, 32.0f}, "Focus Camera"))
    {
        if (sandbox3d_focus_camera_on_selected(state))
        {
            printf("Camera focused on %s.\n", descriptor->display_name);
        }
        else
        {
            printf("Camera could not focus on %s.\n", descriptor->display_name);
        }
        fflush(stdout);
    }

    if (henka_ui_button(state->ui, "reset_selected_transform", (henka_ui_rect){panel_bounds.x + 14.0f, panel_bounds.y + 296.0f, 136.0f, 32.0f}, "Reset Transform"))
    {
        if (sandbox3d_reset_selected_entity_transform(state))
        {
            printf("%s reset to its default transform.\n", descriptor->display_name);
        }
        else
        {
            printf("%s could not be reset.\n", descriptor->display_name);
        }
        fflush(stdout);
    }

    if (henka_ui_button(state->ui, "print_selected_info", (henka_ui_rect){panel_bounds.x + 162.0f, panel_bounds.y + 296.0f, 136.0f, 32.0f}, "Print Object Info"))
    {
        sandbox3d_print_selected_object_info(state);
    }

    henka_ui_label(state->ui, panel_bounds.x + 14.0f, panel_bounds.y + 342.0f, 1.0f, "This panel is for safe inspection only.");
    henka_ui_label(state->ui, panel_bounds.x + 14.0f, panel_bounds.y + 358.0f, 1.0f, "Scene saving, drag and drop, and material editing are not available yet.");
}

static void sandbox3d_build_ui(henka_engine* engine, sandbox3d_state* state)
{
    char asset_path_text[128];
    char camera_text[96];
    char capture_text[64];
    char fps_text[56];
    char mouse_text[48];
    char settings_path_text[128];
    char speed_text[48];
    char user_path_text[128];
    char* settings_path;
    float fps;
    float milliseconds;
    henka_result result;
    henka_ui_frame_desc frame_desc;
    sandbox3d_workspace_layout layout;

    if (engine == NULL || state == NULL || state->ui == NULL)
    {
        return;
    }

    frame_desc.framebuffer_width = 0;
    frame_desc.framebuffer_height = 0;
    if (henka_engine_get_framebuffer_size(engine, &frame_desc.framebuffer_width, &frame_desc.framebuffer_height) != HENKA_SUCCESS)
    {
        frame_desc.framebuffer_width = 1280;
        frame_desc.framebuffer_height = 720;
    }
    frame_desc.mouse_position = henka_input_get_mouse_position(engine);
    frame_desc.mouse_left_down = henka_input_is_mouse_button_down(engine, HENKA_MOUSE_BUTTON_LEFT);
    frame_desc.mouse_left_pressed = henka_input_was_mouse_button_pressed(engine, HENKA_MOUSE_BUTTON_LEFT);
    frame_desc.mouse_left_released = henka_input_was_mouse_button_released(engine, HENKA_MOUSE_BUTTON_LEFT);

    if (henka_ui_begin_frame(state->ui, &frame_desc) != HENKA_SUCCESS)
    {
        return;
    }

    if (henka_ui_is_visible(state->ui))
    {
        settings_path = NULL;
        result = sandbox3d_get_settings_path(engine, &settings_path);
        milliseconds = (float)(henka_engine_get_delta_time(engine) * 1000.0);
        fps = milliseconds > 0.0f ? 1000.0f / milliseconds : 0.0f;
        layout = sandbox3d_get_workspace_layout(frame_desc.framebuffer_width, frame_desc.framebuffer_height);

        sandbox3d_format_display_path("Assets", henka_engine_get_asset_base_path(engine), asset_path_text, sizeof(asset_path_text));
        sandbox3d_format_display_path("User", henka_engine_get_user_data_base_path(engine), user_path_text, sizeof(user_path_text));
        sandbox3d_format_display_path("Settings", result == HENKA_SUCCESS ? settings_path : NULL, settings_path_text, sizeof(settings_path_text));
        snprintf(capture_text, sizeof(capture_text), "Capture: %s", henka_engine_is_mouse_captured(engine) ? "On" : "Off");
        snprintf(fps_text, sizeof(fps_text), "Frame: %.2f ms  FPS: %.1f", milliseconds, fps);
        snprintf(mouse_text, sizeof(mouse_text), "Mouse: %.4f", sandbox3d_get_mouse_sensitivity(state));
        snprintf(speed_text, sizeof(speed_text), "Speed: %.1f", state->camera.movement_speed);
        snprintf(
            camera_text,
            sizeof(camera_text),
            "Camera: %.1f %.1f %.1f",
            state->camera.position.x,
            state->camera.position.y,
            state->camera.position.z);

        sandbox3d_draw_controls_panel(
            engine,
            state,
            &layout,
            asset_path_text,
            user_path_text,
            settings_path_text,
            capture_text,
            fps_text,
            camera_text,
            mouse_text,
            speed_text);
        sandbox3d_draw_scene_objects_panel(state, &layout);
        sandbox3d_draw_object_details_panel(state, &layout);

        if (state->ui_visibility_report_pending)
        {
            printf(
                "Sandbox UI ready: framebuffer %dx%d, draw rects %zu, scene panel %s, details panel %s.\n",
                frame_desc.framebuffer_width,
                frame_desc.framebuffer_height,
                henka_ui_get_draw_rect_count(state->ui),
                state->workspace.scene_objects_panel_visible ? "on" : "off",
                state->workspace.object_details_panel_visible ? "on" : "off");
            fflush(stdout);
            state->ui_visibility_report_pending = false;
        }

        henka_free(settings_path);
    }

    henka_ui_end_frame(state->ui);
}

static henka_result sandbox3d_initialize(henka_engine* engine, void* user_data)
{
    henka_asset_manager* assets;
    henka_material colored_material;
    henka_material cube_material;
    henka_material fallback_material;
    henka_material fallback_model_material;
    henka_material grid_material;
    henka_material ground_material;
    henka_material marker_material;
    henka_result result;
    henka_transform transform;
    int framebuffer_height;
    int framebuffer_width;
    sandbox3d_state* state;

    state = (sandbox3d_state*)user_data;

    result = henka_settings_create(&state->settings);
    if (result != HENKA_SUCCESS)
    {
        goto fail;
    }

    result = henka_ui_create(&state->ui);
    if (result != HENKA_SUCCESS)
    {
        goto fail;
    }

    henka_ui_set_visible(state->ui, false);
    sandbox3d_reset_workspace_layout(state);

    result = henka_scene_create(&state->scene);
    if (result != HENKA_SUCCESS)
    {
        goto fail;
    }

    assets = henka_engine_get_asset_manager(engine);
    if (assets == NULL)
    {
        result = HENKA_ERROR_UNKNOWN;
        goto fail;
    }

    result = henka_assets_load_shader(assets, "assets/shaders/basic_lit.vert", "assets/shaders/basic_lit.frag", &state->basic_shader);
    if (result != HENKA_SUCCESS)
    {
        goto fail;
    }

    result = henka_assets_load_shader(assets, "assets/shaders/debug_grid.vert", "assets/shaders/debug_grid.frag", &state->grid_shader);
    if (result != HENKA_SUCCESS)
    {
        goto fail;
    }

    result = henka_assets_load_texture(assets, "assets/textures/cube_albedo.png", &state->cube_texture);
    if (result != HENKA_SUCCESS)
    {
        goto fail;
    }

    result = henka_assets_load_texture(assets, "assets/textures/ground_checker.png", &state->ground_texture);
    if (result != HENKA_SUCCESS)
    {
        goto fail;
    }

    result = henka_assets_load_texture(assets, "assets/textures/missing_texture.png", &state->missing_texture);
    if (result != HENKA_SUCCESS)
    {
        goto fail;
    }

    result = henka_mesh_create_cube(engine, &state->cube_mesh);
    if (result != HENKA_SUCCESS)
    {
        goto fail;
    }

    result = henka_mesh_create_plane(engine, 12.0f, 12.0f, &state->ground_mesh);
    if (result != HENKA_SUCCESS)
    {
        goto fail;
    }

    result = henka_mesh_create_debug_grid(engine, 12, 1.0f, &state->grid_mesh);
    if (result != HENKA_SUCCESS)
    {
        goto fail;
    }

    result = henka_assets_load_obj_mesh(assets, "assets/models/henka_marker.obj", &state->marker_mesh);
    if (result != HENKA_SUCCESS)
    {
        goto fail;
    }

    result = henka_assets_load_obj_mesh(assets, "assets/models/missing_marker.obj", &state->missing_model_mesh);
    if (result != HENKA_SUCCESS)
    {
        goto fail;
    }

    state->ground_entity = henka_scene_create_entity_named(state->scene, "Ground");
    state->cube_entity = henka_scene_create_entity_named(state->scene, "Textured Cube");
    state->colored_cube_entity = henka_scene_create_entity_named(state->scene, "Colored Cube");
    state->marker_entity = henka_scene_create_entity_named(state->scene, "OBJ Marker");
    state->fallback_cube_entity = henka_scene_create_entity_named(state->scene, "Missing Texture");
    state->fallback_model_entity = henka_scene_create_entity_named(state->scene, "Missing Model");
    state->grid_entity = henka_scene_create_entity_named(state->scene, "Debug Grid");

    if (state->ground_entity == HENKA_INVALID_ENTITY ||
        state->cube_entity == HENKA_INVALID_ENTITY ||
        state->colored_cube_entity == HENKA_INVALID_ENTITY ||
        state->marker_entity == HENKA_INVALID_ENTITY ||
        state->fallback_cube_entity == HENKA_INVALID_ENTITY ||
        state->fallback_model_entity == HENKA_INVALID_ENTITY ||
        state->grid_entity == HENKA_INVALID_ENTITY)
    {
        result = HENKA_ERROR_OUT_OF_MEMORY;
        goto fail;
    }

    ground_material = henka_material_default();
    ground_material.shader = state->basic_shader;
    ground_material.base_color_texture = state->ground_texture;
    ground_material.use_texture = true;
    ground_material.base_color = (henka_vec4){0.88f, 0.88f, 0.90f, 1.0f};

    cube_material = henka_material_default();
    cube_material.shader = state->basic_shader;
    cube_material.base_color_texture = state->cube_texture;
    cube_material.use_texture = true;
    cube_material.base_color = (henka_vec4){1.0f, 1.0f, 1.0f, 1.0f};

    colored_material = henka_material_default();
    colored_material.shader = state->basic_shader;
    colored_material.base_color = (henka_vec4){0.20f, 0.72f, 0.56f, 1.0f};
    colored_material.use_texture = false;

    marker_material = henka_material_default();
    marker_material.shader = state->basic_shader;
    marker_material.base_color = (henka_vec4){0.96f, 0.72f, 0.18f, 1.0f};
    marker_material.use_texture = false;

    fallback_material = henka_material_default();
    fallback_material.shader = state->basic_shader;
    fallback_material.base_color_texture = state->missing_texture;
    fallback_material.use_texture = true;
    fallback_material.base_color = (henka_vec4){1.0f, 1.0f, 1.0f, 1.0f};

    fallback_model_material = henka_material_default();
    fallback_model_material.shader = state->basic_shader;
    fallback_model_material.base_color_texture = state->missing_texture;
    fallback_model_material.use_texture = true;
    fallback_model_material.base_color = (henka_vec4){1.0f, 1.0f, 1.0f, 1.0f};

    grid_material = henka_material_default();
    grid_material.shader = state->grid_shader;
    grid_material.base_color = (henka_vec4){0.48f, 0.78f, 0.98f, 1.0f};
    grid_material.use_texture = false;
    grid_material.use_lighting = false;

    transform = sandbox3d_make_transform((henka_vec3){0.0f, -0.02f, 0.0f}, (henka_vec3){1.0f, 1.0f, 1.0f});
    result = sandbox3d_configure_entity(state->scene, state->ground_entity, state->ground_mesh, ground_material, transform);
    if (result != HENKA_SUCCESS)
    {
        goto fail;
    }
    sandbox3d_register_object_descriptor(
        state,
        SANDBOX3D_OBJECT_GROUND,
        state->ground_entity,
        "Ground",
        "Under the scene,",
        "shows repeated local texture use.",
        "Uses a built-in plane mesh with a lit checker texture.",
        "Built-in plane mesh.",
        "Textured lit material.",
        "Ground checker texture from assets/textures/ground_checker.png.",
        true,
        true,
        transform);

    transform = sandbox3d_make_transform(g_textured_cube_position, (henka_vec3){1.0f, 1.0f, 1.0f});
    result = sandbox3d_configure_entity(state->scene, state->cube_entity, state->cube_mesh, cube_material, transform);
    if (result != HENKA_SUCCESS)
    {
        goto fail;
    }
    sandbox3d_register_object_descriptor(
        state,
        SANDBOX3D_OBJECT_TEXTURED_CUBE,
        state->cube_entity,
        "Textured Cube",
        "Near the center,",
        "shows a cube using a texture material.",
        "Uses the built-in cube mesh with a base-color texture.",
        "Built-in cube mesh.",
        "Textured lit material.",
        "Cube albedo texture from assets/textures/cube_albedo.png.",
        true,
        true,
        transform);

    transform = sandbox3d_make_transform(g_colored_cube_position, (henka_vec3){1.0f, 1.0f, 1.0f});
    result = sandbox3d_configure_entity(state->scene, state->colored_cube_entity, state->cube_mesh, colored_material, transform);
    if (result != HENKA_SUCCESS)
    {
        goto fail;
    }
    sandbox3d_register_object_descriptor(
        state,
        SANDBOX3D_OBJECT_COLORED_CUBE,
        state->colored_cube_entity,
        "Colored Cube",
        "Left of center,",
        "shows an untextured material color.",
        "Uses the built-in cube mesh without a base-color texture.",
        "Built-in cube mesh.",
        "Colored lit material.",
        "No texture is used for this object.",
        true,
        true,
        transform);

    transform = sandbox3d_make_transform(g_marker_position, (henka_vec3){1.35f, 1.35f, 1.35f});
    result = sandbox3d_configure_entity(state->scene, state->marker_entity, state->marker_mesh, marker_material, transform);
    if (result != HENKA_SUCCESS)
    {
        goto fail;
    }
    sandbox3d_register_object_descriptor(
        state,
        SANDBOX3D_OBJECT_OBJ_MARKER,
        state->marker_entity,
        "OBJ Marker",
        "Farther left,",
        "shows the current OBJ loading path.",
        "Uses an OBJ mesh loaded through the asset manager cache.",
        "OBJ mesh from assets/models/henka_marker.obj.",
        "Colored lit material.",
        "No texture is used for this object.",
        true,
        true,
        transform);

    transform = sandbox3d_make_transform(g_missing_texture_position, (henka_vec3){1.0f, 1.0f, 1.0f});
    result = sandbox3d_configure_entity(state->scene, state->fallback_cube_entity, state->cube_mesh, fallback_material, transform);
    if (result != HENKA_SUCCESS)
    {
        goto fail;
    }
    sandbox3d_register_object_descriptor(
        state,
        SANDBOX3D_OBJECT_MISSING_TEXTURE,
        state->fallback_cube_entity,
        "Missing Texture",
        "Right of center,",
        "shows the magenta fallback texture when an image cannot be loaded.",
        "Uses the built-in cube mesh with the engine error texture fallback.",
        "Built-in cube mesh.",
        "Textured lit material.",
        "Error texture fallback after a missing texture load.",
        true,
        true,
        transform);

    transform = sandbox3d_make_transform(g_missing_model_position, (henka_vec3){0.95f, 0.95f, 0.95f});
    result = sandbox3d_configure_entity(state->scene, state->fallback_model_entity, state->missing_model_mesh, fallback_model_material, transform);
    if (result != HENKA_SUCCESS)
    {
        goto fail;
    }
    sandbox3d_register_object_descriptor(
        state,
        SANDBOX3D_OBJECT_MISSING_MODEL,
        state->fallback_model_entity,
        "Missing Model",
        "Farther right,",
        "shows the fallback mesh when an OBJ file is missing.",
        "Uses the asset manager fallback mesh and the error texture fallback.",
        "Fallback mesh from the asset manager.",
        "Textured lit material.",
        "Error texture fallback after a missing OBJ load.",
        true,
        true,
        transform);

    transform = sandbox3d_make_transform((henka_vec3){0.0f, 0.0f, 0.0f}, (henka_vec3){1.0f, 1.0f, 1.0f});
    result = sandbox3d_configure_entity(state->scene, state->grid_entity, state->grid_mesh, grid_material, transform);
    if (result != HENKA_SUCCESS)
    {
        goto fail;
    }
    sandbox3d_register_object_descriptor(
        state,
        SANDBOX3D_OBJECT_DEBUG_GRID,
        state->grid_entity,
        "Debug Grid",
        "Across the floor,",
        "helps judge position, depth, and movement.",
        "Uses the built-in debug grid mesh with an unlit line material.",
        "Built-in debug grid mesh.",
        "Unlit grid material.",
        "No texture is used for this object.",
        true,
        true,
        transform);

    henka_scene_set_light_direction(state->scene, (henka_vec3){-0.5f, -1.0f, -0.3f});
    henka_scene_set_ambient_color(state->scene, (henka_vec3){0.18f, 0.20f, 0.25f});

    result = henka_engine_get_framebuffer_size(engine, &framebuffer_width, &framebuffer_height);
    if (result != HENKA_SUCCESS || framebuffer_height <= 0)
    {
        framebuffer_width = 1280;
        framebuffer_height = 720;
    }

    state->camera = henka_camera_create_perspective(60.0f * HENKA_DEG_TO_RAD, (float)framebuffer_width / (float)framebuffer_height, 0.1f, 100.0f);
    sandbox3d_reset_camera_defaults(state);

    {
        FILE* settings_file;
        char* settings_path;
        henka_result load_result;

        settings_path = NULL;
        result = sandbox3d_get_settings_path(engine, &settings_path);
        if (result == HENKA_SUCCESS)
        {
            settings_file = NULL;
            if (fopen_s(&settings_file, settings_path, "r") == 0 && settings_file != NULL)
            {
                state->settings_file_found = true;
                fclose(settings_file);

                load_result = henka_settings_load_file(state->settings, settings_path);
                if (load_result == HENKA_ERROR_UNKNOWN)
                {
                    HENKA_LOG_WARN("Sandbox settings were loaded with one or more invalid lines. Defaults were kept for the invalid values.");
                }
                else if (load_result != HENKA_SUCCESS)
                {
                    HENKA_LOG_WARN("Sandbox settings were not loaded. The sandbox is using first-run defaults.");
                }
            }

            henka_free(settings_path);
        }
        else
        {
            HENKA_LOG_WARN("Sandbox settings path could not be resolved. The sandbox is using first-run defaults.");
        }
    }

    result = henka_scene_set_camera(state->scene, &state->camera);
    if (result != HENKA_SUCCESS)
    {
        goto fail;
    }

    result = henka_engine_set_scene(engine, state->scene);
    if (result != HENKA_SUCCESS)
    {
        goto fail;
    }

    result = henka_engine_set_ui_context(engine, state->ui);
    if (result != HENKA_SUCCESS)
    {
        goto fail;
    }

    sandbox3d_apply_loaded_settings(engine, state);
    state->startup_panels_auto_opened = !state->settings_file_found;
    if (state->startup_panels_auto_opened)
    {
        henka_ui_set_visible(state->ui, true);
        state->ui_visibility_report_pending = true;
    }

    result = henka_engine_set_mouse_capture(engine, false);
    if (result != HENKA_SUCCESS)
    {
        goto fail;
    }

    sandbox3d_print_help(state);
    sandbox3d_print_startup_ui_cue(state);
    return HENKA_SUCCESS;

fail:
    sandbox3d_release_owned_resources(state);
    return result;
}

static void sandbox3d_update(henka_engine* engine, double delta_seconds, void* user_data)
{
    henka_vec2 mouse_delta;
    bool ui_toggled_with_f4;
    bool ui_visible;
    int framebuffer_height;
    int framebuffer_width;
    sandbox3d_state* state;

    state = (sandbox3d_state*)user_data;
    ui_toggled_with_f4 = false;

    if (henka_input_was_key_pressed(engine, HENKA_KEY_H))
    {
        sandbox3d_print_help(state);
    }

    if (henka_input_was_key_pressed(engine, HENKA_KEY_F4) && state->ui != NULL)
    {
        ui_visible = !henka_ui_is_visible(state->ui);
        henka_ui_set_visible(state->ui, ui_visible);
        if (ui_visible)
        {
            henka_engine_set_mouse_capture(engine, false);
            state->ui_visibility_report_pending = true;
        }
        sandbox3d_print_ui_state(ui_visible);
        ui_toggled_with_f4 = true;
    }

    if (henka_input_was_key_pressed(engine, HENKA_KEY_F2))
    {
        sandbox3d_print_scene_legend(state);
        fflush(stdout);
    }

    if (henka_input_was_key_pressed(engine, HENKA_KEY_F1))
    {
        sandbox3d_toggle_wireframe(engine, !henka_engine_is_wireframe_enabled(engine), true);
    }

    if (henka_input_was_key_pressed(engine, HENKA_KEY_F3))
    {
        sandbox3d_toggle_grid_visibility(state, !henka_scene_is_entity_visible(state->scene, state->grid_entity), true);
    }

    ui_visible = state->ui != NULL && henka_ui_is_visible(state->ui);
    if (!ui_toggled_with_f4 && state->ui_visible_last_frame && !ui_visible)
    {
        sandbox3d_print_ui_state(false);
    }

    if (!ui_visible && henka_input_was_key_pressed(engine, HENKA_KEY_TAB))
    {
        henka_engine_set_mouse_capture(engine, !henka_engine_is_mouse_captured(engine));
        sandbox3d_print_capture_state(engine, "Tab");
    }

    if (!ui_visible && henka_input_was_mouse_button_pressed(engine, HENKA_MOUSE_BUTTON_RIGHT))
    {
        henka_engine_set_mouse_capture(engine, !henka_engine_is_mouse_captured(engine));
        sandbox3d_print_capture_state(engine, "Right Mouse");
    }

    if (henka_engine_get_framebuffer_size(engine, &framebuffer_width, &framebuffer_height) == HENKA_SUCCESS && framebuffer_height > 0)
    {
        henka_camera_set_aspect_ratio(&state->camera, (float)framebuffer_width / (float)framebuffer_height);
    }

    if (!ui_visible && henka_engine_is_mouse_captured(engine))
    {
        mouse_delta = henka_input_get_mouse_delta(engine);
        henka_camera_apply_mouse_look(
            &state->camera,
            -mouse_delta.x * sandbox3d_get_mouse_sensitivity(state),
            -mouse_delta.y * sandbox3d_get_mouse_sensitivity(state));
    }

    if (!ui_visible)
    {
        henka_camera_move_fly(&state->camera, engine, delta_seconds);
    }

    henka_scene_set_camera(state->scene, &state->camera);
    sandbox3d_build_ui(engine, state);
    state->ui_visible_last_frame = ui_visible;
}

static void sandbox3d_shutdown(henka_engine* engine, void* user_data)
{
    sandbox3d_state* state;

    state = (sandbox3d_state*)user_data;
    sandbox3d_save_settings(engine, state);
    henka_engine_set_mouse_capture(engine, false);
    henka_engine_set_scene(engine, NULL);
    henka_engine_set_ui_context(engine, NULL);
    sandbox3d_release_owned_resources(state);
}

int main(void)
{
    henka_engine* engine;
    henka_engine_config config;
    henka_result result;
    sandbox3d_state state;
    size_t index;

    memset(&state, 0, sizeof(state));
    state.camera = henka_camera_create_perspective(60.0f * HENKA_DEG_TO_RAD, 16.0f / 9.0f, 0.1f, 100.0f);
    state.cube_entity = HENKA_INVALID_ENTITY;
    state.ground_entity = HENKA_INVALID_ENTITY;
    state.grid_entity = HENKA_INVALID_ENTITY;
    state.colored_cube_entity = HENKA_INVALID_ENTITY;
    state.fallback_cube_entity = HENKA_INVALID_ENTITY;
    state.marker_entity = HENKA_INVALID_ENTITY;
    state.fallback_model_entity = HENKA_INVALID_ENTITY;
    state.selected_entity = HENKA_INVALID_ENTITY;
    state.ui_visible_last_frame = false;
    sandbox3d_reset_workspace_layout(&state);

    for (index = 0U; index < SANDBOX3D_OBJECT_COUNT; ++index)
    {
        state.descriptors[index].kind = (sandbox3d_object_kind)index;
        state.descriptors[index].entity = HENKA_INVALID_ENTITY;
    }

    config.application_name = "Henka Engine Sandbox 3D";
    config.window_width = 1280;
    config.window_height = 720;
    config.enable_vsync = true;
    config.asset_base_path = NULL;
    config.user_data_base_path = NULL;
    config.on_initialize = sandbox3d_initialize;
    config.on_update = sandbox3d_update;
    config.on_shutdown = sandbox3d_shutdown;
    config.user_data = &state;

    result = henka_engine_create(&config, &engine);
    if (result != HENKA_SUCCESS)
    {
        HENKA_LOG_ERROR("Unable to start the sandbox: %s", henka_result_to_string(result));
        return 1;
    }

    result = henka_engine_run(engine);
    if (result != HENKA_SUCCESS)
    {
        HENKA_LOG_ERROR("The sandbox stopped with an error: %s", henka_result_to_string(result));
        henka_engine_destroy(engine);
        return 1;
    }

    henka_engine_destroy(engine);
    return 0;
}
