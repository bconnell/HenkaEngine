#ifndef SANDBOX3D_GAME_AUTHORING_H
#define SANDBOX3D_GAME_AUTHORING_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <henka/scene.h>
#include <henka/scene_document.h>
#include <henka/assets.h>
#include <henka/audio.h>
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
/* Creates a coordinator for the normal in-place project-load path with the
 * owning engine's borrowed source authorities attached. The engine must
 * outlive the returned coordinator; generated meshes remain coordinator-owned
 * until destruction. */
henka_result sandbox3d_game_authoring_create_with_engine(
    henka_scene* scene,
    const char* relative_path,
    henka_engine* engine,
    sandbox3d_game_authoring** out_authoring);
/* Opens a project into freshly-created runtime/session state. The returned
 * scene is owned by the caller and is borrowed by the returned coordinator;
 * both outputs remain NULL unless manifest selection, document loading,
 * runtime entity binding, and bridge publication all succeed. */
henka_result sandbox3d_game_authoring_open_project(
    const char* project_root,
    henka_scene** out_scene,
    sandbox3d_game_authoring** out_authoring);
/* Opens a project and materializes supported manager-owned mesh and prefab
 * sources into the fresh runtime scene. The asset manager is borrowed and
 * must outlive the returned scene and coordinator. Unsupported or
 * unresolvable sources fail closed before the candidate becomes observable. */
henka_result sandbox3d_game_authoring_open_project_with_assets(
    const char* project_root,
    henka_asset_manager* assets,
    henka_scene** out_scene,
    sandbox3d_game_authoring** out_authoring);
/* Opens a project with the owning engine available for native primitive and
 * confined HAMS authoring-source reconstruction. Generated runtime meshes are
 * owned by the returned coordinator and remain valid until it is destroyed;
 * the coordinator still borrows the returned scene. Unsupported sources fail
 * closed. */
henka_result sandbox3d_game_authoring_open_project_with_engine(
    const char* project_root,
    henka_engine* engine,
    henka_scene** out_scene,
    sandbox3d_game_authoring** out_authoring);
void sandbox3d_game_authoring_destroy(
    sandbox3d_game_authoring* authoring);

/* Registration is idempotent.  A missing live parent chain is registered
 * root-to-leaf transactionally before the requested entity. */
henka_result sandbox3d_game_authoring_register_entity(
    sandbox3d_game_authoring* authoring,
    henka_entity entity,
    henka_scene_document_id* out_document_id);
/* Registers a newly-created ordinary authored duplicate. Prefab instance
 * members are rejected until the coordinator has explicit instance-duplication
 * semantics, so a clone cannot silently lose its prefab provenance. */
henka_result sandbox3d_game_authoring_register_duplicate_entity(
    sandbox3d_game_authoring* authoring,
    henka_entity source_entity,
    henka_entity duplicate_entity,
    henka_scene_document_id* out_document_id);
/* Captures one ordinary authored scene root into a confined prefab asset. The
 * source document and live scene are unchanged; prefab-instance members are
 * rejected until explicit instance-duplication semantics exist. */
henka_result sandbox3d_game_authoring_create_prefab_asset(
    sandbox3d_game_authoring* authoring,
    henka_entity root_entity,
    const char* project_root,
    const char* relative_path);
/* Loads one manager-owned prefab asset and places a real mapped instance in
 * the authoring scene. The new members are registered in the Scene Document
 * with prefab provenance, so the placement remains save/load reconstructible.
 * The asset manager and its configured project root are borrowed authorities. */
henka_result sandbox3d_game_authoring_instantiate_prefab_asset(
    sandbox3d_game_authoring* authoring,
    const char* asset_path,
    henka_transform root_transform,
    henka_entity* out_root_entity);
henka_result sandbox3d_game_authoring_instantiate_prefab_asset_under_parent(
    sandbox3d_game_authoring* authoring,
    const char* asset_path,
    henka_entity parent_entity,
    henka_transform root_transform,
    henka_entity* out_root_entity);
/* Removes every mapped member of one authored prefab instance while retaining
 * the manager-owned prefab asset and all other scene instances. The supplied
 * entity may be any live member of the instance. */
henka_result sandbox3d_game_authoring_destroy_prefab_instance(
    sandbox3d_game_authoring* authoring,
    henka_entity instance_entity);
/* Deliberately removes Prefab ownership from one manager-owned authored
 * instance while retaining its live entities and persistent document IDs.
 * Current supported authored state and hierarchy are synchronized into a
 * prepared Scene Document candidate before Prefab provenance is removed.
 * Live meshes must be absent or resolve to a manager-owned reconstructible
 * mesh source; otherwise unpack fails before publication instead of silently
 * losing source identity. The supplied entity may be any live instance member. */
henka_result sandbox3d_game_authoring_unpack_prefab_instance(
    sandbox3d_game_authoring* authoring,
    henka_entity instance_entity);

/* Captures current supported live edits on one mapped Prefab member as
 * explicit instance-owned Scene Document state. Non-root local transform and
 * manager-owned asset-backed material overrides are supported. This operation
 * deliberately does not rewrite the reusable .hprefab source asset. It enters
 * the existing Game Authoring undo/redo history. */
henka_result sandbox3d_game_authoring_apply_prefab_instance_edits(
    sandbox3d_game_authoring* authoring,
    henka_entity instance_entity);

