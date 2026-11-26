#ifndef SANDBOX3D_PROJECT_SAVE_TRANSACTION_H
#define SANDBOX3D_PROJECT_SAVE_TRANSACTION_H

#include <stdbool.h>

#include <henka/persistence.h>
#include <henka/scene_document.h>

/*
 * Stage both project save authorities before publishing either one. If the
 * manifest commit fails after the scene commit, restore the previous scene
 * file (or remove the newly-created scene file).
 */
henka_result sandbox3d_project_save_transaction(
    const henka_scene_document* candidate_scene,
    const henka_settings* candidate_manifest,
    const char* project_root,
    const char* scene_relative_path,
    const char* manifest_relative_path,
    bool allow_scene_replace);

#endif
