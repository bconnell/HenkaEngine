#ifndef HENKA_SANDBOX3D_SCENE_PATH_EDITOR_H
#define HENKA_SANDBOX3D_SCENE_PATH_EDITOR_H

#include <stdbool.h>

#include <henka/scene_document.h>
#include <henka/ui.h>

#include "editor_layout.h"
#include "project_scene_catalog.h"

typedef enum sandbox3d_scene_path_editor_mode
{
    SANDBOX3D_SCENE_PATH_EDITOR_MODE_SAVE_AS = 0,
    SANDBOX3D_SCENE_PATH_EDITOR_MODE_OPEN
} sandbox3d_scene_path_editor_mode;

typedef struct sandbox3d_scene_path_editor
{
    char relative_path[HENKA_SCENE_DOCUMENT_MAX_PATH_BYTES];
    sandbox3d_project_scene_catalog* catalog;
    sandbox3d_scene_path_editor_mode mode;
    size_t catalog_page;
    henka_result catalog_result;
    bool catalog_stale;
    bool open;
} sandbox3d_scene_path_editor;

typedef enum sandbox3d_scene_path_editor_action
{
    SANDBOX3D_SCENE_PATH_EDITOR_ACTION_NONE = 0,
    SANDBOX3D_SCENE_PATH_EDITOR_ACTION_SAVE,
    SANDBOX3D_SCENE_PATH_EDITOR_ACTION_OPEN,
    SANDBOX3D_SCENE_PATH_EDITOR_ACTION_CANCEL
} sandbox3d_scene_path_editor_action;

void sandbox3d_scene_path_editor_reset(
    sandbox3d_scene_path_editor* editor);
henka_result sandbox3d_scene_path_editor_begin(
    sandbox3d_scene_path_editor* editor,
    sandbox3d_scene_path_editor_mode mode);
henka_result sandbox3d_scene_path_editor_refresh_catalog(
    sandbox3d_scene_path_editor* editor,
    const char* project_root);
void sandbox3d_scene_path_editor_get_catalog_state(
    const sandbox3d_scene_path_editor* editor,
    size_t* out_scene_count,
    size_t* out_page,
    size_t* out_page_count,
    henka_result* out_refresh_result,
    bool* out_stale);
henka_result sandbox3d_scene_path_editor_get_visible_catalog_entry(
    const sandbox3d_scene_path_editor* editor,
    size_t visible_row,
    const char** out_relative_path);
henka_result sandbox3d_scene_path_editor_get_catalog_signature(
    const sandbox3d_scene_path_editor* editor,
    char* out_signature,
    size_t signature_capacity);
henka_result sandbox3d_scene_path_editor_draw(
    sandbox3d_scene_path_editor* editor,
    henka_ui_context* ui,
    const char* project_root,
    const sandbox3d_editor_details_scene_actions_layout* layout,
    sandbox3d_scene_path_editor_action* out_action);

#endif
