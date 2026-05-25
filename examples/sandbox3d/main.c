#include <math.h>
#include <stdio.h>
#include <stdarg.h>
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

typedef enum sandbox3d_layout_mode
{
    SANDBOX3D_LAYOUT_VIEW = 0,
    SANDBOX3D_LAYOUT_INSPECT,
    SANDBOX3D_LAYOUT_FULL,
    SANDBOX3D_LAYOUT_COUNT
} sandbox3d_layout_mode;

typedef enum sandbox3d_utility_view
{
    SANDBOX3D_UTILITY_NONE = 0,
    SANDBOX3D_UTILITY_HELP,
    SANDBOX3D_UTILITY_SCENE_LEGEND,
    SANDBOX3D_UTILITY_OBJECT_INFO,
    SANDBOX3D_UTILITY_PATHS,
    SANDBOX3D_UTILITY_SETTINGS,
    SANDBOX3D_UTILITY_DIAGNOSTICS,
    SANDBOX3D_UTILITY_COUNT
} sandbox3d_utility_view;

typedef struct sandbox3d_workspace_state
{
    sandbox3d_layout_mode layout_mode;
    bool scene_objects_panel_visible;
    bool object_details_panel_visible;
    sandbox3d_utility_view active_utility;
} sandbox3d_workspace_state;

