#include <stdio.h>
#include <string.h>

#include <henka/henka.h>

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
static const float g_ui_panel_width = 520.0f;
static const float g_ui_panel_min_height = 456.0f;

static float sandbox3d_get_mouse_sensitivity(const sandbox3d_state* state);

static henka_ui_rect sandbox3d_get_ui_panel_bounds(int framebuffer_width, int framebuffer_height)
{
    henka_ui_rect bounds;

    bounds.x = g_ui_panel_margin;
    bounds.y = g_ui_panel_margin;
    bounds.width = g_ui_panel_width;
    bounds.height = g_ui_panel_min_height;

    if (framebuffer_width > 0 && bounds.width > (float)framebuffer_width - g_ui_panel_margin * 2.0f)
    {
        bounds.width = (float)framebuffer_width - g_ui_panel_margin * 2.0f;
    }

    if (framebuffer_height > 0 && bounds.height > (float)framebuffer_height - g_ui_panel_margin * 2.0f)
    {
        bounds.height = (float)framebuffer_height - g_ui_panel_margin * 2.0f;
    }

    if (bounds.width < 340.0f)
    {
        bounds.width = 340.0f;
    }

    if (bounds.height < 320.0f)
    {
        bounds.height = 320.0f;
    }

    return bounds;
}

