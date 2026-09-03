#ifndef SANDBOX3D_PLAY_SESSION_H
#define SANDBOX3D_PLAY_SESSION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "scene_document_bridge.h"

#include <henka/math.h>
#include <henka/assets.h>
#include <henka/audio.h>
#include <henka/script.h>
#include <henka/script_source.h>

typedef struct sandbox3d_play_session sandbox3d_play_session;
typedef bool (*sandbox3d_play_input_query)(
    void* user_data,
    uint32_t action_id);

typedef enum sandbox3d_play_session_state
{
    SANDBOX3D_PLAY_SESSION_STOPPED = 0,
    SANDBOX3D_PLAY_SESSION_RUNNING,
    SANDBOX3D_PLAY_SESSION_PAUSED,
    SANDBOX3D_PLAY_SESSION_PAUSED_ERROR,
    SANDBOX3D_PLAY_SESSION_FAILED
} sandbox3d_play_session_state;

/* Runtime Play configuration for one real, bridge-bound scene object. The
 * configuration is copied into the stopped session and does not retain caller
 * memory. It is mutually exclusive with the object's authored generic
 * physics component; the controller creates and owns its dynamic capsule
 * body for the Play session. Persisted controller authoring remains a
 * separate Scene Document capability. */
typedef struct sandbox3d_play_character_controller_config
{
    henka_scene_document_id document_id;
    float radius;
    float half_height;
    float max_speed;
    float jump_speed;
    float acceleration;
    float deceleration;
    float air_control;
    float slope_limit_degrees;
    uint32_t layer;
    uint32_t mask;
} sandbox3d_play_character_controller_config;

henka_result sandbox3d_play_session_create(
    sandbox3d_scene_document_bridge* bridge,
    henka_physics_world* physics_world,
    sandbox3d_play_session** out_session);
henka_result sandbox3d_play_session_create_with_project_root(
    sandbox3d_scene_document_bridge* bridge,
    henka_physics_world* physics_world,
    const char* project_root,
    sandbox3d_play_session** out_session);
/* The state store is borrowed by the session and must outlive the session.
 * It may be replaced only while stopped; Play never saves or destroys it. */
henka_result sandbox3d_play_session_set_script_state_store(
    sandbox3d_play_session* session,
    henka_script_state_store* store);
/* The audio system is borrowed and must outlive the session. Play instantiates
 * persisted emitters only when this system is configured; enabled authored
 * Audio therefore fails closed instead of being silently omitted. */
henka_result sandbox3d_play_session_set_audio_system(
    sandbox3d_play_session* session,
    henka_audio_system* audio_system);
/* The asset manager is borrowed and must outlive the session and every
 * manager-owned Audio clip used by its Play emitters. */
henka_result sandbox3d_play_session_set_audio_asset_manager(
    sandbox3d_play_session* session,
    henka_asset_manager* asset_manager);
/* The input query is borrowed and remains owned by the caller.  The observer
 * position is copied into the session; both values are used only by the
 * isolated Play world and may be refreshed while it is running. */
henka_result sandbox3d_play_session_set_input_context(
    sandbox3d_play_session* session,
    sandbox3d_play_input_query input_query,
    void* input_user_data,
    henka_vec3 observer_position);
/* Installs or replaces one bounded runtime controller configuration while
 * stopped. The document ID must resolve to a live bridge binding. Input is
 * sampled from MOVE_FORWARD/BACK/LEFT/RIGHT and MOVE_UP is treated as jump
 * through the existing Play input callback. */
henka_result sandbox3d_play_session_set_character_controller(
    sandbox3d_play_session* session,
    const sandbox3d_play_character_controller_config* config);
henka_result sandbox3d_play_session_clear_character_controller(
    sandbox3d_play_session* session,
    henka_scene_document_id document_id);
void sandbox3d_play_session_destroy(sandbox3d_play_session* session);
sandbox3d_play_session_state sandbox3d_play_session_get_state(
    const sandbox3d_play_session* session);
henka_result sandbox3d_play_session_get_last_error(
    const sandbox3d_play_session* session);
/* Returns the borrowed Play-only Script Host. It is valid until the session
 * stops and must not be destroyed, retained, or reconfigured by the caller. */
henka_script_host* sandbox3d_play_session_get_script_host(
    const sandbox3d_play_session* session);
henka_result sandbox3d_play_session_start(sandbox3d_play_session* session);
henka_result sandbox3d_play_session_pause(sandbox3d_play_session* session);
henka_result sandbox3d_play_session_resume(sandbox3d_play_session* session);
henka_result sandbox3d_play_session_tick(sandbox3d_play_session* session);
henka_result sandbox3d_play_session_step_fixed(sandbox3d_play_session* session);
/* Reloads one persisted behavior while Play is running or paused. Candidate
 * backend construction and identity validation happen before the live slot is
 * rebound; the existing generation and lifecycle state remain intact on
 * success and every failure path leaves the active backend untouched. */
henka_result sandbox3d_play_session_reload_behavior(
    sandbox3d_play_session* session,
    henka_scene_document_id document_id,
    henka_scene_document_behavior_id behavior_id,
    henka_script_source_diagnostic* out_diagnostic);
henka_result sandbox3d_play_session_stop(sandbox3d_play_session* session);

#endif
