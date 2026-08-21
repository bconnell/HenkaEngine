#include <assert.h>
#include <string.h>

#include <henka/henkascript.h>
#include <henka/memory.h>
#include <henka/script_source.h>

static void test_henkascript_source_document_contract(void)
{
    static const char valid_source[] =
        "fn OnCreate() { }\n"
        "fn OnUpdate() { }\n";
    static const char invalid_source[] = "fn OnCreate() {\n";
    char* oversized_source;
    const char* loaded_source = NULL;
    size_t loaded_size = 0U;
    henka_script_source_document* document = NULL;
    henka_script_source_diagnostic diagnostic;
    uint64_t revision;

    assert(henka_script_source_create(
               HENKA_SCRIPT_LANGUAGE_HENKASCRIPT, &document) == HENKA_SUCCESS);
    assert(document != NULL);
    assert(henka_script_source_get_text(
               document, &loaded_source, &loaded_size) == HENKA_SUCCESS);
    assert(loaded_source != NULL && loaded_size == 0U && loaded_source[0] == '\0');
    assert(!henka_script_source_is_dirty(document));
    assert(henka_script_source_get_revision(document) == 0U);

    assert(henka_script_source_set_text(
               document, valid_source, strlen(valid_source)) == HENKA_SUCCESS);
    assert(henka_script_source_get_text(
               document, &loaded_source, &loaded_size) == HENKA_SUCCESS);
    assert(loaded_size == strlen(valid_source));
    assert(strcmp(loaded_source, valid_source) == 0);
    assert(henka_script_source_is_dirty(document));
    revision = henka_script_source_get_revision(document);
    assert(revision == 1U);
    memset(&diagnostic, 0, sizeof(diagnostic));
    assert(henka_script_source_validate(document, &diagnostic) == HENKA_SUCCESS);
    assert(diagnostic.result == HENKA_SUCCESS);
    memset(&diagnostic, 0, sizeof(diagnostic));
    assert(henka_script_source_get_diagnostic(document, &diagnostic) == HENKA_SUCCESS);
    assert(diagnostic.result == HENKA_SUCCESS);

    assert(henka_script_source_set_text(
               document, invalid_source, strlen(invalid_source)) == HENKA_SUCCESS);
    memset(&diagnostic, 0, sizeof(diagnostic));
    assert(henka_script_source_validate(document, &diagnostic) != HENKA_SUCCESS);
    assert(diagnostic.result != HENKA_SUCCESS);
    assert(diagnostic.line > 0U && diagnostic.message[0] != '\0');
    assert(henka_script_source_get_revision(document) == revision + 1U);

    oversized_source = (char*)henka_malloc(HENKA_HKS_MAX_SOURCE_BYTES + 1U);
    assert(oversized_source != NULL);
    memset(oversized_source, 'x', HENKA_HKS_MAX_SOURCE_BYTES + 1U);
    revision = henka_script_source_get_revision(document);
    assert(henka_script_source_set_text(
               document,
               oversized_source,
               HENKA_HKS_MAX_SOURCE_BYTES + 1U) == HENKA_ERROR_LIMIT);
    assert(henka_script_source_get_revision(document) == revision);
    henka_free(oversized_source);
    henka_script_source_destroy(document);
}

static void test_lua_source_document_validation(void)
{
    static const char valid_source[] =
        "function OnCreate()\n"
        "end\n";
    static const char invalid_source[] =
        "function OnCreate(\n"
        "end\n";
    henka_script_source_document* document = NULL;
    henka_script_source_diagnostic diagnostic;

    assert(henka_script_source_create(
               HENKA_SCRIPT_LANGUAGE_LUA, &document) == HENKA_SUCCESS);
    assert(henka_script_source_set_text(
               document, valid_source, strlen(valid_source)) == HENKA_SUCCESS);
    memset(&diagnostic, 0, sizeof(diagnostic));
    assert(henka_script_source_validate(document, &diagnostic) == HENKA_SUCCESS);
    assert(diagnostic.result == HENKA_SUCCESS);
    assert(henka_script_source_set_text(
               document, invalid_source, strlen(invalid_source)) == HENKA_SUCCESS);
    memset(&diagnostic, 0, sizeof(diagnostic));
    assert(henka_script_source_validate(document, &diagnostic) != HENKA_SUCCESS);
    assert(diagnostic.result != HENKA_SUCCESS);
    assert(diagnostic.message[0] != '\0');
    henka_script_source_destroy(document);
}

static void test_source_document_argument_contracts(void)
{
    henka_script_source_document* document = NULL;

    assert(henka_script_source_create(
               HENKA_SCRIPT_LANGUAGE_LUA, NULL) == HENKA_ERROR_INVALID_ARGUMENT);
    assert(henka_script_source_create(
               (henka_script_language)99, &document) == HENKA_ERROR_INVALID_ARGUMENT);
    assert(document == NULL);
    assert(henka_script_source_set_text(NULL, "x", 1U) == HENKA_ERROR_INVALID_ARGUMENT);
    assert(henka_script_source_set_text(NULL, NULL, 0U) == HENKA_ERROR_INVALID_ARGUMENT);
    assert(henka_script_source_get_text(NULL, NULL, NULL) == HENKA_ERROR_INVALID_ARGUMENT);
}

int main(void)
{
    test_henkascript_source_document_contract();
    test_lua_source_document_validation();
    test_source_document_argument_contracts();
    return 0;
}
