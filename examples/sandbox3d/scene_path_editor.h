#ifndef HENKA_SANDBOX3D_SCENE_PATH_EDITOR_H
#define HENKA_SANDBOX3D_SCENE_PATH_EDITOR_H

#include <stdbool.h>

#include <henka/scene_document.h>
#include <henka/ui.h>

#include "editor_layout.h"

typedef enum sandbox3d_scene_path_editor_mode
{
    SANDBOX3D_SCENE_PATH_EDITOR_MODE_SAVE_AS = 0,
    SANDBOX3D_SCENE_PATH_EDITOR_MODE_OPEN
} sandbox3d_scene_path_editor_mode;

typedef struct sandbox3d_scene_path_editor
{
    char relative_path[HENKA_SCENE_DOCUMENT_MAX_PATH_BYTES];
    sandbox3d_scene_path_editor_mode mode;
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
henka_result sandbox3d_scene_path_editor_draw(
    sandbox3d_scene_path_editor* editor,
    henka_ui_context* ui,
    const sandbox3d_editor_details_scene_actions_layout* layout,
    sandbox3d_scene_path_editor_action* out_action);

#endif
