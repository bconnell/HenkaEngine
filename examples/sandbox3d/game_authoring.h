#ifndef SANDBOX3D_GAME_AUTHORING_H
#define SANDBOX3D_GAME_AUTHORING_H

#include <stdbool.h>
#include <stddef.h>

#include <henka/scene.h>
#include <henka/scene_document.h>

#include "play_session.h"

typedef struct sandbox3d_game_authoring sandbox3d_game_authoring;

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

henka_result sandbox3d_game_authoring_save(
    sandbox3d_game_authoring* authoring,
    const char* project_root);
henka_result sandbox3d_game_authoring_load(
    sandbox3d_game_authoring* authoring,
    const char* project_root);
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
