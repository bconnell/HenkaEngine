#include <stdio.h>

#include <henka/henka.h>

typedef struct sandbox3d_state
{
    henka_scene* scene;
    henka_camera camera;
    henka_mesh* cube_mesh;
    henka_mesh* ground_mesh;
    henka_mesh* grid_mesh;
    henka_shader* basic_shader;
    henka_shader* grid_shader;
    henka_entity cube_entity;
    henka_entity ground_entity;
    henka_entity grid_entity;
} sandbox3d_state;

static void sandbox3d_print_help(void)
{
    printf("Henka Engine Sandbox 3D\n");
    printf("Controls:\n");
    printf("  W A S D  Move camera on the ground plane\n");
    printf("  Q / E    Move camera down / up\n");
    printf("  Shift    Move faster\n");
    printf("  F1       Toggle wireframe\n");
    printf("  H        Print this help again\n");
    printf("  Escape   Exit the sandbox\n");
    printf("  Close    Exit the sandbox\n");
    printf("Current limitations:\n");
    printf("  Keyboard movement is available, but mouse look is not implemented yet.\n");
    printf("  Rendering is limited to built-in primitives, simple materials, and local shader files.\n");
    printf("  Text overlays and in-window help are planned after text and UI rendering exist.\n");
    fflush(stdout);
}

static henka_result sandbox3d_initialize(henka_engine* engine, void* user_data)
{
    henka_result result;
    henka_material cube_material;
    henka_material ground_material;
    henka_material grid_material;
    henka_transform transform;
    int framebuffer_height;
    int framebuffer_width;
    sandbox3d_state* state;

    state = (sandbox3d_state*)user_data;
    sandbox3d_print_help();

    result = henka_scene_create(&state->scene);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    result = henka_shader_create_from_files(engine, "assets/shaders/basic_lit.vert", "assets/shaders/basic_lit.frag", &state->basic_shader);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    result = henka_shader_create_from_files(engine, "assets/shaders/debug_grid.vert", "assets/shaders/debug_grid.frag", &state->grid_shader);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    result = henka_mesh_create_cube(engine, &state->cube_mesh);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    result = henka_mesh_create_plane(engine, 10.0f, 10.0f, &state->ground_mesh);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    result = henka_mesh_create_debug_grid(engine, 10, 1.0f, &state->grid_mesh);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    state->ground_entity = henka_scene_create_entity(state->scene);
    state->cube_entity = henka_scene_create_entity(state->scene);
    state->grid_entity = henka_scene_create_entity(state->scene);

    if (state->ground_entity == HENKA_INVALID_ENTITY ||
        state->cube_entity == HENKA_INVALID_ENTITY ||
        state->grid_entity == HENKA_INVALID_ENTITY)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }

    ground_material.shader = state->basic_shader;
    ground_material.base_color = (henka_vec4){0.28f, 0.31f, 0.34f, 1.0f};
    ground_material.use_lighting = true;

    cube_material.shader = state->basic_shader;
    cube_material.base_color = (henka_vec4){0.78f, 0.48f, 0.26f, 1.0f};
    cube_material.use_lighting = true;

    grid_material.shader = state->grid_shader;
    grid_material.base_color = (henka_vec4){0.30f, 0.55f, 0.74f, 1.0f};
    grid_material.use_lighting = false;

    transform = henka_transform_identity();
    transform.position.y = -0.02f;
    henka_scene_set_entity_transform(state->scene, state->ground_entity, transform);
    henka_scene_set_entity_mesh(state->scene, state->ground_entity, state->ground_mesh);
    henka_scene_set_entity_material(state->scene, state->ground_entity, ground_material);

    transform = henka_transform_identity();
    transform.position.y = 0.5f;
    henka_scene_set_entity_transform(state->scene, state->cube_entity, transform);
    henka_scene_set_entity_mesh(state->scene, state->cube_entity, state->cube_mesh);
    henka_scene_set_entity_material(state->scene, state->cube_entity, cube_material);

    transform = henka_transform_identity();
    henka_scene_set_entity_transform(state->scene, state->grid_entity, transform);
    henka_scene_set_entity_mesh(state->scene, state->grid_entity, state->grid_mesh);
    henka_scene_set_entity_material(state->scene, state->grid_entity, grid_material);

    henka_scene_set_light_direction(state->scene, (henka_vec3){-0.5f, -1.0f, -0.3f});
    henka_scene_set_ambient_color(state->scene, (henka_vec3){0.18f, 0.20f, 0.25f});

    result = henka_engine_get_framebuffer_size(engine, &framebuffer_width, &framebuffer_height);
    if (result != HENKA_SUCCESS || framebuffer_height <= 0)
    {
        framebuffer_width = 1280;
        framebuffer_height = 720;
    }

    state->camera = henka_camera_create_perspective(60.0f * HENKA_DEG_TO_RAD, (float)framebuffer_width / (float)framebuffer_height, 0.1f, 100.0f);
    state->camera.position = (henka_vec3){3.0f, 2.2f, 6.0f};
    state->camera.yaw_radians = -2.2f;
    state->camera.pitch_radians = -0.25f;

    result = henka_scene_set_camera(state->scene, &state->camera);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    result = henka_engine_set_scene(engine, state->scene);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    return henka_engine_set_wireframe(engine, false);
}

