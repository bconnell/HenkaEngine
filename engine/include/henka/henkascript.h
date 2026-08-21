#ifndef HENKA_HENKASCRIPT_H
#define HENKA_HENKASCRIPT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <henka/math.h>
#include <henka/result.h>

#define HENKA_HKS_MAX_SOURCE_BYTES (256U * 1024U)
#define HENKA_HKS_MAX_TOKENS 4096U
#define HENKA_HKS_MAX_BINDINGS 128U
#define HENKA_HKS_MAX_CALLABLES 64U
#define HENKA_HKS_MAX_AST_NODES 2048U
#define HENKA_HKS_MAX_BYTECODE 8192U
#define HENKA_HKS_MAX_VM_STACK 256U
#define HENKA_HKS_DEFAULT_INSTRUCTION_BUDGET 1024U
#define HENKA_HKS_MAX_IDENTIFIER_BYTES 64U
#define HENKA_HKS_MAX_DIAGNOSTIC_BYTES 192U

typedef struct henka_hks_program henka_hks_program;

typedef enum henka_hks_token_kind
{
    HENKA_HKS_TOKEN_EOF = 0,
    HENKA_HKS_TOKEN_IDENTIFIER,
    HENKA_HKS_TOKEN_INTEGER,
    HENKA_HKS_TOKEN_FLOAT,
    HENKA_HKS_TOKEN_STRING,
    HENKA_HKS_TOKEN_KW_BOOL,
    HENKA_HKS_TOKEN_KW_I32,
    HENKA_HKS_TOKEN_KW_U32,
    HENKA_HKS_TOKEN_KW_F32,
    HENKA_HKS_TOKEN_KW_VEC3,
    HENKA_HKS_TOKEN_KW_ENTITY,
    HENKA_HKS_TOKEN_KW_FN,
    HENKA_HKS_TOKEN_KW_BEHAVIOR,
    HENKA_HKS_TOKEN_KW_RETURN,
    HENKA_HKS_TOKEN_KW_EMIT,
    HENKA_HKS_TOKEN_KW_STATE_GET_I32,
    HENKA_HKS_TOKEN_KW_STATE_SET_I32,
    HENKA_HKS_TOKEN_KW_STATE_GET_BOOL,
    HENKA_HKS_TOKEN_KW_STATE_SET_BOOL,
    HENKA_HKS_TOKEN_KW_ENTITY_IS_VALID,
    HENKA_HKS_TOKEN_KW_INPUT_IS_ACTION_DOWN,
    HENKA_HKS_TOKEN_KW_TRANSFORM_GET_POSITION,
    HENKA_HKS_TOKEN_KW_TRANSFORM_SET_POSITION,
    HENKA_HKS_TOKEN_KW_PHYSICS_APPLY_IMPULSE,
    HENKA_HKS_TOKEN_KW_INTERACTION_TRY,
    HENKA_HKS_TOKEN_KW_EVENT_ID,
    HENKA_HKS_TOKEN_KW_EVENT_OTHER_ENTITY,
    HENKA_HKS_TOKEN_KW_EVENT_TYPE,
    HENKA_HKS_TOKEN_KW_TRUE,
    HENKA_HKS_TOKEN_KW_FALSE,
    HENKA_HKS_TOKEN_KW_LET,
    HENKA_HKS_TOKEN_KW_VAR,
    HENKA_HKS_TOKEN_LBRACE,
    HENKA_HKS_TOKEN_RBRACE,
    HENKA_HKS_TOKEN_LPAREN,
    HENKA_HKS_TOKEN_RPAREN,
    HENKA_HKS_TOKEN_SEMICOLON,
    HENKA_HKS_TOKEN_COMMA,
    HENKA_HKS_TOKEN_ASSIGN,
    HENKA_HKS_TOKEN_INFER,
    HENKA_HKS_TOKEN_PLUS,
    HENKA_HKS_TOKEN_MINUS,
    HENKA_HKS_TOKEN_STAR,
    HENKA_HKS_TOKEN_SLASH,
    HENKA_HKS_TOKEN_KW_IF,
    HENKA_HKS_TOKEN_KW_ELSE,
    HENKA_HKS_TOKEN_KW_WHILE,
    HENKA_HKS_TOKEN_KW_FOR,
    HENKA_HKS_TOKEN_KW_BREAK,
    HENKA_HKS_TOKEN_KW_CONTINUE,
    HENKA_HKS_TOKEN_EQUAL_EQUAL,
    HENKA_HKS_TOKEN_NOT_EQUAL,
    HENKA_HKS_TOKEN_LESS,
    HENKA_HKS_TOKEN_LESS_EQUAL,
    HENKA_HKS_TOKEN_GREATER,
    HENKA_HKS_TOKEN_GREATER_EQUAL
} henka_hks_token_kind;

typedef struct henka_hks_token
{
    henka_hks_token_kind kind;
    size_t offset;
    size_t length;
    uint32_t line;
    uint32_t column;
} henka_hks_token;