static henka_result sandbox3d_get_settings_path(const henka_engine* engine, char** out_path)
{
    return henka_path_resolve(henka_engine_get_user_data_base_path(engine), "sandbox3d.settings", out_path);
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

static void sandbox3d_print_scene_legend(const sandbox3d_state* state)
{
    printf("Scene examples:\n");
    printf("  %s: center view, shows textured material rendering.\n",
        sandbox3d_safe_entity_name(state, state != NULL ? state->cube_entity : HENKA_INVALID_ENTITY, "Textured Cube"));
    printf("  %s: under the scene, shows repeated local texture use.\n",
        sandbox3d_safe_entity_name(state, state != NULL ? state->ground_entity : HENKA_INVALID_ENTITY, "Ground"));
    printf("  %s: left side, shows an untextured material color.\n",
        sandbox3d_safe_entity_name(state, state != NULL ? state->colored_cube_entity : HENKA_INVALID_ENTITY, "Colored Cube"));
    printf("  %s: farther left, shows the current OBJ loading path.\n",
        sandbox3d_safe_entity_name(state, state != NULL ? state->marker_entity : HENKA_INVALID_ENTITY, "OBJ Marker"));
    printf("  %s: right side, shows the error texture fallback when a texture file is missing.\n",
        sandbox3d_safe_entity_name(state, state != NULL ? state->fallback_cube_entity : HENKA_INVALID_ENTITY, "Missing Texture"));
    printf("  %s: farther right, shows the fallback mesh when an OBJ file is missing.\n",
        sandbox3d_safe_entity_name(state, state != NULL ? state->fallback_model_entity : HENKA_INVALID_ENTITY, "Missing Model"));
    printf("  %s: spans the floor so you can judge position, depth, and movement.\n",
        sandbox3d_safe_entity_name(state, state != NULL ? state->grid_entity : HENKA_INVALID_ENTITY, "Debug Grid"));
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

    henka_settings_set_float(state->settings, "mouse_sensitivity", next_value);
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

    value = henka_settings_get_float(state->settings, "mouse_sensitivity", g_default_mouse_look_sensitivity);
    return value > 0.0f ? value : g_default_mouse_look_sensitivity;
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

    grid_visible = henka_settings_get_bool(state->settings, "grid_visible", true);
    wireframe_enabled = henka_settings_get_bool(state->settings, "wireframe_enabled", false);
    movement_speed = henka_settings_get_float(state->settings, "camera_movement_speed", g_default_camera_movement_speed);

    state->camera.position.x = henka_settings_get_float(state->settings, "camera_position_x", g_camera_start_position.x);
    state->camera.position.y = henka_settings_get_float(state->settings, "camera_position_y", g_camera_start_position.y);
    state->camera.position.z = henka_settings_get_float(state->settings, "camera_position_z", g_camera_start_position.z);
    state->camera.yaw_radians = henka_settings_get_float(state->settings, "camera_yaw_radians", g_camera_start_yaw);
    state->camera.pitch_radians = henka_settings_get_float(state->settings, "camera_pitch_radians", g_camera_start_pitch);
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

    if (henka_settings_get_float(state->settings, "mouse_sensitivity", g_default_mouse_look_sensitivity) <= 0.0f)
    {
        henka_settings_set_float(state->settings, "mouse_sensitivity", g_default_mouse_look_sensitivity);
    }
}

static henka_result sandbox3d_save_settings(henka_engine* engine, sandbox3d_state* state)
{
    char* settings_path;
    henka_result result;

    if (engine == NULL || state == NULL || state->settings == NULL || state->scene == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    henka_settings_set_bool(state->settings, "grid_visible", henka_scene_is_entity_visible(state->scene, state->grid_entity));
    henka_settings_set_bool(state->settings, "wireframe_enabled", henka_engine_is_wireframe_enabled(engine));
    henka_settings_set_float(state->settings, "mouse_sensitivity", sandbox3d_get_mouse_sensitivity(state));
    henka_settings_set_float(state->settings, "camera_movement_speed", state->camera.movement_speed);
    henka_settings_set_float(state->settings, "camera_position_x", state->camera.position.x);
    henka_settings_set_float(state->settings, "camera_position_y", state->camera.position.y);
    henka_settings_set_float(state->settings, "camera_position_z", state->camera.position.z);
    henka_settings_set_float(state->settings, "camera_yaw_radians", state->camera.yaw_radians);
    henka_settings_set_float(state->settings, "camera_pitch_radians", state->camera.pitch_radians);

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

static void sandbox3d_print_help(const sandbox3d_state* state)
{
    printf("Henka Engine Sandbox 3D\n");
    printf("This scene shows texture rendering, untextured material color, early OBJ loading, and visible fallback behavior for missing assets.\n");
    printf("Controls:\n");
    printf("  W A S D          Move across the scene\n");
    printf("  Q / E            Move down / up\n");
    printf("  Shift            Move faster\n");
    printf("  Mouse            Look around while mouse capture is active\n");
    printf("  Right Mouse / Tab Toggle mouse capture\n");
    printf("  F1               Toggle wireframe\n");
    printf("  F2               Print the scene legend again\n");
    printf("  F3               Show or hide the debug grid\n");
    printf("  F4               Show or hide the sandbox panel\n");
    printf("  H                Print controls and the scene legend again\n");
    printf("  Escape           Close the panel first. Then release the mouse. Then exit.\n");
    printf("Panel shortcuts:\n");
    printf("  Use the sandbox panel to toggle the grid and wireframe view, reset the camera, adjust movement settings, and save or reset local sandbox settings.\n");
    printf("  Mouse look and camera movement pause while the panel is open.\n");
    sandbox3d_print_scene_legend(state);
    printf("Manual QA focus:\n");
    printf("  Confirm each scene example is visible, mouse capture toggles cleanly, wireframe is readable, and the view stays stable when the window is resized.\n");
    printf("Current limitations:\n");
    printf("  OBJ loading is still early and currently limited to a small, documented subset.\n");
    printf("  OBJ material libraries, negative indices, animation, editor tools, and broader 2D or 2.5D workflows are not available yet.\n");
    printf("  The UI overlay is intentionally small and is not an editor.\n");
    printf("  Sandbox settings are saved locally beside the executable in the user folder.\n");
    fflush(stdout);
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

static void sandbox3d_format_display_path(const char* label, const char* path, char* buffer, size_t buffer_size)
{
    const char* value;
    size_t path_length;

    if (buffer == NULL || buffer_size == 0U)
    {
        return;
    }

    value = (path != NULL && path[0] != '\0') ? path : "(unavailable)";
    path_length = strlen(value);

    if (path_length > 54U)
    {
        snprintf(buffer, buffer_size, "%s: ...%s", label, value + path_length - 51U);
    }
    else
    {
        snprintf(buffer, buffer_size, "%s: %s", label, value);
    }
}

static void sandbox3d_print_ui_state(bool visible)
{
    printf("Sandbox panel: %s\n", visible ? "shown" : "hidden");
    fflush(stdout);
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
    float button_width;
    float fps;
    float milliseconds;
    float panel_bottom;
    float scale_text_y;
    float speed_text_y;
    bool grid_visible;
    bool wireframe_enabled;
    henka_ui_rect panel_bounds;
    henka_result result;
    henka_ui_frame_desc frame_desc;

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
        panel_bounds = sandbox3d_get_ui_panel_bounds(frame_desc.framebuffer_width, frame_desc.framebuffer_height);
        button_width = (panel_bounds.width - 52.0f) * 0.5f;
        panel_bottom = panel_bounds.y + panel_bounds.height;
        scale_text_y = panel_bounds.y + 221.0f;
        speed_text_y = panel_bounds.y + 261.0f;
        sandbox3d_format_display_path("Assets", henka_engine_get_asset_base_path(engine), asset_path_text, sizeof(asset_path_text));
        sandbox3d_format_display_path("User", henka_engine_get_user_data_base_path(engine), user_path_text, sizeof(user_path_text));
        sandbox3d_format_display_path("Settings", result == HENKA_SUCCESS ? settings_path : NULL, settings_path_text, sizeof(settings_path_text));
        snprintf(capture_text, sizeof(capture_text), "Capture: %s", henka_engine_is_mouse_captured(engine) ? "On" : "Off");
        grid_visible = henka_scene_is_entity_visible(state->scene, state->grid_entity);
        wireframe_enabled = henka_engine_is_wireframe_enabled(engine);
        milliseconds = (float)(henka_engine_get_delta_time(engine) * 1000.0);
        fps = milliseconds > 0.0f ? 1000.0f / milliseconds : 0.0f;
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

        henka_ui_panel(state->ui, panel_bounds, "Henka Sandbox");
        henka_ui_label(state->ui, panel_bounds.x + 14.0f, panel_bounds.y + 36.0f, 1.0f, "Scene Controls");
        if (henka_ui_toggle(state->ui, "grid", (henka_ui_rect){panel_bounds.x + 14.0f, panel_bounds.y + 54.0f, button_width, 32.0f}, "Debug Grid", &grid_visible))
        {
            printf("Debug grid: %s\n", grid_visible ? "shown" : "hidden");
            fflush(stdout);
        }
        henka_scene_set_entity_visible(state->scene, state->grid_entity, grid_visible);
        if (henka_ui_toggle(state->ui, "wireframe", (henka_ui_rect){panel_bounds.x + 26.0f + button_width, panel_bounds.y + 54.0f, button_width, 32.0f}, "Wireframe", &wireframe_enabled))
        {
            printf("Wireframe: %s\n", wireframe_enabled ? "on" : "off");
            fflush(stdout);
        }
        henka_engine_set_wireframe(engine, wireframe_enabled);

        if (henka_ui_button(state->ui, "reset_camera", (henka_ui_rect){panel_bounds.x + 14.0f, panel_bounds.y + 98.0f, button_width, 32.0f}, "Reset Camera"))
        {
            sandbox3d_reset_camera_defaults(state);
            printf("Camera reset to the default sandbox view.\n");
            fflush(stdout);
        }

        if (henka_ui_button(state->ui, "save_settings", (henka_ui_rect){panel_bounds.x + 26.0f + button_width, panel_bounds.y + 98.0f, button_width, 32.0f}, "Save Settings"))
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

        if (henka_ui_button(state->ui, "print_help", (henka_ui_rect){panel_bounds.x + 26.0f + button_width, panel_bounds.y + 142.0f, button_width, 32.0f}, "Print Help"))
        {
            sandbox3d_print_help(state);
        }

        if (henka_ui_button(state->ui, "print_legend", (henka_ui_rect){panel_bounds.x + 14.0f, panel_bounds.y + 186.0f, button_width, 32.0f}, "Scene Legend"))
        {
            sandbox3d_print_scene_legend(state);
            fflush(stdout);
        }

        henka_ui_label(state->ui, panel_bounds.x + 26.0f + button_width, panel_bounds.y + 194.0f, 1.0f, "Adjust");
        if (henka_ui_button(state->ui, "mouse_less", (henka_ui_rect){panel_bounds.x + 26.0f + button_width, panel_bounds.y + 214.0f, 72.0f, 28.0f}, "Less"))
        {
            sandbox3d_adjust_mouse_sensitivity(state, -0.0005f);
        }
        if (henka_ui_button(state->ui, "mouse_more", (henka_ui_rect){panel_bounds.x + 106.0f + button_width, panel_bounds.y + 214.0f, 72.0f, 28.0f}, "More"))
        {
            sandbox3d_adjust_mouse_sensitivity(state, 0.0005f);
        }
        if (henka_ui_button(state->ui, "speed_less", (henka_ui_rect){panel_bounds.x + 26.0f + button_width, panel_bounds.y + 254.0f, 72.0f, 28.0f}, "Less"))
        {
            sandbox3d_adjust_camera_speed(state, -0.5f);
        }
        if (henka_ui_button(state->ui, "speed_more", (henka_ui_rect){panel_bounds.x + 106.0f + button_width, panel_bounds.y + 254.0f, 72.0f, 28.0f}, "More"))
        {
            sandbox3d_adjust_camera_speed(state, 0.5f);
        }

        henka_ui_label(state->ui, panel_bounds.x + 14.0f, panel_bounds.y + 232.0f, 1.0f, "Status");
        henka_ui_label(state->ui, panel_bounds.x + 14.0f, panel_bounds.y + 250.0f, 1.0f, capture_text);
        henka_ui_label(state->ui, panel_bounds.x + 14.0f, panel_bounds.y + 266.0f, 1.0f, grid_visible ? "Grid: Visible" : "Grid: Hidden");
        henka_ui_label(state->ui, panel_bounds.x + 14.0f, panel_bounds.y + 282.0f, 1.0f, wireframe_enabled ? "Wireframe: On" : "Wireframe: Off");
        henka_ui_label(state->ui, panel_bounds.x + 14.0f, panel_bounds.y + 298.0f, 1.0f, fps_text);
        henka_ui_label(state->ui, panel_bounds.x + 14.0f, panel_bounds.y + 314.0f, 1.0f, camera_text);
        henka_ui_label(state->ui, panel_bounds.x + 26.0f + button_width, scale_text_y, 1.0f, mouse_text);
        henka_ui_label(state->ui, panel_bounds.x + 26.0f + button_width, speed_text_y, 1.0f, speed_text);
        henka_ui_label(state->ui, panel_bounds.x + 14.0f, panel_bottom - 84.0f, 1.0f, asset_path_text);
        henka_ui_label(state->ui, panel_bounds.x + 14.0f, panel_bottom - 68.0f, 1.0f, user_path_text);
        henka_ui_label(state->ui, panel_bounds.x + 14.0f, panel_bottom - 52.0f, 1.0f, settings_path_text);
        henka_ui_label(state->ui, panel_bounds.x + 14.0f, panel_bottom - 28.0f, 1.0f, "Escape closes the panel first. Close the panel to resume camera control.");

        henka_free(settings_path);
    }

    henka_ui_end_frame(state->ui);
}

static henka_result sandbox3d_initialize(henka_engine* engine, void* user_data)
{
    henka_asset_manager* assets;
    henka_result result;
    henka_material cube_material;
    henka_material fallback_material;
    henka_material fallback_model_material;
    henka_material ground_material;
    henka_material grid_material;
    henka_material colored_material;
    henka_material marker_material;
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
    state->grid_entity = henka_scene_create_entity_named(state->scene, "Debug Grid");
    state->colored_cube_entity = henka_scene_create_entity_named(state->scene, "Colored Cube");
    state->fallback_cube_entity = henka_scene_create_entity_named(state->scene, "Missing Texture");
    state->marker_entity = henka_scene_create_entity_named(state->scene, "OBJ Marker");
    state->fallback_model_entity = henka_scene_create_entity_named(state->scene, "Missing Model");

    if (state->ground_entity == HENKA_INVALID_ENTITY ||
        state->cube_entity == HENKA_INVALID_ENTITY ||
        state->grid_entity == HENKA_INVALID_ENTITY ||
        state->colored_cube_entity == HENKA_INVALID_ENTITY ||
        state->fallback_cube_entity == HENKA_INVALID_ENTITY ||
        state->marker_entity == HENKA_INVALID_ENTITY ||
        state->fallback_model_entity == HENKA_INVALID_ENTITY)
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

    transform = henka_transform_identity();
    transform.position.y = -0.02f;
    result = sandbox3d_configure_entity(state->scene, state->ground_entity, state->ground_mesh, ground_material, transform);
    if (result != HENKA_SUCCESS)
    {
        goto fail;
    }

    transform = henka_transform_identity();
    transform.position = g_textured_cube_position;
    result = sandbox3d_configure_entity(state->scene, state->cube_entity, state->cube_mesh, cube_material, transform);
    if (result != HENKA_SUCCESS)
    {
        goto fail;
    }

    transform = henka_transform_identity();
    transform.position = g_colored_cube_position;
    result = sandbox3d_configure_entity(state->scene, state->colored_cube_entity, state->cube_mesh, colored_material, transform);
    if (result != HENKA_SUCCESS)
    {
        goto fail;
    }

    transform = henka_transform_identity();
    transform.position = g_missing_texture_position;
    result = sandbox3d_configure_entity(state->scene, state->fallback_cube_entity, state->cube_mesh, fallback_material, transform);
    if (result != HENKA_SUCCESS)
    {
        goto fail;
    }

    transform = henka_transform_identity();
    transform.position = g_marker_position;
    transform.scale = (henka_vec3){1.35f, 1.35f, 1.35f};
    result = sandbox3d_configure_entity(state->scene, state->marker_entity, state->marker_mesh, marker_material, transform);
    if (result != HENKA_SUCCESS)
    {
        goto fail;
    }

    transform = henka_transform_identity();
    transform.position = g_missing_model_position;
    transform.scale = (henka_vec3){0.95f, 0.95f, 0.95f};
    result = sandbox3d_configure_entity(state->scene, state->fallback_model_entity, state->missing_model_mesh, fallback_model_material, transform);
    if (result != HENKA_SUCCESS)
    {
        goto fail;
    }

    transform = henka_transform_identity();
    result = sandbox3d_configure_entity(state->scene, state->grid_entity, state->grid_mesh, grid_material, transform);
    if (result != HENKA_SUCCESS)
    {
        goto fail;
    }

    henka_scene_set_light_direction(state->scene, (henka_vec3){-0.5f, -1.0f, -0.3f});
    henka_scene_set_ambient_color(state->scene, (henka_vec3){0.18f, 0.20f, 0.25f});

    result = henka_engine_get_framebuffer_size(engine, &framebuffer_width, &framebuffer_height);
    if (result != HENKA_SUCCESS || framebuffer_height <= 0)
    {
        framebuffer_width = 1280;
        framebuffer_height = 720;
    }

    state->camera = henka_camera_create_perspective(60.0f * HENKA_DEG_TO_RAD, (float)framebuffer_width / (float)framebuffer_height, 0.1f, 100.0f);
    state->camera.position = g_camera_start_position;
    state->camera.yaw_radians = g_camera_start_yaw;
    state->camera.pitch_radians = g_camera_start_pitch;
    state->camera.movement_speed = g_default_camera_movement_speed;

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

    result = henka_engine_set_mouse_capture(engine, false);
    if (result != HENKA_SUCCESS)
    {
        goto fail;
    }

    sandbox3d_print_help(state);

    return HENKA_SUCCESS;

fail:
    sandbox3d_release_owned_resources(state);
    return result;
}

static void sandbox3d_update(henka_engine* engine, double delta_seconds, void* user_data)
{
    henka_transform cube_transform;
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
        henka_engine_set_wireframe(engine, !henka_engine_is_wireframe_enabled(engine));
        printf("Wireframe: %s\n", henka_engine_is_wireframe_enabled(engine) ? "on" : "off");
        fflush(stdout);
    }

    if (henka_input_was_key_pressed(engine, HENKA_KEY_F3))
    {
        bool visible;

        visible = henka_scene_is_entity_visible(state->scene, state->grid_entity);
        henka_scene_set_entity_visible(state->scene, state->grid_entity, !visible);
        printf("Debug grid: %s\n", visible ? "hidden" : "shown");
        fflush(stdout);
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

    cube_transform = henka_transform_identity();
    cube_transform.position = g_textured_cube_position;
    cube_transform.rotation = henka_quat_from_euler(0.0f, (float)henka_engine_get_total_time(engine), 0.0f);
    henka_scene_set_entity_transform(state->scene, state->cube_entity, cube_transform);

    cube_transform = henka_transform_identity();
    cube_transform.position = g_marker_position;
    cube_transform.scale = (henka_vec3){1.35f, 1.35f, 1.35f};
    cube_transform.rotation = henka_quat_from_euler(0.0f, (float)henka_engine_get_total_time(engine) * 0.5f, 0.0f);
    henka_scene_set_entity_transform(state->scene, state->marker_entity, cube_transform);

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

    state.scene = NULL;
    state.settings = NULL;
    state.ui = NULL;
    state.camera = henka_camera_create_perspective(60.0f * HENKA_DEG_TO_RAD, 16.0f / 9.0f, 0.1f, 100.0f);
    state.cube_mesh = NULL;
    state.ground_mesh = NULL;
    state.grid_mesh = NULL;
    state.marker_mesh = NULL;
    state.missing_model_mesh = NULL;
    state.basic_shader = NULL;
    state.grid_shader = NULL;
    state.cube_texture = NULL;
    state.ground_texture = NULL;
    state.missing_texture = NULL;
    state.cube_entity = HENKA_INVALID_ENTITY;
    state.ground_entity = HENKA_INVALID_ENTITY;
    state.grid_entity = HENKA_INVALID_ENTITY;
    state.colored_cube_entity = HENKA_INVALID_ENTITY;
    state.fallback_cube_entity = HENKA_INVALID_ENTITY;
    state.marker_entity = HENKA_INVALID_ENTITY;
    state.fallback_model_entity = HENKA_INVALID_ENTITY;
    state.ui_visible_last_frame = false;

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
