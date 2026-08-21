#include <henka/script_backends.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <henka/memory.h>

#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>

#define HENKA_LUA_LIFECYCLE_COUNT 4U
#define HENKA_LUA_INITIALIZATION_BUDGET 4096U

typedef struct henka_lua_allocator
{
    size_t bytes;
} henka_lua_allocator;

struct henka_lua_behavior_backend
{
    lua_State* state;
    henka_lua_allocator allocator;
    int lifecycle_callable[HENKA_LUA_LIFECYCLE_COUNT];
    uint32_t instruction_budget;
    uint32_t instructions_used;
    bool budget_exhausted;
    bool dispatching;
};

static size_t henka_lua_event_index(henka_script_lifecycle_event event)
{
    return (size_t)event;
}

static void* henka_lua_allocate(
    void* user_data,
    void* pointer,
    size_t old_size,
    size_t new_size)
{
    henka_lua_allocator* allocator = (henka_lua_allocator*)user_data;
    size_t current_bytes;
    void* replacement;
    if (allocator == NULL || (pointer != NULL && old_size > allocator->bytes))
    {
        return NULL;
    }
    current_bytes = pointer == NULL ? allocator->bytes : allocator->bytes - old_size;
    if (new_size == 0U)
    {
        henka_free(pointer);
        allocator->bytes = current_bytes;
        return NULL;
    }
    if (new_size > (size_t)HENKA_LUA_MAX_MEMORY_BYTES - current_bytes)
    {
        return NULL;
    }
    replacement = pointer == NULL
        ? henka_malloc(new_size)
        : henka_realloc(pointer, new_size);
    if (replacement == NULL)
    {
        return NULL;
    }
    allocator->bytes = current_bytes + new_size;
    return replacement;
}

static void henka_lua_set_diagnostic(
    henka_lua_diagnostic* diagnostic,
    henka_lua_diagnostic_code code,
    lua_State* state)
{
    const char* message = NULL;
    if (diagnostic == NULL)
    {
        return;
    }
    (void)memset(diagnostic, 0, sizeof(*diagnostic));
    diagnostic->code = code;
    if (state != NULL && lua_gettop(state) > 0 && lua_isstring(state, -1))
    {
        message = lua_tostring(state, -1);
    }
    if (message != NULL)
    {
        (void)snprintf(
            diagnostic->message,
            sizeof(diagnostic->message),
            "%s",
            message);
    }
}

static void henka_lua_disable_global(lua_State* state, const char* name)
{
    lua_pushnil(state);
    lua_setglobal(state, name);
}

static void henka_lua_open_safe_libraries(lua_State* state)
{
    luaL_requiref(state, "_G", luaopen_base, 1);
    lua_pop(state, 1);
    luaL_requiref(state, LUA_TABLIBNAME, luaopen_table, 1);
    lua_pop(state, 1);
    luaL_requiref(state, LUA_STRLIBNAME, luaopen_string, 1);
    lua_pop(state, 1);
    luaL_requiref(state, LUA_MATHLIBNAME, luaopen_math, 1);
    lua_pop(state, 1);
    luaL_requiref(state, LUA_UTF8LIBNAME, luaopen_utf8, 1);
    lua_pop(state, 1);
    henka_lua_disable_global(state, "collectgarbage");
    henka_lua_disable_global(state, "dofile");
    henka_lua_disable_global(state, "load");
    henka_lua_disable_global(state, "loadfile");
    henka_lua_disable_global(state, "print");
    henka_lua_disable_global(state, "warn");
}

static void henka_lua_budget_hook(lua_State* state, lua_Debug* debug)
{
    henka_lua_behavior_backend* backend;
    (void)debug;
    backend = *(henka_lua_behavior_backend**)lua_getextraspace(state);
    if (backend == NULL)
    {
        return;
    }
    if (backend->instructions_used >= backend->instruction_budget)
    {
        backend->budget_exhausted = true;
        (void)luaL_error(state, "Henka Lua instruction budget exhausted");
        return;
    }
    ++backend->instructions_used;
}

static henka_result henka_lua_status_to_result(int status)
{
    return status == LUA_ERRMEM ? HENKA_ERROR_OUT_OF_MEMORY : HENKA_ERROR_ASSET_SOURCE;
}

static void henka_lua_clear_stack(lua_State* state)
{
    if (state != NULL)
    {
        lua_settop(state, 0);
    }
}

