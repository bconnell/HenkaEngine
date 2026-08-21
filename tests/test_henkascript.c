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

static void test_compiler_owned_token_presentation_classes(void)
{
    assert(henka_hks_token_kind_get_class(HENKA_HKS_TOKEN_IDENTIFIER) ==
        HENKA_HKS_TOKEN_CLASS_IDENTIFIER);
    assert(henka_hks_token_kind_get_class(HENKA_HKS_TOKEN_INTEGER) ==
        HENKA_HKS_TOKEN_CLASS_LITERAL);
    assert(henka_hks_token_kind_get_class(HENKA_HKS_TOKEN_STRING) ==
        HENKA_HKS_TOKEN_CLASS_LITERAL);
    assert(henka_hks_token_kind_get_class(HENKA_HKS_TOKEN_KW_I32) ==
        HENKA_HKS_TOKEN_CLASS_TYPE);
    assert(henka_hks_token_kind_get_class(HENKA_HKS_TOKEN_KW_FN) ==
        HENKA_HKS_TOKEN_CLASS_KEYWORD);
    assert(henka_hks_token_kind_get_class(HENKA_HKS_TOKEN_KW_EMIT) ==
        HENKA_HKS_TOKEN_CLASS_BUILTIN);
    assert(henka_hks_token_kind_get_class(HENKA_HKS_TOKEN_LBRACE) ==
        HENKA_HKS_TOKEN_CLASS_PUNCTUATION);
    assert(henka_hks_token_kind_get_class(HENKA_HKS_TOKEN_EOF) ==
        HENKA_HKS_TOKEN_CLASS_NONE);
}

