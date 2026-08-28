#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <henka/henka.h>
#include <henka/scene_behavior_runtime.h>
#include <henka/script_state.h>

typedef struct external_graphical_terrain_state
{
    henka_scene* scene;
    henka_scene* runtime_scene;
    henka_terrain_world* world;
    henka_terrain_render_runtime* render;
    henka_terrain_sample* samples;
    henka_authoring_mesh* authored_mesh;
    henka_mesh* authored_render_mesh;
    henka_shader* authored_shader;
    henka_physics_world* authored_physics;
    henka_physics_body_id authored_body;
    henka_entity authored_entity;
    uint64_t start_frame;
    bool success;
} external_graphical_terrain_state;

typedef struct external_audio_state
{
    henka_scene* scene;
    henka_audio_system* audio_system;
    henka_audio_emitter* emitter;
    henka_scene_document* document;
    henka_scene_document* reloaded_document;
    henka_entity entity;
    bool success;
} external_audio_state;

static void external_write_u16(unsigned char* bytes, size_t offset, uint16_t value)
{
    bytes[offset] = (unsigned char)(value & 0xffU);
    bytes[offset + 1U] = (unsigned char)((value >> 8U) & 0xffU);
}

static void external_write_u32(unsigned char* bytes, size_t offset, uint32_t value)
{
    bytes[offset] = (unsigned char)(value & 0xffU);
    bytes[offset + 1U] = (unsigned char)((value >> 8U) & 0xffU);
    bytes[offset + 2U] = (unsigned char)((value >> 16U) & 0xffU);
    bytes[offset + 3U] = (unsigned char)((value >> 24U) & 0xffU);
}

static bool external_write_audio_fixture(const char* path)
{
    const size_t frame_count = 128U;
    const size_t data_size = frame_count * sizeof(int16_t);
    const size_t file_size = 44U + data_size;
    unsigned char* bytes = (unsigned char*)calloc(1U, file_size);
    FILE* file = NULL;
    size_t frame_index;
    bool success;

    if (bytes == NULL)
    {
        return false;
    }
    memcpy(bytes, "RIFF", 4U);
    external_write_u32(bytes, 4U, (uint32_t)(file_size - 8U));
    memcpy(bytes + 8U, "WAVEfmt ", 8U);
    external_write_u32(bytes, 16U, 16U);
    external_write_u16(bytes, 20U, 1U);
    external_write_u16(bytes, 22U, 1U);
    external_write_u32(bytes, 24U, HENKA_AUDIO_DEFAULT_SAMPLE_RATE);
    external_write_u32(bytes, 28U, HENKA_AUDIO_DEFAULT_SAMPLE_RATE * 2U);
    external_write_u16(bytes, 32U, 2U);
    external_write_u16(bytes, 34U, 16U);
    memcpy(bytes + 36U, "data", 4U);
    external_write_u32(bytes, 40U, (uint32_t)data_size);
    for (frame_index = 0U; frame_index < frame_count; ++frame_index)
    {
        external_write_u16(bytes, 44U + frame_index * 2U, 8192U);
    }
#if defined(_WIN32)
    if (fopen_s(&file, path, "wb") != 0)
    {
        file = NULL;
    }
#else
    file = fopen(path, "wb");
#endif
    success = file != NULL && fwrite(bytes, 1U, file_size, file) == file_size;
    if (file != NULL && fclose(file) != 0)
    {
        success = false;
    }
    free(bytes);
    return success;
}

static bool external_audio_nonzero(const float* samples)
{
    return samples != NULL &&
        (fabsf(samples[0]) > 0.0001f || fabsf(samples[1]) > 0.0001f);
}

static void external_audio_destroy(external_audio_state* state)
{
    if (state == NULL)
    {
        return;
    }
    henka_audio_emitter_destroy(state->emitter);
    henka_audio_system_destroy(state->audio_system);
    henka_scene_destroy(state->scene);
    henka_scene_document_destroy(state->reloaded_document);
    henka_scene_document_destroy(state->document);
    state->emitter = NULL;
    state->audio_system = NULL;
    state->scene = NULL;
    state->reloaded_document = NULL;
    state->document = NULL;
    state->entity = HENKA_INVALID_ENTITY;
    remove("external_audio_workflow.wav");
    remove("external_audio_workflow.hnscene");
}

