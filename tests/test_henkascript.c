#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <henka/henkascript.h>
#include <henka/memory.h>

static void expect_diagnostic(
    const char* source,
    henka_hks_diagnostic_code expected_code)
{
    henka_hks_program* program = NULL;
    henka_hks_diagnostic diagnostic;

    memset(&diagnostic, 0, sizeof(diagnostic));
    assert(henka_hks_compile(
               source,
               strlen(source),
               &program,
               &diagnostic) != HENKA_SUCCESS);
    assert(program == NULL);
    assert(diagnostic.code == expected_code);
    assert(diagnostic.message[0] != '\0');
}

static void test_lex_and_compile_valid_program(void)
{
    static const char source[] =
        "i32 health = 3;\n"
        "count := health + 1;\n"
        "fn Update() { health = health + 1; }\n"
        "behavior Door() { return; }\n";
    henka_hks_token tokens[HENKA_HKS_MAX_TOKENS];
    henka_hks_diagnostic diagnostic;
    henka_hks_program* program = NULL;
    henka_hks_binding_info binding;
    henka_hks_callable_info callable;
    size_t token_count = 0U;
    size_t index;
    bool saw_lbrace = false;
    bool saw_rbrace = false;
    bool saw_semicolon = false;
    bool saw_infer = false;
    bool saw_fn = false;
    bool saw_behavior = false;

    memset(&diagnostic, 0, sizeof(diagnostic));
    assert(henka_hks_lex(
               source,
               strlen(source),
               tokens,
               HENKA_HKS_MAX_TOKENS,
               &token_count,
               &diagnostic) == HENKA_SUCCESS);
    assert(token_count > 0U);
    for (index = 0U; index < token_count; ++index)
    {
        saw_lbrace = saw_lbrace || tokens[index].kind == HENKA_HKS_TOKEN_LBRACE;
        saw_rbrace = saw_rbrace || tokens[index].kind == HENKA_HKS_TOKEN_RBRACE;
        saw_semicolon = saw_semicolon || tokens[index].kind == HENKA_HKS_TOKEN_SEMICOLON;
        saw_infer = saw_infer || tokens[index].kind == HENKA_HKS_TOKEN_INFER;
        saw_fn = saw_fn || tokens[index].kind == HENKA_HKS_TOKEN_KW_FN;
        saw_behavior = saw_behavior || tokens[index].kind == HENKA_HKS_TOKEN_KW_BEHAVIOR;
    }
    assert(saw_lbrace && saw_rbrace && saw_semicolon && saw_infer);
    assert(saw_fn && saw_behavior);

    memset(&diagnostic, 0, sizeof(diagnostic));
    assert(henka_hks_compile(
               source,
               strlen(source),
               &program,
               &diagnostic) == HENKA_SUCCESS);
    assert(program != NULL);
    assert(henka_hks_program_get_binding_count(program) == 2U);
    assert(henka_hks_program_get_binding(program, 0U, &binding) == HENKA_SUCCESS);
    assert(strcmp(binding.name, "health") == 0);
    assert(binding.type == HENKA_HKS_TYPE_I32);
    assert(!binding.inferred);
    assert(henka_hks_program_get_binding(program, 1U, &binding) == HENKA_SUCCESS);
    assert(strcmp(binding.name, "count") == 0);
    assert(binding.type == HENKA_HKS_TYPE_I32);
    assert(binding.inferred);
    assert(henka_hks_program_get_callable_count(program) == 2U);
    assert(henka_hks_program_get_callable(program, 0U, &callable) == HENKA_SUCCESS);
    assert(strcmp(callable.name, "Update") == 0);
    assert(!callable.behavior);
    assert(henka_hks_program_get_callable(program, 1U, &callable) == HENKA_SUCCESS);
    assert(strcmp(callable.name, "Door") == 0);
    assert(callable.behavior);
    assert(henka_hks_program_get_ast_node_count(program) > 0U);
    henka_hks_program_destroy(program);
}

static void test_rejects_unsafe_or_invalid_source(void)
{
    char source[HENKA_HKS_MAX_SOURCE_BYTES + 1U];
    henka_hks_token tokens[HENKA_HKS_MAX_TOKENS];
    henka_hks_diagnostic diagnostic;
    size_t token_count = 0U;

    expect_diagnostic("let i32 x = 1;", HENKA_HKS_DIAGNOSTIC_FORBIDDEN_KEYWORD);
    expect_diagnostic("var x = 1;", HENKA_HKS_DIAGNOSTIC_FORBIDDEN_KEYWORD);
    expect_diagnostic("bool enabled = 1;", HENKA_HKS_DIAGNOSTIC_TYPE_MISMATCH);
    expect_diagnostic("i32 x = missing;", HENKA_HKS_DIAGNOSTIC_UNKNOWN_NAME);
    expect_diagnostic("fn Update() { i32 x = 1; x = missing; }", HENKA_HKS_DIAGNOSTIC_UNKNOWN_NAME);
    expect_diagnostic("fn Update() {} fn Update() {}", HENKA_HKS_DIAGNOSTIC_DUPLICATE_NAME);
    expect_diagnostic("i32 x = 1", HENKA_HKS_DIAGNOSTIC_UNEXPECTED_TOKEN);
    expect_diagnostic("i32 x = \"unterminated;", HENKA_HKS_DIAGNOSTIC_INVALID_LITERAL);

    memset(source, 'x', sizeof(source));
    memset(&diagnostic, 0, sizeof(diagnostic));
    assert(henka_hks_lex(
               source,
               HENKA_HKS_MAX_SOURCE_BYTES + 1U,
               tokens,
               HENKA_HKS_MAX_TOKENS,
               &token_count,
               &diagnostic) != HENKA_SUCCESS);
    assert(diagnostic.code == HENKA_HKS_DIAGNOSTIC_LIMIT);
}

static void test_argument_and_memory_contracts(void)
{
    henka_hks_token tokens[4];
    henka_hks_diagnostic diagnostic;
    size_t token_count = 0U;
    henka_hks_program* program = NULL;

    memset(&diagnostic, 0, sizeof(diagnostic));
    assert(henka_hks_lex(
               "x",
               1U,
               tokens,
               4U,
               &token_count,
               &diagnostic) == HENKA_SUCCESS);
    assert(henka_hks_lex(
               "x",
               1U,
               tokens,
               0U,
               &token_count,
               &diagnostic) == HENKA_ERROR_INVALID_ARGUMENT);
    assert(henka_hks_compile("i32 x = 1;", 10U, NULL, &diagnostic) == HENKA_ERROR_INVALID_ARGUMENT);
    assert(henka_hks_program_get_binding_count(NULL) == 0U);
    assert(henka_hks_program_get_callable_count(NULL) == 0U);
    assert(henka_hks_program_get_ast_node_count(NULL) == 0U);
    henka_hks_program_destroy(program);
}

int main(void)
{
    const size_t allocations_before = henka_memory_get_allocation_count();

    test_lex_and_compile_valid_program();
    test_rejects_unsafe_or_invalid_source();
    test_argument_and_memory_contracts();
    assert(henka_memory_get_allocation_count() == allocations_before);
    puts("henka_henkascript_tests: PASS");
    return 0;
}