typedef struct sandbox3d_workspace_layout
{
    henka_ui_rect controls_panel;
    henka_ui_rect scene_objects_panel;
    henka_ui_rect object_details_panel;
    henka_ui_rect utility_panel;
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
    bool status_warning;
    char status_message[160];
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
static const float g_ui_controls_width = 300.0f;
static const float g_ui_scene_width = 250.0f;
static const float g_ui_details_width = 340.0f;
static const float g_ui_panel_height = 360.0f;

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
static const char* g_setting_key_layout_mode = "ui.layout_mode";
static const char* g_setting_key_active_utility = "ui.active_utility";

static float sandbox3d_get_mouse_sensitivity(const sandbox3d_state* state);
static void sandbox3d_set_status(sandbox3d_state* state, bool warning, const char* message);
static void sandbox3d_set_statusf(sandbox3d_state* state, bool warning, bool print_console, const char* format, ...);

static const char* sandbox3d_get_build_configuration_label(void)
{
#if defined(_DEBUG)
    return "Debug";
#else
    return "Release";
#endif
}

static const char* sandbox3d_get_layout_mode_label(sandbox3d_layout_mode layout_mode)
{
    switch (layout_mode)
    {
        case SANDBOX3D_LAYOUT_VIEW:
            return "View";
        case SANDBOX3D_LAYOUT_INSPECT:
            return "Inspect";
        case SANDBOX3D_LAYOUT_FULL:
            return "Full Tools";
        default:
            return "View";
    }
}

static const char* sandbox3d_get_layout_mode_setting_value(sandbox3d_layout_mode layout_mode)
{
    switch (layout_mode)
    {
        case SANDBOX3D_LAYOUT_VIEW:
            return "view";
        case SANDBOX3D_LAYOUT_INSPECT:
            return "inspect";
        case SANDBOX3D_LAYOUT_FULL:
            return "full";
        default:
            return "view";
    }
}

static sandbox3d_layout_mode sandbox3d_parse_layout_mode(const char* value)
{
    if (value == NULL || value[0] == '\0')
    {
        return SANDBOX3D_LAYOUT_VIEW;
    }

    if (strcmp(value, "view") == 0)
    {
        return SANDBOX3D_LAYOUT_VIEW;
    }
    if (strcmp(value, "inspect") == 0)
    {
        return SANDBOX3D_LAYOUT_INSPECT;
    }
    if (strcmp(value, "full") == 0)
    {
        return SANDBOX3D_LAYOUT_FULL;
    }

    return SANDBOX3D_LAYOUT_VIEW;
}

static const char* sandbox3d_get_utility_label(sandbox3d_utility_view utility)
{
    switch (utility)
    {
        case SANDBOX3D_UTILITY_HELP:
            return "Help";
        case SANDBOX3D_UTILITY_SCENE_LEGEND:
            return "Scene Legend";
        case SANDBOX3D_UTILITY_OBJECT_INFO:
            return "Object Info";
        case SANDBOX3D_UTILITY_PATHS:
            return "Paths";
        case SANDBOX3D_UTILITY_SETTINGS:
            return "Settings";
        case SANDBOX3D_UTILITY_DIAGNOSTICS:
            return "Diagnostics";
        case SANDBOX3D_UTILITY_NONE:
        default:
            return "None";
    }
}

static const char* sandbox3d_get_utility_setting_value(sandbox3d_utility_view utility)
{
    switch (utility)
    {
        case SANDBOX3D_UTILITY_HELP:
            return "help";
        case SANDBOX3D_UTILITY_SCENE_LEGEND:
            return "scene_legend";
        case SANDBOX3D_UTILITY_OBJECT_INFO:
            return "object_info";
        case SANDBOX3D_UTILITY_PATHS:
            return "paths";
        case SANDBOX3D_UTILITY_SETTINGS:
            return "settings";
        case SANDBOX3D_UTILITY_DIAGNOSTICS:
            return "diagnostics";
        case SANDBOX3D_UTILITY_NONE:
        default:
            return "none";
    }
}

static sandbox3d_utility_view sandbox3d_parse_utility_view(const char* value)
{
    if (value == NULL || value[0] == '\0')
    {
        return SANDBOX3D_UTILITY_NONE;
    }

    if (strcmp(value, "help") == 0)
    {
        return SANDBOX3D_UTILITY_HELP;
    }
    if (strcmp(value, "scene_legend") == 0)
    {
        return SANDBOX3D_UTILITY_SCENE_LEGEND;
    }
    if (strcmp(value, "object_info") == 0)
    {
        return SANDBOX3D_UTILITY_OBJECT_INFO;
    }
    if (strcmp(value, "paths") == 0)
    {
        return SANDBOX3D_UTILITY_PATHS;
    }
    if (strcmp(value, "settings") == 0)
    {
        return SANDBOX3D_UTILITY_SETTINGS;
    }
    if (strcmp(value, "diagnostics") == 0)
    {
        return SANDBOX3D_UTILITY_DIAGNOSTICS;
    }

    return SANDBOX3D_UTILITY_NONE;
}

static sandbox3d_layout_mode sandbox3d_cycle_layout_mode(sandbox3d_layout_mode layout_mode)
{
    switch (layout_mode)
    {
        case SANDBOX3D_LAYOUT_VIEW:
            return SANDBOX3D_LAYOUT_INSPECT;
        case SANDBOX3D_LAYOUT_INSPECT:
            return SANDBOX3D_LAYOUT_FULL;
        case SANDBOX3D_LAYOUT_FULL:
        default:
            return SANDBOX3D_LAYOUT_VIEW;
    }
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

static henka_bounds sandbox3d_make_bounds(henka_vec3 center, henka_vec3 extents)
{
    henka_bounds bounds;

    bounds.center = center;
    bounds.extents = extents;
    return bounds;
}

static void sandbox3d_reset_workspace_layout(sandbox3d_state* state)
{
    if (state == NULL)
    {
        return;
    }

    state->workspace.layout_mode = SANDBOX3D_LAYOUT_VIEW;
    state->workspace.scene_objects_panel_visible = true;
    state->workspace.object_details_panel_visible = true;
    state->workspace.active_utility = SANDBOX3D_UTILITY_NONE;
    sandbox3d_set_status(state, false, "View mode keeps the scene open while tools stay nearby.");
}

static bool sandbox3d_workspace_shows_scene_panel(const sandbox3d_state* state)
{
    return state != NULL &&
        state->workspace.layout_mode != SANDBOX3D_LAYOUT_VIEW &&
        state->workspace.scene_objects_panel_visible;
}

static bool sandbox3d_workspace_shows_details_panel(const sandbox3d_state* state)
{
    return state != NULL &&
        state->workspace.layout_mode != SANDBOX3D_LAYOUT_VIEW &&
        state->workspace.object_details_panel_visible;
}

static bool sandbox3d_workspace_shows_utility_panel(const sandbox3d_state* state)
{
    return state != NULL && state->workspace.active_utility != SANDBOX3D_UTILITY_NONE;
}

static void sandbox3d_set_active_utility(sandbox3d_state* state, sandbox3d_utility_view utility)
{
    if (state == NULL)
    {
        return;
    }

    state->workspace.active_utility = utility;
}

static void sandbox3d_set_status(sandbox3d_state* state, bool warning, const char* message)
{
    if (state == NULL)
    {
        return;
    }

    state->status_warning = warning;
    if (message == NULL || message[0] == '\0')
    {
        snprintf(state->status_message, sizeof(state->status_message), "Ready.");
        return;
    }

    snprintf(state->status_message, sizeof(state->status_message), "%s", message);
}

static void sandbox3d_set_statusf(sandbox3d_state* state, bool warning, bool print_console, const char* format, ...)
{
    char buffer[160];
    va_list args;

    if (format == NULL)
    {
        return;
    }

    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    sandbox3d_set_status(state, warning, buffer);
    if (print_console)
    {
        printf("%s\n", buffer);
        fflush(stdout);
    }
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

static const char* sandbox3d_get_descriptor_role_label(const sandbox3d_object_descriptor* descriptor)
{
    if (descriptor == NULL)
    {
        return "Object";
    }

    switch (descriptor->kind)
    {
        case SANDBOX3D_OBJECT_GROUND:
            return "Ground";
        case SANDBOX3D_OBJECT_TEXTURED_CUBE:
            return "Textured";
        case SANDBOX3D_OBJECT_COLORED_CUBE:
            return "Colored";
        case SANDBOX3D_OBJECT_OBJ_MARKER:
            return "OBJ";
        case SANDBOX3D_OBJECT_MISSING_TEXTURE:
            return "Fallback";
        case SANDBOX3D_OBJECT_MISSING_MODEL:
            return "Fallback";
        case SANDBOX3D_OBJECT_DEBUG_GRID:
            return "Grid";
        default:
            return "Object";
    }
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

static void sandbox3d_print_layout_mode(const sandbox3d_state* state, bool include_hint)
{
    if (state == NULL)
    {
        return;
    }

    printf("Sandbox layout: %s\n", sandbox3d_get_layout_mode_label(state->workspace.layout_mode));
    if (include_hint)
    {
        printf("Layout help: F5 cycles View, Inspect, and Full Tools.\n");
    }
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
        printf("Startup UI: the in-window panels start open in View mode on first run so the scene stays visible.\n");
        printf("Startup UI: press F4 to hide the panels and press F5 to switch to Inspect or Full Tools.\n");
        printf("Startup UI: use the in-window Help, Legend, Paths, Settings, and Diagnostics utilities for normal inspection.\n");
        printf("Startup UI: recent actions and warnings appear in the Controls panel so normal use does not depend on the console.\n");
    }
    else
    {
        printf("Startup UI: press F4 to open the in-window panels and press F5 to cycle layout modes.\n");
        printf("Startup UI: use the in-window utilities for help, scene legend, paths, settings, and diagnostics.\n");
        printf("Startup UI: recent actions and warnings appear in-window while the console stays available for fallback logs.\n");
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
    printf("  Left Mouse       Pick a visible object when the UI is closed and capture is released\n");
    printf("  F1               Toggle wireframe\n");
    printf("  F2               Print the scene legend again\n");
    printf("  F3               Show or hide the debug grid\n");
    printf("  F4               Show or hide the sandbox panels\n");
    printf("  F5               Cycle View, Inspect, and Full Tools layouts\n");
    printf("  H                Print controls and the scene legend again\n");
    printf("  Escape           Close the panels first. Then release the mouse. Then exit.\n");
    printf("Panel shortcuts:\n");
    printf("  Press F4 to open the in-window panels.\n");
    printf("  View keeps most of the scene open. Inspect adds object panels. Full Tools shows the full workspace.\n");
    printf("  Use the panels to inspect named scene objects, focus the camera, reset object transforms, toggle visibility, and open in-window Help, Scene Legend, Object Info, Paths, Settings, and Diagnostics utilities.\n");
    printf("  Common actions also report short in-window status messages. Console output stays available for fallback logs.\n");
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

static void sandbox3d_apply_entity_foundation(
    sandbox3d_state* state,
    henka_entity entity,
    const char* tag,
    henka_bounds bounds,
    bool interactable,
    const char* prompt)
{
    henka_interaction_desc interaction;

    if (state == NULL || state->scene == NULL || entity == HENKA_INVALID_ENTITY)
    {
        return;
    }

    henka_scene_set_entity_tag(state->scene, entity, tag);
    henka_scene_set_entity_local_bounds(state->scene, entity, bounds);

    interaction.enabled = interactable;
    interaction.max_distance = interactable ? 6.0f : 0.0f;
    interaction.prompt = prompt;
    henka_scene_set_entity_interaction(state->scene, entity, &interaction);
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
    henka_camera defaults;

    if (state == NULL)
    {
        return;
    }

    defaults = state->camera;
    defaults.position = g_camera_start_position;
    defaults.yaw_radians = g_camera_start_yaw;
    defaults.pitch_radians = g_camera_start_pitch;
    defaults.movement_speed = g_default_camera_movement_speed;
    defaults.fast_movement_multiplier = 2.5f;
    henka_camera_reset(&state->camera, &defaults);
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
    sandbox3d_set_statusf(state, false, false, "Mouse sensitivity set to %.4f.", next_value);
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
    sandbox3d_set_statusf(state, false, false, "Camera speed set to %.1f.", next_value);
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
    const sandbox3d_object_descriptor* descriptor;

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

    descriptor = sandbox3d_get_descriptor_by_entity(state, state->selected_entity);
    if (descriptor != NULL)
    {
        sandbox3d_set_statusf(state, false, false, "Selected %s.", descriptor->display_name);
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
    const char* layout_mode_value;
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

    layout_mode_value = henka_settings_get_string(state->settings, g_setting_key_layout_mode, "view");
    state->workspace.layout_mode = sandbox3d_parse_layout_mode(layout_mode_value);
    state->workspace.scene_objects_panel_visible = henka_settings_get_bool(state->settings, g_setting_key_scene_panel_visible, true);
    state->workspace.object_details_panel_visible = henka_settings_get_bool(state->settings, g_setting_key_details_panel_visible, true);
    state->workspace.active_utility = sandbox3d_parse_utility_view(
        henka_settings_get_string(state->settings, g_setting_key_active_utility, "none"));
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
    henka_settings_set_string(state->settings, g_setting_key_layout_mode, sandbox3d_get_layout_mode_setting_value(state->workspace.layout_mode));
    henka_settings_set_bool(state->settings, g_setting_key_scene_panel_visible, state->workspace.scene_objects_panel_visible);
    henka_settings_set_bool(state->settings, g_setting_key_details_panel_visible, state->workspace.object_details_panel_visible);
    henka_settings_set_string(state->settings, g_setting_key_active_utility, sandbox3d_get_utility_setting_value(state->workspace.active_utility));

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
        sandbox3d_set_statusf(state, false, true, "Grid %s.", visible ? "shown" : "hidden");
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
    henka_bounds bounds;

    if (state == NULL || state->scene == NULL)
    {
        return false;
    }

    descriptor = sandbox3d_get_descriptor_by_entity(state, state->selected_entity);
    if (descriptor == NULL)
    {
        return false;
    }

    if (henka_scene_get_entity_world_bounds(state->scene, descriptor->entity, &bounds) == HENKA_SUCCESS)
    {
        return henka_camera_focus_on_bounds(&state->camera, bounds);
    }

    return false;
}

static void sandbox3d_try_pick_object(henka_engine* engine, sandbox3d_state* state)
{
    henka_entity picked_entity;
    henka_ray ray;
    float distance;
    int framebuffer_height;
    int framebuffer_width;

    if (engine == NULL || state == NULL || state->scene == NULL)
    {
        return;
    }

    if (henka_engine_get_framebuffer_size(engine, &framebuffer_width, &framebuffer_height) != HENKA_SUCCESS)
    {
        return;
    }

    if (henka_camera_screen_point_to_ray(
            &state->camera,
            framebuffer_width,
            framebuffer_height,
            henka_input_get_mouse_position(engine),
            &ray) != HENKA_SUCCESS)
    {
        return;
    }

    if (henka_scene_pick_entity(state->scene, ray, &picked_entity, &distance) == HENKA_SUCCESS)
    {
        sandbox3d_select_entity(state, picked_entity);
        sandbox3d_set_statusf(state, false, false, "Picked %s.", sandbox3d_safe_entity_name(state, picked_entity, "object"));
    }
}

static void sandbox3d_print_selected_object_info(const sandbox3d_state* state)
{
    char material_summary[128];
    bool visible;
    const sandbox3d_object_descriptor* descriptor;
    henka_interaction_desc interaction;
    henka_material material;
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
    if (henka_scene_get_entity_material(state->scene, descriptor->entity, &material) != HENKA_SUCCESS)
    {
        material = henka_material_default();
    }
    if (henka_material_describe(&material, material_summary, sizeof(material_summary)) != HENKA_SUCCESS)
    {
        snprintf(material_summary, sizeof(material_summary), "%s", descriptor->material_summary);
    }
    if (henka_scene_get_entity_interaction(state->scene, descriptor->entity, &interaction) != HENKA_SUCCESS)
    {
        interaction = (henka_interaction_desc){false, 0.0f, NULL};
    }

    printf("Object Info\n");
    printf("  Name: %s\n", descriptor->display_name);
    printf("  Entity: %u\n", (unsigned int)descriptor->entity);
    printf("  Tag: %s\n", henka_scene_get_entity_tag(state->scene, descriptor->entity) != NULL ? henka_scene_get_entity_tag(state->scene, descriptor->entity) : "(none)");
    printf("  Visible: %s\n", visible ? "Yes" : "No");
    printf("  Position: %.2f %.2f %.2f\n", transform.position.x, transform.position.y, transform.position.z);
    printf("  Rotation: %.2f %.2f %.2f %.2f\n", transform.rotation.x, transform.rotation.y, transform.rotation.z, transform.rotation.w);
    printf("  Scale: %.2f %.2f %.2f\n", transform.scale.x, transform.scale.y, transform.scale.z);
    printf("  Demonstrates: %s\n", descriptor->short_explanation);
    printf("  Detail: %s\n", descriptor->developer_detail);
    printf("  Mesh: %s\n", descriptor->mesh_summary);
    printf("  Material: %s\n", material_summary);
    printf("  Texture: %s\n", descriptor->texture_summary);
    printf("  Interaction: %s\n", interaction.enabled ? (interaction.prompt != NULL ? interaction.prompt : "Available") : "Disabled");
    fflush(stdout);
}

static sandbox3d_workspace_layout sandbox3d_get_workspace_layout(
    const sandbox3d_state* state,
    int framebuffer_width,
    int framebuffer_height)
{
    float available_height;
    float controls_width;
    float details_width;
    float scene_width;
    float utility_width;
    sandbox3d_workspace_layout layout;
    sandbox3d_layout_mode layout_mode;

    layout.controls_panel = (henka_ui_rect){g_ui_panel_margin, g_ui_panel_margin, g_ui_controls_width, g_ui_panel_height};
    layout.scene_objects_panel = (henka_ui_rect){g_ui_panel_margin, g_ui_panel_margin, g_ui_scene_width, g_ui_panel_height};
    layout.object_details_panel = (henka_ui_rect){g_ui_panel_margin, g_ui_panel_margin, g_ui_details_width, g_ui_panel_height};
    layout.utility_panel = (henka_ui_rect){g_ui_panel_margin, g_ui_panel_margin, g_ui_details_width, 228.0f};

    if (framebuffer_width <= 0 || framebuffer_height <= 0)
    {
        return layout;
    }

    layout_mode = state != NULL ? state->workspace.layout_mode : SANDBOX3D_LAYOUT_VIEW;
    available_height = (float)framebuffer_height - g_ui_panel_margin * 2.0f;
    if (available_height < 240.0f)
    {
        available_height = 240.0f;
    }

    if (layout_mode == SANDBOX3D_LAYOUT_VIEW)
    {
        layout.controls_panel.x = g_ui_panel_margin;
        layout.controls_panel.y = g_ui_panel_margin;
        layout.controls_panel.width = framebuffer_width >= 1024 ? 300.0f : (framebuffer_width >= 760 ? 278.0f : (float)framebuffer_width - g_ui_panel_margin * 2.0f);
        if (layout.controls_panel.width < 240.0f)
        {
            layout.controls_panel.width = 240.0f;
        }
        layout.controls_panel.height = framebuffer_width >= 760 ? 404.0f : 388.0f;
        layout.utility_panel.x = g_ui_panel_margin;
        layout.utility_panel.y = layout.controls_panel.y + layout.controls_panel.height + g_ui_panel_gap;
        layout.utility_panel.width = layout.controls_panel.width;
        layout.utility_panel.height = 220.0f;
        return layout;
    }

    if (layout_mode == SANDBOX3D_LAYOUT_INSPECT)
    {
        controls_width = framebuffer_width >= 1100 ? 280.0f : 260.0f;
        scene_width = framebuffer_width >= 1100 ? 280.0f : 250.0f;
        details_width = framebuffer_width >= 1200 ? 336.0f : 304.0f;
        utility_width = details_width;

        layout.controls_panel.x = g_ui_panel_margin;
        layout.controls_panel.y = g_ui_panel_margin;
        layout.controls_panel.width = controls_width;
        layout.controls_panel.height = 432.0f;

        layout.scene_objects_panel.x = g_ui_panel_margin;
        layout.scene_objects_panel.y = layout.controls_panel.y + layout.controls_panel.height + g_ui_panel_gap;
        layout.scene_objects_panel.width = scene_width;
        layout.scene_objects_panel.height = available_height - layout.controls_panel.height - g_ui_panel_gap;
        if (layout.scene_objects_panel.height < 208.0f)
        {
            layout.scene_objects_panel.height = 208.0f;
        }

        layout.object_details_panel.width = details_width;
        layout.object_details_panel.height = 420.0f;
        if (layout.object_details_panel.height > available_height)
        {
            layout.object_details_panel.height = available_height;
        }
        layout.object_details_panel.x = (float)framebuffer_width - g_ui_panel_margin - layout.object_details_panel.width;
        layout.object_details_panel.y = g_ui_panel_margin;
        if (layout.object_details_panel.x < layout.scene_objects_panel.x + layout.scene_objects_panel.width + g_ui_panel_gap)
        {
            layout.object_details_panel.x = layout.scene_objects_panel.x + layout.scene_objects_panel.width + g_ui_panel_gap;
        }

        layout.utility_panel.width = utility_width;
        layout.utility_panel.height = available_height - layout.object_details_panel.height - g_ui_panel_gap;
        if (layout.utility_panel.height < 224.0f)
        {
            layout.utility_panel.height = 224.0f;
        }
        layout.utility_panel.x = layout.object_details_panel.x;
        layout.utility_panel.y = layout.object_details_panel.y + layout.object_details_panel.height + g_ui_panel_gap;

        return layout;
    }

    if (framebuffer_width >= 980)
    {
        layout.controls_panel.x = g_ui_panel_margin;
        layout.controls_panel.y = g_ui_panel_margin;
        layout.controls_panel.width = 300.0f;
        layout.controls_panel.height = available_height;

        layout.scene_objects_panel.x = g_ui_panel_margin;
        layout.scene_objects_panel.y = g_ui_panel_margin;
        layout.scene_objects_panel.width = 240.0f;
        layout.controls_panel.height = 460.0f;
        layout.scene_objects_panel.height = available_height - layout.controls_panel.height - g_ui_panel_gap;
        if (layout.scene_objects_panel.height < 208.0f)
        {
            layout.scene_objects_panel.height = 208.0f;
        }

        layout.object_details_panel.width = 340.0f;
        layout.object_details_panel.height = 430.0f;
        layout.object_details_panel.x = (float)framebuffer_width - g_ui_panel_margin - layout.object_details_panel.width;
        layout.object_details_panel.y = g_ui_panel_margin;

        layout.utility_panel.width = layout.object_details_panel.width;
        layout.utility_panel.height = available_height - layout.object_details_panel.height - g_ui_panel_gap;
        if (layout.utility_panel.height < 220.0f)
        {
            layout.utility_panel.height = 220.0f;
        }
        layout.utility_panel.x = layout.object_details_panel.x;
        layout.utility_panel.y = layout.object_details_panel.y + layout.object_details_panel.height + g_ui_panel_gap;

        layout.controls_panel.y = layout.scene_objects_panel.y + layout.scene_objects_panel.height + g_ui_panel_gap;
        layout.controls_panel.height = available_height - layout.scene_objects_panel.height - g_ui_panel_gap;
        if (layout.controls_panel.height < 360.0f)
        {
            layout.controls_panel.height = 360.0f;
            layout.controls_panel.y = g_ui_panel_margin;
        }
    }
    else
    {
        layout.controls_panel.x = g_ui_panel_margin;
        layout.controls_panel.y = g_ui_panel_margin;
        layout.controls_panel.width = (float)framebuffer_width - g_ui_panel_margin * 2.0f;
        if (layout.controls_panel.width < 320.0f)
        {
            layout.controls_panel.width = 320.0f;
        }
        layout.controls_panel.height = 216.0f;

        layout.scene_objects_panel.x = g_ui_panel_margin;
        layout.scene_objects_panel.y = layout.controls_panel.y + layout.controls_panel.height + g_ui_panel_gap;
        layout.scene_objects_panel.width = layout.controls_panel.width;
        layout.scene_objects_panel.height = 170.0f;

        layout.object_details_panel.x = g_ui_panel_margin;
        layout.object_details_panel.y = layout.scene_objects_panel.y + layout.scene_objects_panel.height + g_ui_panel_gap;
        layout.object_details_panel.width = layout.controls_panel.width;
        layout.object_details_panel.height = 190.0f;
        if (layout.object_details_panel.height < 150.0f)
        {
            layout.object_details_panel.height = 150.0f;
        }
        layout.utility_panel.x = g_ui_panel_margin;
        layout.utility_panel.y = layout.object_details_panel.y + layout.object_details_panel.height + g_ui_panel_gap;
        layout.utility_panel.width = layout.controls_panel.width;
        layout.utility_panel.height = available_height - layout.controls_panel.height - layout.scene_objects_panel.height - layout.object_details_panel.height - g_ui_panel_gap * 3.0f;
        if (layout.utility_panel.height < 150.0f)
        {
            layout.utility_panel.height = 150.0f;
        }
    }

    return layout;
}

static void sandbox3d_draw_section_heading(henka_ui_context* ui, float x, float y, const char* title)
{
    if (ui == NULL || title == NULL)
    {
        return;
    }

    henka_ui_heading(ui, x, y, 1.0f, title);
}

static void sandbox3d_draw_value_row(
    henka_ui_context* ui,
    float x,
    float y,
    float width,
    const char* label,
    const char* value)
{
    if (ui == NULL || label == NULL || value == NULL)
    {
        return;
    }

    henka_ui_value_row(ui, (henka_ui_rect){x, y, width, 22.0f}, label, value);
}

static void sandbox3d_draw_status_block(
    const sandbox3d_state* state,
    float x,
    float y,
    float width,
    bool compact)
{
    char layout_row[48];
    const sandbox3d_object_descriptor* descriptor;
    const char* status_text;

    if (state == NULL || state->ui == NULL)
    {
        return;
    }

    status_text = state->status_message[0] != '\0' ? state->status_message : "Ready.";
    descriptor = sandbox3d_get_descriptor_by_entity(state, state->selected_entity);

    henka_ui_status_chip(
        state->ui,
        (henka_ui_rect){x, y, 86.0f, 22.0f},
        state->status_warning ? "Warning" : "Status",
        state->status_warning);
    sandbox3d_draw_value_row(state->ui, x, y + 28.0f, width, "Last", status_text);
    snprintf(layout_row, sizeof(layout_row), "%s / %s", sandbox3d_get_layout_mode_label(state->workspace.layout_mode), sandbox3d_get_utility_label(state->workspace.active_utility));
    sandbox3d_draw_value_row(state->ui, x, y + 54.0f, width, "Mode", layout_row);
    if (!compact)
    {
        sandbox3d_draw_value_row(state->ui, x, y + 80.0f, width, "Selected", descriptor != NULL ? descriptor->display_name : "(none)");
    }
}

static void sandbox3d_draw_controls_panel(
    henka_engine* engine,
    sandbox3d_state* state,
    const sandbox3d_workspace_layout* layout)
{
    bool full_mode;
    bool inspect_mode;
    bool scene_panel_visible;
    bool details_panel_visible;
    bool grid_visible;
    float half_button_width;
    float narrow_button_width;
    sandbox3d_layout_mode layout_mode;
    float third_button_width;
    bool wireframe_enabled;
    float x_left;
    float x_middle;
    float x_right;
    henka_ui_rect panel_bounds;

    if (engine == NULL || state == NULL || layout == NULL)
    {
        return;
    }

    panel_bounds = layout->controls_panel;
    x_left = panel_bounds.x + 14.0f;
    half_button_width = (panel_bounds.width - 42.0f) * 0.5f;
    x_middle = x_left + half_button_width + 14.0f;
    third_button_width = (panel_bounds.width - 44.0f) / 3.0f;
    x_right = x_left + third_button_width * 2.0f + 8.0f;
    scene_panel_visible = state->workspace.scene_objects_panel_visible;
    details_panel_visible = state->workspace.object_details_panel_visible;
    grid_visible = henka_scene_is_entity_visible(state->scene, state->grid_entity);
    wireframe_enabled = henka_engine_is_wireframe_enabled(engine);
    layout_mode = state->workspace.layout_mode;
    inspect_mode = layout_mode == SANDBOX3D_LAYOUT_INSPECT;
    full_mode = layout_mode == SANDBOX3D_LAYOUT_FULL;
    narrow_button_width = (panel_bounds.width - 56.0f) / 3.0f;

    henka_ui_panel(state->ui, panel_bounds, "Controls");
    sandbox3d_draw_section_heading(state->ui, x_left, panel_bounds.y + 38.0f, "Workspace");
    if (henka_ui_tab(state->ui, "layout_view", (henka_ui_rect){x_left, panel_bounds.y + 56.0f, third_button_width, 28.0f}, "View", layout_mode == SANDBOX3D_LAYOUT_VIEW))
    {
        state->workspace.layout_mode = SANDBOX3D_LAYOUT_VIEW;
        sandbox3d_set_statusf(state, false, false, "Layout set to %s.", sandbox3d_get_layout_mode_label(state->workspace.layout_mode));
        sandbox3d_print_layout_mode(state, false);
    }
    if (henka_ui_tab(state->ui, "layout_inspect", (henka_ui_rect){x_left + third_button_width + 4.0f, panel_bounds.y + 56.0f, third_button_width, 28.0f}, "Inspect", layout_mode == SANDBOX3D_LAYOUT_INSPECT))
    {
        state->workspace.layout_mode = SANDBOX3D_LAYOUT_INSPECT;
        sandbox3d_set_statusf(state, false, false, "Layout set to %s.", sandbox3d_get_layout_mode_label(state->workspace.layout_mode));
        sandbox3d_print_layout_mode(state, false);
    }
    if (henka_ui_tab(state->ui, "layout_full", (henka_ui_rect){x_right, panel_bounds.y + 56.0f, third_button_width, 28.0f}, "Full", layout_mode == SANDBOX3D_LAYOUT_FULL))
    {
        state->workspace.layout_mode = SANDBOX3D_LAYOUT_FULL;
        sandbox3d_set_statusf(state, false, false, "Layout set to %s.", sandbox3d_get_layout_mode_label(state->workspace.layout_mode));
        sandbox3d_print_layout_mode(state, false);
    }
    if (henka_ui_button(state->ui, "reset_layout", (henka_ui_rect){x_left, panel_bounds.y + 90.0f, panel_bounds.width - 28.0f, 26.0f}, "Reset Layout"))
    {
        sandbox3d_reset_workspace_layout(state);
        sandbox3d_set_statusf(state, false, true, "Layout reset to View.");
        sandbox3d_print_layout_mode(state, false);
    }

    sandbox3d_draw_section_heading(state->ui, x_left, panel_bounds.y + 126.0f, "Viewer");
    if (henka_ui_toggle(state->ui, "grid", (henka_ui_rect){x_left, panel_bounds.y + 144.0f, half_button_width, 28.0f}, "Grid", &grid_visible))
    {
        sandbox3d_toggle_grid_visibility(state, grid_visible, true);
    }
    else
    {
        sandbox3d_toggle_grid_visibility(state, grid_visible, false);
    }

    if (henka_ui_toggle(state->ui, "wireframe", (henka_ui_rect){x_middle, panel_bounds.y + 144.0f, half_button_width, 28.0f}, "Wire", &wireframe_enabled))
    {
        sandbox3d_toggle_wireframe(engine, wireframe_enabled, true);
        sandbox3d_set_statusf(state, false, false, "Wireframe %s.", wireframe_enabled ? "on" : "off");
    }
    else
    {
        sandbox3d_toggle_wireframe(engine, wireframe_enabled, false);
    }

    if (henka_ui_primary_button(state->ui, "reset_camera", (henka_ui_rect){x_left, panel_bounds.y + 180.0f, half_button_width, 28.0f}, "Reset Camera"))
    {
        sandbox3d_reset_camera_defaults(state);
        sandbox3d_set_statusf(state, false, true, "Camera reset to the default sandbox view.");
    }
    if (henka_ui_button(state->ui, "save_settings", (henka_ui_rect){x_middle, panel_bounds.y + 180.0f, half_button_width, 28.0f}, "Save Settings"))
    {
        if (sandbox3d_save_settings(engine, state) == HENKA_SUCCESS)
        {
            sandbox3d_set_statusf(state, false, true, "Settings saved.");
        }
        else
        {
            sandbox3d_set_statusf(state, true, true, "Settings could not be saved.");
        }
    }

    if (inspect_mode || full_mode)
    {
        sandbox3d_draw_section_heading(state->ui, x_left, panel_bounds.y + 218.0f, "Panels");
        if (henka_ui_toggle(state->ui, "scene_panel_visible", (henka_ui_rect){x_left, panel_bounds.y + 236.0f, half_button_width, 28.0f}, "Objects", &scene_panel_visible))
        {
            state->workspace.scene_objects_panel_visible = scene_panel_visible;
            sandbox3d_set_statusf(state, false, false, "Objects panel %s.", scene_panel_visible ? "shown" : "hidden");
        }
        else
        {
            state->workspace.scene_objects_panel_visible = scene_panel_visible;
        }

        if (henka_ui_toggle(state->ui, "details_panel_visible", (henka_ui_rect){x_middle, panel_bounds.y + 236.0f, half_button_width, 28.0f}, "Details", &details_panel_visible))
        {
            state->workspace.object_details_panel_visible = details_panel_visible;
            sandbox3d_set_statusf(state, false, false, "Details panel %s.", details_panel_visible ? "shown" : "hidden");
        }
        else
        {
            state->workspace.object_details_panel_visible = details_panel_visible;
        }
    }

    sandbox3d_draw_section_heading(state->ui, x_left, inspect_mode || full_mode ? panel_bounds.y + 274.0f : panel_bounds.y + 218.0f, "Utilities");
    if (henka_ui_tab(state->ui, "utility_help", (henka_ui_rect){x_left, inspect_mode || full_mode ? panel_bounds.y + 292.0f : panel_bounds.y + 236.0f, narrow_button_width, 26.0f}, "Help", state->workspace.active_utility == SANDBOX3D_UTILITY_HELP))
    {
        sandbox3d_set_active_utility(state, SANDBOX3D_UTILITY_HELP);
        sandbox3d_set_statusf(state, false, false, "Help is open in the Utility panel.");
        sandbox3d_print_help(state);
    }
    if (henka_ui_tab(state->ui, "utility_legend", (henka_ui_rect){x_left + narrow_button_width + 8.0f, inspect_mode || full_mode ? panel_bounds.y + 292.0f : panel_bounds.y + 236.0f, narrow_button_width, 26.0f}, "Legend", state->workspace.active_utility == SANDBOX3D_UTILITY_SCENE_LEGEND))
    {
        sandbox3d_set_active_utility(state, SANDBOX3D_UTILITY_SCENE_LEGEND);
        sandbox3d_set_statusf(state, false, false, "Scene legend is open in the Utility panel.");
        sandbox3d_print_scene_legend(state);
        fflush(stdout);
    }
    if (henka_ui_tab(state->ui, "utility_paths", (henka_ui_rect){x_left + (narrow_button_width + 8.0f) * 2.0f, inspect_mode || full_mode ? panel_bounds.y + 292.0f : panel_bounds.y + 236.0f, narrow_button_width, 26.0f}, "Paths", state->workspace.active_utility == SANDBOX3D_UTILITY_PATHS))
    {
        sandbox3d_set_active_utility(state, SANDBOX3D_UTILITY_PATHS);
        sandbox3d_set_statusf(state, false, false, "Paths are open in the Utility panel.");
    }
    if (henka_ui_tab(state->ui, "utility_settings", (henka_ui_rect){x_left, inspect_mode || full_mode ? panel_bounds.y + 324.0f : panel_bounds.y + 268.0f, narrow_button_width, 26.0f}, "Settings", state->workspace.active_utility == SANDBOX3D_UTILITY_SETTINGS))
    {
        sandbox3d_set_active_utility(state, SANDBOX3D_UTILITY_SETTINGS);
        sandbox3d_set_statusf(state, false, false, "Settings summary is open in the Utility panel.");
    }
    if (henka_ui_tab(state->ui, "utility_diag", (henka_ui_rect){x_left + narrow_button_width + 8.0f, inspect_mode || full_mode ? panel_bounds.y + 324.0f : panel_bounds.y + 268.0f, narrow_button_width, 26.0f}, "Diag", state->workspace.active_utility == SANDBOX3D_UTILITY_DIAGNOSTICS))
    {
        sandbox3d_set_active_utility(state, SANDBOX3D_UTILITY_DIAGNOSTICS);
        sandbox3d_set_statusf(state, false, false, "Diagnostics are open in the Utility panel.");
    }
    if (inspect_mode || full_mode)
    {
        if (henka_ui_tab(state->ui, "utility_info", (henka_ui_rect){x_left + (narrow_button_width + 8.0f) * 2.0f, panel_bounds.y + 324.0f, narrow_button_width, 26.0f}, "Info", state->workspace.active_utility == SANDBOX3D_UTILITY_OBJECT_INFO))
        {
            sandbox3d_set_active_utility(state, SANDBOX3D_UTILITY_OBJECT_INFO);
            sandbox3d_set_statusf(state, false, false, "Object info is open in the Utility panel.");
        }
    }
    else if (henka_ui_button(state->ui, "utility_close", (henka_ui_rect){x_left + (narrow_button_width + 8.0f) * 2.0f, panel_bounds.y + 268.0f, narrow_button_width, 26.0f}, "Close"))
    {
        if (state->workspace.active_utility != SANDBOX3D_UTILITY_NONE)
        {
            sandbox3d_set_active_utility(state, SANDBOX3D_UTILITY_NONE);
            sandbox3d_set_statusf(state, false, false, "Utility panel closed.");
        }
    }

    if (full_mode)
    {
        sandbox3d_draw_section_heading(state->ui, x_left, panel_bounds.y + panel_bounds.height - 152.0f, "Status");
        sandbox3d_draw_status_block(state, x_left, panel_bounds.y + panel_bounds.height - 134.0f, panel_bounds.width - 28.0f, false);
    }
    else
    {
        sandbox3d_draw_section_heading(state->ui, x_left, panel_bounds.y + panel_bounds.height - 100.0f, "Status");
        sandbox3d_draw_status_block(state, x_left, panel_bounds.y + panel_bounds.height - 82.0f, panel_bounds.width - 28.0f, true);
    }

    if (inspect_mode || full_mode)
    {
        if (henka_ui_button(state->ui, "reset_settings", (henka_ui_rect){x_left, panel_bounds.y + panel_bounds.height - 36.0f, half_button_width, 28.0f}, "Reset Settings"))
        {
            if (sandbox3d_reset_settings(engine, state) == HENKA_SUCCESS)
            {
                sandbox3d_set_statusf(state, false, true, "Settings reset to defaults.");
            }
            else
            {
                sandbox3d_set_statusf(state, true, true, "Settings could not be reset.");
            }
        }

        if (henka_ui_primary_button(state->ui, "open_legend", (henka_ui_rect){x_middle, panel_bounds.y + panel_bounds.height - 36.0f, half_button_width, 28.0f}, "Open Legend"))
        {
            sandbox3d_set_active_utility(state, SANDBOX3D_UTILITY_SCENE_LEGEND);
            sandbox3d_set_statusf(state, false, false, "Scene legend is open in the Utility panel.");
            sandbox3d_print_scene_legend(state);
            fflush(stdout);
        }
    }
}

static void sandbox3d_draw_scene_objects_panel(
    sandbox3d_state* state,
    const sandbox3d_workspace_layout* layout)
{
    char row_label[96];
    char subtitle_text[96];
    const sandbox3d_object_descriptor* descriptor;
    const char* entity_name;
    float row_y;
    henka_entity entity;
    henka_ui_rect panel_bounds;
    size_t scene_index;

    if (state == NULL || layout == NULL || state->scene == NULL || !sandbox3d_workspace_shows_scene_panel(state))
    {
        return;
    }

    panel_bounds = layout->scene_objects_panel;
    henka_ui_panel(state->ui, panel_bounds, "Scene Objects");
    henka_ui_label(state->ui, panel_bounds.x + 14.0f, panel_bounds.y + 38.0f, 1.0f, "Current scene examples");

    row_y = panel_bounds.y + 64.0f;
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

        snprintf(
            subtitle_text,
            sizeof(subtitle_text),
            "%s - %s",
            entity_name != NULL ? entity_name : descriptor->display_name,
            sandbox3d_get_descriptor_role_label(descriptor));
        sandbox3d_truncate_text(subtitle_text, row_label, sizeof(row_label), 30U);

        if (!henka_scene_is_entity_visible(state->scene, entity))
        {
            sandbox3d_truncate_text("[Hidden]", subtitle_text, sizeof(subtitle_text), 12U);
            snprintf(row_label, sizeof(row_label), "%s %s", subtitle_text, row_label);
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
    char detail_text[96];
    char developer_text[96];
    char interaction_text[64];
    char material_text[96];
    char position_text[64];
    char rotation_text[64];
    char texture_text[96];
    char scale_text[64];
    const sandbox3d_object_descriptor* descriptor;
    henka_interaction_desc interaction;
    henka_interaction_result interaction_result;
    henka_material material;
    henka_scene_object_info object_info;
    henka_result result;
    henka_transform transform;
    henka_ui_rect panel_bounds;

    bool compact_mode;
    if (state == NULL || layout == NULL || !sandbox3d_workspace_shows_details_panel(state))
    {
        return;
    }

    panel_bounds = layout->object_details_panel;
    henka_ui_panel(state->ui, panel_bounds, "Object Details");
    compact_mode = state->workspace.layout_mode != SANDBOX3D_LAYOUT_FULL;

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
    henka_scene_get_entity_info(state->scene, descriptor->entity, &object_info);
    if (henka_scene_get_entity_material(state->scene, descriptor->entity, &material) != HENKA_SUCCESS)
    {
        material = henka_material_default();
    }
    if (henka_material_describe(&material, material_text, sizeof(material_text)) != HENKA_SUCCESS)
    {
        snprintf(material_text, sizeof(material_text), "%s", descriptor->material_summary);
    }
    if (henka_scene_get_entity_interaction(state->scene, descriptor->entity, &interaction) != HENKA_SUCCESS)
    {
        interaction = (henka_interaction_desc){false, 0.0f, NULL};
    }
    interaction_result = henka_scene_can_interact(state->scene, descriptor->entity, state->camera.position);

    visible = henka_scene_is_entity_visible(state->scene, descriptor->entity);
    snprintf(position_text, sizeof(position_text), "%.2f %.2f %.2f", transform.position.x, transform.position.y, transform.position.z);
    snprintf(rotation_text, sizeof(rotation_text), "%.2f %.2f %.2f", transform.rotation.x, transform.rotation.y, transform.rotation.z);
    snprintf(scale_text, sizeof(scale_text), "%.2f %.2f %.2f", transform.scale.x, transform.scale.y, transform.scale.z);
    sandbox3d_truncate_text(descriptor->short_explanation, detail_text, sizeof(detail_text), compact_mode ? 42U : 54U);
    sandbox3d_truncate_text(descriptor->developer_detail, developer_text, sizeof(developer_text), compact_mode ? 42U : 54U);
    sandbox3d_truncate_text(descriptor->texture_summary, texture_text, sizeof(texture_text), compact_mode ? 42U : 54U);

    henka_ui_heading(state->ui, panel_bounds.x + 14.0f, panel_bounds.y + 38.0f, 1.0f, descriptor->display_name);
    sandbox3d_draw_section_heading(state->ui, panel_bounds.x + 14.0f, panel_bounds.y + 58.0f, "Overview");
    sandbox3d_draw_value_row(state->ui, panel_bounds.x + 14.0f, panel_bounds.y + 76.0f, panel_bounds.width - 28.0f, "Visible", visible ? "Yes" : "No");
    sandbox3d_draw_value_row(state->ui, panel_bounds.x + 14.0f, panel_bounds.y + 102.0f, panel_bounds.width - 28.0f, "Shows", detail_text);
    sandbox3d_draw_section_heading(state->ui, panel_bounds.x + 14.0f, panel_bounds.y + 132.0f, "Transform");
    sandbox3d_draw_value_row(state->ui, panel_bounds.x + 14.0f, panel_bounds.y + 150.0f, panel_bounds.width - 28.0f, "Position", position_text);
    sandbox3d_draw_value_row(state->ui, panel_bounds.x + 14.0f, panel_bounds.y + 176.0f, panel_bounds.width - 28.0f, "Scale", scale_text);
    if (!compact_mode)
    {
        sandbox3d_draw_value_row(state->ui, panel_bounds.x + 14.0f, panel_bounds.y + 202.0f, panel_bounds.width - 28.0f, "Rotation", rotation_text);
        sandbox3d_draw_value_row(state->ui, panel_bounds.x + 14.0f, panel_bounds.y + 228.0f, panel_bounds.width - 28.0f, "Tag", object_info.tag != NULL ? object_info.tag : "(none)");
    }

    sandbox3d_draw_section_heading(state->ui, panel_bounds.x + 14.0f, compact_mode ? panel_bounds.y + 208.0f : panel_bounds.y + 260.0f, "Render");
    sandbox3d_draw_value_row(state->ui, panel_bounds.x + 14.0f, compact_mode ? panel_bounds.y + 226.0f : panel_bounds.y + 278.0f, panel_bounds.width - 28.0f, "Mesh", descriptor->mesh_summary);
    sandbox3d_draw_value_row(state->ui, panel_bounds.x + 14.0f, compact_mode ? panel_bounds.y + 252.0f : panel_bounds.y + 304.0f, panel_bounds.width - 28.0f, "Material", material_text);
    if (!compact_mode)
    {
        sandbox3d_draw_value_row(state->ui, panel_bounds.x + 14.0f, panel_bounds.y + 330.0f, panel_bounds.width - 28.0f, "Texture", texture_text);
    }

    if (!interaction.enabled)
    {
        snprintf(interaction_text, sizeof(interaction_text), "%s", "Disabled");
    }
    else if (interaction_result == HENKA_INTERACTION_RESULT_OUT_OF_RANGE)
    {
        snprintf(interaction_text, sizeof(interaction_text), "Out of range");
    }
    else
    {
        snprintf(interaction_text, sizeof(interaction_text), "%s", interaction.prompt != NULL ? interaction.prompt : "Available");
    }
    if (!compact_mode)
    {
        sandbox3d_draw_value_row(
            state->ui,
            panel_bounds.x + 14.0f,
            panel_bounds.y + 332.0f,
            panel_bounds.width - 28.0f,
            "Interact",
            interaction_text);
    }

    snprintf(action_label, sizeof(action_label), "%s", visible ? "Hide Object" : "Show Object");
    sandbox3d_draw_section_heading(state->ui, panel_bounds.x + 14.0f, compact_mode ? panel_bounds.y + 286.0f : panel_bounds.y + 356.0f, "Actions");
    if (henka_ui_button(state->ui, "toggle_selected_visibility", (henka_ui_rect){panel_bounds.x + 14.0f, compact_mode ? panel_bounds.y + 304.0f : panel_bounds.y + 374.0f, 136.0f, 28.0f}, action_label))
    {
        if (sandbox3d_toggle_selected_entity_visibility(state))
        {
            sandbox3d_set_statusf(state, false, true, "%s visibility updated.", descriptor->display_name);
        }
        else
        {
            sandbox3d_set_statusf(state, true, true, "%s visibility could not be changed.", descriptor->display_name);
        }
    }

    if (henka_ui_primary_button(state->ui, "focus_selected_camera", (henka_ui_rect){panel_bounds.x + 162.0f, compact_mode ? panel_bounds.y + 304.0f : panel_bounds.y + 374.0f, 136.0f, 28.0f}, "Focus Camera"))
    {
        if (sandbox3d_focus_camera_on_selected(state))
        {
            sandbox3d_set_statusf(state, false, true, "Focused camera on %s.", descriptor->display_name);
        }
        else
        {
            sandbox3d_set_statusf(state, true, true, "Camera could not focus on %s.", descriptor->display_name);
        }
    }

    if (henka_ui_button(state->ui, "reset_selected_transform", (henka_ui_rect){panel_bounds.x + 14.0f, compact_mode ? panel_bounds.y + 338.0f : panel_bounds.y + 402.0f, 136.0f, 28.0f}, "Reset Transform"))
    {
        if (sandbox3d_reset_selected_entity_transform(state))
        {
            sandbox3d_set_statusf(state, false, true, "%s reset to its default transform.", descriptor->display_name);
        }
        else
        {
            sandbox3d_set_statusf(state, true, true, "%s could not be reset.", descriptor->display_name);
        }
    }

    if (henka_ui_button(state->ui, "print_selected_info", (henka_ui_rect){panel_bounds.x + 162.0f, compact_mode ? panel_bounds.y + 338.0f : panel_bounds.y + 402.0f, 136.0f, 28.0f}, "Object Info"))
    {
        sandbox3d_set_active_utility(state, SANDBOX3D_UTILITY_OBJECT_INFO);
        sandbox3d_set_statusf(state, false, false, "Object info is open for %s.", descriptor->display_name);
        sandbox3d_print_selected_object_info(state);
    }

}

static void sandbox3d_draw_utility_panel(
    henka_engine* engine,
    sandbox3d_state* state,
    const sandbox3d_workspace_layout* layout,
    const char* asset_path_text,
    const char* user_path_text,
    const char* settings_path_text,
    const char* save_path_text,
    const char* capture_text,
    const char* fps_text,
    int framebuffer_width,
    int framebuffer_height)
{
    char detail_text[96];
    char diag_text[96];
    char package_text[48];
    char row_value[96];
    const henka_asset_manager* assets;
    const sandbox3d_object_descriptor* descriptor;
    float button_width;
    henka_engine_diagnostics diagnostics;
    float x_left;
    float y_start;
    henka_transform transform;
    henka_ui_rect panel_bounds;

    if (engine == NULL || state == NULL || layout == NULL || !sandbox3d_workspace_shows_utility_panel(state))
    {
        return;
    }

    panel_bounds = layout->utility_panel;
    x_left = panel_bounds.x + 14.0f;
    button_width = (panel_bounds.width - 44.0f) / 3.0f;
    y_start = panel_bounds.y + 38.0f;
    assets = henka_engine_get_asset_manager_const(engine);
    if (henka_engine_get_diagnostics(engine, &diagnostics) != HENKA_SUCCESS)
    {
        memset(&diagnostics, 0, sizeof(diagnostics));
    }
    snprintf(package_text, sizeof(package_text), "%s", henka_engine_get_package_mode_label(diagnostics.package_mode));

    henka_ui_panel(state->ui, panel_bounds, "Utility");
    henka_ui_status_chip(
        state->ui,
        (henka_ui_rect){panel_bounds.x + panel_bounds.width - 96.0f, panel_bounds.y + 7.0f, 78.0f, 18.0f},
        sandbox3d_get_utility_label(state->workspace.active_utility),
        false);
    if (henka_ui_tab(state->ui, "utility_tab_help", (henka_ui_rect){x_left, y_start, button_width, 24.0f}, "Help", state->workspace.active_utility == SANDBOX3D_UTILITY_HELP))
    {
        sandbox3d_set_active_utility(state, SANDBOX3D_UTILITY_HELP);
    }
    if (henka_ui_tab(state->ui, "utility_tab_legend", (henka_ui_rect){x_left + button_width + 8.0f, y_start, button_width, 24.0f}, "Legend", state->workspace.active_utility == SANDBOX3D_UTILITY_SCENE_LEGEND))
    {
        sandbox3d_set_active_utility(state, SANDBOX3D_UTILITY_SCENE_LEGEND);
    }
    if (henka_ui_tab(state->ui, "utility_tab_info", (henka_ui_rect){x_left + (button_width + 8.0f) * 2.0f, y_start, button_width, 24.0f}, "Info", state->workspace.active_utility == SANDBOX3D_UTILITY_OBJECT_INFO))
    {
        sandbox3d_set_active_utility(state, SANDBOX3D_UTILITY_OBJECT_INFO);
    }
    if (henka_ui_tab(state->ui, "utility_tab_paths", (henka_ui_rect){x_left, y_start + 30.0f, button_width, 24.0f}, "Paths", state->workspace.active_utility == SANDBOX3D_UTILITY_PATHS))
    {
        sandbox3d_set_active_utility(state, SANDBOX3D_UTILITY_PATHS);
    }
    if (henka_ui_tab(state->ui, "utility_tab_settings", (henka_ui_rect){x_left + button_width + 8.0f, y_start + 30.0f, button_width, 24.0f}, "Settings", state->workspace.active_utility == SANDBOX3D_UTILITY_SETTINGS))
    {
        sandbox3d_set_active_utility(state, SANDBOX3D_UTILITY_SETTINGS);
    }
    if (henka_ui_tab(state->ui, "utility_tab_diag", (henka_ui_rect){x_left + (button_width + 8.0f) * 2.0f, y_start + 30.0f, button_width, 24.0f}, "Diag", state->workspace.active_utility == SANDBOX3D_UTILITY_DIAGNOSTICS))
    {
        sandbox3d_set_active_utility(state, SANDBOX3D_UTILITY_DIAGNOSTICS);
    }

    y_start = panel_bounds.y + 96.0f;
    switch (state->workspace.active_utility)
    {
        case SANDBOX3D_UTILITY_HELP:
            sandbox3d_draw_section_heading(state->ui, x_left, y_start, "Viewer help");
            henka_ui_label(state->ui, x_left, y_start + 18.0f, 1.0f, "F4 shows or hides the panels.");
            henka_ui_label(state->ui, x_left, y_start + 34.0f, 1.0f, "F5 switches View, Inspect, and Full Tools.");
            henka_ui_label(state->ui, x_left, y_start + 50.0f, 1.0f, "Scene Objects selects examples in the current scene.");
            henka_ui_label(state->ui, x_left, y_start + 66.0f, 1.0f, "Object Details stays read-only and safe.");
            henka_ui_label(state->ui, x_left, y_start + 82.0f, 1.0f, "Use Paths, Settings, and Diag here for normal inspection.");
            sandbox3d_draw_value_row(state->ui, x_left, y_start + 112.0f, panel_bounds.width - 28.0f, "Status", "Normal sandbox use stays in-window.");
            break;

        case SANDBOX3D_UTILITY_SCENE_LEGEND:
            sandbox3d_draw_section_heading(state->ui, x_left, y_start, "Scene legend");
            henka_ui_label(state->ui, x_left, y_start + 18.0f, 1.0f, "Ground: textured plane under the scene.");
            henka_ui_label(state->ui, x_left, y_start + 34.0f, 1.0f, "Textured Cube: texture material rendering.");
            henka_ui_label(state->ui, x_left, y_start + 50.0f, 1.0f, "Colored Cube: untextured base color.");
            henka_ui_label(state->ui, x_left, y_start + 66.0f, 1.0f, "OBJ Marker: current OBJ mesh loading path.");
            henka_ui_label(state->ui, x_left, y_start + 82.0f, 1.0f, "Missing Texture and Missing Model show fallback behavior.");
            break;

        case SANDBOX3D_UTILITY_OBJECT_INFO:
            descriptor = sandbox3d_get_descriptor_by_entity(state, state->selected_entity);
            if (descriptor == NULL || state->scene == NULL || henka_scene_get_entity_transform(state->scene, descriptor->entity, &transform) != HENKA_SUCCESS)
            {
                henka_ui_label(state->ui, x_left, y_start, 1.0f, "Select an object to inspect it here.");
                break;
            }

            sandbox3d_draw_section_heading(state->ui, x_left, y_start, descriptor->display_name);
            sandbox3d_draw_value_row(state->ui, x_left, y_start + 18.0f, panel_bounds.width - 28.0f, "Visible", henka_scene_is_entity_visible(state->scene, descriptor->entity) ? "Yes" : "No");
            sandbox3d_draw_value_row(state->ui, x_left, y_start + 44.0f, panel_bounds.width - 28.0f, "Shows", descriptor->short_explanation);
            sandbox3d_draw_value_row(state->ui, x_left, y_start + 70.0f, panel_bounds.width - 28.0f, "Detail", descriptor->developer_detail);
            sandbox3d_draw_value_row(state->ui, x_left, y_start + 96.0f, panel_bounds.width - 28.0f, "Mesh", descriptor->mesh_summary);
            snprintf(row_value, sizeof(row_value), "%.2f %.2f %.2f", transform.position.x, transform.position.y, transform.position.z);
            sandbox3d_draw_value_row(state->ui, x_left, y_start + 122.0f, panel_bounds.width - 28.0f, "Position", row_value);
            sandbox3d_draw_value_row(state->ui, x_left, y_start + 148.0f, panel_bounds.width - 28.0f, "Tag", henka_scene_get_entity_tag(state->scene, descriptor->entity) != NULL ? henka_scene_get_entity_tag(state->scene, descriptor->entity) : "(none)");
            break;

        case SANDBOX3D_UTILITY_PATHS:
            sandbox3d_draw_section_heading(state->ui, x_left, y_start, "Runtime paths");
            sandbox3d_draw_value_row(state->ui, x_left, y_start + 18.0f, panel_bounds.width - 28.0f, "Assets", asset_path_text + 8);
            sandbox3d_draw_value_row(state->ui, x_left, y_start + 44.0f, panel_bounds.width - 28.0f, "User", user_path_text + 6);
            sandbox3d_draw_value_row(state->ui, x_left, y_start + 70.0f, panel_bounds.width - 28.0f, "Settings", settings_path_text + 10);
            sandbox3d_draw_value_row(state->ui, x_left, y_start + 96.0f, panel_bounds.width - 28.0f, "Save", save_path_text + 6);
            sandbox3d_draw_value_row(state->ui, x_left, y_start + 122.0f, panel_bounds.width - 28.0f, "Mode", package_text);
            break;

        case SANDBOX3D_UTILITY_SETTINGS:
            descriptor = sandbox3d_get_descriptor_by_entity(state, state->selected_entity);
            sandbox3d_draw_section_heading(state->ui, x_left, y_start, "Sandbox settings");
            sandbox3d_draw_value_row(state->ui, x_left, y_start + 18.0f, panel_bounds.width - 28.0f, "Layout", sandbox3d_get_layout_mode_label(state->workspace.layout_mode));
            sandbox3d_draw_value_row(state->ui, x_left, y_start + 44.0f, panel_bounds.width - 28.0f, "Selected", descriptor != NULL ? descriptor->display_name : "(none)");
            sandbox3d_draw_value_row(state->ui, x_left, y_start + 70.0f, panel_bounds.width - 28.0f, "Grid", henka_scene_is_entity_visible(state->scene, state->grid_entity) ? "Visible" : "Hidden");
            sandbox3d_draw_value_row(state->ui, x_left, y_start + 96.0f, panel_bounds.width - 28.0f, "Wireframe", henka_engine_is_wireframe_enabled(engine) ? "On" : "Off");
            snprintf(row_value, sizeof(row_value), "%.4f  /  %.1f", sandbox3d_get_mouse_sensitivity(state), state->camera.movement_speed);
            sandbox3d_draw_value_row(state->ui, x_left, y_start + 122.0f, panel_bounds.width - 28.0f, "Sense/Speed", row_value);
            if (henka_ui_button(state->ui, "utility_mouse_less", (henka_ui_rect){x_left, y_start + 152.0f, 60.0f, 24.0f}, "Sense-"))
            {
                sandbox3d_adjust_mouse_sensitivity(state, -0.0005f);
            }
            if (henka_ui_button(state->ui, "utility_mouse_more", (henka_ui_rect){x_left + 68.0f, y_start + 152.0f, 60.0f, 24.0f}, "Sense+"))
            {
                sandbox3d_adjust_mouse_sensitivity(state, 0.0005f);
            }
            if (henka_ui_button(state->ui, "utility_speed_less", (henka_ui_rect){x_left + 146.0f, y_start + 152.0f, 60.0f, 24.0f}, "Speed-"))
            {
                sandbox3d_adjust_camera_speed(state, -0.5f);
            }
            if (henka_ui_button(state->ui, "utility_speed_more", (henka_ui_rect){x_left + 214.0f, y_start + 152.0f, 60.0f, 24.0f}, "Speed+"))
            {
                sandbox3d_adjust_camera_speed(state, 0.5f);
            }
            break;

        case SANDBOX3D_UTILITY_DIAGNOSTICS:
            descriptor = sandbox3d_get_descriptor_by_entity(state, state->selected_entity);
            sandbox3d_draw_section_heading(state->ui, x_left, y_start, "Diagnostics");
            sandbox3d_draw_value_row(state->ui, x_left, y_start + 18.0f, panel_bounds.width - 28.0f, "Frame", fps_text + 7);
            sandbox3d_draw_value_row(state->ui, x_left, y_start + 44.0f, panel_bounds.width - 28.0f, "Layout", sandbox3d_get_layout_mode_label(state->workspace.layout_mode));
            sandbox3d_draw_value_row(state->ui, x_left, y_start + 70.0f, panel_bounds.width - 28.0f, "UI", henka_ui_is_visible(state->ui) ? "Visible" : "Hidden");
            sandbox3d_draw_value_row(state->ui, x_left, y_start + 96.0f, panel_bounds.width - 28.0f, "Capture", capture_text + 9);
            snprintf(row_value, sizeof(row_value), "%dx%d", framebuffer_width, framebuffer_height);
            sandbox3d_draw_value_row(state->ui, x_left, y_start + 122.0f, panel_bounds.width - 28.0f, "Framebuffer", row_value);
            sandbox3d_draw_value_row(state->ui, x_left, y_start + 148.0f, panel_bounds.width - 28.0f, "Selected", descriptor != NULL ? descriptor->display_name : "(none)");
            snprintf(diag_text, sizeof(diag_text), "%u assets / frame %llu", (unsigned int)henka_assets_get_metadata_count(assets), (unsigned long long)diagnostics.frame_index);
            sandbox3d_draw_value_row(state->ui, x_left, y_start + 174.0f, panel_bounds.width - 28.0f, "Runtime", diag_text);
            sandbox3d_draw_value_row(state->ui, x_left, y_start + 200.0f, panel_bounds.width - 28.0f, "Console", "Fallback logs stay available for troubleshooting.");
            break;

        case SANDBOX3D_UTILITY_NONE:
        default:
            break;
    }
}

static void sandbox3d_build_ui(henka_engine* engine, sandbox3d_state* state)
{
    char asset_path_text[128];
    char capture_text[64];
    char fps_text[56];
    char save_path_text[128];
    char settings_path_text[128];
    char user_path_text[128];
    char* save_path;
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
        save_path = NULL;
        result = sandbox3d_get_settings_path(engine, &settings_path);
        milliseconds = (float)(henka_engine_get_delta_time(engine) * 1000.0);
        fps = milliseconds > 0.0f ? 1000.0f / milliseconds : 0.0f;
        layout = sandbox3d_get_workspace_layout(state, frame_desc.framebuffer_width, frame_desc.framebuffer_height);

        sandbox3d_format_display_path("Assets", henka_engine_get_asset_base_path(engine), asset_path_text, sizeof(asset_path_text));
        sandbox3d_format_display_path("User", henka_engine_get_user_data_base_path(engine), user_path_text, sizeof(user_path_text));
        sandbox3d_format_display_path("Settings", result == HENKA_SUCCESS ? settings_path : NULL, settings_path_text, sizeof(settings_path_text));
        result = henka_save_data_build_slot_path(henka_engine_get_user_data_base_path(engine), "sandbox3d_preview", &save_path);
        sandbox3d_format_display_path("Save", result == HENKA_SUCCESS ? save_path : NULL, save_path_text, sizeof(save_path_text));
        snprintf(capture_text, sizeof(capture_text), "Capture: %s", henka_engine_is_mouse_captured(engine) ? "On" : "Off");
        snprintf(fps_text, sizeof(fps_text), "Frame: %.2f ms  FPS: %.1f", milliseconds, fps);
        sandbox3d_draw_controls_panel(
            engine,
            state,
            &layout);
        sandbox3d_draw_scene_objects_panel(state, &layout);
        sandbox3d_draw_object_details_panel(state, &layout);
        sandbox3d_draw_utility_panel(
            engine,
            state,
            &layout,
            asset_path_text,
            user_path_text,
            settings_path_text,
            save_path_text,
            capture_text,
            fps_text,
            frame_desc.framebuffer_width,
            frame_desc.framebuffer_height);

        if (state->ui_visibility_report_pending)
        {
            printf(
                "Sandbox UI ready: %s mode, framebuffer %dx%d, draw rects %zu, scene panel %s, details panel %s, utility %s.\n",
                sandbox3d_get_layout_mode_label(state->workspace.layout_mode),
                frame_desc.framebuffer_width,
                frame_desc.framebuffer_height,
                henka_ui_get_draw_rect_count(state->ui),
                sandbox3d_workspace_shows_scene_panel(state) ? "on" : "off",
                sandbox3d_workspace_shows_details_panel(state) ? "on" : "off",
                sandbox3d_workspace_shows_utility_panel(state) ? sandbox3d_get_utility_label(state->workspace.active_utility) : "off");
            fflush(stdout);
            state->ui_visibility_report_pending = false;
        }

        henka_free(settings_path);
        henka_free(save_path);
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
    ground_material.name = "Ground Checker";
    ground_material.type = HENKA_MATERIAL_TYPE_LIT;
    ground_material.shader = state->basic_shader;
    ground_material.base_color_texture = state->ground_texture;
    ground_material.use_texture = true;
    ground_material.base_color = (henka_vec4){0.88f, 0.88f, 0.90f, 1.0f};

    cube_material = henka_material_default();
    cube_material.name = "Cube Albedo";
    cube_material.type = HENKA_MATERIAL_TYPE_LIT;
    cube_material.shader = state->basic_shader;
    cube_material.base_color_texture = state->cube_texture;
    cube_material.use_texture = true;
    cube_material.base_color = (henka_vec4){1.0f, 1.0f, 1.0f, 1.0f};

    colored_material = henka_material_default();
    colored_material.name = "Flat Color";
    colored_material.type = HENKA_MATERIAL_TYPE_UNLIT;
    colored_material.shader = state->basic_shader;
    colored_material.base_color = (henka_vec4){0.20f, 0.72f, 0.56f, 1.0f};
    colored_material.use_texture = false;
    colored_material.use_lighting = false;

    marker_material = henka_material_default();
    marker_material.name = "OBJ Marker";
    marker_material.type = HENKA_MATERIAL_TYPE_LIT;
    marker_material.shader = state->basic_shader;
    marker_material.base_color = (henka_vec4){0.96f, 0.72f, 0.18f, 1.0f};
    marker_material.use_texture = false;

    fallback_material = henka_material_default();
    fallback_material.name = "Fallback Texture";
    fallback_material.type = HENKA_MATERIAL_TYPE_LIT;
    fallback_material.shader = state->basic_shader;
    fallback_material.base_color_texture = state->missing_texture;
    fallback_material.use_texture = true;
    fallback_material.base_color = (henka_vec4){1.0f, 1.0f, 1.0f, 1.0f};

    fallback_model_material = henka_material_default();
    fallback_model_material.name = "Fallback Model";
    fallback_model_material.type = HENKA_MATERIAL_TYPE_LIT;
    fallback_model_material.shader = state->basic_shader;
    fallback_model_material.base_color_texture = state->missing_texture;
    fallback_model_material.use_texture = true;
    fallback_model_material.base_color = (henka_vec4){1.0f, 1.0f, 1.0f, 1.0f};

    grid_material = henka_material_default();
    grid_material.name = "Debug Grid";
    grid_material.type = HENKA_MATERIAL_TYPE_UNLIT;
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
    sandbox3d_apply_entity_foundation(
        state,
        state->ground_entity,
        "ground",
        sandbox3d_make_bounds((henka_vec3){0.0f, 0.0f, 0.0f}, (henka_vec3){6.0f, 0.05f, 6.0f}),
        true,
        "Inspect ground sample");
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
    sandbox3d_apply_entity_foundation(
        state,
        state->cube_entity,
        "textured_cube",
        sandbox3d_make_bounds((henka_vec3){0.0f, 0.0f, 0.0f}, (henka_vec3){0.5f, 0.5f, 0.5f}),
        true,
        "Inspect textured cube");
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
    sandbox3d_apply_entity_foundation(
        state,
        state->colored_cube_entity,
        "colored_cube",
        sandbox3d_make_bounds((henka_vec3){0.0f, 0.0f, 0.0f}, (henka_vec3){0.5f, 0.5f, 0.5f}),
        true,
        "Inspect colored cube");
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
    sandbox3d_apply_entity_foundation(
        state,
        state->marker_entity,
        "obj_marker",
        sandbox3d_make_bounds((henka_vec3){0.0f, 0.35f, 0.0f}, (henka_vec3){0.65f, 0.7f, 0.65f}),
        true,
        "Inspect OBJ sample");
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
    sandbox3d_apply_entity_foundation(
        state,
        state->fallback_cube_entity,
        "missing_texture",
        sandbox3d_make_bounds((henka_vec3){0.0f, 0.0f, 0.0f}, (henka_vec3){0.5f, 0.5f, 0.5f}),
        true,
        "Inspect fallback texture sample");
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
    sandbox3d_apply_entity_foundation(
        state,
        state->fallback_model_entity,
        "missing_model",
        sandbox3d_make_bounds((henka_vec3){0.0f, 0.35f, 0.0f}, (henka_vec3){0.65f, 0.7f, 0.65f}),
        true,
        "Inspect fallback mesh sample");
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
    sandbox3d_apply_entity_foundation(
        state,
        state->grid_entity,
        "debug_grid",
        sandbox3d_make_bounds((henka_vec3){0.0f, 0.0f, 0.0f}, (henka_vec3){6.0f, 0.05f, 6.0f}),
        false,
        NULL);
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
        sandbox3d_set_status(state, false, "View mode is open. Press F5 for more tools.");
    }
    else
    {
        sandbox3d_set_status(state, false, "Press F4 to open the in-window panels.");
    }

    result = henka_engine_set_mouse_capture(engine, false);
    if (result != HENKA_SUCCESS)
    {
        goto fail;
    }

    sandbox3d_print_help(state);
    printf("Runtime mode: %s\n", henka_engine_get_package_mode_label(henka_engine_get_package_mode(engine)));
    fflush(stdout);
    sandbox3d_print_startup_ui_cue(state);
    sandbox3d_print_layout_mode(state, true);
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
        if (state->ui != NULL)
        {
            sandbox3d_set_active_utility(state, SANDBOX3D_UTILITY_HELP);
        }
        sandbox3d_set_statusf(state, false, false, "Help is open in the Utility panel.");
        sandbox3d_print_help(state);
    }

    if (henka_input_action_was_pressed(engine, HENKA_INPUT_ACTION_OPEN_PANELS) && state->ui != NULL)
    {
        ui_visible = !henka_ui_is_visible(state->ui);
        henka_ui_set_visible(state->ui, ui_visible);
        if (ui_visible)
        {
            henka_engine_set_mouse_capture(engine, false);
            state->ui_visibility_report_pending = true;
        }
        sandbox3d_set_statusf(state, false, false, "Panels %s.", ui_visible ? "shown" : "hidden");
        sandbox3d_print_ui_state(ui_visible);
        ui_toggled_with_f4 = true;
    }

    if (henka_input_action_was_pressed(engine, HENKA_INPUT_ACTION_CHANGE_LAYOUT) && state->ui != NULL)
    {
        state->workspace.layout_mode = sandbox3d_cycle_layout_mode(state->workspace.layout_mode);
        if (!henka_ui_is_visible(state->ui))
        {
            henka_ui_set_visible(state->ui, true);
            henka_engine_set_mouse_capture(engine, false);
        }
        state->ui_visibility_report_pending = true;
        sandbox3d_set_statusf(state, false, false, "Layout set to %s.", sandbox3d_get_layout_mode_label(state->workspace.layout_mode));
        sandbox3d_print_layout_mode(state, false);
    }

    if (henka_input_was_key_pressed(engine, HENKA_KEY_F2))
    {
        if (state->ui != NULL)
        {
            sandbox3d_set_active_utility(state, SANDBOX3D_UTILITY_SCENE_LEGEND);
        }
        sandbox3d_set_statusf(state, false, false, "Scene legend is open in the Utility panel.");
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

    if (!ui_visible && henka_input_action_was_pressed(engine, HENKA_INPUT_ACTION_TOGGLE_MOUSE_CAPTURE))
    {
        henka_engine_set_mouse_capture(engine, !henka_engine_is_mouse_captured(engine));
        sandbox3d_set_statusf(state, false, false, "Mouse capture %s.", henka_engine_is_mouse_captured(engine) ? "enabled" : "released");
        sandbox3d_print_capture_state(engine, "bound action");
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
        if (!henka_engine_is_mouse_captured(engine) && henka_input_was_mouse_button_pressed(engine, HENKA_MOUSE_BUTTON_LEFT))
        {
            sandbox3d_try_pick_object(engine, state);
        }

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
    config.package_mode = HENKA_PACKAGE_MODE_AUTO;
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
