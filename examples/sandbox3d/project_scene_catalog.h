#ifndef HENKA_SANDBOX3D_PROJECT_SCENE_CATALOG_H
#define HENKA_SANDBOX3D_PROJECT_SCENE_CATALOG_H

#include <stddef.h>

#include <henka/core.h>
#include <henka/scene_document.h>

#define SANDBOX3D_PROJECT_SCENE_CATALOG_MAX_SCENES 128U
#define SANDBOX3D_PROJECT_SCENE_CATALOG_MAX_DIRECTORIES 128U
#define SANDBOX3D_PROJECT_SCENE_CATALOG_MAX_DEPTH 8U
#define SANDBOX3D_PROJECT_SCENE_CATALOG_MAX_ENUMERATED_ENTRIES 8192U

typedef struct sandbox3d_project_scene_catalog_entry
{
    char relative_path[HENKA_SCENE_DOCUMENT_MAX_PATH_BYTES];
} sandbox3d_project_scene_catalog_entry;

typedef struct sandbox3d_project_scene_catalog
{
    sandbox3d_project_scene_catalog_entry entries[
        SANDBOX3D_PROJECT_SCENE_CATALOG_MAX_SCENES];
    size_t count;
} sandbox3d_project_scene_catalog;

/* Builds a deterministic, bounded catalog from project files. The output is
 * replaced only after the complete scan succeeds. Reparse points are skipped
 * so discovery cannot leave the active project tree. */
henka_result sandbox3d_project_scene_catalog_build(
    const char* project_root,
    sandbox3d_project_scene_catalog* out_catalog);

#endif
