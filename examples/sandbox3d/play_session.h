#ifndef SANDBOX3D_PLAY_SESSION_H
#define SANDBOX3D_PLAY_SESSION_H

#include <stddef.h>

#include "scene_document_bridge.h"

typedef struct sandbox3d_play_session sandbox3d_play_session;

typedef enum sandbox3d_play_session_state
{
    SANDBOX3D_PLAY_SESSION_STOPPED = 0,
    SANDBOX3D_PLAY_SESSION_RUNNING,
    SANDBOX3D_PLAY_SESSION_PAUSED,
    SANDBOX3D_PLAY_SESSION_PAUSED_ERROR,
    SANDBOX3D_PLAY_SESSION_FAILED
} sandbox3d_play_session_state;

henka_result sandbox3d_play_session_create(
    sandbox3d_scene_document_bridge* bridge,
    henka_physics_world* physics_world,
    sandbox3d_play_session** out_session);
henka_result sandbox3d_play_session_create_with_project_root(
    sandbox3d_scene_document_bridge* bridge,
    henka_physics_world* physics_world,
    const char* project_root,
    sandbox3d_play_session** out_session);
void sandbox3d_play_session_destroy(sandbox3d_play_session* session);
sandbox3d_play_session_state sandbox3d_play_session_get_state(
    const sandbox3d_play_session* session);
henka_result sandbox3d_play_session_get_last_error(
    const sandbox3d_play_session* session);
henka_result sandbox3d_play_session_start(sandbox3d_play_session* session);
henka_result sandbox3d_play_session_pause(sandbox3d_play_session* session);
henka_result sandbox3d_play_session_resume(sandbox3d_play_session* session);
henka_result sandbox3d_play_session_tick(sandbox3d_play_session* session);
henka_result sandbox3d_play_session_step_fixed(sandbox3d_play_session* session);
henka_result sandbox3d_play_session_stop(sandbox3d_play_session* session);

#endif
