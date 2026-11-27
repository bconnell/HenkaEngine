#include "scene_path_editor.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void sandbox3d_scene_path_editor_reset(
    sandbox3d_scene_path_editor* editor)
{
    if (editor != NULL)
    {
        free(editor->catalog);
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
    if (editor->open)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    memset(editor->relative_path, 0, sizeof(editor->relative_path));
    editor->mode = mode;
    editor->catalog_page = 0U;
    editor->catalog_result = mode == SANDBOX3D_SCENE_PATH_EDITOR_MODE_OPEN
        ? HENKA_ERROR_PLATFORM : HENKA_SUCCESS;
    editor->catalog_stale = false;
    editor->open = true;
    return HENKA_SUCCESS;
}

henka_result sandbox3d_scene_path_editor_refresh_catalog(
    sandbox3d_scene_path_editor* editor,
    const char* project_root)
{
    sandbox3d_project_scene_catalog* candidate;
    henka_result result;

    if (editor == NULL || !editor->open ||
        editor->mode != SANDBOX3D_SCENE_PATH_EDITOR_MODE_OPEN)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (project_root == NULL || project_root[0] == '\0')
    {
        editor->catalog_result = HENKA_ERROR_INVALID_ARGUMENT;
        editor->catalog_stale = editor->catalog != NULL;
        return editor->catalog_result;
    }
    candidate = (sandbox3d_project_scene_catalog*)malloc(sizeof(*candidate));
    if (candidate == NULL)
    {
        editor->catalog_result = HENKA_ERROR_OUT_OF_MEMORY;
        editor->catalog_stale = editor->catalog != NULL;
        return editor->catalog_result;
    }
    result = sandbox3d_project_scene_catalog_build(project_root, candidate);
    if (result != HENKA_SUCCESS)
    {
        free(candidate);
        editor->catalog_result = result;
        editor->catalog_stale = editor->catalog != NULL;
        return result;
    }
    free(editor->catalog);
    editor->catalog = candidate;
    editor->catalog_page = 0U;
    editor->catalog_result = HENKA_SUCCESS;
    editor->catalog_stale = false;
    return HENKA_SUCCESS;
}

void sandbox3d_scene_path_editor_get_catalog_state(
    const sandbox3d_scene_path_editor* editor,
    size_t* out_scene_count,
    size_t* out_page,
    size_t* out_page_count,
    henka_result* out_refresh_result,
    bool* out_stale)
{
    const size_t scene_count = editor != NULL && editor->catalog != NULL
        ? editor->catalog->count : 0U;
    const size_t page_count = scene_count == 0U ? 0U
        : (scene_count + SANDBOX3D_EDITOR_SCENE_CATALOG_VISIBLE_ROWS - 1U) /
            SANDBOX3D_EDITOR_SCENE_CATALOG_VISIBLE_ROWS;

    if (out_scene_count != NULL)
    {
        *out_scene_count = scene_count;
    }
    if (out_page != NULL)
    {
        *out_page = editor != NULL ? editor->catalog_page : 0U;
    }
    if (out_page_count != NULL)
    {
        *out_page_count = page_count;
    }
    if (out_refresh_result != NULL)
    {
        *out_refresh_result = editor != NULL
            ? editor->catalog_result : HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (out_stale != NULL)
    {
        *out_stale = editor != NULL && editor->catalog_stale;
    }
}

henka_result sandbox3d_scene_path_editor_get_visible_catalog_entry(
    const sandbox3d_scene_path_editor* editor,
    size_t visible_row,
    const char** out_relative_path)
{
    size_t scene_index;
    if (out_relative_path != NULL)
    {
        *out_relative_path = NULL;
    }
    if (editor == NULL || out_relative_path == NULL ||
        visible_row >= SANDBOX3D_EDITOR_SCENE_CATALOG_VISIBLE_ROWS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (editor->catalog == NULL ||
        editor->catalog_page >
            (SIZE_MAX - visible_row) /
                SANDBOX3D_EDITOR_SCENE_CATALOG_VISIBLE_ROWS)
    {
        return HENKA_ERROR_LIMIT;
    }
    scene_index = editor->catalog_page *
        SANDBOX3D_EDITOR_SCENE_CATALOG_VISIBLE_ROWS + visible_row;
    if (scene_index >= editor->catalog->count)
    {
        return HENKA_ERROR_LIMIT;
    }
    *out_relative_path = editor->catalog->entries[scene_index].relative_path;
    return HENKA_SUCCESS;
}

henka_result sandbox3d_scene_path_editor_get_catalog_signature(
    const sandbox3d_scene_path_editor* editor,
    char* out_signature,
    size_t signature_capacity)
{
    size_t scene_count = 0U;
    size_t page = 0U;
    size_t page_count = 0U;
    henka_result refresh_result = HENKA_ERROR_INVALID_ARGUMENT;
    bool stale = false;
    size_t signature_length;
    size_t row;
    int written;

    if (editor == NULL || out_signature == NULL || signature_capacity == 0U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    out_signature[0] = '\0';
    sandbox3d_scene_path_editor_get_catalog_state(
        editor,
        &scene_count,
        &page,
        &page_count,
        &refresh_result,
        &stale);
    written = snprintf(
        out_signature,
        signature_capacity,
        "%zu:%zu:%zu:%d:%u:%s",
        scene_count,
        page,
        page_count,
        (int)refresh_result,
        stale ? 1U : 0U,
        editor->relative_path);
    if (written < 0 || (size_t)written >= signature_capacity)
    {
        out_signature[0] = '\0';
        return HENKA_ERROR_LIMIT;
    }
    signature_length = (size_t)written;
    for (row = 0U;
         row < SANDBOX3D_EDITOR_SCENE_CATALOG_VISIBLE_ROWS;
         ++row)
    {
        const char* relative_path = NULL;
        const char* signature_path =
            sandbox3d_scene_path_editor_get_visible_catalog_entry(
                editor, row, &relative_path) == HENKA_SUCCESS
            ? relative_path : "-";
        const size_t remaining = signature_capacity - signature_length;
        written = snprintf(
            out_signature + signature_length,
            remaining,
            ":%s",
            signature_path);
        if (written < 0 || (size_t)written >= remaining)
        {
            out_signature[0] = '\0';
            return HENKA_ERROR_LIMIT;
        }
        signature_length += (size_t)written;
    }
    return HENKA_SUCCESS;
}

henka_result sandbox3d_scene_path_editor_draw(
    sandbox3d_scene_path_editor* editor,
    henka_ui_context* ui,
    const char* project_root,
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

    if (opening_scene)
    {
        size_t scene_count = 0U;
        size_t page_count = 0U;
        size_t row;
        char status[64];

        if (!layout->path_editor_open_scene ||
            layout->path_catalog_visible_entry_count !=
                SANDBOX3D_EDITOR_SCENE_CATALOG_VISIBLE_ROWS)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        if (henka_ui_button(
                ui,
                "game_authoring_open_scene_catalog_refresh",
                layout->path_refresh_catalog_button,
                "Refresh"))
        {
            (void)sandbox3d_scene_path_editor_refresh_catalog(
                editor, project_root);
        }
        scene_count = editor->catalog != NULL ? editor->catalog->count : 0U;
        page_count = scene_count == 0U ? 0U
            : (scene_count +
                SANDBOX3D_EDITOR_SCENE_CATALOG_VISIBLE_ROWS - 1U) /
                SANDBOX3D_EDITOR_SCENE_CATALOG_VISIBLE_ROWS;
        if (page_count > 0U && editor->catalog_page >= page_count)
        {
            editor->catalog_page = page_count - 1U;
        }
        else if (page_count == 0U)
        {
            editor->catalog_page = 0U;
        }
        if (editor->catalog_result != HENKA_SUCCESS)
        {
            (void)snprintf(status, sizeof(status),
                editor->catalog_stale
                    ? "Refresh failed; stale list."
                    : "Scene list unavailable.");
        }
        else if (scene_count == 0U)
        {
            (void)snprintf(status, sizeof(status), "No scenes found.");
        }
        else
        {
            (void)snprintf(status, sizeof(status), "%zu scenes", scene_count);
        }
        result = henka_ui_label(
            ui,
            layout->path_catalog_status_label.x + 2.0f,
            layout->path_catalog_status_label.y + 4.0f,
            1.0f,
            status);
        if (result != HENKA_SUCCESS)
        {
            return result;
        }
        for (row = 0U;
             row < SANDBOX3D_EDITOR_SCENE_CATALOG_VISIBLE_ROWS;
             ++row)
        {
            const char* selected_path = NULL;
            if (sandbox3d_scene_path_editor_get_visible_catalog_entry(
                    editor, row, &selected_path) != HENKA_SUCCESS)
            {
                continue;
            }
            if (henka_ui_button(
                    ui,
                    row == 0U
                        ? "game_authoring_open_scene_catalog_entry_0"
                        : (row == 1U
                            ? "game_authoring_open_scene_catalog_entry_1"
                            : "game_authoring_open_scene_catalog_entry_2"),
                    layout->path_catalog_entry_buttons[row],
                    selected_path))
            {
                memcpy(editor->relative_path, selected_path,
                    strlen(selected_path) + 1U);
            }
        }
        if (page_count > 1U)
        {
            char page_label[32];
            if (henka_ui_button(
                    ui,
                    "game_authoring_open_scene_catalog_previous",
                    layout->path_catalog_previous_button,
                    "Prev") && editor->catalog_page > 0U)
            {
                --editor->catalog_page;
            }
            if (henka_ui_button(
                    ui,
                    "game_authoring_open_scene_catalog_next",
                    layout->path_catalog_next_button,
                    "Next") && editor->catalog_page + 1U < page_count)
            {
                ++editor->catalog_page;
            }
            (void)snprintf(page_label, sizeof(page_label), "%zu/%zu",
                editor->catalog_page + 1U, page_count);
            result = henka_ui_label(
                ui,
                layout->path_catalog_page_label.x + 2.0f,
                layout->path_catalog_page_label.y + 4.0f,
                1.0f,
                page_label);
            if (result != HENKA_SUCCESS)
            {
                return result;
            }
        }
        else if (page_count == 1U)
        {
            result = henka_ui_label(
                ui,
                layout->path_catalog_page_label.x + 2.0f,
                layout->path_catalog_page_label.y + 4.0f,
                1.0f,
                "1/1");
            if (result != HENKA_SUCCESS)
            {
                return result;
            }
        }
    }

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
