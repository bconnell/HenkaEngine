#ifndef HENKA_SCRIPT_ASSET_H
#define HENKA_SCRIPT_ASSET_H

#include <stdbool.h>
#include <stdint.h>

#include <henka/scene_document.h>
#include <henka/script_runtime.h>
#include <henka/script_source.h>

typedef struct henka_script_behavior_asset henka_script_behavior_asset;

/* Creates a minimal valid source file beneath project_root. The relative path
 * is confined, the language suffix must match, parent directories are created
 * as needed, and an existing file is never overwritten. */
henka_result henka_script_asset_create_template(
    const char* project_root,
    const char* relative_path,
    henka_script_language language);

/* Reads a bounded project-relative script source without taking ownership of
 * the caller's path. The returned buffer is engine-owned, NUL-terminated, and
 * must be released with henka_free. Path resolution remains confined to
 * project_root. */
henka_result henka_script_asset_read_source(
    const char* project_root,
    const char* relative_path,
    char** out_source,
    size_t* out_source_size);

/* Loads or atomically saves a bounded editable source document under a
 * confined project-relative path. Loading marks the document clean. Saving
 * permits invalid source for recovery, but marks the document clean only
 * after the destination replacement succeeds. */
henka_result henka_script_asset_load_source_document(
    const char* project_root,
    const char* relative_path,
    henka_script_source_document** out_document);
henka_result henka_script_asset_save_source_document(
    const char* project_root,
    const char* relative_path,
    henka_script_source_document* document);

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