static void test_compiler_owned_token_indentation(void)
{
    static const char source[] =
        "fn Update() {\n"
        "    if (true) {\n"
        "        return;\n"
        "    }\n"
        "}\n";
    henka_hks_token tokens[HENKA_HKS_MAX_TOKENS];
    henka_hks_diagnostic diagnostic;
    size_t token_count = 0U;
    size_t line_two_offset;
    size_t line_three_end_offset;
    size_t line_four_offset;
    size_t line_five_offset;
    uint32_t indent_level = 0U;

    memset(&diagnostic, 0, sizeof(diagnostic));
    assert(henka_hks_lex(
               source,
               strlen(source),
               tokens,
               HENKA_HKS_MAX_TOKENS,
               &token_count,
               &diagnostic) == HENKA_SUCCESS);
    line_two_offset = (size_t)(strstr(source, "    if") - source);
    line_three_end_offset = (size_t)(strstr(source, "        return;") - source) +
        strlen("        return;");
    line_four_offset = (size_t)(strstr(source, "    }") - source);
    line_five_offset = (size_t)(strrchr(source, '}') - source);
    assert(henka_hks_token_stream_get_indent_level(
               tokens,
               token_count,
               line_two_offset,
               2U,
               &indent_level) == HENKA_SUCCESS);
    assert(indent_level == 1U);
    assert(henka_hks_token_stream_get_indent_level(
               tokens,
               token_count,
               line_three_end_offset,
               3U,
               &indent_level) == HENKA_SUCCESS);
    assert(indent_level == 2U);
    assert(henka_hks_token_stream_get_indent_level(
               tokens,
               token_count,
               line_four_offset,
               4U,
               &indent_level) == HENKA_SUCCESS);
    assert(indent_level == 1U);
    assert(henka_hks_token_stream_get_indent_level(
               tokens,
               token_count,
               line_five_offset,
               5U,
               &indent_level) == HENKA_SUCCESS);
    assert(indent_level == 0U);
    assert(henka_hks_token_stream_get_indent_level(
               NULL,
               1U,
               0U,
               1U,
               &indent_level) == HENKA_ERROR_INVALID_ARGUMENT);
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

static henka_result test_entity_is_valid_dispatch(
    void* user_data,
    uint32_t api_id,
    const henka_script_api_value* arguments,
    size_t argument_count,
    henka_script_api_value* out_value)
{
    (void)user_data;
    if (api_id != HENKA_SCRIPT_API_ENTITY_IS_VALID ||
        arguments == NULL || argument_count != 1U || out_value == NULL ||
        arguments[0].type != HENKA_SCRIPT_API_VALUE_ENTITY)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_value = (henka_script_api_value){
        HENKA_SCRIPT_API_VALUE_BOOL,
        {.boolean = arguments[0].as.entity == 99U}};
    return HENKA_SUCCESS;
}

static void test_entity_host_contract(void)
{
    henka_hks_program* program = compile_program(
        "fn Check() { "
        "entity target = event_other_entity(); "
        "if (entity_is_valid(target)) { return 1; } "
        "return 0; "
        "}");
    henka_script_host* host = NULL;
    henka_hks_execution_context context;
    henka_hks_execution_report report;
    henka_hks_value value;

    assert(henka_script_host_create(&host) == HENKA_SUCCESS);
    assert(henka_script_host_bind_api(
               host, HENKA_SCRIPT_API_ENTITY_IS_VALID, &(size_t){0U}) == HENKA_SUCCESS);
    assert(henka_script_host_set_dispatcher(
               host, test_entity_is_valid_dispatch, NULL) == HENKA_SUCCESS);
    context = (henka_hks_execution_context){
        host, 42U, 3U, 11U, false, 0U, 99U, 99U, 7U, true};
    assert(henka_hks_execute_with_context(
               program, 0U, 128U, &context, &value, &report) ==
           HENKA_HKS_EXECUTION_COMPLETED);
    assert(value.type == HENKA_HKS_TYPE_I32 && value.as.i32 == 1);
    context.event_other_entity = 100U;
    assert(henka_hks_execute_with_context(
               program, 0U, 128U, &context, &value, &report) ==
           HENKA_HKS_EXECUTION_COMPLETED);
    assert(value.type == HENKA_HKS_TYPE_I32 && value.as.i32 == 0);
    henka_script_host_destroy(host);
    henka_hks_program_destroy(program);
}

typedef struct test_transform_dispatch_state
{
    bool set_position_called;
    bool impulse_called;
    henka_vec3 last_vector;
} test_transform_dispatch_state;

static henka_result test_transform_dispatch(
    void* user_data,
    uint32_t api_id,
    const henka_script_api_value* arguments,
    size_t argument_count,
    henka_script_api_value* out_value)
{
    test_transform_dispatch_state* state =
        (test_transform_dispatch_state*)user_data;
    if (state == NULL || arguments == NULL || out_value == NULL ||
        argument_count == 0U || arguments[0].type != HENKA_SCRIPT_API_VALUE_ENTITY)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    switch (api_id)
    {
        case HENKA_SCRIPT_API_ENTITY_IS_VALID:
            out_value->type = HENKA_SCRIPT_API_VALUE_BOOL;
            out_value->as.boolean = arguments[0].as.entity == 99U;
            return HENKA_SUCCESS;
        case HENKA_SCRIPT_API_TRANSFORM_GET_POSITION:
            if (argument_count != 1U)
            {
                return HENKA_ERROR_INVALID_ARGUMENT;
            }
            out_value->type = HENKA_SCRIPT_API_VALUE_VEC3;
            out_value->as.vec3 = (henka_vec3){4.0f, 5.0f, 6.0f};
            return HENKA_SUCCESS;
        case HENKA_SCRIPT_API_TRANSFORM_SET_POSITION:
            state->set_position_called = true;
            break;
        case HENKA_SCRIPT_API_PHYSICS_APPLY_IMPULSE:
            state->impulse_called = true;
            break;
        default:
            return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (argument_count != 2U ||
        arguments[1].type != HENKA_SCRIPT_API_VALUE_VEC3)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    state->last_vector = arguments[1].as.vec3;
    *out_value = (henka_script_api_value){
        HENKA_SCRIPT_API_VALUE_RESULT,
        {.result = HENKA_SUCCESS}};
    return HENKA_SUCCESS;
}

static void test_transform_and_physics_host_contract(void)
{
    henka_hks_program* program = compile_program(
        "fn Move() { "
        "entity target = event_other_entity(); "
        "vec3 start = transform_get_position(target); "
        "vec3 impulse = vec3(1.0, 2.0, 3.0); "
        "if (entity_is_valid(target)) { "
        "transform_set_position(target, impulse); "
        "physics_apply_impulse(target, impulse); "
        "} "
        "}");
    test_transform_dispatch_state dispatch_state = {false, false, {0.0f, 0.0f, 0.0f}};
    henka_script_host* host = NULL;
    henka_hks_execution_context context;
    henka_hks_execution_report report;
    henka_hks_value value;
    const uint32_t api_ids[] = {
        HENKA_SCRIPT_API_ENTITY_IS_VALID,
        HENKA_SCRIPT_API_TRANSFORM_GET_POSITION,
        HENKA_SCRIPT_API_TRANSFORM_SET_POSITION,
        HENKA_SCRIPT_API_PHYSICS_APPLY_IMPULSE};

    assert(henka_script_host_create(&host) == HENKA_SUCCESS);
    for (size_t index = 0U; index < sizeof(api_ids) / sizeof(api_ids[0]); ++index)
    {
        assert(henka_script_host_bind_api(
                   host, api_ids[index], &(size_t){0U}) == HENKA_SUCCESS);
    }
    assert(henka_script_host_set_dispatcher(
               host, test_transform_dispatch, &dispatch_state) == HENKA_SUCCESS);
    context = (henka_hks_execution_context){
        host, 42U, 3U, 11U, false, 0U, 99U, 99U, 7U, true};
    assert(henka_hks_execute_with_context(
               program, 0U, 256U, &context, &value, &report) ==
           HENKA_HKS_EXECUTION_COMPLETED);
    assert(dispatch_state.set_position_called && dispatch_state.impulse_called);
    assert(dispatch_state.last_vector.x == 1.0f &&
           dispatch_state.last_vector.y == 2.0f &&
           dispatch_state.last_vector.z == 3.0f);
    henka_script_host_destroy(host);
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

static henka_result test_input_interaction_dispatch(
    void* user_data,
    uint32_t api_id,
    const henka_script_api_value* arguments,
    size_t argument_count,
    henka_script_api_value* out_value)
{
    bool* called = (bool*)user_data;
    if (called == NULL || arguments == NULL || out_value == NULL ||
        argument_count != 1U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *called = true;
    if (api_id == HENKA_SCRIPT_API_INPUT_IS_ACTION_DOWN)
    {
        if (arguments[0].type != HENKA_SCRIPT_API_VALUE_ACTION_ID ||
            arguments[0].as.action_id != 7U)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        *out_value = (henka_script_api_value){
            HENKA_SCRIPT_API_VALUE_BOOL,
            {.boolean = true}};
        return HENKA_SUCCESS;
    }
    if (api_id == HENKA_SCRIPT_API_INTERACTION_TRY)
    {
        if (arguments[0].type != HENKA_SCRIPT_API_VALUE_ENTITY ||
            arguments[0].as.entity != 99U)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        *out_value = (henka_script_api_value){
            HENKA_SCRIPT_API_VALUE_RESULT,
            {.result = (henka_result)3}};
        return HENKA_SUCCESS;
    }
    return HENKA_ERROR_INVALID_ARGUMENT;
}

static void test_input_and_interaction_host_contract(void)
{
    henka_hks_program* program = compile_program(
        "fn Probe() { "
        "bool down = input_is_action_down(7); "
        "entity target = event_other_entity(); "
        "i32 result = interaction_try(target); "
        "if (down) { if (result == 3) { return 1; } } "
        "return 0; "
        "}");
    henka_script_host* host = NULL;
    henka_hks_execution_context context;
    henka_hks_execution_report report;
    henka_hks_value value;
    bool called = false;
    const uint32_t api_ids[] = {
        HENKA_SCRIPT_API_INPUT_IS_ACTION_DOWN,
        HENKA_SCRIPT_API_INTERACTION_TRY};

    assert(henka_script_host_create(&host) == HENKA_SUCCESS);
    for (size_t index = 0U; index < sizeof(api_ids) / sizeof(api_ids[0]); ++index)
    {
        assert(henka_script_host_bind_api(
                   host, api_ids[index], &(size_t){0U}) == HENKA_SUCCESS);
    }
    assert(henka_script_host_set_dispatcher(
               host, test_input_interaction_dispatch, &called) == HENKA_SUCCESS);
    context = (henka_hks_execution_context){
        host, 42U, 3U, 11U, false, 0U, 99U, 99U, 7U, true};
    assert(henka_hks_execute_with_context(
               program, 0U, 256U, &context, &value, &report) ==
           HENKA_HKS_EXECUTION_COMPLETED);
    assert(called);
    assert(value.type == HENKA_HKS_TYPE_I32 && value.as.i32 == 1);
    henka_script_host_destroy(host);
    henka_hks_program_destroy(program);
    expect_diagnostic(
        "fn Probe() { bool down = input_is_action_down(true); }",
        HENKA_HKS_DIAGNOSTIC_TYPE_MISMATCH);
}

int main(void)
{
    const size_t allocations_before = henka_memory_get_allocation_count();

    test_lex_and_compile_valid_program();
    test_compiler_owned_token_presentation_classes();
    test_compiler_owned_token_indentation();
    test_rejects_unsafe_or_invalid_source();
    test_argument_and_memory_contracts();
    test_bounded_vm_execution();
    test_bounded_conditionals_and_comparisons();
    test_bounded_while_and_loop_control();
    test_bounded_for_and_loop_control();
    test_signal_context_access();
    test_entity_host_contract();
    test_transform_and_physics_host_contract();
    test_state_host_contract();
    test_input_and_interaction_host_contract();
    assert(henka_memory_get_allocation_count() == allocations_before);
    puts("henka_henkascript_tests: PASS");
    return 0;
}
