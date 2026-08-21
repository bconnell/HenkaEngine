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

int main(void)
{
    test_editor_model_preserves_structure_and_moves_caret();
    test_editor_model_stages_invalid_text_and_reverts();
    test_editor_model_rejects_play_mutation();
    test_editor_model_focus_and_position_are_bounded();
    return 0;
}
