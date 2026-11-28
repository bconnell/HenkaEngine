#include <assert.h>
#include <string.h>

#include "script_editor_model.h"

static void test_editor_model_preserves_structure_and_moves_caret(void)
{
    static const char source[] =
        "fn OnUpdate() {\n"
        "    i32 value = 1;\n"
        "}\n";
    sandbox3d_script_editor_model* model = NULL;
    const char* text = NULL;
    size_t text_size = 0U;
    size_t line_two_offset = strlen("fn OnUpdate() {\n");

    assert(sandbox3d_script_editor_model_create(
               HENKA_SCRIPT_LANGUAGE_HENKASCRIPT, &model) == HENKA_SUCCESS);
    assert(sandbox3d_script_editor_model_load_text(
               model, source, strlen(source)) == HENKA_SUCCESS);
    assert(!sandbox3d_script_editor_model_is_dirty(model));
    assert(sandbox3d_script_editor_model_set_caret_offset(
               model, line_two_offset + 4U) == HENKA_SUCCESS);
    assert(sandbox3d_script_editor_model_get_caret_line(model) == 2U);
    assert(sandbox3d_script_editor_model_get_caret_column(model) == 5U);
    assert(sandbox3d_script_editor_model_move_vertical(model, 1) == HENKA_SUCCESS);
    assert(sandbox3d_script_editor_model_get_caret_line(model) == 3U);
    assert(sandbox3d_script_editor_model_get_caret_column(model) == 2U);
    assert(sandbox3d_script_editor_model_get_source(
               model, &text, &text_size) == HENKA_SUCCESS);
    assert(text_size == strlen(source));
    assert(memcmp(text, source, text_size) == 0);
    sandbox3d_script_editor_model_destroy(model);
}

static void test_editor_model_stages_invalid_text_and_reverts(void)
{
    static const char source[] =
        "fn OnUpdate() {\n"
        "    i32 value = 1;\n"
        "}\n";
    static const char replacement[] = "fn OnUpdate() {";
    sandbox3d_script_editor_model* model = NULL;
    henka_script_source_diagnostic diagnostic;
    const char* text = NULL;
    size_t text_size = 0U;

    assert(sandbox3d_script_editor_model_create(
               HENKA_SCRIPT_LANGUAGE_HENKASCRIPT, &model) == HENKA_SUCCESS);
    assert(sandbox3d_script_editor_model_load_text(
               model, source, strlen(source)) == HENKA_SUCCESS);
    assert(sandbox3d_script_editor_model_set_selection(
               model, 0U, strlen(source)) == HENKA_SUCCESS);
    assert(sandbox3d_script_editor_model_replace_selection(
               model, replacement, strlen(replacement)) == HENKA_SUCCESS);
    assert(sandbox3d_script_editor_model_is_dirty(model));
    memset(&diagnostic, 0, sizeof(diagnostic));
    assert(sandbox3d_script_editor_model_validate(
               model, &diagnostic) != HENKA_SUCCESS);
    assert(diagnostic.line > 0U && diagnostic.message[0] != '\0');
    assert(sandbox3d_script_editor_model_revert(model) == HENKA_SUCCESS);
    assert(!sandbox3d_script_editor_model_is_dirty(model));
    assert(sandbox3d_script_editor_model_get_source(
               model, &text, &text_size) == HENKA_SUCCESS);
    assert(text_size == strlen(source));
    assert(memcmp(text, source, text_size) == 0);
    sandbox3d_script_editor_model_destroy(model);
}

static void test_dirty_source_rejects_replacement_without_losing_state(void)
{
    static const char source[] = "fn OnUpdate() { }\n";
    static const char edit[] = "fn OnUpdate() { let keep = 1; }\n";
    static const char replacement[] = "fn OnUpdate() { let other = 2; }\n";
    sandbox3d_script_editor_model* model = NULL;
    const char* loaded = NULL;
    const char* asset_path = NULL;
    size_t loaded_size = 0U;

    assert(sandbox3d_script_editor_model_create(
               HENKA_SCRIPT_LANGUAGE_HENKASCRIPT, &model) == HENKA_SUCCESS);
    assert(sandbox3d_script_editor_model_load_text(
               model, source, sizeof(source) - 1U) == HENKA_SUCCESS);
    assert(sandbox3d_script_editor_model_set_selection(
               model, 0U, sizeof(source) - 1U) == HENKA_SUCCESS);
    assert(sandbox3d_script_editor_model_replace_selection(
               model, edit, sizeof(edit) - 1U) == HENKA_SUCCESS);
    assert(sandbox3d_script_editor_model_is_dirty(model));

    assert(sandbox3d_script_editor_model_load_text(
               model, replacement, sizeof(replacement) - 1U) ==
           HENKA_ERROR_INVALID_STATE);
    assert(sandbox3d_script_editor_model_get_source(
               model, &loaded, &loaded_size) == HENKA_SUCCESS);
    assert(loaded_size == sizeof(edit) - 1U);
    assert(memcmp(loaded, edit, loaded_size) == 0);
    assert(sandbox3d_script_editor_model_is_dirty(model));
    assert(sandbox3d_script_editor_model_get_caret_offset(model) ==
           sizeof(edit) - 1U);
    asset_path = sandbox3d_script_editor_model_get_asset_path(model);
    assert(asset_path != NULL && asset_path[0] == '\0');

    /* A dirty buffer must reject a context switch before touching the source
     * filesystem, even when the requested asset is unavailable. */
    assert(sandbox3d_script_editor_model_load_asset(
               model, "missing-project-root", "scripts/other.hks") ==
           HENKA_ERROR_INVALID_STATE);
    assert(sandbox3d_script_editor_model_get_source(
               model, &loaded, &loaded_size) == HENKA_SUCCESS);
    assert(loaded_size == sizeof(edit) - 1U);
    assert(memcmp(loaded, edit, loaded_size) == 0);
    assert(sandbox3d_script_editor_model_is_dirty(model));
    assert(strcmp(sandbox3d_script_editor_model_get_asset_path(model), "") == 0);
    sandbox3d_script_editor_model_destroy(model);
}

