#include "scene_path_editor.h"

#include <string.h>

void sandbox3d_scene_path_editor_reset(
    sandbox3d_scene_path_editor* editor)
{
    if (editor != NULL)
    {
        memset(editor, 0, sizeof(*editor));
    }
}

henka_result sandbox3d_scene_path_editor_begin(
    sandbox3d_scene_path_editor* editor,
    sandbox3d_scene_path_editor_mode mode)
{
    if (editor == NULL ||
        (mode != SANDBOX3D_SCENE_PATH_EDITOR_MODE_SAVE_AS &&
         mode != SANDBOX3D_SCENE_PATH_EDITOR_MODE_OPEN))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    memset(editor->relative_path, 0, sizeof(editor->relative_path));
    editor->mode = mode;
    editor->open = true;
    return HENKA_SUCCESS;
}

henka_result sandbox3d_scene_path_editor_draw(
    sandbox3d_scene_path_editor* editor,
    henka_ui_context* ui,
    const sandbox3d_editor_details_scene_actions_layout* layout,
    sandbox3d_scene_path_editor_action* out_action)
{
    bool text_changed = false;
    const bool opening_scene = editor != NULL &&
        editor->mode == SANDBOX3D_SCENE_PATH_EDITOR_MODE_OPEN;
    henka_result result;

    if (out_action != NULL)
    {
        *out_action = SANDBOX3D_SCENE_PATH_EDITOR_ACTION_NONE;
    }
    if (editor == NULL || !editor->open || ui == NULL || layout == NULL ||
        out_action == NULL || !layout->visible || !layout->path_editor_open ||
        !layout->path_editor_available ||
        (editor->mode != SANDBOX3D_SCENE_PATH_EDITOR_MODE_SAVE_AS &&
         editor->mode != SANDBOX3D_SCENE_PATH_EDITOR_MODE_OPEN))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    result = henka_ui_label(
        ui,
        layout->path_label.x + 3.0f,
        layout->path_label.y + 5.0f,
        1.0f,
        "Path");
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    result = henka_ui_text_field(
        ui,
        opening_scene
            ? "game_authoring_open_scene_relative_path"
            : "game_authoring_save_as_relative_path",
        layout->path_field,
        editor->relative_path,
        sizeof(editor->relative_path),
        &text_changed);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    (void)text_changed;

    if (henka_ui_button(
            ui,
            opening_scene
                ? "game_authoring_open_scene_confirm"
                : "game_authoring_save_as_confirm",
            layout->path_confirm_button,
            opening_scene ? "Open" : "Save"))
    {
        *out_action = opening_scene
            ? SANDBOX3D_SCENE_PATH_EDITOR_ACTION_OPEN
            : SANDBOX3D_SCENE_PATH_EDITOR_ACTION_SAVE;
    }
    else if (henka_ui_button(
                 ui,
                 opening_scene
                    ? "game_authoring_open_scene_cancel"
                    : "game_authoring_save_as_cancel",
                 layout->path_cancel_button,
                 "Cancel"))
    {
        *out_action = SANDBOX3D_SCENE_PATH_EDITOR_ACTION_CANCEL;
    }
    return HENKA_SUCCESS;
}