/* Restores supported instance-owned state on one mapped Prefab member to the
 * current Prefab source. The root transform remains instance placement;
 * non-root local transform and manager-owned asset-backed material overrides
 * are reverted. This operation enters the existing Game Authoring history. */
henka_result sandbox3d_game_authoring_revert_prefab_instance_edits(
    sandbox3d_game_authoring* authoring,
    henka_entity instance_entity);
/* Removes the binding and promotes direct children to authored roots, matching
 * the runtime scene's parent-destruction semantics.  Any still-live child is
 * detached from the runtime scene before its authored parent link is cleared. */
henka_result sandbox3d_game_authoring_unregister_entity(
    sandbox3d_game_authoring* authoring,
    henka_entity entity);
henka_result sandbox3d_game_authoring_get_object_for_entity(
    const sandbox3d_game_authoring* authoring,
    henka_entity entity,
    henka_scene_document_id* out_document_id,
    henka_scene_document_object* out_object);
/* Resolves a persistent authored identity to its currently bound live entity.
 * The coordinator remains the sole owner of this mapping; callers do not
 * receive a raw document or bridge pointer. */
henka_result sandbox3d_game_authoring_get_entity_for_document_id(
    const sandbox3d_game_authoring* authoring,
    henka_scene_document_id document_id,
    henka_entity* out_entity);
henka_result sandbox3d_game_authoring_update_object_for_entity(
    sandbox3d_game_authoring* authoring,
    henka_entity entity,
    const henka_scene_document_object* object);
/* Reparents through the canonical Scene Document and runtime bridge. The
 * requested mode is evaluated on a disposable clone first so invalid
 * parents, cycles, stale entities, and unsupported transform relationships
 * fail before authored or live state is changed. */
henka_result sandbox3d_game_authoring_reparent_entity(
    sandbox3d_game_authoring* authoring,
    henka_entity child,
    henka_entity parent,
    henka_scene_parenting_mode mode);
henka_result sandbox3d_game_authoring_unparent_entity(
    sandbox3d_game_authoring* authoring,
    henka_entity child,
    henka_scene_parenting_mode mode);
/* Supported Game Authoring Scene Document object transactions use one bounded
 * history owner. Undo and redo restore the authored object and its live
 * entity; binding changes explicitly invalidate entries that may be stale. */
bool sandbox3d_game_authoring_can_undo(
    const sandbox3d_game_authoring* authoring);
bool sandbox3d_game_authoring_can_redo(
    const sandbox3d_game_authoring* authoring);
henka_result sandbox3d_game_authoring_undo(
    sandbox3d_game_authoring* authoring);
henka_result sandbox3d_game_authoring_redo(
    sandbox3d_game_authoring* authoring);
henka_result sandbox3d_game_authoring_set_play_input_context(
    sandbox3d_game_authoring* authoring,
    sandbox3d_game_authoring_input_query input_query,
    void* input_user_data,
    henka_vec3 observer_position);
/* The audio system is borrowed and must outlive the authoring coordinator and
 * every Play session it starts. Enabled authored emitters fail closed when no
 * system is configured; Audio is never silently omitted from Play. */
henka_result sandbox3d_game_authoring_set_audio_system(
    sandbox3d_game_authoring* authoring,
    henka_audio_system* audio_system);
/* The asset manager is borrowed and must outlive the authoring coordinator
 * and every Play session it starts. */
henka_result sandbox3d_game_authoring_set_audio_asset_manager(
    sandbox3d_game_authoring* authoring,
    henka_asset_manager* asset_manager);
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

/* Snapshots and synchronizes every bound runtime object, camera, and scene
 * presentation setting into a validated document candidate before writing the
 * candidate and its bounded henka.project entry. The candidate is published
 * in memory only after both writes succeed. */
henka_result sandbox3d_game_authoring_save(
    sandbox3d_game_authoring* authoring,
    const char* project_root);
/* Loads the manifest-selected startup scene when henka.project is present;
 * an absent manifest falls back to the coordinator path. When the coordinator
 * was opened with an engine or asset manager, supported persisted mesh sources
 * are rematerialized into the candidate before publication. Malformed
 * manifests, unsafe paths, missing scenes, unsupported sources, or candidates
 * whose persistent object IDs do not exactly match the live bindings are
 * rejected before publication. */
henka_result sandbox3d_game_authoring_load(
    sandbox3d_game_authoring* authoring,
    const char* project_root);
henka_result sandbox3d_game_authoring_save_play_state(
    sandbox3d_game_authoring* authoring,
    const char* project_root);
henka_result sandbox3d_game_authoring_load_play_state(
    sandbox3d_game_authoring* authoring,
    const char* project_root);
/* State access stays behind the authoring coordinator. Both operations reject
 * while Play is active; callers cannot mutate or inspect the borrowed runtime
 * store through a raw pointer and bypass the Edit-vs-Play lock. */
henka_result sandbox3d_game_authoring_set_script_state_value(
    sandbox3d_game_authoring* authoring,
    henka_script_state_identity identity,
    uint32_t key,
    henka_script_state_value value);
henka_result sandbox3d_game_authoring_get_script_state_value(
    const sandbox3d_game_authoring* authoring,
    henka_script_state_identity identity,
    uint32_t key,
    henka_script_state_value* out_value,
    bool* out_present);
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