static void test_editor_model_rejects_play_mutation(void)
{
    static const char source[] = "fn OnUpdate() { }\n";
    static const char replacement[] = "fn OnStop() { }\n";
    sandbox3d_script_editor_model* model = NULL;
    const char* text = NULL;
    size_t text_size = 0U;

    assert(sandbox3d_script_editor_model_create(
               HENKA_SCRIPT_LANGUAGE_HENKASCRIPT, &model) == HENKA_SUCCESS);
    assert(sandbox3d_script_editor_model_load_text(
               model, source, strlen(source)) == HENKA_SUCCESS);
    assert(sandbox3d_script_editor_model_set_play_active(model, true) == HENKA_SUCCESS);
    assert(sandbox3d_script_editor_model_set_selection(
               model, 0U, strlen(source)) == HENKA_SUCCESS);
    assert(sandbox3d_script_editor_model_replace_selection(
               model, replacement, strlen(replacement)) == HENKA_ERROR_INVALID_ARGUMENT);
    assert(sandbox3d_script_editor_model_get_source(
               model, &text, &text_size) == HENKA_SUCCESS);
    assert(text_size == strlen(source));
    assert(memcmp(text, source, text_size) == 0);
    sandbox3d_script_editor_model_destroy(model);
}

static void test_editor_model_focus_and_position_are_bounded(void)
{
    static const char source[] = "one\n  two\n";
    sandbox3d_script_editor_model* model = NULL;

    assert(sandbox3d_script_editor_model_create(
               HENKA_SCRIPT_LANGUAGE_LUA, &model) == HENKA_SUCCESS);
    assert(sandbox3d_script_editor_model_load_text(
               model, source, strlen(source)) == HENKA_SUCCESS);
    assert(!sandbox3d_script_editor_model_is_focused(model));
    assert(sandbox3d_script_editor_model_set_focused(model, true) == HENKA_SUCCESS);
    assert(sandbox3d_script_editor_model_is_focused(model));
    assert(sandbox3d_script_editor_model_set_caret_position(model, 1U, 99U) == HENKA_SUCCESS);
    assert(sandbox3d_script_editor_model_get_caret_line(model) == 2U);
    assert(sandbox3d_script_editor_model_get_caret_column(model) == 6U);
    assert(strcmp(
               sandbox3d_script_editor_model_get_asset_path(model), "") == 0);
    sandbox3d_script_editor_model_destroy(model);
}

static void test_editor_model_crlf_lines_exclude_terminator_from_columns(void)
{
    static const char source[] = "a\r\nbc\r\n";
    sandbox3d_script_editor_model* model = NULL;
    const size_t second_line_offset = 3U;
    const char* loaded_source = NULL;
    size_t loaded_source_size = 0U;
    size_t line_offset = 0U;
    size_t line_length = 0U;

    assert(sandbox3d_script_editor_model_get_line_span(
        source,
        sizeof(source) - 1U,
        0U,
        &line_offset,
        &line_length));
    assert(line_offset == 0U);
    assert(line_length == 1U);
    assert(sandbox3d_script_editor_model_get_line_span(
        source,
        sizeof(source) - 1U,
        1U,
        &line_offset,
        &line_length));
    assert(line_offset == second_line_offset);
    assert(line_length == 2U);

    assert(sandbox3d_script_editor_model_create(
               HENKA_SCRIPT_LANGUAGE_LUA, &model) == HENKA_SUCCESS);
    assert(sandbox3d_script_editor_model_load_text(
               model, source, sizeof(source) - 1U) == HENKA_SUCCESS);

    /* The visible end of the first line is before CR, while source offsets
     * continue to count both bytes of the original CRLF sequence. */
    assert(sandbox3d_script_editor_model_set_caret_position(
               model, 0U, 99U) == HENKA_SUCCESS);
    assert(sandbox3d_script_editor_model_get_caret_offset(model) == 1U);
    assert(sandbox3d_script_editor_model_get_caret_column(model) == 2U);

    assert(sandbox3d_script_editor_model_set_caret_offset(model, 2U) == HENKA_SUCCESS);
    /* An offset between CR and LF remains on the visible end of line one. */
    assert(sandbox3d_script_editor_model_get_caret_line(model) == 1U);
    assert(sandbox3d_script_editor_model_get_caret_column(model) == 2U);
    assert(sandbox3d_script_editor_model_move_vertical(model, 1) == HENKA_SUCCESS);
    assert(sandbox3d_script_editor_model_get_caret_offset(model) ==
           second_line_offset + 1U);
    assert(sandbox3d_script_editor_model_get_caret_column(model) == 2U);
    assert(sandbox3d_script_editor_model_get_source(
        model,
        &loaded_source,
        &loaded_source_size) == HENKA_SUCCESS);
    assert(loaded_source_size == sizeof(source) - 1U);
    assert(memcmp(loaded_source, source, loaded_source_size) == 0);

    sandbox3d_script_editor_model_destroy(model);
}

int main(void)
{
    test_editor_model_preserves_structure_and_moves_caret();
    test_editor_model_stages_invalid_text_and_reverts();
    test_dirty_source_rejects_replacement_without_losing_state();
    test_editor_model_rejects_play_mutation();
    test_editor_model_focus_and_position_are_bounded();
    test_editor_model_crlf_lines_exclude_terminator_from_columns();
    return 0;
}
