#ifndef HENKA_SCRIPT_ASSET_H
#define HENKA_SCRIPT_ASSET_H

#include <stdbool.h>
#include <stdint.h>

#include <henka/scene_document.h>
#include <henka/script_runtime.h>

typedef struct henka_script_behavior_asset henka_script_behavior_asset;

/* Creates a minimal valid source file beneath project_root. The relative path
 * is confined, the language suffix must match, parent directories are created
 * as needed, and an existing file is never overwritten. */
henka_result henka_script_asset_create_template(
    const char* project_root,
    const char* relative_path,
    henka_script_language language);

/* Loads one persisted Scene Document behavior source from a confined project
 * path and owns the selected bounded language backend until destruction. */
henka_result henka_script_behavior_asset_create(
    const char* project_root,
    const henka_scene_document_behavior* behavior,
    uint64_t entity_id,
    bool enabled,
    uint32_t instruction_budget,
    henka_script_behavior_asset** out_asset);
void henka_script_behavior_asset_destroy(henka_script_behavior_asset* asset);
henka_result henka_script_behavior_asset_get_runtime_desc(
    const henka_script_behavior_asset* asset,
    henka_script_behavior_desc* out_desc);

#endif
