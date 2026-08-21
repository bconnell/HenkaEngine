#include "script_editor_model.h"

#include <string.h>

#include <henka/memory.h>
#include <henka/scene_document.h>
#include <henka/script_asset.h>

struct sandbox3d_script_editor_model
{
    henka_script_source_document* document;
    henka_script_source_document* saved_document;
    size_t anchor_offset;
    size_t caret_offset;
    size_t vertical_column;
    bool has_vertical_column;
    bool play_active;
    bool focused;
    char asset_path[HENKA_SCENE_DOCUMENT_MAX_PATH_BYTES];
};

static bool sandbox3d_script_editor_model_copy_path(
    char* destination,
    size_t destination_capacity,
    const char* source)
{
    size_t length;
    if (destination == NULL || destination_capacity == 0U || source == NULL)
    {
        return false;
    }
    length = strlen(source);
    if (length >= destination_capacity)
    {
        return false;
    }
    memcpy(destination, source, length + 1U);
    return true;
}

static size_t sandbox3d_script_editor_model_min(
    size_t first,
    size_t second)
{
    return first < second ? first : second;
}

static size_t sandbox3d_script_editor_model_max(
    size_t first,
    size_t second)
{
    return first > second ? first : second;
}

static bool sandbox3d_script_editor_model_line_bounds(
    const char* source,
    size_t source_size,
    size_t line_index,
    size_t* out_start,
    size_t* out_length)
{
    size_t current_line = 0U;
    size_t line_start = 0U;
    size_t index;
    if (out_start != NULL)
    {
        *out_start = 0U;
    }
    if (out_length != NULL)
    {
        *out_length = 0U;
    }
    if (source == NULL || out_start == NULL || out_length == NULL)
    {
        return false;
    }
    for (index = 0U; index <= source_size; ++index)
    {
        if (index == source_size || source[index] == '\n')
        {
            if (current_line == line_index)
            {
                *out_start = line_start;
                *out_length = index - line_start;
                return true;
            }
            ++current_line;
            line_start = index + 1U;
        }
    }
    return false;
}

static bool sandbox3d_script_editor_model_offset_location(
    const char* source,
    size_t source_size,
    size_t offset,
    size_t* out_line_index,
    size_t* out_column)
{
    size_t current_line = 0U;
    size_t line_start = 0U;
    size_t index;
    if (out_line_index != NULL)
    {
        *out_line_index = 0U;
    }
    if (out_column != NULL)
    {
        *out_column = 0U;
    }
    if (source == NULL || offset > source_size ||
        out_line_index == NULL || out_column == NULL)
    {
        return false;
    }
    for (index = 0U; index <= source_size; ++index)
    {
        if (index == source_size || source[index] == '\n')
        {
            if (offset <= index)
            {
                *out_line_index = current_line;
                *out_column = offset - line_start;
                return true;
            }
            ++current_line;
            line_start = index + 1U;
        }
    }
    return false;
}

static void sandbox3d_script_editor_model_reset_selection(
    sandbox3d_script_editor_model* model)
{
    if (model != NULL)
    {
        model->anchor_offset = 0U;
        model->caret_offset = 0U;
        model->vertical_column = 0U;
        model->has_vertical_column = false;
    }
}