typedef enum henka_hks_value_type
{
    HENKA_HKS_TYPE_UNKNOWN = 0,
    HENKA_HKS_TYPE_VOID,
    HENKA_HKS_TYPE_BOOL,
    HENKA_HKS_TYPE_I32,
    HENKA_HKS_TYPE_U32,
    HENKA_HKS_TYPE_F32,
    HENKA_HKS_TYPE_VEC3,
    HENKA_HKS_TYPE_ENTITY,
    HENKA_HKS_TYPE_STRING
} henka_hks_value_type;

typedef enum henka_hks_diagnostic_code
{
    HENKA_HKS_DIAGNOSTIC_NONE = 0,
    HENKA_HKS_DIAGNOSTIC_INVALID_SOURCE,
    HENKA_HKS_DIAGNOSTIC_UNEXPECTED_TOKEN,
    HENKA_HKS_DIAGNOSTIC_INVALID_LITERAL,
    HENKA_HKS_DIAGNOSTIC_UNKNOWN_NAME,
    HENKA_HKS_DIAGNOSTIC_DUPLICATE_NAME,
    HENKA_HKS_DIAGNOSTIC_TYPE_MISMATCH,
    HENKA_HKS_DIAGNOSTIC_FORBIDDEN_KEYWORD,
    HENKA_HKS_DIAGNOSTIC_LIMIT
} henka_hks_diagnostic_code;

typedef struct henka_hks_diagnostic
{
    henka_hks_diagnostic_code code;
    size_t offset;
    uint32_t line;
    uint32_t column;
    char message[HENKA_HKS_MAX_DIAGNOSTIC_BYTES];
} henka_hks_diagnostic;

typedef struct henka_hks_binding_info
{
    char name[HENKA_HKS_MAX_IDENTIFIER_BYTES];
    henka_hks_value_type type;
    bool inferred;
} henka_hks_binding_info;

typedef struct henka_hks_callable_info
{
    char name[HENKA_HKS_MAX_IDENTIFIER_BYTES];
    bool behavior;
    size_t bytecode_offset;
    size_t bytecode_length;
    size_t local_count;
} henka_hks_callable_info;

typedef struct henka_hks_value
{
    henka_hks_value_type type;
    union
    {
        bool boolean;
        int32_t i32;
        uint32_t u32;
        float f32;
        henka_vec3 vec3;
        uint64_t entity;
    } as;
} henka_hks_value;

typedef enum henka_hks_execution_result
{
    HENKA_HKS_EXECUTION_COMPLETED = 0,
    HENKA_HKS_EXECUTION_INVALID_PROGRAM,
    HENKA_HKS_EXECUTION_STACK_OVERFLOW,
    HENKA_HKS_EXECUTION_STACK_UNDERFLOW,
    HENKA_HKS_EXECUTION_TYPE_ERROR,
    HENKA_HKS_EXECUTION_DIVIDE_BY_ZERO,
    HENKA_HKS_EXECUTION_UNSUPPORTED_VALUE,
    HENKA_HKS_EXECUTION_BUDGET_EXHAUSTED,
    HENKA_HKS_EXECUTION_HOST_ERROR
} henka_hks_execution_result;

typedef struct henka_hks_execution_report
{
    henka_hks_execution_result result;
    uint32_t instructions_executed;
    size_t stack_depth;
    henka_result host_error;
} henka_hks_execution_report;

typedef struct henka_hks_execution_context
{
    struct henka_script_host* host;
    uint64_t entity_id;
    uint64_t frame_index;
    uint64_t behavior_id;
    bool is_event;
    uint32_t event_id;
    uint64_t event_source_entity;
    uint64_t event_other_entity;
    uint32_t event_type;
    bool is_signal;
} henka_hks_execution_context;

henka_result henka_hks_lex(
    const char* source,
    size_t source_size,
    henka_hks_token* tokens,
    size_t token_capacity,
    size_t* out_token_count,
    henka_hks_diagnostic* out_diagnostic);

henka_result henka_hks_compile(
    const char* source,
    size_t source_size,
    henka_hks_program** out_program,
    henka_hks_diagnostic* out_diagnostic);

void henka_hks_program_destroy(henka_hks_program* program);
size_t henka_hks_program_get_binding_count(const henka_hks_program* program);
henka_result henka_hks_program_get_binding(
    const henka_hks_program* program,
    size_t index,
    henka_hks_binding_info* out_binding);
size_t henka_hks_program_get_callable_count(const henka_hks_program* program);
henka_result henka_hks_program_get_callable(
    const henka_hks_program* program,
    size_t index,
    henka_hks_callable_info* out_callable);
henka_result henka_hks_program_find_callable(
    const henka_hks_program* program,
    const char* name,
    size_t* out_index);
size_t henka_hks_program_get_ast_node_count(const henka_hks_program* program);

henka_hks_execution_result henka_hks_execute(
    const henka_hks_program* program,
    size_t callable_index,
    uint32_t instruction_budget,
    henka_hks_value* out_return_value,
    henka_hks_execution_report* out_report);

henka_hks_execution_result henka_hks_execute_with_context(
    const henka_hks_program* program,
    size_t callable_index,
    uint32_t instruction_budget,
    const henka_hks_execution_context* context,
    henka_hks_value* out_return_value,
    henka_hks_execution_report* out_report);

#endif