static void sandbox3d_update(henka_engine* engine, double delta_seconds, void* user_data)
{
    henka_transform cube_transform;
    int framebuffer_height;
    int framebuffer_width;
    sandbox3d_state* state;

    state = (sandbox3d_state*)user_data;

    if (henka_input_was_key_pressed(engine, HENKA_KEY_H))
    {
        sandbox3d_print_help();
    }

    if (henka_input_was_key_pressed(engine, HENKA_KEY_F1))
    {
        henka_engine_set_wireframe(engine, !henka_engine_is_wireframe_enabled(engine));
        printf("Wireframe: %s\n", henka_engine_is_wireframe_enabled(engine) ? "on" : "off");
    }

    if (henka_engine_get_framebuffer_size(engine, &framebuffer_width, &framebuffer_height) == HENKA_SUCCESS && framebuffer_height > 0)
    {
        henka_camera_set_aspect_ratio(&state->camera, (float)framebuffer_width / (float)framebuffer_height);
    }

    henka_camera_move_fly(&state->camera, engine, delta_seconds);
    henka_scene_set_camera(state->scene, &state->camera);

    cube_transform = henka_transform_identity();
    cube_transform.position = (henka_vec3){0.0f, 0.5f, 0.0f};
    cube_transform.rotation = henka_quat_from_euler(0.0f, (float)henka_engine_get_total_time(engine), 0.0f);
    henka_scene_set_entity_transform(state->scene, state->cube_entity, cube_transform);
}

static void sandbox3d_shutdown(henka_engine* engine, void* user_data)
{
    sandbox3d_state* state;

    (void)engine;

    state = (sandbox3d_state*)user_data;
    henka_engine_set_scene(engine, NULL);
    henka_mesh_destroy(state->grid_mesh);
    henka_mesh_destroy(state->ground_mesh);
    henka_mesh_destroy(state->cube_mesh);
    henka_shader_destroy(state->grid_shader);
    henka_shader_destroy(state->basic_shader);
    henka_scene_destroy(state->scene);
}

int main(void)
{
    henka_engine* engine;
    henka_engine_config config;
    henka_result result;
    sandbox3d_state state;

    state.scene = NULL;
    state.camera = henka_camera_create_perspective(60.0f * HENKA_DEG_TO_RAD, 16.0f / 9.0f, 0.1f, 100.0f);
    state.cube_mesh = NULL;
    state.ground_mesh = NULL;
    state.grid_mesh = NULL;
    state.basic_shader = NULL;
    state.grid_shader = NULL;
    state.cube_entity = HENKA_INVALID_ENTITY;
    state.ground_entity = HENKA_INVALID_ENTITY;
    state.grid_entity = HENKA_INVALID_ENTITY;

    config.application_name = "Henka Engine Sandbox 3D";
    config.window_width = 1280;
    config.window_height = 720;
    config.enable_vsync = true;
    config.on_initialize = sandbox3d_initialize;
    config.on_update = sandbox3d_update;
    config.on_shutdown = sandbox3d_shutdown;
    config.user_data = &state;

    result = henka_engine_create(&config, &engine);
    if (result != HENKA_SUCCESS)
    {
        HENKA_LOG_ERROR("sandbox startup failed: %s", henka_result_to_string(result));
        return 1;
    }

    result = henka_engine_run(engine);
    if (result != HENKA_SUCCESS)
    {
        HENKA_LOG_ERROR("sandbox run failed: %s", henka_result_to_string(result));
        henka_engine_destroy(engine);
        return 1;
    }

    henka_engine_destroy(engine);
    return 0;
}