henka_result sandbox3d_script_editor_model_create(
    henka_script_language language,
    sandbox3d_script_editor_model** out_model)
{
    sandbox3d_script_editor_model* model;
    henka_result result;
    if (out_model == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_model = NULL;
    model = (sandbox3d_script_editor_model*)henka_calloc(1U, sizeof(*model));
    if (model == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    result = henka_script_source_create(language, &model->document);
    if (result == HENKA_SUCCESS)
    {
        result = henka_script_source_create(language, &model->saved_document);
    }
    if (result != HENKA_SUCCESS)
    {
        henka_script_source_destroy(model->saved_document);
        henka_script_source_destroy(model->document);
        henka_free(model);
        return result;
    }
    *out_model = model;
    return HENKA_SUCCESS;
}

void sandbox3d_script_editor_model_destroy(
    sandbox3d_script_editor_model* model)
{
    if (model == NULL)
    {
        return;
    }
    henka_script_source_destroy(model->saved_document);
    henka_script_source_destroy(model->document);
    henka_free(model);
}

henka_result sandbox3d_script_editor_model_load_text(
    sandbox3d_script_editor_model* model,
    const char* source,
    size_t source_size)
{
    henka_result result;
    if (model == NULL || source == NULL || model->play_active)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_script_source_set_text(
        model->saved_document, source, source_size);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    result = henka_script_source_mark_clean(model->saved_document);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    result = henka_script_source_set_text(
        model->document, source, source_size);
    if (result == HENKA_SUCCESS)
    {
        result = henka_script_source_mark_clean(model->document);
    }
    if (result == HENKA_SUCCESS)
    {
        sandbox3d_script_editor_model_reset_selection(model);
    }
    return result;
}

henka_result sandbox3d_script_editor_model_load_asset(
    sandbox3d_script_editor_model* model,
    const char* project_root,
    const char* relative_path)
{
    henka_script_source_document* loaded_document = NULL;
    const char* source = NULL;
    size_t source_size = 0U;
    henka_script_language loaded_language;
    char validated_path[HENKA_SCENE_DOCUMENT_MAX_PATH_BYTES];
    henka_result result;
    if (model == NULL || model->play_active || project_root == NULL ||
        relative_path == NULL || relative_path[0] == '\0')
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (!sandbox3d_script_editor_model_copy_path(
            validated_path, sizeof(validated_path), relative_path))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_script_asset_load_source_document(
        project_root, relative_path, &loaded_document);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    loaded_language = henka_script_source_get_language(loaded_document);
    if (loaded_language != henka_script_source_get_language(model->document) ||
        henka_script_source_get_text(
            loaded_document, &source, &source_size) != HENKA_SUCCESS)
    {
        henka_script_source_destroy(loaded_document);
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = sandbox3d_script_editor_model_load_text(
        model, source, source_size);
    if (result == HENKA_SUCCESS)
    {
        memcpy(model->asset_path, validated_path, sizeof(validated_path));
    }
    henka_script_source_destroy(loaded_document);
    return result;
}

henka_result sandbox3d_script_editor_model_get_source(
    const sandbox3d_script_editor_model* model,
    const char** out_source,
    size_t* out_source_size)
{
    if (model == NULL)
    {
        if (out_source != NULL)
        {
            *out_source = NULL;
        }
        if (out_source_size != NULL)
        {
            *out_source_size = 0U;
        }
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return henka_script_source_get_text(
        model->document, out_source, out_source_size);
}

henka_script_language sandbox3d_script_editor_model_get_language(
    const sandbox3d_script_editor_model* model)
{
    return model == NULL
        ? HENKA_SCRIPT_LANGUAGE_NONE
        : henka_script_source_get_language(model->document);
}

henka_result sandbox3d_script_editor_model_set_selection(
    sandbox3d_script_editor_model* model,
    size_t anchor_offset,
    size_t caret_offset)
{
    const char* source;
    size_t source_size;
    if (model == NULL ||
        henka_script_source_get_text(
            model->document, &source, &source_size) != HENKA_SUCCESS ||
        anchor_offset > source_size || caret_offset > source_size)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    model->anchor_offset = anchor_offset;
    model->caret_offset = caret_offset;
    model->has_vertical_column = false;
    return HENKA_SUCCESS;
}

henka_result sandbox3d_script_editor_model_set_caret_offset(
    sandbox3d_script_editor_model* model,
    size_t caret_offset)
{
    return sandbox3d_script_editor_model_set_selection(
        model, caret_offset, caret_offset);
}

henka_result sandbox3d_script_editor_model_set_caret_position(
    sandbox3d_script_editor_model* model,
    size_t line_index,
    size_t column)
{
    const char* source;
    size_t source_size;
    size_t line_start;
    size_t line_length;
    if (model == NULL ||
        henka_script_source_get_text(
            model->document, &source, &source_size) != HENKA_SUCCESS ||
        !sandbox3d_script_editor_model_line_bounds(
            source,
            source_size,
            line_index,
            &line_start,
            &line_length))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (column > line_length)
    {
        column = line_length;
    }
    return sandbox3d_script_editor_model_set_caret_offset(
        model, line_start + column);
}

henka_result sandbox3d_script_editor_model_replace_selection(
    sandbox3d_script_editor_model* model,
    const char* replacement,
    size_t replacement_size)
{
    size_t start;
    size_t end;
    henka_result result;
    if (model == NULL || model->play_active ||
        (replacement == NULL && replacement_size != 0U))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    start = sandbox3d_script_editor_model_min(
        model->anchor_offset, model->caret_offset);
    end = sandbox3d_script_editor_model_max(
        model->anchor_offset, model->caret_offset);
    result = henka_script_source_replace_range(
        model->document,
        start,
        end - start,
        replacement,
        replacement_size);
    if (result == HENKA_SUCCESS)
    {
        model->caret_offset = start + replacement_size;
        model->anchor_offset = model->caret_offset;
        model->has_vertical_column = false;
    }
    return result;
}

henka_result sandbox3d_script_editor_model_move_vertical(
    sandbox3d_script_editor_model* model,
    int direction)
{
    const char* source;
    size_t source_size;
    size_t line_index;
    size_t column;
    size_t current_start;
    size_t current_length;
    size_t target_start;
    size_t target_length;
    size_t target_line;
    size_t target_column;
    if (model == NULL ||
        (direction != -1 && direction != 1) ||
        henka_script_source_get_text(
            model->document, &source, &source_size) != HENKA_SUCCESS ||
        !sandbox3d_script_editor_model_offset_location(
            source,
            source_size,
            model->caret_offset,
            &line_index,
            &column) ||
        !sandbox3d_script_editor_model_line_bounds(
            source,
            source_size,
            line_index,
            &current_start,
            &current_length))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    (void)current_start;
    (void)current_length;
    if (!model->has_vertical_column)
    {
        model->vertical_column = column;
        model->has_vertical_column = true;
    }
    if ((direction < 0 && line_index == 0U) ||
        (direction > 0 && !sandbox3d_script_editor_model_line_bounds(
            source,
            source_size,
            line_index + 1U,
            &target_start,
            &target_length)))
    {
        if (direction < 0)
        {
            return HENKA_SUCCESS;
        }
        return HENKA_SUCCESS;
    }
    target_line = direction < 0 ? line_index - 1U : line_index + 1U;
    if (!sandbox3d_script_editor_model_line_bounds(
            source,
            source_size,
            target_line,
            &target_start,
            &target_length))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    target_column = model->vertical_column < target_length
        ? model->vertical_column
        : target_length;
    model->caret_offset = target_start + target_column;
    model->anchor_offset = model->caret_offset;
    return HENKA_SUCCESS;
}

size_t sandbox3d_script_editor_model_get_caret_offset(
    const sandbox3d_script_editor_model* model)
{
    return model == NULL ? 0U : model->caret_offset;
}

size_t sandbox3d_script_editor_model_get_caret_line(
    const sandbox3d_script_editor_model* model)
{
    const char* source;
    size_t source_size;
    size_t line_index;
    size_t column;
    return model != NULL &&
        henka_script_source_get_text(
            model->document, &source, &source_size) == HENKA_SUCCESS &&
        sandbox3d_script_editor_model_offset_location(
            source,
            source_size,
            model->caret_offset,
            &line_index,
            &column)
        ? line_index + 1U
        : 0U;
}

size_t sandbox3d_script_editor_model_get_caret_column(
    const sandbox3d_script_editor_model* model)
{
    const char* source;
    size_t source_size;
    size_t line_index;
    size_t column;
    return model != NULL &&
        henka_script_source_get_text(
            model->document, &source, &source_size) == HENKA_SUCCESS &&
        sandbox3d_script_editor_model_offset_location(
            source,
            source_size,
            model->caret_offset,
            &line_index,
            &column)
        ? column + 1U
        : 0U;
}

henka_result sandbox3d_script_editor_model_set_play_active(
    sandbox3d_script_editor_model* model,
    bool active)
{
    if (model == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    model->play_active = active;
    return HENKA_SUCCESS;
}

bool sandbox3d_script_editor_model_is_play_active(
    const sandbox3d_script_editor_model* model)
{
    return model != NULL && model->play_active;
}

bool sandbox3d_script_editor_model_is_dirty(
    const sandbox3d_script_editor_model* model)
{
    return model != NULL && henka_script_source_is_dirty(model->document);
}

const char* sandbox3d_script_editor_model_get_asset_path(
    const sandbox3d_script_editor_model* model)
{
    return model == NULL ? NULL : model->asset_path;
}

henka_result sandbox3d_script_editor_model_set_focused(
    sandbox3d_script_editor_model* model,
    bool focused)
{
    if (model == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    model->focused = focused;
    return HENKA_SUCCESS;
}

bool sandbox3d_script_editor_model_is_focused(
    const sandbox3d_script_editor_model* model)
{
    return model != NULL && model->focused;
}

henka_result sandbox3d_script_editor_model_validate(
    sandbox3d_script_editor_model* model,
    henka_script_source_diagnostic* out_diagnostic)
{
    if (model == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return henka_script_source_validate(model->document, out_diagnostic);
}

henka_result sandbox3d_script_editor_model_revert(
    sandbox3d_script_editor_model* model)
{
    const char* source;
    size_t source_size;
    size_t caret_offset;
    size_t anchor_offset;
    henka_result result;
    if (model == NULL || model->play_active ||
        henka_script_source_get_text(
            model->saved_document, &source, &source_size) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_script_source_set_text(
        model->document, source, source_size);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    result = henka_script_source_mark_clean(model->document);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    caret_offset = model->caret_offset < source_size
        ? model->caret_offset
        : source_size;
    anchor_offset = model->anchor_offset < source_size
        ? model->anchor_offset
        : source_size;
    model->caret_offset = caret_offset;
    model->anchor_offset = anchor_offset;
    model->has_vertical_column = false;
    return HENKA_SUCCESS;
}

henka_result sandbox3d_script_editor_model_save(
    sandbox3d_script_editor_model* model,
    const char* project_root,
    const char* relative_path)
{
    const char* source;
    size_t source_size;
    henka_script_source_document* staged_saved_document = NULL;
    henka_result result;
    if (model == NULL || model->play_active || project_root == NULL ||
        relative_path == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_script_source_create(
        henka_script_source_get_language(model->document),
        &staged_saved_document);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    result = henka_script_source_get_text(
        model->document, &source, &source_size);
    if (result == HENKA_SUCCESS)
    {
        result = henka_script_source_set_text(
            staged_saved_document, source, source_size);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_script_source_mark_clean(staged_saved_document);
    }
    if (result != HENKA_SUCCESS)
    {
        henka_script_source_destroy(staged_saved_document);
        return result;
    }
    result = henka_script_asset_save_source_document(
        project_root, relative_path, model->document);
    if (result != HENKA_SUCCESS)
    {
        henka_script_source_destroy(staged_saved_document);
        return result;
    }
    henka_script_source_destroy(model->saved_document);
    model->saved_document = staged_saved_document;
    return result;
}
