#include <henka/script_backends.h>

#include <stdbool.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <henka/memory.h>

#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>

#define HENKA_LUA_INITIALIZATION_BUDGET 4096U

typedef struct henka_lua_allocator
{
    size_t bytes;
} henka_lua_allocator;

struct henka_lua_behavior_backend
{
    lua_State* state;
    henka_lua_allocator allocator;
    int callable[HENKA_SCRIPT_LIFECYCLE_SLOT_COUNT];
    uint32_t instruction_budget;
    uint32_t instructions_used;
    bool budget_exhausted;
    bool dispatching;
    henka_script_host* active_host;
};

static bool henka_lua_event_has_arguments(henka_script_lifecycle_event event)
{
    const henka_script_lifecycle_descriptor* descriptor = NULL;
    return henka_script_lifecycle_schema_find(event, &descriptor) == HENKA_SUCCESS &&
        descriptor != NULL && descriptor->parameter_count != 0U;
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

static henka_lua_behavior_backend* henka_lua_backend_from_upvalue(
    lua_State* state)
{
    return state == NULL
        ? NULL
        : (henka_lua_behavior_backend*)lua_touserdata(
            state, lua_upvalueindex(1));
}

static bool henka_lua_get_unsigned_integer(
    lua_State* state,
    int index,
    uint64_t maximum,
    uint64_t* out_value)
{
    lua_Integer value;
    if (state == NULL || out_value == NULL || !lua_isinteger(state, index))
    {
        return false;
    }
    value = lua_tointeger(state, index);
    if (value < 0 || (uint64_t)value > maximum)
    {
        return false;
    }
    *out_value = (uint64_t)value;
    return true;
}

static bool henka_lua_get_i32(
    lua_State* state,
    int index,
    int32_t* out_value)
{
    lua_Integer value;
    if (state == NULL || out_value == NULL || !lua_isinteger(state, index))
    {
        return false;
    }
    value = lua_tointeger(state, index);
    if (value < INT32_MIN || value > INT32_MAX)
    {
        return false;
    }
    *out_value = (int32_t)value;
    return true;
}

static bool henka_lua_get_vec3(
    lua_State* state,
    int index,
    henka_vec3* out_value)
{
    int is_number;
    lua_Number x;
    lua_Number y;
    lua_Number z;
    if (state == NULL || out_value == NULL || !lua_istable(state, index))
    {
        return false;
    }
    lua_getfield(state, index, "x");
    x = lua_tonumberx(state, -1, &is_number);
    if (!is_number)
    {
        lua_pop(state, 1);
        return false;
    }
    lua_getfield(state, index, "y");
    y = lua_tonumberx(state, -1, &is_number);
    if (!is_number)
    {
        lua_pop(state, 2);
        return false;
    }
    lua_getfield(state, index, "z");
    z = lua_tonumberx(state, -1, &is_number);
    if (!is_number)
    {
        lua_pop(state, 3);
        return false;
    }
    lua_pop(state, 3);
    if (!isfinite((double)x) || !isfinite((double)y) || !isfinite((double)z) ||
        !isfinite((double)(float)x) || !isfinite((double)(float)y) ||
        !isfinite((double)(float)z))
    {
        return false;
    }
    *out_value = (henka_vec3){(float)x, (float)y, (float)z};
    return true;
}

static henka_result henka_lua_invoke_host(
    henka_lua_behavior_backend* backend,
    uint32_t api_id,
    const henka_script_api_value* arguments,
    size_t argument_count,
    henka_script_api_value* out_value)
{
    if (backend == NULL || backend->active_host == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return henka_script_host_invoke(
        backend->active_host,
        api_id,
        arguments,
        argument_count,
        out_value);
}

static int henka_lua_entity_is_valid(lua_State* state)
{
    henka_lua_behavior_backend* backend = henka_lua_backend_from_upvalue(state);
    henka_script_api_value argument;
    henka_script_api_value output;
    uint64_t entity;
    if (!henka_lua_get_unsigned_integer(state, 1, UINT64_MAX, &entity))
    {
        return luaL_error(state, "Entity.IsValid requires a non-negative integer entity ID");
    }
    argument = (henka_script_api_value){
        HENKA_SCRIPT_API_VALUE_ENTITY, {.entity = entity}};
    if (henka_lua_invoke_host(
            backend,
            HENKA_SCRIPT_API_ENTITY_IS_VALID,
            &argument,
            1U,
            &output) != HENKA_SUCCESS ||
        output.type != HENKA_SCRIPT_API_VALUE_BOOL)
    {
        return luaL_error(state, "Entity.IsValid is unavailable in this runtime");
    }
    lua_pushboolean(state, output.as.boolean);
    return 1;
}

static int henka_lua_transform_get_position(lua_State* state)
{
    henka_lua_behavior_backend* backend = henka_lua_backend_from_upvalue(state);
    henka_script_api_value argument;
    henka_script_api_value output;
    uint64_t entity;
    if (!henka_lua_get_unsigned_integer(state, 1, UINT64_MAX, &entity))
    {
        return luaL_error(state, "Transform.GetPosition requires a non-negative integer entity ID");
    }
    argument = (henka_script_api_value){
        HENKA_SCRIPT_API_VALUE_ENTITY, {.entity = entity}};
    if (henka_lua_invoke_host(
            backend,
            HENKA_SCRIPT_API_TRANSFORM_GET_POSITION,
            &argument,
            1U,
            &output) != HENKA_SUCCESS ||
        output.type != HENKA_SCRIPT_API_VALUE_VEC3)
    {
        return luaL_error(state, "Transform.GetPosition failed");
    }
    lua_createtable(state, 0, 3);
    lua_pushnumber(state, (lua_Number)output.as.vec3.x);
    lua_setfield(state, -2, "x");
    lua_pushnumber(state, (lua_Number)output.as.vec3.y);
    lua_setfield(state, -2, "y");
    lua_pushnumber(state, (lua_Number)output.as.vec3.z);
    lua_setfield(state, -2, "z");
    return 1;
}

static int henka_lua_transform_set_position(lua_State* state)
{
    henka_lua_behavior_backend* backend = henka_lua_backend_from_upvalue(state);
    henka_script_api_value arguments[2];
    henka_script_api_value output;
    henka_vec3 position;
    uint64_t entity;
    if (!henka_lua_get_unsigned_integer(state, 1, UINT64_MAX, &entity) ||
        !henka_lua_get_vec3(state, 2, &position))
    {
        return luaL_error(state, "Transform.SetPosition requires entity ID and {x,y,z}");
    }
    arguments[0] = (henka_script_api_value){
        HENKA_SCRIPT_API_VALUE_ENTITY, {.entity = entity}};
    arguments[1] = (henka_script_api_value){
        HENKA_SCRIPT_API_VALUE_VEC3, {.vec3 = position}};
    if (henka_lua_invoke_host(
            backend,
            HENKA_SCRIPT_API_TRANSFORM_SET_POSITION,
            arguments,
            2U,
            &output) != HENKA_SUCCESS ||
        output.type != HENKA_SCRIPT_API_VALUE_RESULT)
    {
        return luaL_error(state, "Transform.SetPosition failed");
    }
    lua_pushinteger(state, (lua_Integer)output.as.result);
    return 1;
}

static int henka_lua_input_is_action_down(lua_State* state)
{
    henka_lua_behavior_backend* backend = henka_lua_backend_from_upvalue(state);
    henka_script_api_value argument;
    henka_script_api_value output;
    uint64_t action_id;
    if (!henka_lua_get_unsigned_integer(state, 1, UINT32_MAX, &action_id))
    {
        return luaL_error(state, "Input.IsActionDown requires a uint32 action ID");
    }
    argument = (henka_script_api_value){
        HENKA_SCRIPT_API_VALUE_ACTION_ID, {.action_id = (uint32_t)action_id}};
    if (henka_lua_invoke_host(
            backend,
            HENKA_SCRIPT_API_INPUT_IS_ACTION_DOWN,
            &argument,
            1U,
            &output) != HENKA_SUCCESS ||
        output.type != HENKA_SCRIPT_API_VALUE_BOOL)
    {
        return luaL_error(state, "Input.IsActionDown is unavailable in this runtime");
    }
    lua_pushboolean(state, output.as.boolean);
    return 1;
}

static int henka_lua_physics_apply_impulse(lua_State* state)
{
    henka_lua_behavior_backend* backend = henka_lua_backend_from_upvalue(state);
    henka_script_api_value arguments[2];
    henka_script_api_value output;
    henka_vec3 impulse;
    uint64_t entity;
    if (!henka_lua_get_unsigned_integer(state, 1, UINT64_MAX, &entity) ||
        !henka_lua_get_vec3(state, 2, &impulse))
    {
        return luaL_error(state, "Physics.ApplyImpulse requires entity ID and {x,y,z}");
    }
    arguments[0] = (henka_script_api_value){
        HENKA_SCRIPT_API_VALUE_ENTITY, {.entity = entity}};
    arguments[1] = (henka_script_api_value){
        HENKA_SCRIPT_API_VALUE_VEC3, {.vec3 = impulse}};
    if (henka_lua_invoke_host(
            backend,
            HENKA_SCRIPT_API_PHYSICS_APPLY_IMPULSE,
            arguments,
            2U,
            &output) != HENKA_SUCCESS ||
        output.type != HENKA_SCRIPT_API_VALUE_RESULT)
    {
        return luaL_error(state, "Physics.ApplyImpulse failed");
    }
    lua_pushinteger(state, (lua_Integer)output.as.result);
    return 1;
}

static int henka_lua_interaction_try(lua_State* state)
{
    henka_lua_behavior_backend* backend = henka_lua_backend_from_upvalue(state);
    henka_script_api_value argument;
    henka_script_api_value output;
    uint64_t entity;
    if (!henka_lua_get_unsigned_integer(state, 1, UINT64_MAX, &entity))
    {
        return luaL_error(state, "Interaction.Try requires a non-negative integer entity ID");
    }
    argument = (henka_script_api_value){
        HENKA_SCRIPT_API_VALUE_ENTITY, {.entity = entity}};
    if (henka_lua_invoke_host(
            backend,
            HENKA_SCRIPT_API_INTERACTION_TRY,
            &argument,
            1U,
            &output) != HENKA_SUCCESS ||
        output.type != HENKA_SCRIPT_API_VALUE_RESULT)
    {
        return luaL_error(state, "Interaction.Try failed");
    }
    lua_pushinteger(state, (lua_Integer)output.as.result);
    return 1;
}

static int henka_lua_audio_stop(lua_State* state)
{
    henka_lua_behavior_backend* backend = henka_lua_backend_from_upvalue(state);
    henka_script_api_value argument;
    henka_script_api_value output;
    uint64_t entity;
    if (!henka_lua_get_unsigned_integer(state, 1, UINT64_MAX, &entity))
    {
        return luaL_error(state, "Audio.Stop requires a non-negative integer entity ID");
    }
    argument = (henka_script_api_value){
        HENKA_SCRIPT_API_VALUE_ENTITY, {.entity = entity}};
    if (henka_lua_invoke_host(
            backend, HENKA_SCRIPT_API_AUDIO_STOP, &argument, 1U, &output) != HENKA_SUCCESS ||
        output.type != HENKA_SCRIPT_API_VALUE_RESULT)
    {
        return luaL_error(state, "Audio.Stop failed");
    }
    lua_pushinteger(state, (lua_Integer)output.as.result);
    return 1;
}

static int henka_lua_audio_restart(lua_State* state)
{
    henka_lua_behavior_backend* backend = henka_lua_backend_from_upvalue(state);
    henka_script_api_value argument;
    henka_script_api_value output;
    uint64_t entity;
    if (!henka_lua_get_unsigned_integer(state, 1, UINT64_MAX, &entity))
    {
        return luaL_error(state, "Audio.Restart requires a non-negative integer entity ID");
    }
    argument = (henka_script_api_value){
        HENKA_SCRIPT_API_VALUE_ENTITY, {.entity = entity}};
    if (henka_lua_invoke_host(
            backend, HENKA_SCRIPT_API_AUDIO_RESTART, &argument, 1U, &output) != HENKA_SUCCESS ||
        output.type != HENKA_SCRIPT_API_VALUE_RESULT)
    {
        return luaL_error(state, "Audio.Restart failed");
    }
    lua_pushinteger(state, (lua_Integer)output.as.result);
    return 1;
}

static int henka_lua_audio_is_playing(lua_State* state)
{
    henka_lua_behavior_backend* backend = henka_lua_backend_from_upvalue(state);
    henka_script_api_value argument;
    henka_script_api_value output;
    uint64_t entity;
    if (!henka_lua_get_unsigned_integer(state, 1, UINT64_MAX, &entity))
    {
        return luaL_error(state, "Audio.IsPlaying requires a non-negative integer entity ID");
    }
    argument = (henka_script_api_value){
        HENKA_SCRIPT_API_VALUE_ENTITY, {.entity = entity}};
    if (henka_lua_invoke_host(
            backend, HENKA_SCRIPT_API_AUDIO_IS_PLAYING, &argument, 1U, &output) != HENKA_SUCCESS ||
        output.type != HENKA_SCRIPT_API_VALUE_BOOL)
    {
        return luaL_error(state, "Audio.IsPlaying is unavailable in this runtime");
    }
    lua_pushboolean(state, output.as.boolean);
    return 1;
}

static int henka_lua_audio_result_call(
    lua_State* state,
    uint32_t api_id,
    const char* error_message,
    const henka_script_api_value* arguments,
    size_t argument_count)
{
    henka_lua_behavior_backend* backend = henka_lua_backend_from_upvalue(state);
    henka_script_api_value output;
    if (henka_lua_invoke_host(
            backend, api_id, arguments, argument_count, &output) != HENKA_SUCCESS ||
        output.type != HENKA_SCRIPT_API_VALUE_RESULT)
    {
        return luaL_error(state, "%s", error_message);
    }
    lua_pushinteger(state, (lua_Integer)output.as.result);
    return 1;
}

static bool henka_lua_get_audio_float(
    lua_State* state,
    int index,
    float* out_value)
{
    int is_number = 0;
    lua_Number value;
    if (state == NULL || out_value == NULL)
    {
        return false;
    }
    value = lua_tonumberx(state, index, &is_number);
    if (!is_number || !isfinite((double)value) ||
        !isfinite((double)(float)value))
    {
        return false;
    }
    *out_value = (float)value;
    return true;
}

static int henka_lua_audio_play(lua_State* state)
{
    uint64_t entity;
    henka_script_api_value argument;
    if (!henka_lua_get_unsigned_integer(state, 1, UINT64_MAX, &entity))
    {
        return luaL_error(state, "Audio.Play requires a non-negative integer entity ID");
    }
    argument = (henka_script_api_value){
        HENKA_SCRIPT_API_VALUE_ENTITY, {.entity = entity}};
    return henka_lua_audio_result_call(
        state, HENKA_SCRIPT_API_AUDIO_PLAY, "Audio.Play failed", &argument, 1U);
}

static int henka_lua_audio_pause(lua_State* state)
{
    uint64_t entity;
    henka_script_api_value argument;
    if (!henka_lua_get_unsigned_integer(state, 1, UINT64_MAX, &entity))
    {
        return luaL_error(state, "Audio.Pause requires a non-negative integer entity ID");
    }
    argument = (henka_script_api_value){
        HENKA_SCRIPT_API_VALUE_ENTITY, {.entity = entity}};
    return henka_lua_audio_result_call(
        state, HENKA_SCRIPT_API_AUDIO_PAUSE, "Audio.Pause failed", &argument, 1U);
}

static int henka_lua_audio_resume(lua_State* state)
{
    uint64_t entity;
    henka_script_api_value argument;
    if (!henka_lua_get_unsigned_integer(state, 1, UINT64_MAX, &entity))
    {
        return luaL_error(state, "Audio.Resume requires a non-negative integer entity ID");
    }
    argument = (henka_script_api_value){
        HENKA_SCRIPT_API_VALUE_ENTITY, {.entity = entity}};
    return henka_lua_audio_result_call(
        state, HENKA_SCRIPT_API_AUDIO_RESUME, "Audio.Resume failed", &argument, 1U);
}

static int henka_lua_audio_set_gain_or_pitch(
    lua_State* state,
    uint32_t api_id,
    const char* function_name,
    const char* error_message)
{
    uint64_t entity;
    float value;
    henka_script_api_value arguments[2];
    if (!henka_lua_get_unsigned_integer(state, 1, UINT64_MAX, &entity) ||
        !henka_lua_get_audio_float(state, 2, &value))
    {
        return luaL_error(
            state, "%s requires an entity ID and finite number", function_name);
    }
    arguments[0] = (henka_script_api_value){
        HENKA_SCRIPT_API_VALUE_ENTITY, {.entity = entity}};
    arguments[1] = (henka_script_api_value){
        HENKA_SCRIPT_API_VALUE_FLOAT32, {.f32 = value}};
    return henka_lua_audio_result_call(
        state, api_id, error_message, arguments, 2U);
}

static int henka_lua_audio_set_gain(lua_State* state)
{
    return henka_lua_audio_set_gain_or_pitch(
        state, HENKA_SCRIPT_API_AUDIO_SET_GAIN,
        "Audio.SetGain", "Audio.SetGain failed");
}

static int henka_lua_audio_set_pitch(lua_State* state)
{
    return henka_lua_audio_set_gain_or_pitch(
        state, HENKA_SCRIPT_API_AUDIO_SET_PITCH,
        "Audio.SetPitch", "Audio.SetPitch failed");
}

static int henka_lua_audio_set_bool(
    lua_State* state,
    uint32_t api_id,
    const char* function_name,
    const char* error_message)
{
    uint64_t entity;
    henka_script_api_value arguments[2];
    if (!henka_lua_get_unsigned_integer(state, 1, UINT64_MAX, &entity) ||
        !lua_isboolean(state, 2))
    {
        return luaL_error(
            state, "%s requires an entity ID and boolean", function_name);
    }
    arguments[0] = (henka_script_api_value){
        HENKA_SCRIPT_API_VALUE_ENTITY, {.entity = entity}};
    arguments[1] = (henka_script_api_value){
        HENKA_SCRIPT_API_VALUE_BOOL, {.boolean = lua_toboolean(state, 2) != 0}};
    return henka_lua_audio_result_call(
        state, api_id, error_message, arguments, 2U);
}

static int henka_lua_audio_set_looping(lua_State* state)
{
    return henka_lua_audio_set_bool(
        state, HENKA_SCRIPT_API_AUDIO_SET_LOOPING,
        "Audio.SetLooping", "Audio.SetLooping failed");
}

static int henka_lua_audio_set_spatial(lua_State* state)
{
    return henka_lua_audio_set_bool(
        state, HENKA_SCRIPT_API_AUDIO_SET_SPATIAL,
        "Audio.SetSpatial", "Audio.SetSpatial failed");
}

static int henka_lua_audio_set_i32(
    lua_State* state,
    uint32_t api_id,
    const char* function_name,
    const char* error_message)
{
    uint64_t entity;
    int32_t value;
    henka_script_api_value arguments[2];
    if (!henka_lua_get_unsigned_integer(state, 1, UINT64_MAX, &entity) ||
        !henka_lua_get_i32(state, 2, &value))
    {
        return luaL_error(
            state, "%s requires an entity ID and 32-bit integer", function_name);
    }
    arguments[0] = (henka_script_api_value){
        HENKA_SCRIPT_API_VALUE_ENTITY, {.entity = entity}};
    arguments[1] = (henka_script_api_value){
        HENKA_SCRIPT_API_VALUE_I32, {.i32 = value}};
    return henka_lua_audio_result_call(
        state, api_id, error_message, arguments, 2U);
}

static int henka_lua_audio_set_bus(lua_State* state)
{
    return henka_lua_audio_set_i32(
        state, HENKA_SCRIPT_API_AUDIO_SET_BUS,
        "Audio.SetBus", "Audio.SetBus failed");
}

static int henka_lua_audio_seek(lua_State* state)
{
    return henka_lua_audio_set_i32(
        state, HENKA_SCRIPT_API_AUDIO_SEEK,
        "Audio.Seek", "Audio.Seek failed");
}

static int henka_lua_events_emit(lua_State* state)
{
    henka_lua_behavior_backend* backend = henka_lua_backend_from_upvalue(state);
    henka_script_api_value arguments[2];
    henka_script_api_value output;
    uint64_t event_id;
    uint64_t entity;
    if (!henka_lua_get_unsigned_integer(state, 1, UINT32_MAX, &event_id) ||
        !henka_lua_get_unsigned_integer(state, 2, UINT64_MAX, &entity))
    {
        return luaL_error(state, "Events.Emit requires event ID and entity ID");
    }
    arguments[0] = (henka_script_api_value){
        HENKA_SCRIPT_API_VALUE_EVENT_ID, {.event_id = (uint32_t)event_id}};
    arguments[1] = (henka_script_api_value){
        HENKA_SCRIPT_API_VALUE_ENTITY, {.entity = entity}};
    if (henka_lua_invoke_host(
            backend,
            HENKA_SCRIPT_API_EVENTS_EMIT,
            arguments,
            2U,
            &output) != HENKA_SUCCESS ||
        output.type != HENKA_SCRIPT_API_VALUE_RESULT)
    {
        return luaL_error(state, "Events.Emit failed");
    }
    lua_pushinteger(state, (lua_Integer)output.as.result);
    return 1;
}

static int henka_lua_state_get_i32(lua_State* state)
{
    henka_lua_behavior_backend* backend = henka_lua_backend_from_upvalue(state);
    henka_script_api_value argument;
    henka_script_api_value output;
    uint64_t state_key;
    if (!henka_lua_get_unsigned_integer(state, 1, UINT32_MAX, &state_key) ||
        state_key == 0U)
    {
        return luaL_error(state, "State.GetI32 requires a non-zero uint32 state key");
    }
    argument = (henka_script_api_value){
        HENKA_SCRIPT_API_VALUE_STATE_KEY, {.state_key = (uint32_t)state_key}};
    if (henka_lua_invoke_host(
            backend, HENKA_SCRIPT_API_STATE_GET_I32, &argument, 1U, &output) != HENKA_SUCCESS ||
        output.type != HENKA_SCRIPT_API_VALUE_I32)
    {
        return luaL_error(state, "State.GetI32 is unavailable in this runtime");
    }
    lua_pushinteger(state, (lua_Integer)output.as.i32);
    lua_pushboolean(state, output.present);
    return 2;
}

static int henka_lua_state_set_i32(lua_State* state)
{
    henka_lua_behavior_backend* backend = henka_lua_backend_from_upvalue(state);
    henka_script_api_value arguments[2];
    henka_script_api_value output;
    uint64_t state_key;
    int32_t value;
    if (!henka_lua_get_unsigned_integer(state, 1, UINT32_MAX, &state_key) ||
        state_key == 0U || !henka_lua_get_i32(state, 2, &value))
    {
        return luaL_error(state, "State.SetI32 requires a non-zero uint32 state key and int32 value");
    }
    arguments[0] = (henka_script_api_value){
        HENKA_SCRIPT_API_VALUE_STATE_KEY, {.state_key = (uint32_t)state_key}};
    arguments[1] = (henka_script_api_value){
        HENKA_SCRIPT_API_VALUE_I32, {.i32 = value}};
    if (henka_lua_invoke_host(
            backend, HENKA_SCRIPT_API_STATE_SET_I32, arguments, 2U, &output) != HENKA_SUCCESS ||
        output.type != HENKA_SCRIPT_API_VALUE_RESULT)
    {
        return luaL_error(state, "State.SetI32 failed");
    }
    lua_pushinteger(state, (lua_Integer)output.as.result);
    return 1;
}

static int henka_lua_state_get_bool(lua_State* state)
{
    henka_lua_behavior_backend* backend = henka_lua_backend_from_upvalue(state);
    henka_script_api_value argument;
    henka_script_api_value output;
    uint64_t state_key;
    if (!henka_lua_get_unsigned_integer(state, 1, UINT32_MAX, &state_key) ||
        state_key == 0U)
    {
        return luaL_error(state, "State.GetBool requires a non-zero uint32 state key");
    }
    argument = (henka_script_api_value){
        HENKA_SCRIPT_API_VALUE_STATE_KEY, {.state_key = (uint32_t)state_key}};
    if (henka_lua_invoke_host(
            backend, HENKA_SCRIPT_API_STATE_GET_BOOL, &argument, 1U, &output) != HENKA_SUCCESS ||
        output.type != HENKA_SCRIPT_API_VALUE_BOOL)
    {
        return luaL_error(state, "State.GetBool is unavailable in this runtime");
    }
    lua_pushboolean(state, output.as.boolean);
    lua_pushboolean(state, output.present);
    return 2;
}

static int henka_lua_state_set_bool(lua_State* state)
{
    henka_lua_behavior_backend* backend = henka_lua_backend_from_upvalue(state);
    henka_script_api_value arguments[2];
    henka_script_api_value output;
    uint64_t state_key;
    if (!henka_lua_get_unsigned_integer(state, 1, UINT32_MAX, &state_key) ||
        state_key == 0U || !lua_isboolean(state, 2))
    {
        return luaL_error(state, "State.SetBool requires a non-zero uint32 state key and boolean value");
    }
    arguments[0] = (henka_script_api_value){
        HENKA_SCRIPT_API_VALUE_STATE_KEY, {.state_key = (uint32_t)state_key}};
    arguments[1] = (henka_script_api_value){
        HENKA_SCRIPT_API_VALUE_BOOL, {.boolean = lua_toboolean(state, 2) != 0}};
    if (henka_lua_invoke_host(
            backend, HENKA_SCRIPT_API_STATE_SET_BOOL, arguments, 2U, &output) != HENKA_SUCCESS ||
        output.type != HENKA_SCRIPT_API_VALUE_RESULT)
    {
        return luaL_error(state, "State.SetBool failed");
    }
    lua_pushinteger(state, (lua_Integer)output.as.result);
    return 1;
}

static void henka_lua_register_api_function(
    lua_State* state,
    henka_lua_behavior_backend* backend,
    const char* table_name,
    const char* function_name,
    lua_CFunction function)
{
    lua_getglobal(state, table_name);
    if (!lua_istable(state, -1))
    {
        lua_pop(state, 1);
        lua_newtable(state);
        lua_pushvalue(state, -1);
        lua_setglobal(state, table_name);
    }
    lua_pushlightuserdata(state, backend);
    lua_pushcclosure(state, function, 1);
    lua_setfield(state, -2, function_name);
    lua_pop(state, 1);
}

static void henka_lua_register_api(henka_lua_behavior_backend* backend)
{
    if (backend == NULL || backend->state == NULL)
    {
        return;
    }
    henka_lua_register_api_function(
        backend->state, backend, "Entity", "IsValid", henka_lua_entity_is_valid);
    henka_lua_register_api_function(
        backend->state, backend, "Transform", "GetPosition", henka_lua_transform_get_position);
    henka_lua_register_api_function(
        backend->state, backend, "Transform", "SetPosition", henka_lua_transform_set_position);
    henka_lua_register_api_function(
        backend->state, backend, "Input", "IsActionDown", henka_lua_input_is_action_down);
    henka_lua_register_api_function(
        backend->state, backend, "Physics", "ApplyImpulse", henka_lua_physics_apply_impulse);
    henka_lua_register_api_function(
        backend->state, backend, "Interaction", "Try", henka_lua_interaction_try);
    henka_lua_register_api_function(
        backend->state, backend, "Audio", "Stop", henka_lua_audio_stop);
    henka_lua_register_api_function(
        backend->state, backend, "Audio", "Restart", henka_lua_audio_restart);
    henka_lua_register_api_function(
        backend->state, backend, "Audio", "IsPlaying", henka_lua_audio_is_playing);
    henka_lua_register_api_function(
        backend->state, backend, "Audio", "Play", henka_lua_audio_play);
    henka_lua_register_api_function(
        backend->state, backend, "Audio", "Pause", henka_lua_audio_pause);
    henka_lua_register_api_function(
        backend->state, backend, "Audio", "Resume", henka_lua_audio_resume);
    henka_lua_register_api_function(
        backend->state, backend, "Audio", "SetGain", henka_lua_audio_set_gain);
    henka_lua_register_api_function(
        backend->state, backend, "Audio", "SetPitch", henka_lua_audio_set_pitch);
    henka_lua_register_api_function(
        backend->state, backend, "Audio", "SetLooping", henka_lua_audio_set_looping);
    henka_lua_register_api_function(
        backend->state, backend, "Audio", "SetSpatial", henka_lua_audio_set_spatial);
    henka_lua_register_api_function(
        backend->state, backend, "Audio", "SetBus", henka_lua_audio_set_bus);
    henka_lua_register_api_function(
        backend->state, backend, "Audio", "Seek", henka_lua_audio_seek);
    henka_lua_register_api_function(
        backend->state, backend, "Events", "Emit", henka_lua_events_emit);
    henka_lua_register_api_function(
        backend->state, backend, "State", "GetI32", henka_lua_state_get_i32);
    henka_lua_register_api_function(
        backend->state, backend, "State", "SetI32", henka_lua_state_set_i32);
    henka_lua_register_api_function(
        backend->state, backend, "State", "GetBool", henka_lua_state_get_bool);
    henka_lua_register_api_function(
        backend->state, backend, "State", "SetBool", henka_lua_state_set_bool);
}

henka_result henka_lua_behavior_backend_create(
    const char* source,
    size_t source_size,
    henka_lua_behavior_backend** out_backend,
    henka_lua_diagnostic* out_diagnostic)
{
    const henka_script_lifecycle_descriptor* lifecycle_schema = NULL;
    size_t lifecycle_count = 0U;
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
    for (index = 0U; index < HENKA_SCRIPT_LIFECYCLE_SLOT_COUNT; ++index)
    {
        backend->callable[index] = LUA_NOREF;
    }
    backend->state = lua_newstate(henka_lua_allocate, &backend->allocator);
    if (backend->state == NULL)
    {
        henka_free(backend);
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    *(henka_lua_behavior_backend**)lua_getextraspace(backend->state) = backend;
    henka_lua_open_safe_libraries(backend->state);
    henka_lua_register_api(backend);
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
    if (henka_script_lifecycle_schema_get(
            &lifecycle_schema,
            &lifecycle_count) != HENKA_SUCCESS ||
        lifecycle_schema == NULL)
    {
        henka_lua_behavior_backend_destroy(backend);
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    for (index = 0U; index < lifecycle_count; ++index)
    {
        const henka_script_lifecycle_descriptor* descriptor = &lifecycle_schema[index];
        if (descriptor->event >= HENKA_SCRIPT_LIFECYCLE_SLOT_COUNT)
        {
            henka_lua_behavior_backend_destroy(backend);
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        lua_getglobal(backend->state, descriptor->name);
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
            backend->callable[descriptor->event] =
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
        context->event > HENKA_SCRIPT_LIFECYCLE_DESTROY ||
        backend->dispatching)
    {
        return HENKA_SCRIPT_CALLBACK_FAILED;
    }
    callable = backend->callable[context->event];
    if (callable == LUA_NOREF)
    {
        return HENKA_SCRIPT_CALLBACK_COMPLETED;
    }
    backend->dispatching = true;
    henka_lua_register_api(backend);
    backend->active_host = context->host;
    backend->instruction_budget = context->instruction_budget;
    backend->instructions_used = 0U;
    backend->budget_exhausted = false;
    lua_sethook(backend->state, henka_lua_budget_hook, LUA_MASKCOUNT, 1);
    lua_rawgeti(backend->state, LUA_REGISTRYINDEX, callable);
    if (context->event == HENKA_SCRIPT_LIFECYCLE_EVENT)
    {
        lua_pushinteger(backend->state, (lua_Integer)context->event_id);
        lua_pushinteger(backend->state, (lua_Integer)context->event_source_entity);
    }
    else if (henka_lua_event_has_arguments(context->event))
    {
        lua_pushinteger(backend->state, (lua_Integer)context->event_other_entity);
        lua_pushinteger(backend->state, (lua_Integer)context->event_type);
    }
    status = lua_pcall(
        backend->state,
        context->event == HENKA_SCRIPT_LIFECYCLE_EVENT ||
            henka_lua_event_has_arguments(context->event) ? 2 : 0,
        0,
        0);
    lua_sethook(backend->state, NULL, 0, 0);
    backend->active_host = NULL;
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
