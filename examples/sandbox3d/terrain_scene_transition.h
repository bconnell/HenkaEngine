#ifndef SANDBOX3D_TERRAIN_SCENE_TRANSITION_H
#define SANDBOX3D_TERRAIN_SCENE_TRANSITION_H

#include <stdbool.h>
#include <stdint.h>

#include <henka/terrain_edit_history.h>
#include <henka/terrain_storage.h>

typedef henka_result (*sandbox3d_terrain_transition_load_baseline_fn)(
    void* user_data,
    henka_terrain_region_id region_id,
    henka_terrain_region_storage_info* out_info,
    henka_terrain_sample* out_samples,
    size_t sample_count);

typedef henka_result (*sandbox3d_terrain_transition_step_fn)(void* user_data);

typedef struct sandbox3d_terrain_transition_reload_desc
{
    henka_terrain_world* world;
    henka_terrain_edit_history** edit_history;
    sandbox3d_terrain_transition_load_baseline_fn load_baseline;
    sandbox3d_terrain_transition_step_fn refresh_collision;
    sandbox3d_terrain_transition_step_fn refresh_render;
    sandbox3d_terrain_transition_step_fn reload_scene_document;
    void* user_data;
} sandbox3d_terrain_transition_reload_desc;

typedef struct sandbox3d_terrain_transition_reload_report
{
    uint32_t dirty_region_count_before;
    uint32_t restored_region_count;
    uint32_t dirty_region_count_after;
    bool rollback_attempted;
    bool rollback_succeeded;
    const char* failure_stage;
    henka_result operation_result;
} sandbox3d_terrain_transition_reload_report;

henka_result sandbox3d_terrain_scene_transition_reload_discard(
    const sandbox3d_terrain_transition_reload_desc* desc,
    sandbox3d_terrain_transition_reload_report* out_report);

henka_result sandbox3d_terrain_world_content_hash(
    const henka_terrain_world* world,
    uint64_t* out_hash);

#endif
