#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <henka/henkascript.h>
#include <henka/memory.h>
#include <henka/script.h>

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
    expect_diagnostic("i32 x = 2147483648;", HENKA_HKS_DIAGNOSTIC_INVALID_LITERAL);
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

static henka_hks_program* compile_program(const char* source)
{
    henka_hks_program* program = NULL;
    henka_hks_diagnostic diagnostic;

    memset(&diagnostic, 0, sizeof(diagnostic));
    assert(henka_hks_compile(source, strlen(source), &program, &diagnostic) == HENKA_SUCCESS);
    assert(program != NULL);
    return program;
}

static void test_bounded_vm_execution(void)
{
    henka_hks_value value;
    henka_hks_execution_report report;
    henka_hks_program* program;
    henka_hks_callable_info callable;

    program = compile_program("fn Compute() { i32 x = 3; x = x * 4 + 2; return x; }");
    assert(henka_hks_program_get_callable_count(program) == 1U);
    memset(&callable, 0, sizeof(callable));
    assert(henka_hks_program_get_callable(program, 0U, &callable) == HENKA_SUCCESS);
    assert(callable.bytecode_length > 0U);
    assert(callable.local_count == 1U);
    memset(&value, 0, sizeof(value));
    memset(&report, 0, sizeof(report));
    assert(henka_hks_execute(program, 0U, 100U, &value, &report) == HENKA_HKS_EXECUTION_COMPLETED);
    assert(value.type == HENKA_HKS_TYPE_I32);
    assert(value.as.i32 == 14);
    assert(report.instructions_executed > 0U);
    henka_hks_program_destroy(program);

    program = compile_program("fn Toggle() { bool enabled = true; return enabled; }");
    assert(henka_hks_execute(program, 0U, 100U, &value, &report) == HENKA_HKS_EXECUTION_COMPLETED);
    assert(value.type == HENKA_HKS_TYPE_BOOL && value.as.boolean);
    henka_hks_program_destroy(program);

    program = compile_program("fn Divide() { f32 value = 1.5; value = value / 0.0; return value; }");
    assert(henka_hks_execute(program, 0U, 100U, &value, &report) == HENKA_HKS_EXECUTION_DIVIDE_BY_ZERO);
    assert(report.instructions_executed > 0U);
    henka_hks_program_destroy(program);

    program = compile_program("fn Overflow() { i32 value = 2147483647; value = value + 1; return value; }");
    assert(henka_hks_execute(program, 0U, 100U, &value, &report) == HENKA_HKS_EXECUTION_TYPE_ERROR);
    henka_hks_program_destroy(program);

    program = compile_program("fn Unsupported() { return \"text\"; }");
    assert(henka_hks_execute(program, 0U, 100U, &value, &report) == HENKA_HKS_EXECUTION_UNSUPPORTED_VALUE);
    henka_hks_program_destroy(program);

    program = compile_program("fn Budget() { i32 value = 1; value = value + 1; return value; }");
    assert(henka_hks_execute(program, 0U, 1U, &value, &report) == HENKA_HKS_EXECUTION_BUDGET_EXHAUSTED);
    assert(report.instructions_executed == 1U);
    assert(henka_hks_execute(program, 1U, 100U, &value, &report) == HENKA_HKS_EXECUTION_INVALID_PROGRAM);
    assert(henka_hks_execute(program, 0U, 100U, NULL, &report) == HENKA_HKS_EXECUTION_COMPLETED);
    henka_hks_program_destroy(program);
}

static void test_bounded_conditionals_and_comparisons(void)
{
    henka_hks_program* program;
    henka_hks_value value;
    henka_hks_execution_report report;

    program = compile_program(
        "fn Choose() { "
        "i32 value = 3; "
        "if (value < 4) { value = value + 6; } "
        "else { value = 0; } "
        "return value; "
        "}");
    assert(henka_hks_execute(program, 0U, 128U, &value, &report) == HENKA_HKS_EXECUTION_COMPLETED);
    assert(value.type == HENKA_HKS_TYPE_I32 && value.as.i32 == 9);
    henka_hks_program_destroy(program);

    program = compile_program(
        "fn Equality() { "
        "bool enabled = true; "
        "if (enabled == false) { return 1; } "
        "return 2; "
        "}");
    assert(henka_hks_execute(program, 0U, 128U, &value, &report) == HENKA_HKS_EXECUTION_COMPLETED);
    assert(value.type == HENKA_HKS_TYPE_I32 && value.as.i32 == 2);
    henka_hks_program_destroy(program);
}

static void test_bounded_while_and_loop_control(void)
{
    henka_hks_program* program = compile_program(
        "fn Loop() { "
        "i32 index = 0; "
        "i32 total = 0; "
        "while (index < 5) { "
        "index = index + 1; "
        "if (index == 3) { continue; } "
        "if (index == 5) { break; } "
        "total = total + index; "
        "} "
        "return total; "
        "}");
    henka_hks_value value;
    henka_hks_execution_report report;

    assert(henka_hks_execute(program, 0U, 256U, &value, &report) == HENKA_HKS_EXECUTION_COMPLETED);
    assert(value.type == HENKA_HKS_TYPE_I32 && value.as.i32 == 7);
    henka_hks_program_destroy(program);
}