henka_result henka_lua_behavior_backend_create(
    const char* source,
    size_t source_size,
    henka_lua_behavior_backend** out_backend,
    henka_lua_diagnostic* out_diagnostic)
{
    static const char* const lifecycle_names[HENKA_LUA_LIFECYCLE_COUNT] =
    {
        "OnCreate",
        "OnStart",
        "OnUpdate",
        "OnStop"
    };
    henka_lua_behavior_backend* backend;
    size_t index;
    int status;
    bool budget_exhausted;
    if (out_backend == NULL ||
        (source == NULL && source_size != 0U) ||
        source_size > HENKA_LUA_MAX_SOURCE_BYTES)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_backend = NULL;
    backend = (henka_lua_behavior_backend*)henka_calloc(1U, sizeof(*backend));
    if (backend == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    for (index = 0U; index < HENKA_LUA_LIFECYCLE_COUNT; ++index)
    {
        backend->lifecycle_callable[index] = LUA_NOREF;
    }
    backend->state = lua_newstate(henka_lua_allocate, &backend->allocator);
    if (backend->state == NULL)
    {
        henka_free(backend);
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    *(henka_lua_behavior_backend**)lua_getextraspace(backend->state) = backend;
    henka_lua_open_safe_libraries(backend->state);
    backend->instruction_budget = HENKA_LUA_INITIALIZATION_BUDGET;
    backend->instructions_used = 0U;
    backend->budget_exhausted = false;
    lua_sethook(backend->state, henka_lua_budget_hook, LUA_MASKCOUNT, 1);
    status = luaL_loadbufferx(
        backend->state,
        source == NULL ? "" : source,
        source_size,
        "@henka-lua-behavior",
        "t");
    if (status == LUA_OK)
    {
        status = lua_pcall(backend->state, 0, 0, 0);
    }
    lua_sethook(backend->state, NULL, 0, 0);
    if (status != LUA_OK)
    {
        if (backend->budget_exhausted)
        {
            henka_lua_set_diagnostic(
                out_diagnostic,
                HENKA_LUA_DIAGNOSTIC_LIMIT,
                backend->state);
        }
        else
        {
            henka_lua_set_diagnostic(
                out_diagnostic,
                status == LUA_ERRMEM
                    ? HENKA_LUA_DIAGNOSTIC_MEMORY
                    : (status == LUA_ERRSYNTAX
                        ? HENKA_LUA_DIAGNOSTIC_COMPILE
                        : HENKA_LUA_DIAGNOSTIC_RUNTIME),
                backend->state);
        }
        budget_exhausted = backend->budget_exhausted;
        henka_lua_behavior_backend_destroy(backend);
        return budget_exhausted
            ? HENKA_ERROR_LIMIT
            : henka_lua_status_to_result(status);
    }
    henka_lua_clear_stack(backend->state);
    for (index = 0U; index < HENKA_LUA_LIFECYCLE_COUNT; ++index)
    {
        lua_getglobal(backend->state, lifecycle_names[index]);
        if (lua_isnil(backend->state, -1))
        {
            lua_pop(backend->state, 1);
        }
        else if (!lua_isfunction(backend->state, -1))
        {
            henka_lua_set_diagnostic(
                out_diagnostic,
                HENKA_LUA_DIAGNOSTIC_INVALID_SOURCE,
                backend->state);
            henka_lua_behavior_backend_destroy(backend);
            return HENKA_ERROR_ASSET_SOURCE;
        }
        else
        {
            backend->lifecycle_callable[index] =
                luaL_ref(backend->state, LUA_REGISTRYINDEX);
        }
    }
    *out_backend = backend;
    return HENKA_SUCCESS;
}

void henka_lua_behavior_backend_destroy(henka_lua_behavior_backend* backend)
{
    if (backend != NULL)
    {
        if (backend->state != NULL)
        {
            lua_close(backend->state);
        }
        henka_free(backend);
    }
}

henka_script_behavior_callback_result henka_lua_behavior_backend_callback(
    const henka_script_behavior_context* context,
    void* user_data,
    uint32_t* out_instructions_used)
{
    henka_lua_behavior_backend* backend =
        (henka_lua_behavior_backend*)user_data;
    int callable;
    int status;
    if (out_instructions_used != NULL)
    {
        *out_instructions_used = 0U;
    }
    if (context == NULL || backend == NULL || backend->state == NULL ||
        out_instructions_used == NULL ||
        context->language != HENKA_SCRIPT_LANGUAGE_LUA ||
        context->event > HENKA_SCRIPT_LIFECYCLE_STOP ||
        backend->dispatching)
    {
        return HENKA_SCRIPT_CALLBACK_FAILED;
    }
    callable = backend->lifecycle_callable[henka_lua_event_index(context->event)];
    if (callable == LUA_NOREF)
    {
        return HENKA_SCRIPT_CALLBACK_COMPLETED;
    }
    backend->dispatching = true;
    backend->instruction_budget = context->instruction_budget;
    backend->instructions_used = 0U;
    backend->budget_exhausted = false;
    lua_sethook(backend->state, henka_lua_budget_hook, LUA_MASKCOUNT, 1);
    lua_rawgeti(backend->state, LUA_REGISTRYINDEX, callable);
    status = lua_pcall(backend->state, 0, 0, 0);
    lua_sethook(backend->state, NULL, 0, 0);
    backend->dispatching = false;
    *out_instructions_used = backend->instructions_used;
    henka_lua_clear_stack(backend->state);
    if (backend->budget_exhausted)
    {
        return HENKA_SCRIPT_CALLBACK_BUDGET_EXHAUSTED;
    }
    return status == LUA_OK
        ? HENKA_SCRIPT_CALLBACK_COMPLETED
        : HENKA_SCRIPT_CALLBACK_FAILED;
}
