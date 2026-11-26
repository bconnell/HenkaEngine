#include "scene_save_as_editor.h"

#include <string.h>

void sandbox3d_scene_save_as_editor_reset(
    sandbox3d_scene_save_as_editor* editor)
{
    if (editor != NULL)
    {
        memset(editor, 0, sizeof(*editor));
    }
}

henka_result sandbox3d_scene_save_as_editor_begin(
    sandbox3d_scene_save_as_editor* editor)
{
    if (editor == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    memset(editor->relative_path, 0, sizeof(editor->relative_path));
    editor->open = true;
    return HENKA_SUCCESS;
}

henka_result sandbox3d_scene_save_as_editor_draw(
    sandbox3d_scene_save_as_editor* editor,
    henka_ui_context* ui,
    const sandbox3d_editor_details_scene_actions_layout* layout,
    sandbox3d_scene_save_as_editor_action* out_action)
{
    bool text_changed = false;
    henka_result result;

    if (out_action != NULL)
    {
        *out_action = SANDBOX3D_SCENE_SAVE_AS_EDITOR_ACTION_NONE;
    }
    if (editor == NULL || !editor->open || ui == NULL || layout == NULL ||
        out_action == NULL || !layout->visible || !layout->save_as_open ||
        !layout->save_as_form_available)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    result = henka_ui_label(
        ui,
        layout->save_as_path_label.x + 3.0f,
        layout->save_as_path_label.y + 5.0f,
        1.0f,
        "Path");
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    result = henka_ui_text_field(
        ui,
        "game_authoring_save_as_relative_path",
        layout->save_as_path_field,
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
            "game_authoring_save_as_confirm",
            layout->save_as_confirm_button,
            "Save"))
    {
        *out_action = SANDBOX3D_SCENE_SAVE_AS_EDITOR_ACTION_SAVE;
    }
    else if (henka_ui_button(
                 ui,
                 "game_authoring_save_as_cancel",
                 layout->save_as_cancel_button,
                 "Cancel"))
    {
        *out_action = SANDBOX3D_SCENE_SAVE_AS_EDITOR_ACTION_CANCEL;
    }
    return HENKA_SUCCESS;
}