static void test_bounded_for_and_loop_control(void)
{
    henka_hks_program* program = compile_program(
        "fn Count() { "
        "i32 total = 0; "
        "for (i32 i = 0; i < 5; i = i + 1) { "
        "if (i == 2) { continue; } "
        "total = total + i; "
        "if (i == 4) { break; } "
        "} "
        "return total; "
        "}");
    henka_hks_value value;
    henka_hks_execution_report report;

    assert(henka_hks_execute(
               program, 0U, 256U, &value, &report) == HENKA_HKS_EXECUTION_COMPLETED);
    assert(value.type == HENKA_HKS_TYPE_I32 && value.as.i32 == 8);
    henka_hks_program_destroy(program);
}

static void test_signal_context_access(void)
{
    henka_hks_program* program = compile_program(
        "fn Contact() { "
        "entity other = event_other_entity(); "
        "entity same = event_other_entity(); "
        "i32 kind = event_type(); "
        "if (other == same) { if (kind == 7) { return kind; } } "
        "return 0; "
        "}");
    henka_hks_execution_context context = {
        NULL, 42U, 3U, 11U, false, 0U, 99U, 99U, 7U, true};
    henka_hks_value value;
    henka_hks_execution_report report;

    assert(henka_hks_execute_with_context(
               program, 0U, 128U, &context, &value, &report) == HENKA_HKS_EXECUTION_COMPLETED);
    assert(value.type == HENKA_HKS_TYPE_I32 && value.as.i32 == 7);
    context.is_signal = false;
    assert(henka_hks_execute_with_context(
               program, 0U, 128U, &context, &value, &report) == HENKA_HKS_EXECUTION_HOST_ERROR);
    henka_hks_program_destroy(program);
}

static void test_state_host_contract(void)
{
    static const char source[] =
        "fn Update() { "
        "i32 current = state_get_i32(17); "
        "state_set_i32(17, current + 2); "
        "bool enabled = state_get_bool(18); "
        "state_set_bool(18, true); "
        "}";
    henka_hks_program* program = compile_program(source);
    henka_script_host* host = NULL;
    henka_script_state_store* store = NULL;
    henka_hks_execution_context context;
    henka_hks_execution_report report;
    henka_hks_value return_value;
    henka_script_state_value state_value;
    bool present;

    assert(henka_script_host_create(&host) == HENKA_SUCCESS);
    assert(henka_script_state_store_create(&store) == HENKA_SUCCESS);
    assert(henka_script_host_bind_api(
               host, HENKA_SCRIPT_API_STATE_GET_I32, &(size_t){0U}) == HENKA_SUCCESS);
    assert(henka_script_host_bind_api(
               host, HENKA_SCRIPT_API_STATE_SET_I32, &(size_t){0U}) == HENKA_SUCCESS);
    assert(henka_script_host_bind_api(
               host, HENKA_SCRIPT_API_STATE_GET_BOOL, &(size_t){0U}) == HENKA_SUCCESS);
    assert(henka_script_host_bind_api(
               host, HENKA_SCRIPT_API_STATE_SET_BOOL, &(size_t){0U}) == HENKA_SUCCESS);
    assert(henka_script_state_store_set(
               store,
               (henka_script_state_identity){4U, 40U},
               17U,
               (henka_script_state_value){
                   HENKA_SCRIPT_STATE_VALUE_I32, {.i32 = 5}}) == HENKA_SUCCESS);
    assert(henka_script_host_set_state_store(host, store) == HENKA_SUCCESS);
    assert(henka_script_host_set_execution_context(
               host, (henka_script_state_identity){4U, 40U}) == HENKA_SUCCESS);
    context = (henka_hks_execution_context){host, 4U, 3U, 40U};
    assert(henka_hks_execute_with_context(
               program, 0U, 128U, &context, &return_value, &report) ==
           HENKA_HKS_EXECUTION_COMPLETED);
    assert(henka_script_state_store_get(
               store, (henka_script_state_identity){4U, 40U}, 17U,
               &state_value, &present) == HENKA_SUCCESS);
    assert(present && state_value.type == HENKA_SCRIPT_STATE_VALUE_I32 &&
           state_value.as.i32 == 7);
    assert(henka_script_state_store_get(
               store, (henka_script_state_identity){4U, 40U}, 18U,
               &state_value, &present) == HENKA_SUCCESS);
    assert(present && state_value.type == HENKA_SCRIPT_STATE_VALUE_BOOL &&
           state_value.as.boolean);
    henka_script_state_store_destroy(store);
    henka_script_host_destroy(host);
    henka_hks_program_destroy(program);
}

int main(void)
{
    const size_t allocations_before = henka_memory_get_allocation_count();

    test_lex_and_compile_valid_program();
    test_rejects_unsafe_or_invalid_source();
    test_argument_and_memory_contracts();
    test_bounded_vm_execution();
    test_bounded_conditionals_and_comparisons();
    test_bounded_while_and_loop_control();
    test_bounded_for_and_loop_control();
    test_signal_context_access();
    test_state_host_contract();
    assert(henka_memory_get_allocation_count() == allocations_before);
    puts("henka_henkascript_tests: PASS");
    return 0;
}
