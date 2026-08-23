#ifndef HENKA_SCRIPT_BACKENDS_H
#define HENKA_SCRIPT_BACKENDS_H

#include <stddef.h>

#include <henka/henkascript.h>
#include <henka/script_runtime.h>

typedef struct henka_hks_behavior_backend henka_hks_behavior_backend;

/* Compiles a bounded HenkaScript behavior asset. Lifecycle functions are
 * discovered by the exact names OnCreate, OnStart, OnUpdate, and OnStop.
 * Missing lifecycle functions are deterministic no-ops. */
henka_result henka_hks_behavior_backend_create(
    const char* source,
    size_t source_size,
    henka_hks_behavior_backend** out_backend,
    henka_hks_diagnostic* out_diagnostic);
void henka_hks_behavior_backend_destroy(henka_hks_behavior_backend* backend);

henka_script_behavior_callback_result henka_hks_behavior_backend_callback(
    const henka_script_behavior_context* context,
    void* user_data,
    uint32_t* out_instructions_used);

#define HENKA_LUA_MAX_SOURCE_BYTES (256U * 1024U)
#define HENKA_LUA_MAX_MEMORY_BYTES (4U * 1024U * 1024U)
#define HENKA_LUA_MAX_TOKENS 4096U
#define HENKA_LUA_MAX_DIAGNOSTIC_BYTES 192U

typedef enum henka_lua_diagnostic_code
{
    HENKA_LUA_DIAGNOSTIC_NONE = 0,
    HENKA_LUA_DIAGNOSTIC_INVALID_SOURCE,
    HENKA_LUA_DIAGNOSTIC_COMPILE,
    HENKA_LUA_DIAGNOSTIC_RUNTIME,
    HENKA_LUA_DIAGNOSTIC_LIMIT,
    HENKA_LUA_DIAGNOSTIC_MEMORY
} henka_lua_diagnostic_code;

typedef struct henka_lua_diagnostic
{
    henka_lua_diagnostic_code code;
    uint32_t line;
    uint32_t column;
    char message[HENKA_LUA_MAX_DIAGNOSTIC_BYTES];
} henka_lua_diagnostic;

/* Lua's parser remains the acceptance authority. This bounded lexical seam is
 * backend-owned presentation metadata for editor syntax spans and indentation;
 * it is not a second compiler or a promise that invalid Lua is executable. */
typedef enum henka_lua_token_class
{
    HENKA_LUA_TOKEN_CLASS_NONE = 0,
    HENKA_LUA_TOKEN_CLASS_IDENTIFIER,
    HENKA_LUA_TOKEN_CLASS_LITERAL,
    HENKA_LUA_TOKEN_CLASS_KEYWORD,
    HENKA_LUA_TOKEN_CLASS_BUILTIN,
    HENKA_LUA_TOKEN_CLASS_COMMENT,
    HENKA_LUA_TOKEN_CLASS_PUNCTUATION,
    HENKA_LUA_TOKEN_CLASS_OPERATOR
} henka_lua_token_class;

typedef struct henka_lua_token
{
    henka_lua_token_class token_class;
    size_t offset;
    size_t length;
    uint32_t line;
    uint32_t column;
} henka_lua_token;

henka_result henka_lua_lex(
    const char* source,
    size_t source_size,
    henka_lua_token* tokens,
    size_t token_capacity,
    size_t* out_token_count,
    henka_lua_diagnostic* out_diagnostic);

henka_result henka_lua_token_stream_get_indent_level(
    const char* source,
    size_t source_size,
    const henka_lua_token* tokens,
    size_t token_count,
    size_t source_offset,
    uint32_t source_line,
    uint32_t* out_indent_level);

typedef struct henka_lua_behavior_backend henka_lua_behavior_backend;

henka_result henka_lua_behavior_backend_create(
    const char* source,
    size_t source_size,
    henka_lua_behavior_backend** out_backend,
    henka_lua_diagnostic* out_diagnostic);
void henka_lua_behavior_backend_destroy(henka_lua_behavior_backend* backend);
henka_script_behavior_callback_result henka_lua_behavior_backend_callback(
    const henka_script_behavior_context* context,
    void* user_data,
    uint32_t* out_instructions_used);

#endif
