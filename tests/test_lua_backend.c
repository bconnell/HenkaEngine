#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <henka/memory.h>
#include <henka/script.h>
#include <henka/script_backends.h>

typedef struct lua_host_fixture
{
    henka_script_host* host;
    size_t calls;
} lua_host_fixture;

static henka_result lua_host_dispatch(
    void* user_data,
    uint32_t api_id,
    const henka_script_api_value* arguments,
    size_t argument_count,
    henka_script_api_value* out_value)
{
    lua_host_fixture* fixture = (lua_host_fixture*)user_data;
    if (fixture == NULL || arguments == NULL || out_value == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    ++fixture->calls;
    switch (api_id)
    {
        case HENKA_SCRIPT_API_ENTITY_IS_VALID:
            out_value->type = HENKA_SCRIPT_API_VALUE_BOOL;
            out_value->as.boolean = arguments[0].as.entity == 9U;
            return HENKA_SUCCESS;
        case HENKA_SCRIPT_API_TRANSFORM_GET_POSITION:
            out_value->type = HENKA_SCRIPT_API_VALUE_VEC3;
            out_value->as.vec3 = (henka_vec3){1.0f, 2.0f, 3.0f};
            return HENKA_SUCCESS;
        case HENKA_SCRIPT_API_TRANSFORM_SET_POSITION:
        case HENKA_SCRIPT_API_PHYSICS_APPLY_IMPULSE:
        case HENKA_SCRIPT_API_INTERACTION_TRY:
            out_value->type = HENKA_SCRIPT_API_VALUE_RESULT;
            out_value->as.result = HENKA_SUCCESS;
            return HENKA_SUCCESS;
        case HENKA_SCRIPT_API_INPUT_IS_ACTION_DOWN:
            out_value->type = HENKA_SCRIPT_API_VALUE_BOOL;
            out_value->as.boolean = false;
            return HENKA_SUCCESS;
        case HENKA_SCRIPT_API_EVENTS_EMIT:
            if (argument_count != 2U ||
                henka_script_host_emit_event(
                    fixture->host,
                    arguments[0].as.event_id,
                    arguments[1].as.entity,
                    3U) != HENKA_SUCCESS)
            {
                return HENKA_ERROR_LIMIT;
            }
            out_value->type = HENKA_SCRIPT_API_VALUE_RESULT;
            out_value->as.result = HENKA_SUCCESS;
            return HENKA_SUCCESS;
        default:
            return HENKA_ERROR_INVALID_ARGUMENT;
    }
}

static void bind_all_script_apis(henka_script_host* host)
{
    const henka_script_api_function* functions = NULL;
    size_t count = 0U;
    size_t index;
    assert(henka_script_api_schema_get(&functions, &count) == HENKA_SUCCESS);
    for (index = 0U; index < count; ++index)
    {
        assert(henka_script_host_bind_api(
                   host, functions[index].id, &(size_t){0U}) == HENKA_SUCCESS);
    }
}

static void test_lua_lifecycle_adapter(void)
{
    static const char source[] =
        "function OnCreate() local value = 2; value = value + 3 end\n"
        "function OnUpdate() local value = 1; value = value * 4 end\n"
        "function OnFixedUpdate() local value = 6; value = value + 1 end\n"
        "function OnDestroy() local value = 8; value = value + 1 end\n";
    henka_lua_behavior_backend* backend = NULL;
    henka_lua_diagnostic diagnostic;
    henka_script_behavior_runtime* runtime = NULL;
    henka_script_behavior_desc desc;
    henka_script_behavior_handle behavior = HENKA_INVALID_SCRIPT_BEHAVIOR_HANDLE;
    henka_script_behavior_report report;

    memset(&diagnostic, 0, sizeof(diagnostic));
    assert(henka_lua_behavior_backend_create(
               source, strlen(source), &backend, &diagnostic) == HENKA_SUCCESS);
    assert(backend != NULL);
    assert(henka_script_behavior_runtime_create(&runtime) == HENKA_SUCCESS);
    desc = (henka_script_behavior_desc){
        9U,
        HENKA_SCRIPT_LANGUAGE_LUA,
        true,
        128U,
        henka_lua_behavior_backend_callback,
        backend};
    assert(henka_script_behavior_runtime_add(runtime, &desc, &behavior) == HENKA_SUCCESS);
    assert(henka_script_behavior_runtime_dispatch(
               runtime, behavior, HENKA_SCRIPT_LIFECYCLE_CREATE, 0.0f, 1U, &report) == HENKA_SUCCESS);
    assert(report.result == HENKA_SCRIPT_BEHAVIOR_EXECUTED);
    assert(report.instructions_used > 0U);
    assert(henka_script_behavior_runtime_dispatch(
               runtime, behavior, HENKA_SCRIPT_LIFECYCLE_START, 0.0f, 2U, &report) == HENKA_SUCCESS);
    assert(henka_script_behavior_runtime_dispatch(
               runtime, behavior, HENKA_SCRIPT_LIFECYCLE_UPDATE, 0.016f, 3U, &report) == HENKA_SUCCESS);
    assert(report.instructions_used > 0U);
    assert(henka_script_behavior_runtime_dispatch(
               runtime, behavior, HENKA_SCRIPT_LIFECYCLE_FIXED_UPDATE, 0.016f, 4U, &report) == HENKA_SUCCESS);
    assert(report.instructions_used > 0U);
    assert(henka_script_behavior_runtime_dispatch(
               runtime, behavior, HENKA_SCRIPT_LIFECYCLE_DESTROY, 0.0f, 5U, &report) == HENKA_SUCCESS);
    assert(henka_script_behavior_runtime_dispatch(
               runtime, behavior, HENKA_SCRIPT_LIFECYCLE_STOP, 0.0f, 6U, &report) == HENKA_SUCCESS);
    henka_script_behavior_runtime_destroy(runtime);
    henka_lua_behavior_backend_destroy(backend);
}

static void test_lua_budget_and_sandbox(void)
{
    static const char source[] =
        "function OnUpdate() "
        "local total = 0; "
        "for i = 1, 1000 do total = total + i end "
        "end";
    henka_lua_behavior_backend* backend = NULL;
    henka_lua_diagnostic diagnostic;
    henka_script_behavior_context context;
    uint32_t used = 0U;

    memset(&diagnostic, 0, sizeof(diagnostic));
    assert(henka_lua_behavior_backend_create(
               source, strlen(source), &backend, &diagnostic) == HENKA_SUCCESS);
    context = (henka_script_behavior_context){
        1U,
        2U,
        HENKA_SCRIPT_LANGUAGE_LUA,
        HENKA_SCRIPT_LIFECYCLE_CREATE,
        0.0f,
        1U,
        64U};
    assert(henka_lua_behavior_backend_callback(
               &context, backend, &used) == HENKA_SCRIPT_CALLBACK_COMPLETED);
    assert(used == 0U);
    context.event = HENKA_SCRIPT_LIFECYCLE_UPDATE;
    context.instruction_budget = 1U;
    assert(henka_lua_behavior_backend_callback(
               &context, backend, &used) == HENKA_SCRIPT_CALLBACK_BUDGET_EXHAUSTED);
    assert(used == 1U);
    context.instruction_budget = 64U;
    context.language = HENKA_SCRIPT_LANGUAGE_HENKASCRIPT;
    assert(henka_lua_behavior_backend_callback(
               &context, backend, &used) == HENKA_SCRIPT_CALLBACK_FAILED);
    henka_lua_behavior_backend_destroy(backend);
}

static void test_lua_rejection_and_memory(void)
{
    henka_lua_behavior_backend* backend = NULL;
    henka_lua_diagnostic diagnostic;

    memset(&diagnostic, 0, sizeof(diagnostic));
    assert(henka_lua_behavior_backend_create(
               "function OnCreate( ", strlen("function OnCreate( "),
               &backend, &diagnostic) != HENKA_SUCCESS);
    assert(backend == NULL);
    assert(diagnostic.code == HENKA_LUA_DIAGNOSTIC_COMPILE);
    assert(henka_lua_behavior_backend_create(
               NULL, HENKA_LUA_MAX_SOURCE_BYTES + 1U,
               &backend, &diagnostic) == HENKA_ERROR_INVALID_ARGUMENT);
}

static void test_lua_shared_host_api(void)
{
    static const char source[] =
        "function OnUpdate() "
        "if Entity.IsValid(9) then "
        "local position = Transform.GetPosition(9); "
        "Transform.SetPosition(9, {x = position.x + 1, y = position.y, z = position.z}); "
        "Physics.ApplyImpulse(9, {x = 0, y = -1, z = 0}); "
        "Input.IsActionDown(1); "
        "Interaction.Try(9); "
        "Events.Emit(7, 9) "
        "end end";
    henka_lua_behavior_backend* backend = NULL;
    henka_lua_diagnostic diagnostic;
    henka_script_host* host = NULL;
    lua_host_fixture fixture;
    henka_script_behavior_context context;
    henka_script_event event;
    uint32_t used = 0U;

    memset(&diagnostic, 0, sizeof(diagnostic));
    assert(henka_lua_behavior_backend_create(
               source, strlen(source), &backend, &diagnostic) == HENKA_SUCCESS);
    assert(henka_script_host_create(&host) == HENKA_SUCCESS);
    bind_all_script_apis(host);
    fixture = (lua_host_fixture){host, 0U};
    assert(henka_script_host_set_dispatcher(host, lua_host_dispatch, &fixture) == HENKA_SUCCESS);
    context = (henka_script_behavior_context){
        1U, 9U, HENKA_SCRIPT_LANGUAGE_LUA, HENKA_SCRIPT_LIFECYCLE_UPDATE,
        0.016f, 3U, 128U, host};
    assert(henka_lua_behavior_backend_callback(
               &context, backend, &used) == HENKA_SCRIPT_CALLBACK_COMPLETED);
    assert(used > 0U && fixture.calls == 7U);
    assert(henka_script_host_poll_event(host, &event) == HENKA_SUCCESS);
    assert(event.event_id == 7U && event.source_entity == 9U && event.frame_index == 3U);
    henka_script_host_destroy(host);
    henka_lua_behavior_backend_destroy(backend);
}

static void test_lua_shared_state_api(void)
{
    static const char source[] =
        "function OnUpdate() "
        "local value, present = State.GetI32(17); "
        "State.SetI32(17, value + 5); "
        "local flag, flag_present = State.GetBool(18); "
        "State.SetBool(18, not flag) "
        "end "
        "function OnCollisionEnter(other_entity, event_type) "
        "if other_entity == 99 and event_type == 7 then State.SetI32(17, event_type) end "
        "end";
    henka_lua_behavior_backend* backend = NULL;
    henka_lua_diagnostic diagnostic;
    henka_script_host* host = NULL;
    henka_script_state_store* store = NULL;
    henka_script_behavior_context context;
    henka_script_state_value value;
    bool present;
    uint32_t used = 0U;

    memset(&diagnostic, 0, sizeof(diagnostic));
    assert(henka_lua_behavior_backend_create(
               source, strlen(source), &backend, &diagnostic) == HENKA_SUCCESS);
    assert(henka_script_host_create(&host) == HENKA_SUCCESS);
    assert(henka_script_state_store_create(&store) == HENKA_SUCCESS);
    bind_all_script_apis(host);
    assert(henka_script_state_store_set(
               store,
               (henka_script_state_identity){4U, 40U},
               17U,
               (henka_script_state_value){
                   HENKA_SCRIPT_STATE_VALUE_I32, {.i32 = 10}}) == HENKA_SUCCESS);
    assert(henka_script_host_set_state_store(host, store) == HENKA_SUCCESS);
    assert(henka_script_host_set_execution_context(
               host, (henka_script_state_identity){4U, 40U}) == HENKA_SUCCESS);
    context = (henka_script_behavior_context){
        1U, 4U, HENKA_SCRIPT_LANGUAGE_LUA, HENKA_SCRIPT_LIFECYCLE_UPDATE,
        0.016f, 3U, 128U, host, 40U};
    assert(henka_lua_behavior_backend_callback(
               &context, backend, &used) == HENKA_SCRIPT_CALLBACK_COMPLETED);
    assert(used > 0U);
    assert(henka_script_state_store_get(
               store, (henka_script_state_identity){4U, 40U}, 17U,
               &value, &present) == HENKA_SUCCESS);
    assert(present && value.type == HENKA_SCRIPT_STATE_VALUE_I32 && value.as.i32 == 15);
    assert(henka_script_state_store_get(
               store, (henka_script_state_identity){4U, 40U}, 18U,
               &value, &present) == HENKA_SUCCESS);
    assert(present && value.type == HENKA_SCRIPT_STATE_VALUE_BOOL && value.as.boolean);
    context.event = HENKA_SCRIPT_LIFECYCLE_COLLISION_ENTER;
    context.event_other_entity = 99U;
    context.event_type = 7U;
    assert(henka_lua_behavior_backend_callback(
               &context, backend, &used) == HENKA_SCRIPT_CALLBACK_COMPLETED);
    assert(henka_script_state_store_get(
               store, (henka_script_state_identity){4U, 40U}, 17U,
               &value, &present) == HENKA_SUCCESS);
    assert(present && value.type == HENKA_SCRIPT_STATE_VALUE_I32 && value.as.i32 == 7);
    henka_script_state_store_destroy(store);
    henka_script_host_destroy(host);
    henka_lua_behavior_backend_destroy(backend);
}

static const henka_lua_token* find_lua_token(
    const char* source,
    const henka_lua_token* tokens,
    size_t token_count,
    const char* text)
{
    size_t index;
    const size_t text_size = strlen(text);
    for (index = 0U; index < token_count; ++index)
    {
        if (tokens[index].length == text_size &&
            memcmp(source + tokens[index].offset, text, text_size) == 0)
        {
            return &tokens[index];
        }
    }
    return NULL;
}

static uint32_t lua_indent_at(
    const char* source,
    const henka_lua_token* tokens,
    size_t token_count,
    const char* marker,
    uint32_t line)
{
    uint32_t indent = UINT32_MAX;
    const char* location = strstr(source, marker);
    assert(location != NULL);
    const size_t offset = (size_t)(location - source);
    assert(henka_lua_token_stream_get_indent_level(
               source,
               strlen(source),
               tokens,
               token_count,
               offset,
               line,
               &indent) == HENKA_SUCCESS);
    return indent;
}

static void test_lua_editor_lexer_and_indent_authority(void)
{
    static const char source[] =
        "function OnUpdate()\n"
        "if true then\n"
        "State.SetI32(1, 2)\n"
        "else\n"
        "State.SetI32(1, 3)\n"
        "end\n"
        "end\n";
    henka_lua_token tokens[HENKA_LUA_MAX_TOKENS];
    henka_lua_diagnostic diagnostic;
    size_t token_count = 0U;
    const henka_lua_token* token;

    memset(&diagnostic, 0, sizeof(diagnostic));
    assert(henka_lua_lex(
               source,
               strlen(source),
               tokens,
               HENKA_LUA_MAX_TOKENS,
               &token_count,
               &diagnostic) == HENKA_SUCCESS);
    assert(token_count > 0U);
    token = find_lua_token(source, tokens, token_count, "function");
    assert(token != NULL && token->token_class == HENKA_LUA_TOKEN_CLASS_KEYWORD);
    token = find_lua_token(source, tokens, token_count, "State");
    assert(token != NULL && token->token_class == HENKA_LUA_TOKEN_CLASS_BUILTIN);
    token = find_lua_token(source, tokens, token_count, "true");
    assert(token != NULL && token->token_class == HENKA_LUA_TOKEN_CLASS_LITERAL);
    assert(lua_indent_at(source, tokens, token_count, "if true", 2U) == 1U);
    assert(lua_indent_at(source, tokens, token_count, "State.SetI32(1, 2)", 3U) == 2U);
    assert(lua_indent_at(source, tokens, token_count, "else", 4U) == 1U);
    assert(lua_indent_at(source, tokens, token_count, "State.SetI32(1, 3)", 5U) == 2U);
    assert(lua_indent_at(source, tokens, token_count, "end\nend", 6U) == 1U);
    {
        uint32_t final_indent = UINT32_MAX;
        assert(henka_lua_token_stream_get_indent_level(
                   source,
                   strlen(source),
                   tokens,
                   token_count,
                   strlen(source),
                   8U,
                   &final_indent) == HENKA_SUCCESS);
        assert(final_indent == 0U);
    }

    assert(henka_lua_lex(
               "--[[ unfinished",
               strlen("--[[ unfinished"),
               tokens,
               HENKA_LUA_MAX_TOKENS,
               &token_count,
               &diagnostic) == HENKA_ERROR_ASSET_SOURCE);
    assert(diagnostic.code == HENKA_LUA_DIAGNOSTIC_COMPILE);
    assert(diagnostic.line == 1U && diagnostic.column == 1U);
    assert(henka_lua_lex(
               source,
               HENKA_LUA_MAX_SOURCE_BYTES + 1U,
               tokens,
               HENKA_LUA_MAX_TOKENS,
               &token_count,
               &diagnostic) == HENKA_ERROR_INVALID_ARGUMENT);
}

int main(void)
{
    const size_t allocations_before = henka_memory_get_allocation_count();
    test_lua_lifecycle_adapter();
    test_lua_budget_and_sandbox();
    test_lua_rejection_and_memory();
    test_lua_shared_host_api();
    test_lua_shared_state_api();
    test_lua_editor_lexer_and_indent_authority();
    assert(henka_memory_get_allocation_count() == allocations_before);
    puts("henka_lua_backend_tests: PASS");
    return 0;
}
