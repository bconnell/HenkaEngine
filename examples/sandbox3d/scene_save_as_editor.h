#ifndef HENKA_SANDBOX3D_SCENE_SAVE_AS_EDITOR_H
#define HENKA_SANDBOX3D_SCENE_SAVE_AS_EDITOR_H

#include <stdbool.h>

#include <henka/scene_document.h>
#include <henka/ui.h>

#include "editor_layout.h"

typedef struct sandbox3d_scene_save_as_editor
{
    char relative_path[HENKA_SCENE_DOCUMENT_MAX_PATH_BYTES];
    bool open;
} sandbox3d_scene_save_as_editor;

typedef enum sandbox3d_scene_save_as_editor_action
{
    SANDBOX3D_SCENE_SAVE_AS_EDITOR_ACTION_NONE = 0,
    SANDBOX3D_SCENE_SAVE_AS_EDITOR_ACTION_SAVE,
    SANDBOX3D_SCENE_SAVE_AS_EDITOR_ACTION_CANCEL
} sandbox3d_scene_save_as_editor_action;

void sandbox3d_scene_save_as_editor_reset(
    sandbox3d_scene_save_as_editor* editor);
henka_result sandbox3d_scene_save_as_editor_begin(
    sandbox3d_scene_save_as_editor* editor);
henka_result sandbox3d_scene_save_as_editor_draw(
    sandbox3d_scene_save_as_editor* editor,
    henka_ui_context* ui,
    const sandbox3d_editor_details_scene_actions_layout* layout,
    sandbox3d_scene_save_as_editor_action* out_action);

#endif