static henka_result external_audio_initialize(
    henka_engine* engine,
    void* user_data)
{
    external_audio_state* state = (external_audio_state*)user_data;
    henka_scene_document_object object = henka_scene_document_object_default();
    henka_scene_document_object reloaded_object = henka_scene_document_object_default();
    henka_scene_document_id object_id = HENKA_INVALID_SCENE_DOCUMENT_ID;
    henka_audio_clip* clip = NULL;
    henka_asset_metadata metadata = {0};
    henka_audio_emitter_config emitter_config;
    henka_audio_listener listener;
    float first_samples[HENKA_AUDIO_OUTPUT_CHANNELS];
    float second_samples[HENKA_AUDIO_OUTPUT_CHANNELS];
    float near_samples[HENKA_AUDIO_OUTPUT_CHANNELS];
    float far_samples[HENKA_AUDIO_OUTPUT_CHANNELS];
    float near_sum;
    float far_sum;
    henka_result result;

    if (engine == NULL || state == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    state->entity = HENKA_INVALID_ENTITY;
    if (!external_write_audio_fixture("external_audio_workflow.wav"))
    {
        return HENKA_ERROR_ASSET_SOURCE;
    }
    result = henka_scene_document_create(&state->document);
    if (result == HENKA_SUCCESS)
    {
        result = henka_scene_document_create(&state->reloaded_document);
    }
    if (result != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    (void)snprintf(object.name, sizeof(object.name), "%s", "External Audio Object");
    object.source.kind = HENKA_SCENE_DOCUMENT_SOURCE_PRIMITIVE;
    object.source.primitive = HENKA_SCENE_DOCUMENT_PRIMITIVE_BOX;
    object.source.primitive_dimensions = (henka_vec3){1.0f, 1.0f, 1.0f};
    object.audio.enabled = true;
    object.audio.looping = true;
    object.audio.spatial = true;
    (void)snprintf(
        object.audio.clip_path,
        sizeof(object.audio.clip_path),
        "%s",
        "external_audio_workflow.wav");
    result = henka_scene_document_add_object(
        state->document, &object, &object_id);
    if (result == HENKA_SUCCESS)
    {
        result = henka_scene_document_save_file(
            state->document, ".", "external_audio_workflow.hnscene");
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_scene_document_load_file(
            state->reloaded_document, ".", "external_audio_workflow.hnscene");
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_scene_document_get_object(
            state->reloaded_document, object_id, &reloaded_object);
    }
    if (result != HENKA_SUCCESS || !reloaded_object.audio.enabled ||
        !reloaded_object.audio.looping || !reloaded_object.audio.spatial ||
        strcmp(reloaded_object.audio.clip_path, object.audio.clip_path) != 0)
    {
        result = HENKA_ERROR_INVALID_ARGUMENT;
        goto cleanup;
    }
    result = henka_scene_create(&state->scene);
    if (result == HENKA_SUCCESS)
    {
        state->entity = henka_scene_create_entity_named(
            state->scene, reloaded_object.name);
        result = state->entity == HENKA_INVALID_ENTITY
            ? HENKA_ERROR_OUT_OF_MEMORY
            : HENKA_SUCCESS;
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_scene_set_entity_transform(
            state->scene,
            state->entity,
            (henka_transform){
                (henka_vec3){1.0f, 0.0f, -4.0f},
                (henka_quat){0.0f, 0.0f, 0.0f, 1.0f},
                (henka_vec3){1.0f, 1.0f, 1.0f}});
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_audio_system_create(NULL, &state->audio_system);
    }
    emitter_config = reloaded_object.audio;
    if (result == HENKA_SUCCESS)
    {
        result = henka_assets_load_audio_clip(
            henka_engine_get_asset_manager(engine),
            emitter_config.clip_path,
            &clip);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_assets_get_audio_metadata(
            henka_engine_get_asset_manager(engine), clip, &metadata);
    }
    if (result == HENKA_SUCCESS && metadata.type != HENKA_ASSET_TYPE_AUDIO)
    {
        result = HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_audio_emitter_create_with_clip(
            state->audio_system,
            state->scene,
            state->entity,
            clip,
            &emitter_config,
            &state->emitter);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_audio_system_mix(
            state->audio_system, first_samples, 1U);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_scene_translate_entity(
            state->scene, state->entity, (henka_vec3){-2.0f, 0.0f, 0.0f});
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_audio_system_mix(
            state->audio_system, second_samples, 1U);
    }
    if (result != HENKA_SUCCESS || !external_audio_nonzero(first_samples) ||
        !external_audio_nonzero(second_samples) ||
        first_samples[1] <= first_samples[0] ||
        second_samples[0] <= second_samples[1])
    {
        result = HENKA_ERROR_UNKNOWN;
        goto cleanup;
    }
    listener = henka_audio_listener_default();
    listener.position = (henka_vec3){-1.0f, 0.0f, 0.0f};
    result = henka_audio_system_set_listener(state->audio_system, listener);
    if (result == HENKA_SUCCESS)
    {
        result = henka_audio_system_mix(
            state->audio_system, near_samples, 1U);
    }
    listener.position = (henka_vec3){1.0f, 0.0f, 0.0f};
    if (result == HENKA_SUCCESS)
    {
        result = henka_audio_system_set_listener(state->audio_system, listener);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_audio_system_mix(
            state->audio_system, far_samples, 1U);
    }
    near_sum = fabsf(near_samples[0]) + fabsf(near_samples[1]);
    far_sum = fabsf(far_samples[0]) + fabsf(far_samples[1]);
    if (result != HENKA_SUCCESS || near_sum <= far_sum)
    {
        result = HENKA_ERROR_UNKNOWN;
        goto cleanup;
    }
    henka_scene_destroy_entity(state->scene, state->entity);
    result = henka_audio_system_mix(state->audio_system, far_samples, 1U);
    if (result != HENKA_SUCCESS ||
        henka_audio_system_get_active_voice_count(state->audio_system) != 0U ||
        henka_audio_emitter_is_valid(state->emitter))
    {
        result = HENKA_ERROR_UNKNOWN;
        goto cleanup;
    }
    state->success = true;
    printf("External public Audio asset, real scene object, persistence, spatial movement, listener movement, and stale cleanup workflow passed.\n");

cleanup:
    external_audio_destroy(state);
    return result;
}

static void external_audio_update(
    henka_engine* engine,
    double delta_seconds,
    void* user_data)
{
    (void)delta_seconds;
    (void)user_data;
    henka_engine_request_exit(engine);
}

static void external_audio_shutdown(henka_engine* engine, void* user_data)
{
    (void)engine;
    external_audio_destroy((external_audio_state*)user_data);
}

static bool external_audio_workflow(void)
{
    external_audio_state state = {0};
    henka_engine_config config = {0};
    henka_engine* engine = NULL;
    henka_result result;

    config.application_name = "Henka External Audio Consumer";
    config.window_width = 320;
    config.window_height = 240;
    config.user_data_base_path = "external_audio_user_data";
    config.package_mode = HENKA_PACKAGE_MODE_DEVELOPMENT;
    config.on_initialize = external_audio_initialize;
    config.on_update = external_audio_update;
    config.on_shutdown = external_audio_shutdown;
    config.user_data = &state;
    result = henka_engine_create(&config, &engine);
    if (result == HENKA_SUCCESS)
    {
        result = henka_engine_run(engine);
    }
    henka_engine_destroy(engine);
    external_audio_destroy(&state);
    return result == HENKA_SUCCESS && state.success;
}

static bool external_authoring_bounds(
    const henka_authoring_render_data* render_data,
    henka_bounds* out_bounds)
{
    henka_vec3 minimum;
    henka_vec3 maximum;
    size_t index;
    if (render_data == NULL || out_bounds == NULL || render_data->vertices == NULL ||
        render_data->vertex_count == 0U)
    {
        return false;
    }
    minimum = render_data->vertices[0].position;
    maximum = minimum;
    for (index = 1U; index < render_data->vertex_count; ++index)
    {
        const henka_vec3 point = render_data->vertices[index].position;
        minimum.x = fminf(minimum.x, point.x);
        minimum.y = fminf(minimum.y, point.y);
        minimum.z = fminf(minimum.z, point.z);
        maximum.x = fmaxf(maximum.x, point.x);
        maximum.y = fmaxf(maximum.y, point.y);
        maximum.z = fmaxf(maximum.z, point.z);
    }
    out_bounds->center = henka_vec3_scale(henka_vec3_add(minimum, maximum), 0.5f);
    out_bounds->extents = henka_vec3_scale(henka_vec3_subtract(maximum, minimum), 0.5f);
    return true;
}

static henka_result external_authoring_render_mesh(
    henka_engine* engine,
    const henka_authoring_mesh* source,
    henka_mesh** out_mesh,
    henka_bounds* out_bounds)
{
    henka_authoring_render_vertex* source_vertices = NULL;
    uint32_t* source_indices = NULL;
    henka_model_vertex* model_vertices = NULL;
    uint32_t* model_indices = NULL;
    henka_authoring_render_data render_data = {0};
    henka_model_data model = {0};
    henka_result result = HENKA_ERROR_INVALID_ARGUMENT;
    size_t index;
    henka_material material;

    if (engine == NULL || source == NULL || out_mesh == NULL || out_bounds == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_mesh = NULL;
    source_vertices = henka_calloc(256U, sizeof(*source_vertices));
    source_indices = henka_calloc(768U, sizeof(*source_indices));
    if (source_vertices == NULL || source_indices == NULL)
    {
        goto cleanup;
    }
    render_data.vertices = source_vertices;
    render_data.vertex_capacity = 256U;
    render_data.indices = source_indices;
    render_data.index_capacity = 768U;
    result = henka_authoring_mesh_evaluate(source, &render_data);
    if (result != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    if (!external_authoring_bounds(&render_data, out_bounds))
    {
        result = HENKA_ERROR_INVALID_ARGUMENT;
        goto cleanup;
    }
    model_vertices = henka_calloc(render_data.vertex_count, sizeof(*model_vertices));
    model_indices = henka_calloc(render_data.index_count, sizeof(*model_indices));
    if (model_vertices == NULL || model_indices == NULL)
    {
        result = HENKA_ERROR_OUT_OF_MEMORY;
        goto cleanup;
    }
    for (index = 0U; index < render_data.vertex_count; ++index)
    {
        model_vertices[index].position = render_data.vertices[index].position;
        model_vertices[index].normal = render_data.vertices[index].normal;
        model_vertices[index].tangent = render_data.vertices[index].tangent;
        /* The evaluated tangent may be parallel to a box face normal; let the
         * renderer derive an orthogonal frame for this public upload path. */
        model_vertices[index].tangent_valid = false;
        model_vertices[index].uv = render_data.vertices[index].uv;
        model_vertices[index].color = (henka_vec4){1.0f, 1.0f, 1.0f, 1.0f};
        model_vertices[index].material_region = render_data.vertices[index].material_region;
    }
    memcpy(model_indices, render_data.indices, render_data.index_count * sizeof(*model_indices));
    material = henka_material_default();
    material.base_color = (henka_vec4){0.18f, 0.52f, 0.86f, 1.0f};
    material.roughness = 0.32f;
    model.vertices = model_vertices;
    model.vertex_count = (uint32_t)render_data.vertex_count;
    model.indices = model_indices;
    model.index_count = (uint32_t)render_data.index_count;
    model.has_material = true;
    model.material_source.material = material;
    result = henka_mesh_create_from_model_data(engine, &model, out_mesh);

cleanup:
    henka_free(model_indices);
    henka_free(model_vertices);
    henka_free(source_indices);
    henka_free(source_vertices);
    return result;
}

static void external_graphical_terrain_destroy(external_graphical_terrain_state* state)
{
    if (state == NULL)
    {
        return;
    }
    henka_terrain_render_runtime_destroy(state->render);
    if (state->authored_physics != NULL && state->authored_body != HENKA_INVALID_PHYSICS_BODY_ID)
    {
        (void)henka_physics_body_destroy(state->authored_physics, state->authored_body);
    }
    henka_physics_world_destroy(state->authored_physics);
    henka_scene_destroy(state->runtime_scene);
    henka_scene_destroy(state->scene);
    henka_shader_destroy(state->authored_shader);
    henka_terrain_world_destroy(state->world);
    henka_mesh_destroy(state->authored_render_mesh);
    henka_authoring_mesh_destroy(state->authored_mesh);
    henka_free(state->samples);
    state->render = NULL;
    state->scene = NULL;
    state->runtime_scene = NULL;
    state->world = NULL;
    state->samples = NULL;
    state->authored_render_mesh = NULL;
    state->authored_shader = NULL;
    state->authored_mesh = NULL;
    state->authored_physics = NULL;
    state->authored_body = HENKA_INVALID_PHYSICS_BODY_ID;
    state->authored_entity = HENKA_INVALID_ENTITY;
}

static henka_result external_authoring_initialize(
    henka_engine* engine,
    external_graphical_terrain_state* state)
{
    henka_authoring_mesh_desc mesh_desc = henka_authoring_mesh_desc_default();
    henka_authoring_mesh* loaded_mesh = NULL;
    henka_authoring_mesh* history_candidate = NULL;
    henka_authoring_mesh_history* history = NULL;
    henka_authoring_face_id face_id = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_face_id extruded_face_id = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_edge_id edge_id = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_vertex_id vertex_id = HENKA_AUTHORING_INVALID_ID;
    henka_bounds bounds;
    henka_transform transform = henka_transform_identity();
    henka_entity duplicate = HENKA_INVALID_ENTITY;
    henka_material material;
    henka_ray pick_ray;
    henka_entity picked_entity = HENKA_INVALID_ENTITY;
    float picked_distance = 0.0f;
    henka_physics_body_desc body_desc = {0};
    henka_physics_raycast_hit physics_hit = {0};
    henka_result result;

    if (engine == NULL || state == NULL || state->scene == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    mesh_desc.max_vertices = 128U;
    mesh_desc.max_edges = 256U;
    mesh_desc.max_faces = 128U;
    mesh_desc.max_face_corners = 8U;
    result = henka_authoring_mesh_create_box(
        &mesh_desc, 2.0f, 2.0f, 2.0f, &state->authored_mesh);
    if (result != HENKA_SUCCESS)
    {
        printf("External authoring failure: create box (%s).\n", henka_result_to_string(result));
        return result;
    }

    for (uint32_t candidate_id = 1U; candidate_id < 128U; ++candidate_id)
    {
        if (vertex_id == HENKA_AUTHORING_INVALID_ID &&
            henka_authoring_mesh_get_vertex(state->authored_mesh, candidate_id) != NULL)
            vertex_id = candidate_id;
        if (edge_id == HENKA_AUTHORING_INVALID_ID &&
            henka_authoring_mesh_get_edge(state->authored_mesh, candidate_id) != NULL)
            edge_id = candidate_id;
        if (face_id == HENKA_AUTHORING_INVALID_ID &&
            henka_authoring_mesh_get_face(state->authored_mesh, candidate_id) != NULL)
            face_id = candidate_id;
    }
    if (vertex_id == HENKA_AUTHORING_INVALID_ID || edge_id == HENKA_AUTHORING_INVALID_ID ||
        face_id == HENKA_AUTHORING_INVALID_ID ||
        henka_authoring_mesh_set_vertex_position(
            state->authored_mesh, vertex_id, (henka_vec3){-1.05f, -1.0f, -1.0f}) != HENKA_SUCCESS ||
        henka_authoring_mesh_set_edge_hard(state->authored_mesh, edge_id, true) != HENKA_SUCCESS ||
        henka_authoring_mesh_set_face_material_region(state->authored_mesh, face_id, 2U) != HENKA_SUCCESS ||
        henka_authoring_mesh_extrude_face(
            state->authored_mesh, face_id, 0.2f, &extruded_face_id) != HENKA_SUCCESS ||
        !henka_authoring_mesh_validate(state->authored_mesh) ||
        henka_authoring_mesh_save_file(state->authored_mesh, "external_authoring_workflow.hams") != HENKA_SUCCESS)
    {
        printf("External authoring failure: edit/extrude/save (%s).\n", henka_result_to_string(HENKA_ERROR_INVALID_ARGUMENT));
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_authoring_mesh_create(&mesh_desc, &loaded_mesh);
    if (result == HENKA_SUCCESS)
    {
        result = henka_authoring_mesh_load_file(
            loaded_mesh, "external_authoring_workflow.hams");
    }
    if (result != HENKA_SUCCESS)
    {
        printf("External authoring failure: reload (%s).\n", henka_result_to_string(result));
        henka_authoring_mesh_destroy(loaded_mesh);
        return result;
    }
    henka_authoring_mesh_destroy(state->authored_mesh);
    state->authored_mesh = loaded_mesh;
    loaded_mesh = NULL;

    result = henka_authoring_mesh_history_create(state->authored_mesh, 4U, &history);
    if (result == HENKA_SUCCESS)
        result = henka_authoring_mesh_clone(state->authored_mesh, &history_candidate);
    if (result == HENKA_SUCCESS)
        result = henka_authoring_mesh_set_vertex_position(
            history_candidate, vertex_id, (henka_vec3){-1.0f, -1.0f, -1.0f});
    if (result == HENKA_SUCCESS)
        result = henka_authoring_mesh_history_checkpoint(history, history_candidate);
    if (result == HENKA_SUCCESS)
        result = henka_authoring_mesh_history_undo(history, history_candidate);
    if (result == HENKA_SUCCESS)
        result = henka_authoring_mesh_history_redo(history, history_candidate);
    henka_authoring_mesh_destroy(history_candidate);
    henka_authoring_mesh_history_destroy(history);
    if (result != HENKA_SUCCESS ||
        !henka_authoring_mesh_get_face(state->authored_mesh, extruded_face_id) ||
        !henka_authoring_mesh_get_edge(state->authored_mesh, edge_id))
    {
        printf("External authoring failure: history or identity continuity (%s).\n", henka_result_to_string(result));
        return HENKA_ERROR_UNKNOWN;
    }
    result = external_authoring_render_mesh(
        engine, state->authored_mesh, &state->authored_render_mesh, &bounds);
    if (result != HENKA_SUCCESS)
    {
        printf("External authoring failure: evaluate renderer mesh (%s).\n", henka_result_to_string(result));
        return result;
    }
    result = henka_shader_create_from_files(
        engine,
        "assets/shaders/basic_lit.vert",
        "assets/shaders/basic_lit.frag",
        &state->authored_shader);
    if (result != HENKA_SUCCESS)
    {
        printf("External authoring failure: shader (%s).\n", henka_result_to_string(result));
        return result;
    }
    state->authored_entity = henka_scene_create_entity_named(state->scene, "External Authored Box");
    if (state->authored_entity == HENKA_INVALID_ENTITY)
    {
        printf("External authoring failure: create entity.\n");
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    transform.position = (henka_vec3){32.0f, 3.0f, 32.0f};
    material = henka_material_default();
    material.shader = state->authored_shader;
    material.base_color = (henka_vec4){0.18f, 0.52f, 0.86f, 1.0f};
    material.roughness = 0.32f;
    result = henka_scene_set_entity_transform(state->scene, state->authored_entity, transform);
    if (result == HENKA_SUCCESS)
        result = henka_scene_set_entity_mesh(state->scene, state->authored_entity, state->authored_render_mesh);
    if (result == HENKA_SUCCESS)
        result = henka_scene_set_entity_material(state->scene, state->authored_entity, material);
    if (result == HENKA_SUCCESS)
        result = henka_scene_set_entity_local_bounds(state->scene, state->authored_entity, bounds);
    if (result != HENKA_SUCCESS)
    {
        printf("External authoring failure: install entity state (%s).\n", henka_result_to_string(result));
        return result;
    }

    result = henka_scene_clone(state->scene, &state->runtime_scene);
    if (result != HENKA_SUCCESS || state->runtime_scene == state->scene ||
        !henka_scene_is_entity_valid(state->runtime_scene, state->authored_entity))
    {
        printf("External authoring failure: runtime scene clone (%s).\n",
            henka_result_to_string(result));
        return result == HENKA_SUCCESS ? HENKA_ERROR_UNKNOWN : result;
    }
    {
        henka_transform runtime_transform = transform;
        henka_transform source_transform = {0};
        henka_transform read_back = {0};
        runtime_transform.position.x += 4.0f;
        if (henka_scene_set_entity_transform(
                state->runtime_scene, state->authored_entity, runtime_transform) != HENKA_SUCCESS ||
            henka_scene_get_entity_transform(
                state->scene, state->authored_entity, &source_transform) != HENKA_SUCCESS ||
            henka_scene_get_entity_transform(
                state->runtime_scene, state->authored_entity, &read_back) != HENKA_SUCCESS ||
            source_transform.position.x != transform.position.x ||
            read_back.position.x != runtime_transform.position.x)
        {
            printf("External authoring failure: runtime scene isolation.\n");
            return HENKA_ERROR_UNKNOWN;
        }
    }

    duplicate = henka_scene_create_entity_named(state->scene, "External Authored Duplicate");
    if (duplicate == HENKA_INVALID_ENTITY ||
        henka_scene_set_entity_transform(state->scene, duplicate, transform) != HENKA_SUCCESS ||
        henka_scene_set_entity_mesh(state->scene, duplicate, state->authored_render_mesh) != HENKA_SUCCESS ||
        henka_scene_set_entity_material(state->scene, duplicate, material) != HENKA_SUCCESS ||
        henka_scene_set_entity_local_bounds(state->scene, duplicate, bounds) != HENKA_SUCCESS)
    {
        printf("External authoring failure: duplicate/delete.\n");
        if (duplicate != HENKA_INVALID_ENTITY) henka_scene_destroy_entity(state->scene, duplicate);
        return HENKA_ERROR_UNKNOWN;
    }
    henka_scene_destroy_entity(state->scene, duplicate);
    if (henka_scene_is_entity_valid(state->scene, duplicate))
    {
        printf("External authoring failure: scene pick or physics create.\n");
        return HENKA_ERROR_UNKNOWN;
    }

    pick_ray.origin = (henka_vec3){32.0f, 3.5f, 72.0f};
    pick_ray.direction = (henka_vec3){0.0f, 0.0f, -1.0f};
    if (henka_scene_pick_entity(state->scene, pick_ray, &picked_entity, &picked_distance) != HENKA_SUCCESS ||
        picked_entity != state->authored_entity ||
        henka_physics_world_create(&state->authored_physics) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_UNKNOWN;
    }
    body_desc.type = HENKA_PHYSICS_BODY_STATIC;
    body_desc.transform = transform;
    body_desc.mass = 1.0f;
    body_desc.material = henka_physics_material_default();
    body_desc.collider = henka_physics_collider_box(bounds.extents);
    body_desc.linked_scene = state->scene;
    body_desc.linked_entity = state->authored_entity;
    if (henka_physics_body_create(
            state->authored_physics, &body_desc, &state->authored_body) != HENKA_SUCCESS ||
        henka_physics_world_raycast(
            state->authored_physics, pick_ray, 100.0f, HENKA_PHYSICS_ALL_LAYERS, &physics_hit) != HENKA_SUCCESS ||
        !physics_hit.hit)
    {
        printf("External authoring failure: linked physics raycast.\n");
        return HENKA_ERROR_UNKNOWN;
    }
    printf("External public authoring mesh, scene, runtime clone isolation, collision, duplicate/delete, and reload handoff passed.\n");
    return HENKA_SUCCESS;
}

static henka_result external_graphical_terrain_initialize(
    henka_engine* engine,
    void* user_data)
{
    external_graphical_terrain_state* state = (external_graphical_terrain_state*)user_data;
    henka_terrain_world_desc world_desc = henka_terrain_world_desc_default();
    henka_terrain_layout layout = {0};
    henka_camera camera;
    henka_terrain_render_desc render_desc;
    size_t index;
    if (engine == NULL || state == NULL ||
        henka_terrain_world_desc_get_layout(&world_desc, &layout) != HENKA_SUCCESS ||
        henka_scene_create(&state->scene) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (external_authoring_initialize(engine, state) != HENKA_SUCCESS)
    {
        external_graphical_terrain_destroy(state);
        return HENKA_ERROR_UNKNOWN;
    }
    world_desc.max_resident_regions = 1U;
    world_desc.max_resident_chunks = 1U;
    if (henka_terrain_world_create(&world_desc, &state->world) != HENKA_SUCCESS)
    {
        external_graphical_terrain_destroy(state);
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    state->samples = henka_calloc(layout.samples_per_region, sizeof(*state->samples));
    if (state->samples == NULL)
    {
        external_graphical_terrain_destroy(state);
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    for (index = 0U; index < layout.samples_per_region; ++index)
    {
        state->samples[index].height_millimeters = 900 + (int32_t)((index * 17U) % 300U);
        state->samples[index].material_weights[0] = 180U;
        state->samples[index].material_weights[1] = 40U;
        state->samples[index].material_weights[2] = 25U;
        state->samples[index].material_weights[3] = 10U;
        (void)henka_terrain_normalize_weights(state->samples[index].material_weights);
    }
    if (henka_terrain_world_apply_region_snapshot(
            state->world,
            (henka_terrain_region_storage_info){{0, 0}, 1U, 1U},
            state->samples,
            layout.samples_per_region) != HENKA_SUCCESS ||
        henka_terrain_world_set_region_residency(
            state->world, (henka_terrain_region_id){0, 0}, false, true, false) != HENKA_SUCCESS)
    {
        external_graphical_terrain_destroy(state);
        return HENKA_ERROR_UNKNOWN;
    }
    camera = henka_camera_create_perspective(1.0f, 4.0f / 3.0f, 0.1f, 500.0f);
    camera.position = (henka_vec3){32.0f, 28.0f, 72.0f};
    if (!henka_camera_look_at(&camera, (henka_vec3){32.0f, 1.5f, 32.0f}) ||
        henka_scene_set_camera(state->scene, &camera) != HENKA_SUCCESS)
    {
        external_graphical_terrain_destroy(state);
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    henka_scene_set_light_direction(state->scene, (henka_vec3){-0.4f, -1.0f, -0.2f});
    henka_scene_set_light_intensity(state->scene, 3.0f);
    render_desc = henka_terrain_render_desc_default();
    render_desc.max_resident_chunks = 1U;
    render_desc.max_pending_requests = 2U;
    if (henka_terrain_render_runtime_create(
            engine, state->scene, state->world, &render_desc, &state->render) != HENKA_SUCCESS ||
        henka_terrain_render_runtime_update_observer(state->render, camera.position) != HENKA_SUCCESS ||
        henka_terrain_render_runtime_pump(state->render, 1U) != HENKA_SUCCESS ||
        henka_engine_set_scene(engine, state->scene) != HENKA_SUCCESS ||
        henka_engine_set_viewport_shading_mode(engine, HENKA_VIEWPORT_SHADING_RENDERED) != HENKA_SUCCESS)
    {
        external_graphical_terrain_destroy(state);
        return HENKA_ERROR_RENDERER;
    }
    state->start_frame = henka_engine_get_frame_index(engine);
    return HENKA_SUCCESS;
}

static void external_graphical_terrain_update(
    henka_engine* engine,
    double delta_seconds,
    void* user_data)
{
    external_graphical_terrain_state* state = (external_graphical_terrain_state*)user_data;
    henka_terrain_render_stats render_stats;
    henka_engine_diagnostics diagnostics;
    (void)delta_seconds;
    if (engine == NULL || state == NULL || state->render == NULL ||
        henka_terrain_render_runtime_update_observer(
            state->render, (henka_vec3){32.0f, 28.0f, 72.0f}) != HENKA_SUCCESS ||
        henka_terrain_render_runtime_pump(state->render, 1U) != HENKA_SUCCESS)
    {
        henka_engine_request_exit(engine);
        return;
    }
    if (henka_engine_get_frame_index(engine) < state->start_frame + 4U)
    {
        return;
    }
    state->success =
        henka_terrain_render_runtime_get_stats(state->render, &render_stats) == HENKA_SUCCESS &&
        henka_engine_get_diagnostics(engine, &diagnostics) == HENKA_SUCCESS &&
        render_stats.resident_chunks == 1U &&
        render_stats.rebuilt_chunks >= 1U &&
        diagnostics.rendered_hdr_ready &&
        diagnostics.rendered_shadow_ready &&
        diagnostics.rendered_scene_draw_calls > 0U &&
        diagnostics.rendered_scene_visible_entities > 0U;
    henka_engine_request_exit(engine);
}

static void external_graphical_terrain_shutdown(
    henka_engine* engine,
    void* user_data)
{
    (void)engine;
    external_graphical_terrain_destroy((external_graphical_terrain_state*)user_data);
}

static bool external_graphical_terrain_workflow(void)
{
    external_graphical_terrain_state state = {0};
    henka_engine_config config = {0};
    henka_engine* engine = NULL;
    henka_result result;
    config.application_name = "Henka External Terrain Consumer";
    config.window_width = 640;
    config.window_height = 480;
    config.user_data_base_path = "external_graphical_user_data";
    config.package_mode = HENKA_PACKAGE_MODE_DEVELOPMENT;
    config.on_initialize = external_graphical_terrain_initialize;
    config.on_update = external_graphical_terrain_update;
    config.on_shutdown = external_graphical_terrain_shutdown;
    config.user_data = &state;
    result = henka_engine_create(&config, &engine);
    if (result == HENKA_SUCCESS)
    {
        result = henka_engine_run(engine);
    }
    henka_engine_destroy(engine);
    return result == HENKA_SUCCESS && state.success;
}

static bool external_terrain_workflow(void)
{
    const henka_terrain_region_id region_id = {0, 0};
    const henka_terrain_chunk_id chunk_id = {0, 0};
    henka_terrain_world_desc world_desc = henka_terrain_world_desc_default();
    henka_terrain_layout layout = {0};
    henka_terrain_world* world = NULL;
    henka_terrain_world* restarted_world = NULL;
    henka_terrain_storage* storage = NULL;
    henka_terrain_storage* restarted_storage = NULL;
    henka_terrain_physics* physics = NULL;
    henka_terrain_sample* samples = NULL;
    henka_terrain_sample* loaded_samples = NULL;
    int32_t* collision_heights = NULL;
    henka_terrain_mesh_vertex* vertices = NULL;
    uint32_t* indices = NULL;
    henka_terrain_mesh_data mesh = {0};
    henka_terrain_collision_patch collision_patch = {0};
    henka_terrain_physics_hit hit = {0};
    henka_terrain_region_storage_info region_info = {0};
    henka_terrain_region_state region_state = {0};
    const henka_terrain_sample* world_samples = NULL;
    size_t sample_count = 0U;
    size_t index;
    uint32_t center_index;
    int32_t saved_height = 0;
    uint8_t saved_rock_weight = 0U;
    bool success = false;

    world_desc.max_resident_regions = 1U;
    world_desc.max_resident_chunks = 1U;
    world_desc.max_pending_io = 4U;
    if (henka_terrain_world_desc_get_layout(&world_desc, &layout) != HENKA_SUCCESS ||
        henka_terrain_world_create(&world_desc, &world) != HENKA_SUCCESS ||
        henka_terrain_world_create(&world_desc, &restarted_world) != HENKA_SUCCESS ||
        henka_terrain_storage_create(&world_desc, "external_terrain_workflow", &storage) != HENKA_SUCCESS ||
        henka_terrain_storage_ensure_manifest(storage) != HENKA_SUCCESS)
    {
        goto cleanup;
    }

    samples = henka_calloc(layout.samples_per_region, sizeof(*samples));
    loaded_samples = henka_calloc(layout.samples_per_region, sizeof(*loaded_samples));
    collision_heights = henka_calloc(HENKA_TERRAIN_COLLISION_PATCH_SAMPLES, sizeof(*collision_heights));
    vertices = henka_calloc(HENKA_TERRAIN_MESH_MAX_VERTICES, sizeof(*vertices));
    indices = henka_calloc(HENKA_TERRAIN_MESH_MAX_INDICES, sizeof(*indices));
    if (samples == NULL || loaded_samples == NULL || collision_heights == NULL ||
        vertices == NULL || indices == NULL)
    {
        goto cleanup;
    }

    center_index = 32U * layout.samples_per_region_edge + 32U;
    for (index = 0U; index < layout.samples_per_region; ++index)
    {
        samples[index].height_millimeters = 1000;
        samples[index].material_weights[0] = 220U;
        samples[index].material_weights[1] = 20U;
        samples[index].material_weights[2] = 10U;
        samples[index].material_weights[3] = 5U;
        (void)henka_terrain_normalize_weights(samples[index].material_weights);
    }
    if (henka_terrain_world_apply_region_snapshot(
            world, (henka_terrain_region_storage_info){region_id, 1U, 1U},
            samples, layout.samples_per_region) != HENKA_SUCCESS ||
        henka_terrain_world_set_region_residency(world, region_id, true, true, false) != HENKA_SUCCESS)
    {
        goto cleanup;
    }

    {
        henka_material material = henka_material_terrain_default();
        if (!material.terrain_layers_enabled ||
            material.terrain_layers[0].texture_scale_meters <= 0.0f ||
            material.terrain_layers[1].roughness < 0.045f ||
            material.terrain_layers[2].base_color.x <= 0.0f)
        {
            goto cleanup;
        }
    }

    if (henka_terrain_world_apply_edit(
            world,
            &(henka_terrain_edit_command){
                1U, HENKA_TERRAIN_EDIT_ALGORITHM_VERSION,
                HENKA_TERRAIN_EDIT_RAISE, 32, 32, 4U,
                HENKA_TERRAIN_EDIT_FALLOFF_SMOOTH, 500, 0U, 0U},
            2U) != HENKA_SUCCESS ||
        henka_terrain_world_apply_edit(
            world,
            &(henka_terrain_edit_command){
                2U, HENKA_TERRAIN_EDIT_ALGORITHM_VERSION,
                HENKA_TERRAIN_EDIT_PAINT, 32, 32, 4U,
                HENKA_TERRAIN_EDIT_FALLOFF_LINEAR, 0, 2U, 255U},
            3U) != HENKA_SUCCESS ||
        henka_terrain_world_get_region_samples(world, region_id, &world_samples, &sample_count) != HENKA_SUCCESS ||
        sample_count != layout.samples_per_region ||
        world_samples[center_index].height_millimeters <= 1000 ||
        world_samples[center_index].material_weights[2] <= 10U)
    {
        goto cleanup;
    }
    saved_height = world_samples[center_index].height_millimeters;
    saved_rock_weight = world_samples[center_index].material_weights[2];

    if (henka_terrain_physics_create(&(henka_terrain_physics_desc){1U}, &physics) != HENKA_SUCCESS ||
        henka_terrain_world_build_collision_patch(
            world, chunk_id, collision_heights,
            HENKA_TERRAIN_COLLISION_PATCH_SAMPLES, &collision_patch) != HENKA_SUCCESS ||
        henka_terrain_physics_replace_patch(
            physics, &(henka_terrain_physics_patch_desc){collision_patch, 1.0F, 0.0F, 0.0F}) != HENKA_SUCCESS ||
        henka_terrain_physics_raycast(
            physics,
            (henka_ray){{32.0F, 20.0F, 32.0F}, {0.0F, -1.0F, 0.0F}},
            40.0F, &hit) != HENKA_SUCCESS ||
        !hit.hit || hit.height_meters <= 1.0F)
    {
        goto cleanup;
    }

    mesh = (henka_terrain_mesh_data){
        chunk_id, 0U, 0U, 0U,
        vertices, HENKA_TERRAIN_MESH_MAX_VERTICES, 0U,
        indices, HENKA_TERRAIN_MESH_MAX_INDICES, 0U};
    if (henka_terrain_mesh_build_chunk(world, chunk_id, 0U, &mesh) != HENKA_SUCCESS ||
        mesh.revision != 3U || mesh.vertex_count == 0U || mesh.index_count == 0U)
    {
        goto cleanup;
    }

    if (henka_terrain_world_get_region_state(world, region_id, &region_state) != HENKA_SUCCESS ||
        henka_terrain_storage_begin(storage, 1U) != HENKA_SUCCESS ||
        henka_terrain_storage_write_region(
            storage, region_id, region_state.revision, region_state.generation,
            world_samples, sample_count) != HENKA_SUCCESS ||
        henka_terrain_storage_commit(storage, 1U) != HENKA_SUCCESS ||
        henka_terrain_storage_load_region(
            storage, region_id, &region_info, loaded_samples, layout.samples_per_region) != HENKA_SUCCESS ||
        region_info.revision != region_state.revision ||
        loaded_samples[center_index].height_millimeters != saved_height ||
        loaded_samples[center_index].material_weights[2] != saved_rock_weight)
    {
        goto cleanup;
    }

    henka_terrain_storage_destroy(storage);
    storage = NULL;
    if (henka_terrain_storage_create(
            &world_desc, "external_terrain_workflow", &restarted_storage) != HENKA_SUCCESS ||
        henka_terrain_storage_recover(restarted_storage) != HENKA_SUCCESS ||
        henka_terrain_storage_load_region(
            restarted_storage, region_id, &region_info, loaded_samples,
            layout.samples_per_region) != HENKA_SUCCESS ||
        henka_terrain_world_apply_region_snapshot(
            restarted_world, region_info, loaded_samples, layout.samples_per_region) != HENKA_SUCCESS ||
        henka_terrain_world_set_region_residency(
            restarted_world, region_id, true, true, false) != HENKA_SUCCESS ||
        henka_terrain_world_get_region_samples(
            restarted_world, region_id, &world_samples, &sample_count) != HENKA_SUCCESS ||
        world_samples[center_index].height_millimeters != saved_height ||
        world_samples[center_index].material_weights[2] != saved_rock_weight)
    {
        goto cleanup;
    }

    printf("External game template initialized.\n");
    printf("External Terrain material, edit, collision, render-data, save, and restart workflow passed.\n");
    success = true;

cleanup:
    henka_terrain_physics_destroy(physics);
    henka_terrain_storage_destroy(restarted_storage);
    henka_terrain_storage_destroy(storage);
    henka_terrain_world_destroy(restarted_world);
    henka_terrain_world_destroy(world);
    henka_free(indices);
    henka_free(vertices);
    henka_free(collision_heights);
    henka_free(loaded_samples);
    henka_free(samples);
    return success;
}

typedef struct external_script_dispatch_state
{
    henka_script_host* host;
    bool input_called;
    bool interaction_called;
    bool physics_called;
    bool event_called;
} external_script_dispatch_state;

static henka_result external_script_dispatch(
    void* user_data,
    uint32_t api_id,
    const henka_script_api_value* arguments,
    size_t argument_count,
    henka_script_api_value* out_value)
{
    external_script_dispatch_state* state =
        (external_script_dispatch_state*)user_data;
    if (state == NULL || arguments == NULL || out_value == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    switch (api_id)
    {
        case HENKA_SCRIPT_API_INPUT_IS_ACTION_DOWN:
            if (argument_count != 1U ||
                arguments[0].type != HENKA_SCRIPT_API_VALUE_ACTION_ID ||
                arguments[0].as.action_id != 7U)
            {
                return HENKA_ERROR_INVALID_ARGUMENT;
            }
            state->input_called = true;
            out_value->type = HENKA_SCRIPT_API_VALUE_BOOL;
            out_value->as.boolean = true;
            return HENKA_SUCCESS;

        case HENKA_SCRIPT_API_INTERACTION_TRY:
            if (argument_count != 1U ||
                arguments[0].type != HENKA_SCRIPT_API_VALUE_ENTITY)
            {
                return HENKA_ERROR_INVALID_ARGUMENT;
            }
            state->interaction_called = true;
            out_value->type = HENKA_SCRIPT_API_VALUE_RESULT;
            out_value->as.result = (henka_result)HENKA_INTERACTION_RESULT_AVAILABLE;
            return HENKA_SUCCESS;

        case HENKA_SCRIPT_API_PHYSICS_APPLY_IMPULSE:
            if (argument_count != 2U ||
                arguments[0].type != HENKA_SCRIPT_API_VALUE_ENTITY ||
                arguments[1].type != HENKA_SCRIPT_API_VALUE_VEC3 ||
                !isfinite(arguments[1].as.vec3.x) ||
                !isfinite(arguments[1].as.vec3.y) ||
                !isfinite(arguments[1].as.vec3.z))
            {
                return HENKA_ERROR_INVALID_ARGUMENT;
            }
            state->physics_called = true;
            out_value->type = HENKA_SCRIPT_API_VALUE_RESULT;
            out_value->as.result = HENKA_SUCCESS;
            return HENKA_SUCCESS;

        case HENKA_SCRIPT_API_EVENTS_EMIT:
            if (argument_count != 2U ||
                arguments[0].type != HENKA_SCRIPT_API_VALUE_EVENT_ID ||
                arguments[1].type != HENKA_SCRIPT_API_VALUE_ENTITY ||
                state->host == NULL)
            {
                return HENKA_ERROR_INVALID_ARGUMENT;
            }
            state->event_called = true;
            out_value->type = HENKA_SCRIPT_API_VALUE_RESULT;
            out_value->as.result = henka_script_host_emit_event(
                state->host,
                arguments[0].as.event_id,
                arguments[1].as.entity,
                0U);
            return HENKA_SUCCESS;

        default:
            return HENKA_ERROR_INVALID_ARGUMENT;
    }
}

static bool external_scripting_workflow(void)
{
    henka_scene_document* document = NULL;
    henka_scene_behavior_runtime* runtime = NULL;
    henka_script_host* host = NULL;
    henka_script_state_store* store = NULL;
    henka_scene_document_object object = henka_scene_document_object_default();
    henka_scene_document_id object_id = HENKA_INVALID_SCENE_DOCUMENT_ID;
    henka_script_state_value state_value = {0};
    henka_script_behavior_batch_report report = {0};
    external_script_dispatch_state dispatch_state = {0};
    henka_result result = HENKA_SUCCESS;
    bool state_present = false;
    bool success = false;
    const uint32_t api_ids[] = {
        HENKA_SCRIPT_API_EVENTS_EMIT,
        HENKA_SCRIPT_API_INPUT_IS_ACTION_DOWN,
        HENKA_SCRIPT_API_INTERACTION_TRY,
        HENKA_SCRIPT_API_PHYSICS_APPLY_IMPULSE,
        HENKA_SCRIPT_API_STATE_SET_I32};

    (void)snprintf(object.name, sizeof(object.name), "%s", "External Script Pair");
    object.source.kind = HENKA_SCENE_DOCUMENT_SOURCE_PRIMITIVE;
    object.source.primitive = HENKA_SCENE_DOCUMENT_PRIMITIVE_BOX;
    object.source.primitive_dimensions = (henka_vec3){1.0f, 1.0f, 1.0f};
    object.behaviors[0] = henka_scene_document_behavior_default();
    object.behaviors[0].id = 10U;
    object.behaviors[0].language = HENKA_SCRIPT_LANGUAGE_HENKASCRIPT;
    (void)snprintf(
        object.behaviors[0].asset_path,
        sizeof(object.behaviors[0].asset_path),
        "%s",
        "assets/scripts/publisher.hks");
    object.behaviors[1] = henka_scene_document_behavior_default();
    object.behaviors[1].id = 11U;
    object.behaviors[1].language = HENKA_SCRIPT_LANGUAGE_LUA;
    (void)snprintf(
        object.behaviors[1].asset_path,
        sizeof(object.behaviors[1].asset_path),
        "%s",
        "assets/scripts/subscriber.lua");
    object.behavior_count = 2U;

#define EXTERNAL_SCRIPT_STEP(label, expression) \
    do { \
        result = (expression); \
        if (result != HENKA_SUCCESS) { \
            fprintf(stderr, "External scripting failure: %s (%s).\n", \
                (label), henka_result_to_string(result)); \
            goto cleanup; \
        } \
    } while (0)

    EXTERNAL_SCRIPT_STEP("create document", henka_scene_document_create(&document));
    EXTERNAL_SCRIPT_STEP(
        "add behavior document", henka_scene_document_add_object(document, &object, &object_id));
    EXTERNAL_SCRIPT_STEP("create host", henka_script_host_create(&host));
    dispatch_state.host = host;
    EXTERNAL_SCRIPT_STEP("create state store", henka_script_state_store_create(&store));
    for (size_t index = 0U; index < sizeof(api_ids) / sizeof(api_ids[0]); ++index)
    {
        EXTERNAL_SCRIPT_STEP(
            "bind gameplay API",
            henka_script_host_bind_api(host, api_ids[index], &(size_t){0U}));
    }
    EXTERNAL_SCRIPT_STEP("attach state store", henka_script_host_set_state_store(host, store));
    EXTERNAL_SCRIPT_STEP(
        "set gameplay dispatcher",
        henka_script_host_set_dispatcher(host, external_script_dispatch, &dispatch_state));
    EXTERNAL_SCRIPT_STEP(
        "load behavior assets",
        henka_scene_behavior_runtime_create_with_host(document, ".", 128U, host, &runtime));
    result = henka_scene_behavior_runtime_dispatch(
        runtime, HENKA_SCRIPT_LIFECYCLE_CREATE, 0.0f, 1U, &report);
    if (result != HENKA_SUCCESS)
    {
        fprintf(
            stderr,
            "External scripting failure: dispatch Create (%s), report=%d, host=%s, input=%s, interaction=%s, physics=%s, event=%s.\n",
            henka_result_to_string(result),
            (int)report.first_error,
            henka_result_to_string(report.first_error),
            dispatch_state.input_called ? "yes" : "no",
            dispatch_state.interaction_called ? "yes" : "no",
            dispatch_state.physics_called ? "yes" : "no",
            dispatch_state.event_called ? "yes" : "no");
        goto cleanup;
    }
    EXTERNAL_SCRIPT_STEP(
        "dispatch Start",
        henka_scene_behavior_runtime_dispatch(
            runtime, HENKA_SCRIPT_LIFECYCLE_START, 0.0f, 1U, &report));
    EXTERNAL_SCRIPT_STEP(
        "dispatch interaction",
        henka_scene_behavior_runtime_dispatch_signal_for_entity(
            runtime,
            object_id,
            HENKA_SCRIPT_LIFECYCLE_INTERACT,
            0.0f,
            2U,
            0U,
            object_id,
            99U,
            1U,
            &report));
    EXTERNAL_SCRIPT_STEP(
        "dispatch events",
        henka_scene_behavior_runtime_dispatch_events(runtime, &report));
    EXTERNAL_SCRIPT_STEP(
        "read subscriber state",
        henka_script_state_store_get(
            store,
            (henka_script_state_identity){object_id, 11U},
            77U,
            &state_value,
            &state_present));
    if (!state_present || state_value.type != HENKA_SCRIPT_STATE_VALUE_I32 ||
        state_value.as.i32 != 17)
    {
        fprintf(stderr, "External scripting failure: subscriber state mismatch.\n");
        goto cleanup;
    }
    if (!dispatch_state.input_called || !dispatch_state.interaction_called ||
        !dispatch_state.physics_called || !dispatch_state.event_called)
    {
        fprintf(stderr, "External scripting failure: shared gameplay host calls were not exercised.\n");
        goto cleanup;
    }

    success = true;
    printf("External Lua/HenkaScript mixed-language gameplay host workflow passed.\n");

cleanup:
#undef EXTERNAL_SCRIPT_STEP
    henka_scene_behavior_runtime_destroy(runtime);
    henka_script_state_store_destroy(store);
    henka_script_host_destroy(host);
    henka_scene_document_destroy(document);
    return success;
}

int main(void)
{
    if (!external_terrain_workflow() ||
        !external_graphical_terrain_workflow() ||
        !external_audio_workflow() ||
        !external_scripting_workflow())
    {
        return 1;
    }
    printf("External Terrain graphical Rendered path passed.\n");
    return 0;
}
