#include "scene_name_editor.h"

#include <math.h>
#include <string.h>

static bool sandbox3d_scene_name_editor_rect_is_valid(henka_ui_rect rect)
{
    return isfinite(rect.x) && isfinite(rect.y) &&
        isfinite(rect.width) && isfinite(rect.height) &&
        rect.width > 0.0f && rect.height > 0.0f;
}

void sandbox3d_scene_name_editor_reset(
    sandbox3d_scene_name_editor* editor)
{
    if (editor != NULL)
    {
        memset(editor, 0, sizeof(*editor));
    }
}

henka_result sandbox3d_scene_name_editor_sync(
    sandbox3d_scene_name_editor* editor,
    const char* canonical_scene_name)
{
    char candidate_name[HENKA_SCENE_DOCUMENT_MAX_NAME_BYTES];
    size_t name_length = 0U;

    if (editor == NULL || canonical_scene_name == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    while (name_length < HENKA_SCENE_DOCUMENT_MAX_NAME_BYTES &&
        canonical_scene_name[name_length] != '\0')
    {
        ++name_length;
    }
    if (name_length == 0U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (name_length >= HENKA_SCENE_DOCUMENT_MAX_NAME_BYTES)
    {
        return HENKA_ERROR_LIMIT;
    }
    memcpy(candidate_name, canonical_scene_name, name_length + 1U);

    if (!editor->initialized ||
        strcmp(editor->observed_scene_name, candidate_name) != 0)
    {
        memset(editor->edit_buffer, 0, sizeof(editor->edit_buffer));
        memset(editor->observed_scene_name, 0,
            sizeof(editor->observed_scene_name));
        memcpy(editor->edit_buffer, candidate_name, name_length + 1U);
        memcpy(editor->observed_scene_name, candidate_name, name_length + 1U);
        editor->initialized = true;
    }
    return HENKA_SUCCESS;
}

henka_result sandbox3d_scene_name_editor_draw(
    sandbox3d_scene_name_editor* editor,
    henka_ui_context* ui,
    sandbox3d_game_authoring* authoring,
    const sandbox3d_editor_scene_objects_layout* layout,
    bool* out_applied)
{
    char canonical_scene_name[HENKA_SCENE_DOCUMENT_MAX_NAME_BYTES];
    bool text_changed = false;
    henka_result result;

    if (out_applied != NULL)
    {
        *out_applied = false;
    }
    if (editor == NULL || ui == NULL || authoring == NULL || layout == NULL ||
        out_applied == NULL ||
        !sandbox3d_scene_name_editor_rect_is_valid(layout->scene_name_label) ||
        !sandbox3d_scene_name_editor_rect_is_valid(layout->scene_name_field) ||
        !sandbox3d_scene_name_editor_rect_is_valid(layout->scene_name_apply))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    result = sandbox3d_game_authoring_get_scene_name(
        authoring,
        canonical_scene_name,
        sizeof(canonical_scene_name));
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    result = sandbox3d_scene_name_editor_sync(editor, canonical_scene_name);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    result = henka_ui_label(
        ui,
        layout->scene_name_label.x,
        layout->scene_name_label.y,
        1.0f,
        "Name");
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    result = henka_ui_text_field(
        ui,
        "scene_name_editor_value",
        layout->scene_name_field,
        editor->edit_buffer,
        sizeof(editor->edit_buffer),
        &text_changed);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    (void)text_changed;

    if (henka_ui_button(
            ui,
            "scene_name_editor_apply",
            layout->scene_name_apply,
            "Rename"))
    {
        result = sandbox3d_game_authoring_set_scene_name(
            authoring,
            editor->edit_buffer);
        if (result != HENKA_SUCCESS)
        {
            return result;
        }
        result = sandbox3d_scene_name_editor_sync(
            editor,
            editor->edit_buffer);
        if (result != HENKA_SUCCESS)
        {
            return result;
        }
        *out_applied = true;
    }
    return HENKA_SUCCESS;
}
