#ifndef SANDBOX3D_GAME_AUTHORING_H
#define SANDBOX3D_GAME_AUTHORING_H

#include <stdbool.h>
#include <stddef.h>

#include <henka/scene.h>
#include <henka/scene_document.h>
#include <henka/script.h>
#include <henka/script_source.h>

#include "play_session.h"

typedef struct sandbox3d_game_authoring sandbox3d_game_authoring;
typedef sandbox3d_play_input_query sandbox3d_game_authoring_input_query;

/* The coordinator owns authoring persistence, the dedicated Play world, and a
 * transactional runtime scene/bridge created for each Play session. */
henka_result sandbox3d_game_authoring_create(
    henka_scene* scene,
    const char* relative_path,
    sandbox3d_game_authoring** out_authoring);
void sandbox3d_game_authoring_destroy(
    sandbox3d_game_authoring* authoring);

henka_result sandbox3d_game_authoring_register_entity(
    sandbox3d_game_authoring* authoring,
    henka_entity entity,
    henka_scene_document_id* out_document_id);
henka_result sandbox3d_game_authoring_unregister_entity(
    sandbox3d_game_authoring* authoring,
    henka_entity entity);
henka_result sandbox3d_game_authoring_get_object_for_entity(
    const sandbox3d_game_authoring* authoring,
    henka_entity entity,
    henka_scene_document_id* out_document_id,
    henka_scene_document_object* out_object);
henka_result sandbox3d_game_authoring_update_object_for_entity(
    sandbox3d_game_authoring* authoring,
    henka_entity entity,
    const henka_scene_document_object* object);
henka_result sandbox3d_game_authoring_set_play_input_context(
    sandbox3d_game_authoring* authoring,
    sandbox3d_game_authoring_input_query input_query,
    void* input_user_data,
    henka_vec3 observer_position);
size_t sandbox3d_game_authoring_get_behavior_count_for_entity(
    const sandbox3d_game_authoring* authoring,
    henka_entity entity);
henka_result sandbox3d_game_authoring_get_behavior_at_for_entity(
    const sandbox3d_game_authoring* authoring,
    henka_entity entity,
    size_t index,
    henka_scene_document_behavior* out_behavior);
henka_result sandbox3d_game_authoring_get_behavior_for_entity(
    const sandbox3d_game_authoring* authoring,
    henka_entity entity,
    henka_scene_document_behavior_id behavior_id,
    henka_scene_document_behavior* out_behavior);
henka_result sandbox3d_game_authoring_add_behavior_for_entity(
    sandbox3d_game_authoring* authoring,
    henka_entity entity,
    const henka_scene_document_behavior* behavior,
    henka_scene_document_behavior_id* out_behavior_id);
henka_result sandbox3d_game_authoring_update_behavior_for_entity(
    sandbox3d_game_authoring* authoring,
    henka_entity entity,
    const henka_scene_document_behavior* behavior);
henka_result sandbox3d_game_authoring_remove_behavior_for_entity(
    sandbox3d_game_authoring* authoring,
    henka_entity entity,
    henka_scene_document_behavior_id behavior_id);
/* Reloads one persisted behavior in the isolated Play session. The authored
 * Scene Document is not mutated; candidate construction and generation-checked
 * runtime rebinding remain owned by the Play/script runtime layers. */
henka_result sandbox3d_game_authoring_reload_behavior_for_entity(
    sandbox3d_game_authoring* authoring,
    henka_entity entity,
    henka_scene_document_behavior_id behavior_id,
    henka_script_source_diagnostic* out_diagnostic);
/* Creates a confined template file and attaches its behavior transactionally.
 * The authoring coordinator owns the document mutation; project_root is only
 * borrowed for the duration of the call. */
henka_result sandbox3d_game_authoring_attach_script_template(
    sandbox3d_game_authoring* authoring,
    const char* project_root,
    henka_entity entity,
    henka_script_language language);

henka_result sandbox3d_game_authoring_save(
    sandbox3d_game_authoring* authoring,
    const char* project_root);
henka_result sandbox3d_game_authoring_load(
    sandbox3d_game_authoring* authoring,
    const char* project_root);
henka_result sandbox3d_game_authoring_save_play_state(
    sandbox3d_game_authoring* authoring,
    const char* project_root);
henka_result sandbox3d_game_authoring_load_play_state(
    sandbox3d_game_authoring* authoring,
    const char* project_root);
/* Borrowed editor-owned state store. It remains valid until authoring is
 * destroyed and is never implicitly saved when Play stops. */
henka_script_state_store* sandbox3d_game_authoring_get_script_state_store(
    const sandbox3d_game_authoring* authoring);
const char* sandbox3d_game_authoring_get_relative_path(
    const sandbox3d_game_authoring* authoring);
henka_scene* sandbox3d_game_authoring_get_authoring_scene(
    const sandbox3d_game_authoring* authoring);
henka_scene* sandbox3d_game_authoring_get_play_scene(
    const sandbox3d_game_authoring* authoring);

sandbox3d_play_session_state sandbox3d_game_authoring_get_play_state(
    const sandbox3d_game_authoring* authoring);
henka_result sandbox3d_game_authoring_start_play(
    sandbox3d_game_authoring* authoring);
henka_result sandbox3d_game_authoring_pause_play(
    sandbox3d_game_authoring* authoring);
henka_result sandbox3d_game_authoring_resume_play(
    sandbox3d_game_authoring* authoring);
henka_result sandbox3d_game_authoring_tick_play(
    sandbox3d_game_authoring* authoring);
henka_result sandbox3d_game_authoring_step_play(
    sandbox3d_game_authoring* authoring);
henka_result sandbox3d_game_authoring_stop_play(
    sandbox3d_game_authoring* authoring);
bool sandbox3d_game_authoring_is_play_locked(
    const sandbox3d_game_authoring* authoring);

#endif
