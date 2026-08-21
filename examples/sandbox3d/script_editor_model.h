#ifndef SANDBOX3D_SCRIPT_EDITOR_MODEL_H
#define SANDBOX3D_SCRIPT_EDITOR_MODEL_H

#include <stdbool.h>
#include <stddef.h>

#include <henka/script_source.h>

typedef struct sandbox3d_script_editor_model sandbox3d_script_editor_model;

henka_result sandbox3d_script_editor_model_create(
    henka_script_language language,
    sandbox3d_script_editor_model** out_model);
void sandbox3d_script_editor_model_destroy(
    sandbox3d_script_editor_model* model);

/* Loads both the working and last-saved bounded documents. The loaded text is
 * clean and the caret/selection are collapsed at the beginning. */
henka_result sandbox3d_script_editor_model_load_text(
    sandbox3d_script_editor_model* model,
    const char* source,
    size_t source_size);
henka_result sandbox3d_script_editor_model_load_asset(
    sandbox3d_script_editor_model* model,
    const char* project_root,
    const char* relative_path);
henka_result sandbox3d_script_editor_model_get_source(
    const sandbox3d_script_editor_model* model,
    const char** out_source,
    size_t* out_source_size);
henka_script_language sandbox3d_script_editor_model_get_language(
    const sandbox3d_script_editor_model* model);

/* Selection and caret offsets are UTF-8 source byte offsets. Untouched source
 * bytes, including indentation and line endings, remain byte-for-byte intact. */
henka_result sandbox3d_script_editor_model_set_selection(
    sandbox3d_script_editor_model* model,
    size_t anchor_offset,
    size_t caret_offset);
henka_result sandbox3d_script_editor_model_set_caret_offset(
    sandbox3d_script_editor_model* model,
    size_t caret_offset);
henka_result sandbox3d_script_editor_model_set_caret_position(
    sandbox3d_script_editor_model* model,
    size_t line_index,
    size_t column);
henka_result sandbox3d_script_editor_model_replace_selection(
    sandbox3d_script_editor_model* model,
    const char* replacement,
    size_t replacement_size);
henka_result sandbox3d_script_editor_model_move_vertical(
    sandbox3d_script_editor_model* model,
    int direction);
size_t sandbox3d_script_editor_model_get_caret_offset(
    const sandbox3d_script_editor_model* model);
size_t sandbox3d_script_editor_model_get_caret_line(
    const sandbox3d_script_editor_model* model);
size_t sandbox3d_script_editor_model_get_caret_column(
    const sandbox3d_script_editor_model* model);

henka_result sandbox3d_script_editor_model_set_play_active(
    sandbox3d_script_editor_model* model,
    bool active);
bool sandbox3d_script_editor_model_is_play_active(
    const sandbox3d_script_editor_model* model);
bool sandbox3d_script_editor_model_is_dirty(
    const sandbox3d_script_editor_model* model);
const char* sandbox3d_script_editor_model_get_asset_path(
    const sandbox3d_script_editor_model* model);
henka_result sandbox3d_script_editor_model_set_focused(
    sandbox3d_script_editor_model* model,
    bool focused);
bool sandbox3d_script_editor_model_is_focused(
    const sandbox3d_script_editor_model* model);

henka_result sandbox3d_script_editor_model_validate(
    sandbox3d_script_editor_model* model,
    henka_script_source_diagnostic* out_diagnostic);
henka_result sandbox3d_script_editor_model_revert(
    sandbox3d_script_editor_model* model);
henka_result sandbox3d_script_editor_model_save(
    sandbox3d_script_editor_model* model,
    const char* project_root,
    const char* relative_path);

#endif
