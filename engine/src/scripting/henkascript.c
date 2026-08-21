#include <henka/henkascript.h>

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <henka/memory.h>
#include <henka/script.h>

typedef enum henka_hks_ast_node_kind
{
    HENKA_HKS_AST_DECLARATION = 0,
    HENKA_HKS_AST_CALLABLE,
    HENKA_HKS_AST_LITERAL,
    HENKA_HKS_AST_NAME,
    HENKA_HKS_AST_BINARY,
    HENKA_HKS_AST_ASSIGNMENT,
    HENKA_HKS_AST_RETURN,
    HENKA_HKS_AST_BLOCK,
    HENKA_HKS_AST_HOST_CALL
} henka_hks_ast_node_kind;

typedef struct henka_hks_ast_node
{
    henka_hks_ast_node_kind kind;
    henka_hks_value_type type;
} henka_hks_ast_node;

typedef enum henka_hks_opcode
{
    HENKA_HKS_OPCODE_PUSH_I32 = 0,
    HENKA_HKS_OPCODE_PUSH_F32,
    HENKA_HKS_OPCODE_PUSH_BOOL,
    HENKA_HKS_OPCODE_PUSH_UNSUPPORTED,
    HENKA_HKS_OPCODE_LOAD,
    HENKA_HKS_OPCODE_STORE,
    HENKA_HKS_OPCODE_ADD,
    HENKA_HKS_OPCODE_SUB,
    HENKA_HKS_OPCODE_MUL,
    HENKA_HKS_OPCODE_DIV,
    HENKA_HKS_OPCODE_NEG,
    HENKA_HKS_OPCODE_RETURN,
    HENKA_HKS_OPCODE_POP,
    HENKA_HKS_OPCODE_EMIT_EVENT
} henka_hks_opcode;

typedef struct henka_hks_instruction
{
    henka_hks_opcode opcode;
    henka_hks_value_type type;
    uint16_t slot;
    int32_t i32;
    float f32;
} henka_hks_instruction;

struct henka_hks_program
{
    size_t binding_count;
    henka_hks_binding_info bindings[HENKA_HKS_MAX_BINDINGS];
    size_t callable_count;
    henka_hks_callable_info callables[HENKA_HKS_MAX_CALLABLES];
    size_t ast_node_count;
    henka_hks_ast_node ast_nodes[HENKA_HKS_MAX_AST_NODES];
    size_t bytecode_count;
    henka_hks_instruction bytecode[HENKA_HKS_MAX_BYTECODE];
};

typedef struct henka_hks_parser_binding
{
    char name[HENKA_HKS_MAX_IDENTIFIER_BYTES];
    henka_hks_value_type type;
    uint16_t slot;
} henka_hks_parser_binding;

typedef struct henka_hks_parser
{
    const char* source;
    const henka_hks_token* tokens;
    size_t token_count;
    size_t index;
    size_t scope_mark;
    size_t binding_count;
    henka_hks_parser_binding bindings[HENKA_HKS_MAX_BINDINGS];
    henka_hks_program* program;
    henka_hks_diagnostic* diagnostic;
    bool in_callable;
    size_t current_callable_index;
    size_t callable_code_start;
} henka_hks_parser;

static void henka_hks_diagnostic_clear(henka_hks_diagnostic* diagnostic)
{
    if (diagnostic != NULL)
    {
        memset(diagnostic, 0, sizeof(*diagnostic));
    }
}

static henka_result henka_hks_fail(
    henka_hks_diagnostic* diagnostic,
    henka_hks_diagnostic_code code,
    const henka_hks_token* token,
    const char* format,
    ...)
{
    if (diagnostic != NULL)
    {
        va_list arguments;
        diagnostic->code = code;
        diagnostic->offset = token == NULL ? 0U : token->offset;
        diagnostic->line = token == NULL ? 0U : token->line;
        diagnostic->column = token == NULL ? 0U : token->column;
        va_start(arguments, format);
        (void)vsnprintf(
            diagnostic->message,
            sizeof(diagnostic->message),
            format,
            arguments);
        va_end(arguments);
    }
    return code == HENKA_HKS_DIAGNOSTIC_LIMIT
        ? HENKA_ERROR_LIMIT
        : HENKA_ERROR_INVALID_ARGUMENT;
}

static bool henka_hks_is_identifier_start(unsigned char character)
{
    return (character >= (unsigned char)'a' && character <= (unsigned char)'z') ||
        (character >= (unsigned char)'A' && character <= (unsigned char)'Z') ||
        character == (unsigned char)'_';
}

static bool henka_hks_is_identifier_continue(unsigned char character)
{
    return henka_hks_is_identifier_start(character) ||
        (character >= (unsigned char)'0' && character <= (unsigned char)'9');
}

static bool henka_hks_text_equals(
    const char* source,
    const henka_hks_token* token,
    const char* text)
{
    const size_t length = strlen(text);
    return source != NULL && token != NULL &&
        token->length == length &&
        memcmp(source + token->offset, text, length) == 0;
}

static henka_hks_token_kind henka_hks_keyword_kind(
    const char* source,
    size_t offset,
    size_t length)
{
    struct keyword
    {
        const char* text;
        henka_hks_token_kind kind;
    };
    static const struct keyword keywords[] =
    {
        {"bool", HENKA_HKS_TOKEN_KW_BOOL},
        {"i32", HENKA_HKS_TOKEN_KW_I32},
        {"u32", HENKA_HKS_TOKEN_KW_U32},
        {"f32", HENKA_HKS_TOKEN_KW_F32},
        {"entity", HENKA_HKS_TOKEN_KW_ENTITY},
        {"fn", HENKA_HKS_TOKEN_KW_FN},
        {"behavior", HENKA_HKS_TOKEN_KW_BEHAVIOR},
        {"return", HENKA_HKS_TOKEN_KW_RETURN},
        {"emit", HENKA_HKS_TOKEN_KW_EMIT},
        {"true", HENKA_HKS_TOKEN_KW_TRUE},
        {"false", HENKA_HKS_TOKEN_KW_FALSE},
        {"let", HENKA_HKS_TOKEN_KW_LET},
        {"var", HENKA_HKS_TOKEN_KW_VAR}
    };
    size_t index;
    for (index = 0U; index < sizeof(keywords) / sizeof(keywords[0]); ++index)
    {
        const size_t keyword_length = strlen(keywords[index].text);
        if (length == keyword_length &&
            memcmp(source + offset, keywords[index].text, length) == 0)
        {
            return keywords[index].kind;
        }
    }
    return HENKA_HKS_TOKEN_IDENTIFIER;
}

henka_result henka_hks_lex(
    const char* source,
    size_t source_size,
    henka_hks_token* tokens,
    size_t token_capacity,
    size_t* out_token_count,
    henka_hks_diagnostic* out_diagnostic)
{
    size_t offset = 0U;
    size_t token_count = 0U;
    uint32_t line = 1U;
    uint32_t column = 1U;

    henka_hks_diagnostic_clear(out_diagnostic);
    if ((source == NULL && source_size != 0U) || tokens == NULL ||
        token_capacity == 0U || out_token_count == NULL)
    {
        return henka_hks_fail(
            out_diagnostic,
            HENKA_HKS_DIAGNOSTIC_INVALID_SOURCE,
            NULL,
            "source and token output are required");
    }
    *out_token_count = 0U;
    if (source_size > HENKA_HKS_MAX_SOURCE_BYTES)
    {
        return henka_hks_fail(
            out_diagnostic,
            HENKA_HKS_DIAGNOSTIC_LIMIT,
            NULL,
            "source exceeds the %u-byte limit",
            HENKA_HKS_MAX_SOURCE_BYTES);
    }
    while (offset < source_size)
    {
        const unsigned char character = (unsigned char)source[offset];
        henka_hks_token token;
        if (isspace(character) != 0)
        {
            if (character == (unsigned char)'\n')
            {
                ++line;
                column = 1U;
            }
            else
            {
                ++column;
            }
            ++offset;
            continue;
        }
        if (character == (unsigned char)'/' && offset + 1U < source_size &&
            source[offset + 1U] == '/')
        {
            offset += 2U;
            column += 2U;
            while (offset < source_size && source[offset] != '\n')
            {
                ++offset;
                ++column;
            }
            continue;
        }
        if (token_count >= token_capacity - 1U || token_count >= HENKA_HKS_MAX_TOKENS - 1U)
        {
            return henka_hks_fail(
                out_diagnostic,
                HENKA_HKS_DIAGNOSTIC_LIMIT,
                NULL,
                "token stream exceeds the bounded token limit");
        }
        token = (henka_hks_token){
            HENKA_HKS_TOKEN_EOF,
            offset,
            1U,
            line,
            column};
        if (henka_hks_is_identifier_start(character))
        {
            const size_t start = offset;
            while (offset < source_size &&
                   henka_hks_is_identifier_continue((unsigned char)source[offset]))
            {
                ++offset;
                ++column;
            }
            token.length = offset - start;
            token.kind = henka_hks_keyword_kind(source, start, token.length);
        }
        else if (character >= (unsigned char)'0' && character <= (unsigned char)'9')
        {
            bool floating = false;
            const size_t start = offset;
            while (offset < source_size && isdigit((unsigned char)source[offset]) != 0)
            {
                ++offset;
                ++column;
            }
            if (offset < source_size && source[offset] == '.')
            {
                floating = true;
                ++offset;
                ++column;
                if (offset >= source_size || isdigit((unsigned char)source[offset]) == 0)
                {
                    return henka_hks_fail(
                        out_diagnostic,
                        HENKA_HKS_DIAGNOSTIC_INVALID_LITERAL,
                        &token,
                        "a floating literal requires digits after the decimal point");
                }
                while (offset < source_size && isdigit((unsigned char)source[offset]) != 0)
                {
                    ++offset;
                    ++column;
                }
            }
            token.length = offset - start;
            token.kind = floating ? HENKA_HKS_TOKEN_FLOAT : HENKA_HKS_TOKEN_INTEGER;
        }
        else if (character == (unsigned char)'"')
        {
            const size_t start = offset++;
            ++column;
            while (offset < source_size && source[offset] != '"')
            {
                if (source[offset] == '\n')
                {
                    return henka_hks_fail(
                        out_diagnostic,
                        HENKA_HKS_DIAGNOSTIC_INVALID_LITERAL,
                        &token,
                        "string literals cannot contain an unescaped newline");
                }
                if (source[offset] == '\\')
                {
                    if (offset + 1U >= source_size ||
                        strchr("\\\"nrt", source[offset + 1U]) == NULL)
                    {
                        return henka_hks_fail(
                            out_diagnostic,
                            HENKA_HKS_DIAGNOSTIC_INVALID_LITERAL,
                            &token,
                            "unsupported string escape");
                    }
                    offset += 2U;
                    column += 2U;
                }
                else
                {
                    ++offset;
                    ++column;
                }
            }
            if (offset >= source_size)
            {
                return henka_hks_fail(
                    out_diagnostic,
                    HENKA_HKS_DIAGNOSTIC_INVALID_LITERAL,
                    &token,
                    "unterminated string literal");
            }
            ++offset;
            ++column;
            token.length = offset - start;
            token.kind = HENKA_HKS_TOKEN_STRING;
        }
        else
        {
            ++offset;
            ++column;
            switch (character)
            {
                case '{': token.kind = HENKA_HKS_TOKEN_LBRACE; break;
                case '}': token.kind = HENKA_HKS_TOKEN_RBRACE; break;
                case '(': token.kind = HENKA_HKS_TOKEN_LPAREN; break;
                case ')': token.kind = HENKA_HKS_TOKEN_RPAREN; break;
                case ';': token.kind = HENKA_HKS_TOKEN_SEMICOLON; break;
                case ',': token.kind = HENKA_HKS_TOKEN_COMMA; break;
                case '=': token.kind = HENKA_HKS_TOKEN_ASSIGN; break;
                case '+': token.kind = HENKA_HKS_TOKEN_PLUS; break;
                case '-': token.kind = HENKA_HKS_TOKEN_MINUS; break;
                case '*': token.kind = HENKA_HKS_TOKEN_STAR; break;
                case '/': token.kind = HENKA_HKS_TOKEN_SLASH; break;
                case ':':
                    if (offset >= source_size || source[offset] != '=')
                    {
                        return henka_hks_fail(
                            out_diagnostic,
                            HENKA_HKS_DIAGNOSTIC_UNEXPECTED_TOKEN,
                            &token,
                            "':' must be followed by '=' for inferred declarations");
                    }
                    ++offset;
                    ++column;
                    token.length = 2U;
                    token.kind = HENKA_HKS_TOKEN_INFER;
                    break;
                default:
                    return henka_hks_fail(
                        out_diagnostic,
                        HENKA_HKS_DIAGNOSTIC_UNEXPECTED_TOKEN,
                        &token,
                        "unsupported character '%c'",
                        character);
            }
        }
        tokens[token_count++] = token;
    }
    if (token_count >= token_capacity)
    {
        return henka_hks_fail(
            out_diagnostic,
            HENKA_HKS_DIAGNOSTIC_LIMIT,
            NULL,
            "token output capacity is exhausted");
    }
    tokens[token_count++] = (henka_hks_token){
        HENKA_HKS_TOKEN_EOF,
        source_size,
        0U,
        line,
        column};
    *out_token_count = token_count;
    return HENKA_SUCCESS;
}

static const henka_hks_token* henka_hks_current(const henka_hks_parser* parser)
{
    return parser == NULL || parser->index >= parser->token_count
        ? NULL
        : &parser->tokens[parser->index];
}

static bool henka_hks_accept(henka_hks_parser* parser, henka_hks_token_kind kind)
{
    if (henka_hks_current(parser) != NULL && henka_hks_current(parser)->kind == kind)
    {
        ++parser->index;
        return true;
    }
    return false;
}

static henka_result henka_hks_expect(
    henka_hks_parser* parser,
    henka_hks_token_kind kind,
    const char* message)
{
    if (henka_hks_accept(parser, kind))
    {
        return HENKA_SUCCESS;
    }
    return henka_hks_fail(
        parser->diagnostic,
        HENKA_HKS_DIAGNOSTIC_UNEXPECTED_TOKEN,
        henka_hks_current(parser),
        "%s",
        message);
}

static bool henka_hks_copy_name(
    char* destination,
    size_t destination_capacity,
    const char* source,
    const henka_hks_token* token)
{
    if (destination == NULL || source == NULL || token == NULL ||
        token->length == 0U || token->length >= destination_capacity)
    {
        return false;
    }
    memcpy(destination, source + token->offset, token->length);
    destination[token->length] = '\0';
    return true;
}

static henka_hks_value_type henka_hks_type_from_token(henka_hks_token_kind kind)
{
    switch (kind)
    {
        case HENKA_HKS_TOKEN_KW_BOOL: return HENKA_HKS_TYPE_BOOL;
        case HENKA_HKS_TOKEN_KW_I32: return HENKA_HKS_TYPE_I32;
        case HENKA_HKS_TOKEN_KW_U32: return HENKA_HKS_TYPE_U32;
        case HENKA_HKS_TOKEN_KW_F32: return HENKA_HKS_TYPE_F32;
        case HENKA_HKS_TOKEN_KW_ENTITY: return HENKA_HKS_TYPE_ENTITY;
        default: return HENKA_HKS_TYPE_UNKNOWN;
    }
}

static bool henka_hks_is_type_token(henka_hks_token_kind kind)
{
    return henka_hks_type_from_token(kind) != HENKA_HKS_TYPE_UNKNOWN;
}

static bool henka_hks_types_compatible(
    henka_hks_value_type expected,
    henka_hks_value_type actual)
{
    return expected == actual;
}

static henka_result henka_hks_add_node(
    henka_hks_parser* parser,
    henka_hks_ast_node_kind kind,
    henka_hks_value_type type)
{
    if (parser->program->ast_node_count >= HENKA_HKS_MAX_AST_NODES)
    {
        return henka_hks_fail(
            parser->diagnostic,
            HENKA_HKS_DIAGNOSTIC_LIMIT,
            henka_hks_current(parser),
            "AST node limit is exhausted");
    }
    parser->program->ast_nodes[parser->program->ast_node_count++] = (henka_hks_ast_node){kind, type};
    return HENKA_SUCCESS;
}

static henka_result henka_hks_emit(
    henka_hks_parser* parser,
    henka_hks_opcode opcode,
    henka_hks_value_type type,
    uint16_t slot,
    int32_t i32,
    float f32)
{
    if (!parser->in_callable)
    {
        return HENKA_SUCCESS;
    }
    if (parser->program->bytecode_count >= HENKA_HKS_MAX_BYTECODE)
    {
        return henka_hks_fail(
            parser->diagnostic,
            HENKA_HKS_DIAGNOSTIC_LIMIT,
            henka_hks_current(parser),
            "bytecode limit is exhausted");
    }
    parser->program->bytecode[parser->program->bytecode_count++] =
        (henka_hks_instruction){opcode, type, slot, i32, f32};
    return HENKA_SUCCESS;
}

static const henka_hks_parser_binding* henka_hks_lookup_binding(
    const henka_hks_parser* parser,
    const henka_hks_token* token)
{
    size_t index;
    for (index = parser->binding_count; index > 0U; --index)
    {
        const henka_hks_parser_binding* binding = &parser->bindings[index - 1U];
        if (henka_hks_text_equals(parser->source, token, binding->name))
        {
            return binding;
        }
    }
    return NULL;
}

static henka_hks_value_type henka_hks_lookup(
    const henka_hks_parser* parser,
    const henka_hks_token* token)
{
    const henka_hks_parser_binding* binding = henka_hks_lookup_binding(parser, token);
    return binding == NULL ? HENKA_HKS_TYPE_UNKNOWN : binding->type;
}

static henka_result henka_hks_add_binding(
    henka_hks_parser* parser,
    const henka_hks_token* name_token,
    henka_hks_value_type type,
    bool inferred,
    bool public_binding)
{
    henka_hks_parser_binding binding;
    size_t index;
    if (!henka_hks_copy_name(
            binding.name,
            sizeof(binding.name),
            parser->source,
            name_token))
    {
        return henka_hks_fail(
            parser->diagnostic,
            HENKA_HKS_DIAGNOSTIC_INVALID_SOURCE,
            name_token,
            "identifier exceeds the %u-byte limit",
            HENKA_HKS_MAX_IDENTIFIER_BYTES - 1U);
    }
    for (index = parser->scope_mark; index < parser->binding_count; ++index)
    {
        if (strcmp(parser->bindings[index].name, binding.name) == 0)
        {
            return henka_hks_fail(
                parser->diagnostic,
                HENKA_HKS_DIAGNOSTIC_DUPLICATE_NAME,
                name_token,
                "duplicate name '%s' in the current scope",
                binding.name);
        }
    }
    if (parser->binding_count >= HENKA_HKS_MAX_BINDINGS)
    {
        return henka_hks_fail(
            parser->diagnostic,
            HENKA_HKS_DIAGNOSTIC_LIMIT,
            name_token,
            "binding limit is exhausted");
    }
    binding.type = type;
    binding.slot = (uint16_t)parser->binding_count;
    parser->bindings[parser->binding_count++] = binding;
    if (public_binding)
    {
        if (parser->program->binding_count >= HENKA_HKS_MAX_BINDINGS)
        {
            return henka_hks_fail(
                parser->diagnostic,
                HENKA_HKS_DIAGNOSTIC_LIMIT,
                name_token,
                "public binding limit is exhausted");
        }
        (void)snprintf(
            parser->program->bindings[parser->program->binding_count].name,
            sizeof(parser->program->bindings[parser->program->binding_count].name),
            "%s",
            binding.name);
        parser->program->bindings[parser->program->binding_count].type = type;
        parser->program->bindings[parser->program->binding_count].inferred = inferred;
        ++parser->program->binding_count;
    }
    return henka_hks_add_node(parser, HENKA_HKS_AST_DECLARATION, type);
}

static henka_result henka_hks_parse_expression(
    henka_hks_parser* parser,
    henka_hks_value_type* out_type);

static int henka_hks_precedence(henka_hks_token_kind kind)
{
    return kind == HENKA_HKS_TOKEN_STAR || kind == HENKA_HKS_TOKEN_SLASH
        ? 2
        : kind == HENKA_HKS_TOKEN_PLUS || kind == HENKA_HKS_TOKEN_MINUS
            ? 1
            : 0;
}

static bool henka_hks_is_numeric(henka_hks_value_type type)
{
    return type == HENKA_HKS_TYPE_I32 || type == HENKA_HKS_TYPE_U32 ||
        type == HENKA_HKS_TYPE_F32;
}

static bool henka_hks_parse_i32(
    const char* source,
    const henka_hks_token* token,
    int32_t* out_value)
{
    char text[32];
    char* end = NULL;
    long value;
    if (token->length == 0U || token->length >= sizeof(text))
    {
        return false;
    }
    memcpy(text, source + token->offset, token->length);
    text[token->length] = '\0';
    errno = 0;
    value = strtol(text, &end, 10);
    if (errno == ERANGE || end == text || *end != '\0' ||
        value < (long)INT32_MIN || value > (long)INT32_MAX)
    {
        return false;
    }
    *out_value = (int32_t)value;
    return true;
}

static bool henka_hks_parse_f32(
    const char* source,
    const henka_hks_token* token,
    float* out_value)
{
    char text[48];
    char* end = NULL;
    float value;
    if (token->length == 0U || token->length >= sizeof(text))
    {
        return false;
    }
    memcpy(text, source + token->offset, token->length);
    text[token->length] = '\0';
    errno = 0;
    value = strtof(text, &end);
    if (errno == ERANGE || end == text || *end != '\0' || !isfinite(value))
    {
        return false;
    }
    *out_value = value;
    return true;
}

static henka_result henka_hks_parse_primary(
    henka_hks_parser* parser,
    henka_hks_value_type* out_type)
{
    const henka_hks_token* token = henka_hks_current(parser);
    henka_hks_value_type type;
    if (token == NULL || out_type == NULL)
    {
        return henka_hks_fail(
            parser->diagnostic,
            HENKA_HKS_DIAGNOSTIC_UNEXPECTED_TOKEN,
            token,
            "expression is incomplete");
    }
    if (henka_hks_accept(parser, HENKA_HKS_TOKEN_LPAREN))
    {
        henka_result result = henka_hks_parse_expression(parser, out_type);
        if (result != HENKA_SUCCESS)
        {
            return result;
        }
        return henka_hks_expect(parser, HENKA_HKS_TOKEN_RPAREN, "expected ')' after expression");
    }
    if (henka_hks_accept(parser, HENKA_HKS_TOKEN_MINUS))
    {
        henka_result result = henka_hks_parse_primary(parser, &type);
        if (result != HENKA_SUCCESS || !henka_hks_is_numeric(type))
        {
            return result == HENKA_SUCCESS
                ? henka_hks_fail(
                    parser->diagnostic,
                    HENKA_HKS_DIAGNOSTIC_TYPE_MISMATCH,
                    token,
                    "unary '-' requires a numeric value")
                : result;
        }
        *out_type = type;
        result = henka_hks_add_node(parser, HENKA_HKS_AST_BINARY, type);
        if (result != HENKA_SUCCESS)
        {
            return result;
        }
        return henka_hks_emit(parser, HENKA_HKS_OPCODE_NEG, type, 0U, 0, 0.0F);
    }
    ++parser->index;
    switch (token->kind)
    {
        case HENKA_HKS_TOKEN_INTEGER:
            {
                int32_t value;
                if (!henka_hks_parse_i32(parser->source, token, &value))
                {
                    return henka_hks_fail(
                        parser->diagnostic,
                        HENKA_HKS_DIAGNOSTIC_INVALID_LITERAL,
                        token,
                        "integer literal is outside the i32 range");
                }
                if (henka_hks_emit(parser, HENKA_HKS_OPCODE_PUSH_I32, HENKA_HKS_TYPE_I32, 0U, value, 0.0F) != HENKA_SUCCESS)
                {
                    return HENKA_ERROR_LIMIT;
                }
            }
            *out_type = HENKA_HKS_TYPE_I32;
            break;
        case HENKA_HKS_TOKEN_FLOAT:
            {
                float value;
                if (!henka_hks_parse_f32(parser->source, token, &value))
                {
                    return henka_hks_fail(
                        parser->diagnostic,
                        HENKA_HKS_DIAGNOSTIC_INVALID_LITERAL,
                        token,
                        "floating literal is outside the finite f32 range");
                }
                if (henka_hks_emit(parser, HENKA_HKS_OPCODE_PUSH_F32, HENKA_HKS_TYPE_F32, 0U, 0, value) != HENKA_SUCCESS)
                {
                    return HENKA_ERROR_LIMIT;
                }
            }
            *out_type = HENKA_HKS_TYPE_F32;
            break;
        case HENKA_HKS_TOKEN_STRING:
            if (henka_hks_emit(parser, HENKA_HKS_OPCODE_PUSH_UNSUPPORTED, HENKA_HKS_TYPE_STRING, 0U, 0, 0.0F) != HENKA_SUCCESS)
            {
                return HENKA_ERROR_LIMIT;
            }
            *out_type = HENKA_HKS_TYPE_STRING;
            break;
        case HENKA_HKS_TOKEN_KW_TRUE:
        case HENKA_HKS_TOKEN_KW_FALSE:
            if (henka_hks_emit(
                    parser,
                    HENKA_HKS_OPCODE_PUSH_BOOL,
                    HENKA_HKS_TYPE_BOOL,
                    0U,
                    token->kind == HENKA_HKS_TOKEN_KW_TRUE ? 1 : 0,
                    0.0F) != HENKA_SUCCESS)
            {
                return HENKA_ERROR_LIMIT;
            }
            *out_type = HENKA_HKS_TYPE_BOOL;
            break;
        case HENKA_HKS_TOKEN_IDENTIFIER:
            *out_type = henka_hks_lookup(parser, token);
            if (*out_type == HENKA_HKS_TYPE_UNKNOWN)
            {
                return henka_hks_fail(
                    parser->diagnostic,
                    HENKA_HKS_DIAGNOSTIC_UNKNOWN_NAME,
                    token,
                    "unknown name '%.*s'",
                    (int)token->length,
                    parser->source + token->offset);
            }
            {
                const henka_hks_parser_binding* binding = henka_hks_lookup_binding(parser, token);
                if (henka_hks_emit(parser, HENKA_HKS_OPCODE_LOAD, binding->type, binding->slot, 0, 0.0F) != HENKA_SUCCESS)
                {
                    return HENKA_ERROR_LIMIT;
                }
            }
            break;
        default:
            return henka_hks_fail(
                parser->diagnostic,
                HENKA_HKS_DIAGNOSTIC_UNEXPECTED_TOKEN,
                token,
                "expected a literal, name, or parenthesized expression");
    }
    return henka_hks_add_node(parser, HENKA_HKS_AST_LITERAL, *out_type);
}

static henka_result henka_hks_parse_binary(
    henka_hks_parser* parser,
    int minimum_precedence,
    henka_hks_value_type* out_type)
{
    henka_hks_value_type left_type;
    henka_result result = henka_hks_parse_primary(parser, &left_type);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    for (;;)
    {
        const henka_hks_token* operator_token = henka_hks_current(parser);
        const int precedence = operator_token == NULL
            ? 0
            : henka_hks_precedence(operator_token->kind);
        henka_hks_value_type right_type;
        if (precedence < minimum_precedence || precedence == 0)
        {
            break;
        }
        ++parser->index;
        result = henka_hks_parse_binary(parser, precedence + 1, &right_type);
        if (result != HENKA_SUCCESS)
        {
            return result;
        }
        if (!henka_hks_is_numeric(left_type) || !henka_hks_is_numeric(right_type) ||
            left_type != right_type)
        {
            return henka_hks_fail(
                parser->diagnostic,
                HENKA_HKS_DIAGNOSTIC_TYPE_MISMATCH,
                operator_token,
                "arithmetic operands must have the same numeric type");
        }
        result = henka_hks_add_node(parser, HENKA_HKS_AST_BINARY, left_type);
        if (result != HENKA_SUCCESS)
        {
            return result;
        }
        {
            henka_hks_opcode opcode;
            switch (operator_token->kind)
            {
                case HENKA_HKS_TOKEN_PLUS: opcode = HENKA_HKS_OPCODE_ADD; break;
                case HENKA_HKS_TOKEN_MINUS: opcode = HENKA_HKS_OPCODE_SUB; break;
                case HENKA_HKS_TOKEN_STAR: opcode = HENKA_HKS_OPCODE_MUL; break;
                case HENKA_HKS_TOKEN_SLASH: opcode = HENKA_HKS_OPCODE_DIV; break;
                default: return HENKA_ERROR_INVALID_ARGUMENT;
            }
            result = henka_hks_emit(parser, opcode, left_type, 0U, 0, 0.0F);
            if (result != HENKA_SUCCESS)
            {
                return result;
            }
        }
    }
    *out_type = left_type;
    return HENKA_SUCCESS;
}

static henka_result henka_hks_parse_expression(
    henka_hks_parser* parser,
    henka_hks_value_type* out_type)
{
    return henka_hks_parse_binary(parser, 1, out_type);
}

static henka_result henka_hks_parse_declaration(
    henka_hks_parser* parser,
    bool public_binding)
{
    const henka_hks_token* first = henka_hks_current(parser);
    const henka_hks_token* name_token;
    henka_hks_value_type declared_type = HENKA_HKS_TYPE_UNKNOWN;
    henka_hks_value_type expression_type;
    bool inferred = false;
    if (first == NULL)
    {
        return henka_hks_fail(
            parser->diagnostic,
            HENKA_HKS_DIAGNOSTIC_UNEXPECTED_TOKEN,
            first,
            "declaration is incomplete");
    }
    if (henka_hks_is_type_token(first->kind))
    {
        declared_type = henka_hks_type_from_token(first->kind);
        ++parser->index;
        name_token = henka_hks_current(parser);
        if (name_token == NULL || name_token->kind != HENKA_HKS_TOKEN_IDENTIFIER)
        {
            return henka_hks_fail(
                parser->diagnostic,
                HENKA_HKS_DIAGNOSTIC_UNEXPECTED_TOKEN,
                name_token,
                "expected an identifier after an explicit type");
        }
        ++parser->index;
        if (henka_hks_expect(parser, HENKA_HKS_TOKEN_ASSIGN, "explicit declarations require '='") != HENKA_SUCCESS)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
    }
    else if (first->kind == HENKA_HKS_TOKEN_IDENTIFIER)
    {
        name_token = first;
        ++parser->index;
        if (henka_hks_expect(parser, HENKA_HKS_TOKEN_INFER, "inferred declarations require ':='") != HENKA_SUCCESS)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        inferred = true;
    }
    else
    {
        return henka_hks_fail(
            parser->diagnostic,
            HENKA_HKS_DIAGNOSTIC_UNEXPECTED_TOKEN,
            first,
            "expected a typed declaration or ':=' inferred declaration");
    }
    if (henka_hks_parse_expression(parser, &expression_type) != HENKA_SUCCESS ||
        (!inferred && !henka_hks_types_compatible(declared_type, expression_type)))
    {
        if (parser->diagnostic != NULL && parser->diagnostic->code != HENKA_HKS_DIAGNOSTIC_NONE)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        return henka_hks_fail(
            parser->diagnostic,
            HENKA_HKS_DIAGNOSTIC_TYPE_MISMATCH,
            henka_hks_current(parser),
            "initializer type does not match the declaration");
    }
    if (inferred)
    {
        declared_type = expression_type;
    }
    if (henka_hks_expect(parser, HENKA_HKS_TOKEN_SEMICOLON, "declarations require a semicolon") != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    {
        henka_result result = henka_hks_add_binding(parser, name_token, declared_type, inferred, public_binding);
        if (result != HENKA_SUCCESS)
        {
            return result;
        }
        return henka_hks_emit(
            parser,
            HENKA_HKS_OPCODE_STORE,
            declared_type,
            parser->bindings[parser->binding_count - 1U].slot,
            0,
            0.0F);
    }
}

static henka_result henka_hks_parse_statement(henka_hks_parser* parser)
{
    const henka_hks_token* token = henka_hks_current(parser);
    henka_hks_value_type expression_type = HENKA_HKS_TYPE_UNKNOWN;
    if (token == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (token->kind == HENKA_HKS_TOKEN_LBRACE)
    {
        const size_t previous_scope = parser->scope_mark;
        parser->scope_mark = parser->binding_count;
        ++parser->index;
        while (henka_hks_current(parser) != NULL &&
               henka_hks_current(parser)->kind != HENKA_HKS_TOKEN_RBRACE)
        {
            if (henka_hks_parse_statement(parser) != HENKA_SUCCESS)
            {
                return HENKA_ERROR_INVALID_ARGUMENT;
            }
        }
        if (henka_hks_expect(parser, HENKA_HKS_TOKEN_RBRACE, "expected '}' to close block") != HENKA_SUCCESS)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        parser->binding_count = parser->scope_mark;
        parser->scope_mark = previous_scope;
        return henka_hks_add_node(parser, HENKA_HKS_AST_BLOCK, HENKA_HKS_TYPE_VOID);
    }
    if (token->kind == HENKA_HKS_TOKEN_KW_RETURN)
    {
        bool has_value = false;
        ++parser->index;
        if (!henka_hks_accept(parser, HENKA_HKS_TOKEN_SEMICOLON))
        {
            if (henka_hks_parse_expression(parser, &expression_type) != HENKA_SUCCESS ||
                henka_hks_expect(parser, HENKA_HKS_TOKEN_SEMICOLON, "return requires a semicolon") != HENKA_SUCCESS)
            {
                return HENKA_ERROR_INVALID_ARGUMENT;
            }
            has_value = true;
        }
        if (henka_hks_add_node(parser, HENKA_HKS_AST_RETURN, has_value ? expression_type : HENKA_HKS_TYPE_VOID) != HENKA_SUCCESS)
        {
            return HENKA_ERROR_LIMIT;
        }
        return henka_hks_emit(
            parser,
            HENKA_HKS_OPCODE_RETURN,
            has_value ? expression_type : HENKA_HKS_TYPE_VOID,
            0U,
            has_value ? 1 : 0,
            0.0F);
    }
    if (token->kind == HENKA_HKS_TOKEN_KW_EMIT)
    {
        ++parser->index;
        if (henka_hks_expect(
                parser,
                HENKA_HKS_TOKEN_LPAREN,
                "emit requires '('") != HENKA_SUCCESS ||
            henka_hks_parse_expression(parser, &expression_type) != HENKA_SUCCESS ||
            expression_type != HENKA_HKS_TYPE_I32 ||
            henka_hks_expect(
                parser,
                HENKA_HKS_TOKEN_RPAREN,
                "emit requires a single i32 event ID") != HENKA_SUCCESS ||
            henka_hks_expect(
                parser,
                HENKA_HKS_TOKEN_SEMICOLON,
                "emit requires a semicolon") != HENKA_SUCCESS)
        {
            if (expression_type != HENKA_HKS_TYPE_I32 &&
                parser->diagnostic != NULL &&
                parser->diagnostic->code == HENKA_HKS_DIAGNOSTIC_NONE)
            {
                (void)henka_hks_fail(
                    parser->diagnostic,
                    HENKA_HKS_DIAGNOSTIC_TYPE_MISMATCH,
                    token,
                    "emit event ID must be an i32 expression");
            }
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        if (henka_hks_add_node(
                parser,
                HENKA_HKS_AST_HOST_CALL,
                HENKA_HKS_TYPE_VOID) != HENKA_SUCCESS)
        {
            return HENKA_ERROR_LIMIT;
        }
        return henka_hks_emit(
            parser,
            HENKA_HKS_OPCODE_EMIT_EVENT,
            HENKA_HKS_TYPE_I32,
            0U,
            0,
            0.0F);
    }
    if (henka_hks_is_type_token(token->kind))
    {
        return henka_hks_parse_declaration(parser, false);
    }
    if (token->kind == HENKA_HKS_TOKEN_IDENTIFIER)
    {
        const henka_hks_token* name_token = token;
        ++parser->index;
        if (henka_hks_accept(parser, HENKA_HKS_TOKEN_INFER))
        {
            --parser->index;
            return henka_hks_parse_declaration(parser, false);
        }
        if (henka_hks_accept(parser, HENKA_HKS_TOKEN_ASSIGN))
        {
            const henka_hks_parser_binding* target_binding = henka_hks_lookup_binding(parser, name_token);
            const henka_hks_value_type target_type = target_binding == NULL
                ? HENKA_HKS_TYPE_UNKNOWN
                : target_binding->type;
            henka_result result;
            if (target_type == HENKA_HKS_TYPE_UNKNOWN)
            {
                return henka_hks_fail(
                    parser->diagnostic,
                    HENKA_HKS_DIAGNOSTIC_UNKNOWN_NAME,
                    name_token,
                    "unknown assignment target '%.*s'",
                    (int)name_token->length,
                    parser->source + name_token->offset);
            }
            result = henka_hks_parse_expression(parser, &expression_type);
            if (result != HENKA_SUCCESS)
            {
                return result;
            }
            if (!henka_hks_types_compatible(target_type, expression_type))
            {
                return henka_hks_fail(
                    parser->diagnostic,
                    HENKA_HKS_DIAGNOSTIC_TYPE_MISMATCH,
                    name_token,
                    "assignment target and value types do not match");
            }
            if (henka_hks_expect(parser, HENKA_HKS_TOKEN_SEMICOLON, "assignments require a semicolon") != HENKA_SUCCESS)
            {
                return HENKA_ERROR_INVALID_ARGUMENT;
            }
            result = henka_hks_add_node(parser, HENKA_HKS_AST_ASSIGNMENT, target_type);
            if (result != HENKA_SUCCESS)
            {
                return result;
            }
            return henka_hks_emit(
                parser,
                HENKA_HKS_OPCODE_STORE,
                target_type,
                target_binding->slot,
                0,
                0.0F);
        }
        return henka_hks_fail(
            parser->diagnostic,
            HENKA_HKS_DIAGNOSTIC_UNEXPECTED_TOKEN,
            henka_hks_current(parser),
            "expected ':=' or '=' after identifier");
    }
    if (token->kind == HENKA_HKS_TOKEN_KW_LET || token->kind == HENKA_HKS_TOKEN_KW_VAR)
    {
        return henka_hks_fail(
            parser->diagnostic,
            HENKA_HKS_DIAGNOSTIC_FORBIDDEN_KEYWORD,
            token,
            "'%.*s' is not part of HenkaScript; use an explicit type or ':='",
            (int)token->length,
            parser->source + token->offset);
    }
    return henka_hks_fail(
        parser->diagnostic,
        HENKA_HKS_DIAGNOSTIC_UNEXPECTED_TOKEN,
        token,
        "unexpected token in block");
}

static henka_result henka_hks_parse_callable(henka_hks_parser* parser)
{
    const henka_hks_token* kind_token = henka_hks_current(parser);
    const bool behavior = kind_token->kind == HENKA_HKS_TOKEN_KW_BEHAVIOR;
    const henka_hks_token* name_token;
    henka_hks_callable_info callable = {0};
    const size_t previous_scope = parser->scope_mark;
    ++parser->index;
    name_token = henka_hks_current(parser);
    if (name_token == NULL || name_token->kind != HENKA_HKS_TOKEN_IDENTIFIER ||
        !henka_hks_copy_name(callable.name, sizeof(callable.name), parser->source, name_token))
    {
        return henka_hks_fail(
            parser->diagnostic,
            HENKA_HKS_DIAGNOSTIC_UNEXPECTED_TOKEN,
            name_token,
            "callable requires a bounded identifier");
    }
    {
        size_t index;
        for (index = 0U; index < parser->program->callable_count; ++index)
        {
            if (strcmp(parser->program->callables[index].name, callable.name) == 0)
            {
                return henka_hks_fail(
                    parser->diagnostic,
                    HENKA_HKS_DIAGNOSTIC_DUPLICATE_NAME,
                    name_token,
                    "duplicate callable '%s'",
                    callable.name);
            }
        }
    }
    ++parser->index;
    if (henka_hks_expect(parser, HENKA_HKS_TOKEN_LPAREN, "callable requires '('") != HENKA_SUCCESS ||
        henka_hks_expect(parser, HENKA_HKS_TOKEN_RPAREN, "V1 callables do not accept parameters") != HENKA_SUCCESS ||
        henka_hks_expect(parser, HENKA_HKS_TOKEN_LBRACE, "callable requires a brace-delimited body") != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (parser->program->callable_count >= HENKA_HKS_MAX_CALLABLES)
    {
        return henka_hks_fail(
            parser->diagnostic,
            HENKA_HKS_DIAGNOSTIC_LIMIT,
            name_token,
            "callable limit is exhausted");
    }
    callable.behavior = behavior;
    callable.bytecode_offset = parser->program->bytecode_count;
    parser->program->callables[parser->program->callable_count++] = callable;
    if (henka_hks_add_node(parser, HENKA_HKS_AST_CALLABLE, HENKA_HKS_TYPE_VOID) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_LIMIT;
    }
    parser->scope_mark = parser->binding_count;
    parser->in_callable = true;
    parser->current_callable_index = parser->program->callable_count - 1U;
    parser->callable_code_start = callable.bytecode_offset;
    while (henka_hks_current(parser) != NULL &&
           henka_hks_current(parser)->kind != HENKA_HKS_TOKEN_RBRACE)
    {
        if (henka_hks_parse_statement(parser) != HENKA_SUCCESS)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
    }
    if (henka_hks_expect(parser, HENKA_HKS_TOKEN_RBRACE, "expected '}' to close callable") != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (henka_hks_emit(parser, HENKA_HKS_OPCODE_RETURN, HENKA_HKS_TYPE_VOID, 0U, 0, 0.0F) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_LIMIT;
    }
    parser->program->callables[parser->current_callable_index].bytecode_offset = parser->callable_code_start;
    parser->program->callables[parser->current_callable_index].bytecode_length =
        parser->program->bytecode_count - parser->callable_code_start;
    parser->program->callables[parser->current_callable_index].local_count =
        parser->binding_count - parser->scope_mark;
    parser->binding_count = parser->scope_mark;
    parser->scope_mark = previous_scope;
    parser->in_callable = false;
    return HENKA_SUCCESS;
}

static henka_result henka_hks_parse_program(henka_hks_parser* parser)
{
    while (henka_hks_current(parser) != NULL &&
           henka_hks_current(parser)->kind != HENKA_HKS_TOKEN_EOF)
    {
        const henka_hks_token_kind kind = henka_hks_current(parser)->kind;
        if (kind == HENKA_HKS_TOKEN_KW_FN || kind == HENKA_HKS_TOKEN_KW_BEHAVIOR)
        {
            if (henka_hks_parse_callable(parser) != HENKA_SUCCESS)
            {
                return HENKA_ERROR_INVALID_ARGUMENT;
            }
        }
        else if (kind == HENKA_HKS_TOKEN_KW_LET || kind == HENKA_HKS_TOKEN_KW_VAR)
        {
            return henka_hks_fail(
                parser->diagnostic,
                HENKA_HKS_DIAGNOSTIC_FORBIDDEN_KEYWORD,
                henka_hks_current(parser),
                "use an explicit type or ':='; '%.*s' is not a declaration keyword",
                (int)henka_hks_current(parser)->length,
                parser->source + henka_hks_current(parser)->offset);
        }
        else if (henka_hks_parse_declaration(parser, true) != HENKA_SUCCESS)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
    }
    return HENKA_SUCCESS;
}

henka_result henka_hks_compile(
    const char* source,
    size_t source_size,
    henka_hks_program** out_program,
    henka_hks_diagnostic* out_diagnostic)
{
    henka_hks_token* tokens = NULL;
    henka_hks_program* program = NULL;
    size_t token_count = 0U;
    henka_hks_parser parser;
    henka_result result;
    henka_hks_diagnostic_clear(out_diagnostic);
    if (out_program == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_program = NULL;
    tokens = (henka_hks_token*)henka_calloc(HENKA_HKS_MAX_TOKENS, sizeof(*tokens));
    program = (henka_hks_program*)henka_calloc(1U, sizeof(*program));
    if (tokens == NULL || program == NULL)
    {
        henka_free(tokens);
        henka_free(program);
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    result = henka_hks_lex(
        source,
        source_size,
        tokens,
        HENKA_HKS_MAX_TOKENS,
        &token_count,
        out_diagnostic);
    if (result != HENKA_SUCCESS)
    {
        henka_free(tokens);
        henka_free(program);
        return result;
    }
    parser = (henka_hks_parser){
        source,
        tokens,
        token_count,
        0U,
        0U,
        0U,
        {{0}},
        program,
        out_diagnostic};
    result = henka_hks_parse_program(&parser);
    henka_free(tokens);
    if (result != HENKA_SUCCESS)
    {
        henka_free(program);
        return result;
    }
    *out_program = program;
    return HENKA_SUCCESS;
}

void henka_hks_program_destroy(henka_hks_program* program)
{
    henka_free(program);
}

size_t henka_hks_program_get_binding_count(const henka_hks_program* program)
{
    return program == NULL ? 0U : program->binding_count;
}

henka_result henka_hks_program_get_binding(
    const henka_hks_program* program,
    size_t index,
    henka_hks_binding_info* out_binding)
{
    if (program == NULL || out_binding == NULL || index >= program->binding_count)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_binding = program->bindings[index];
    return HENKA_SUCCESS;
}

size_t henka_hks_program_get_callable_count(const henka_hks_program* program)
{
    return program == NULL ? 0U : program->callable_count;
}

henka_result henka_hks_program_find_callable(
    const henka_hks_program* program,
    const char* name,
    size_t* out_index)
{
    size_t index;
    if (out_index != NULL)
    {
        *out_index = SIZE_MAX;
    }
    if (program == NULL || name == NULL || name[0] == '\0' || out_index == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    for (index = 0U; index < program->callable_count; ++index)
    {
        if (strcmp(program->callables[index].name, name) == 0)
        {
            *out_index = index;
            return HENKA_SUCCESS;
        }
    }
    return HENKA_ERROR_INVALID_ARGUMENT;
}

henka_result henka_hks_program_get_callable(
    const henka_hks_program* program,
    size_t index,
    henka_hks_callable_info* out_callable)
{
    if (program == NULL || out_callable == NULL || index >= program->callable_count)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_callable = program->callables[index];
    return HENKA_SUCCESS;
}

size_t henka_hks_program_get_ast_node_count(const henka_hks_program* program)
{
    return program == NULL ? 0U : program->ast_node_count;
}

static bool henka_hks_vm_is_numeric(henka_hks_value_type type)
{
    return type == HENKA_HKS_TYPE_I32 || type == HENKA_HKS_TYPE_F32;
}

static henka_hks_execution_result henka_hks_vm_finish(
    henka_hks_execution_report* report,
    henka_hks_execution_result result,
    uint32_t instructions_executed,
    size_t stack_depth)
{
    if (report != NULL)
    {
        report->result = result;
        report->instructions_executed = instructions_executed;
        report->stack_depth = stack_depth;
    }
    return result;
}

henka_hks_execution_result henka_hks_execute_with_context(
    const henka_hks_program* program,
    size_t callable_index,
    uint32_t instruction_budget,
    const henka_hks_execution_context* context,
    henka_hks_value* out_return_value,
    henka_hks_execution_report* out_report)
{
    henka_hks_value locals[HENKA_HKS_MAX_BINDINGS];
    henka_hks_value stack[HENKA_HKS_MAX_VM_STACK];
    const henka_hks_callable_info* callable;
    size_t stack_depth = 0U;
    size_t instruction_index;
    size_t instruction_end;
    uint32_t instructions_executed = 0U;

    if (out_return_value != NULL)
    {
        memset(out_return_value, 0, sizeof(*out_return_value));
        out_return_value->type = HENKA_HKS_TYPE_VOID;
    }
    if (out_report != NULL)
    {
        memset(out_report, 0, sizeof(*out_report));
        out_report->result = HENKA_HKS_EXECUTION_INVALID_PROGRAM;
        out_report->host_error = HENKA_SUCCESS;
    }
    if (program == NULL || callable_index >= program->callable_count)
    {
        return henka_hks_vm_finish(
            out_report,
            HENKA_HKS_EXECUTION_INVALID_PROGRAM,
            0U,
            0U);
    }
    callable = &program->callables[callable_index];
    if (callable->bytecode_offset > program->bytecode_count ||
        callable->bytecode_length > program->bytecode_count - callable->bytecode_offset)
    {
        return henka_hks_vm_finish(
            out_report,
            HENKA_HKS_EXECUTION_INVALID_PROGRAM,
            0U,
            0U);
    }
    instruction_budget = instruction_budget == 0U
        ? HENKA_HKS_DEFAULT_INSTRUCTION_BUDGET
        : instruction_budget;
    memset(locals, 0, sizeof(locals));
    memset(stack, 0, sizeof(stack));
    instruction_index = callable->bytecode_offset;
    instruction_end = callable->bytecode_offset + callable->bytecode_length;
    while (instruction_index < instruction_end)
    {
        const henka_hks_instruction* instruction = &program->bytecode[instruction_index++];
        henka_hks_value left;
        henka_hks_value right;
        henka_hks_value value;
        int64_t wide_result;
        if (instructions_executed >= instruction_budget)
        {
            return henka_hks_vm_finish(
                out_report,
                HENKA_HKS_EXECUTION_BUDGET_EXHAUSTED,
                instructions_executed,
                stack_depth);
        }
        ++instructions_executed;
        switch (instruction->opcode)
        {
            case HENKA_HKS_OPCODE_PUSH_I32:
                if (stack_depth >= HENKA_HKS_MAX_VM_STACK)
                {
                    return henka_hks_vm_finish(out_report, HENKA_HKS_EXECUTION_STACK_OVERFLOW, instructions_executed, stack_depth);
                }
                stack[stack_depth++] = (henka_hks_value){HENKA_HKS_TYPE_I32, {.i32 = instruction->i32}};
                break;
            case HENKA_HKS_OPCODE_PUSH_F32:
                if (stack_depth >= HENKA_HKS_MAX_VM_STACK)
                {
                    return henka_hks_vm_finish(out_report, HENKA_HKS_EXECUTION_STACK_OVERFLOW, instructions_executed, stack_depth);
                }
                stack[stack_depth++] = (henka_hks_value){HENKA_HKS_TYPE_F32, {.f32 = instruction->f32}};
                break;
            case HENKA_HKS_OPCODE_PUSH_BOOL:
                if (stack_depth >= HENKA_HKS_MAX_VM_STACK)
                {
                    return henka_hks_vm_finish(out_report, HENKA_HKS_EXECUTION_STACK_OVERFLOW, instructions_executed, stack_depth);
                }
                stack[stack_depth++] = (henka_hks_value){HENKA_HKS_TYPE_BOOL, {.boolean = instruction->i32 != 0}};
                break;
            case HENKA_HKS_OPCODE_PUSH_UNSUPPORTED:
                return henka_hks_vm_finish(out_report, HENKA_HKS_EXECUTION_UNSUPPORTED_VALUE, instructions_executed, stack_depth);
            case HENKA_HKS_OPCODE_LOAD:
                if (instruction->slot >= HENKA_HKS_MAX_BINDINGS ||
                    locals[instruction->slot].type != instruction->type)
                {
                    return henka_hks_vm_finish(out_report, HENKA_HKS_EXECUTION_TYPE_ERROR, instructions_executed, stack_depth);
                }
                if (stack_depth >= HENKA_HKS_MAX_VM_STACK)
                {
                    return henka_hks_vm_finish(out_report, HENKA_HKS_EXECUTION_STACK_OVERFLOW, instructions_executed, stack_depth);
                }
                stack[stack_depth++] = locals[instruction->slot];
                break;
            case HENKA_HKS_OPCODE_STORE:
                if (instruction->slot >= HENKA_HKS_MAX_BINDINGS || stack_depth == 0U)
                {
                    return henka_hks_vm_finish(out_report, stack_depth == 0U ? HENKA_HKS_EXECUTION_STACK_UNDERFLOW : HENKA_HKS_EXECUTION_INVALID_PROGRAM, instructions_executed, stack_depth);
                }
                value = stack[--stack_depth];
                if (value.type != instruction->type)
                {
                    return henka_hks_vm_finish(out_report, HENKA_HKS_EXECUTION_TYPE_ERROR, instructions_executed, stack_depth);
                }
                locals[instruction->slot] = value;
                break;
            case HENKA_HKS_OPCODE_ADD:
            case HENKA_HKS_OPCODE_SUB:
            case HENKA_HKS_OPCODE_MUL:
            case HENKA_HKS_OPCODE_DIV:
                if (stack_depth < 2U)
                {
                    return henka_hks_vm_finish(out_report, HENKA_HKS_EXECUTION_STACK_UNDERFLOW, instructions_executed, stack_depth);
                }
                right = stack[--stack_depth];
                left = stack[--stack_depth];
                if (left.type != instruction->type || right.type != instruction->type ||
                    !henka_hks_vm_is_numeric(instruction->type))
                {
                    return henka_hks_vm_finish(out_report, HENKA_HKS_EXECUTION_TYPE_ERROR, instructions_executed, stack_depth);
                }
                if (instruction->type == HENKA_HKS_TYPE_I32)
                {
                    if (instruction->opcode == HENKA_HKS_OPCODE_DIV && right.as.i32 == 0)
                    {
                        return henka_hks_vm_finish(out_report, HENKA_HKS_EXECUTION_DIVIDE_BY_ZERO, instructions_executed, stack_depth);
                    }
                    if (instruction->opcode == HENKA_HKS_OPCODE_ADD)
                    {
                        wide_result = (int64_t)left.as.i32 + (int64_t)right.as.i32;
                    }
                    else if (instruction->opcode == HENKA_HKS_OPCODE_SUB)
                    {
                        wide_result = (int64_t)left.as.i32 - (int64_t)right.as.i32;
                    }
                    else if (instruction->opcode == HENKA_HKS_OPCODE_MUL)
                    {
                        wide_result = (int64_t)left.as.i32 * (int64_t)right.as.i32;
                    }
                    else if (left.as.i32 == INT32_MIN && right.as.i32 == -1)
                    {
                        return henka_hks_vm_finish(out_report, HENKA_HKS_EXECUTION_TYPE_ERROR, instructions_executed, stack_depth);
                    }
                    else
                    {
                        wide_result = left.as.i32 / right.as.i32;
                    }
                    if (wide_result < (int64_t)INT32_MIN || wide_result > (int64_t)INT32_MAX)
                    {
                        return henka_hks_vm_finish(out_report, HENKA_HKS_EXECUTION_TYPE_ERROR, instructions_executed, stack_depth);
                    }
                    value = (henka_hks_value){HENKA_HKS_TYPE_I32, {.i32 = (int32_t)wide_result}};
                }
                else
                {
                    float float_result;
                    if (instruction->opcode == HENKA_HKS_OPCODE_DIV && right.as.f32 == 0.0F)
                    {
                        return henka_hks_vm_finish(out_report, HENKA_HKS_EXECUTION_DIVIDE_BY_ZERO, instructions_executed, stack_depth);
                    }
                    if (instruction->opcode == HENKA_HKS_OPCODE_ADD)
                    {
                        float_result = left.as.f32 + right.as.f32;
                    }
                    else if (instruction->opcode == HENKA_HKS_OPCODE_SUB)
                    {
                        float_result = left.as.f32 - right.as.f32;
                    }
                    else if (instruction->opcode == HENKA_HKS_OPCODE_MUL)
                    {
                        float_result = left.as.f32 * right.as.f32;
                    }
                    else
                    {
                        float_result = left.as.f32 / right.as.f32;
                    }
                    if (!isfinite(float_result))
                    {
                        return henka_hks_vm_finish(out_report, HENKA_HKS_EXECUTION_TYPE_ERROR, instructions_executed, stack_depth);
                    }
                    value = (henka_hks_value){HENKA_HKS_TYPE_F32, {.f32 = float_result}};
                }
                stack[stack_depth++] = value;
                break;
            case HENKA_HKS_OPCODE_NEG:
                if (stack_depth == 0U)
                {
                    return henka_hks_vm_finish(out_report, HENKA_HKS_EXECUTION_STACK_UNDERFLOW, instructions_executed, stack_depth);
                }
                value = stack[stack_depth - 1U];
                if (value.type == HENKA_HKS_TYPE_I32)
                {
                    if (value.as.i32 == INT32_MIN)
                    {
                        return henka_hks_vm_finish(out_report, HENKA_HKS_EXECUTION_TYPE_ERROR, instructions_executed, stack_depth);
                    }
                    value.as.i32 = -value.as.i32;
                }
                else if (value.type == HENKA_HKS_TYPE_F32 && isfinite(-value.as.f32))
                {
                    value.as.f32 = -value.as.f32;
                }
                else
                {
                    return henka_hks_vm_finish(out_report, HENKA_HKS_EXECUTION_TYPE_ERROR, instructions_executed, stack_depth);
                }
                stack[stack_depth - 1U] = value;
                break;
            case HENKA_HKS_OPCODE_RETURN:
                if (instruction->i32 != 0)
                {
                    if (stack_depth == 0U)
                    {
                        return henka_hks_vm_finish(out_report, HENKA_HKS_EXECUTION_STACK_UNDERFLOW, instructions_executed, stack_depth);
                    }
                    value = stack[--stack_depth];
                    if (out_return_value != NULL)
                    {
                        *out_return_value = value;
                    }
                }
                else if (out_return_value != NULL)
                {
                    out_return_value->type = HENKA_HKS_TYPE_VOID;
                }
                return henka_hks_vm_finish(out_report, HENKA_HKS_EXECUTION_COMPLETED, instructions_executed, stack_depth);
            case HENKA_HKS_OPCODE_POP:
                if (stack_depth == 0U)
                {
                    return henka_hks_vm_finish(out_report, HENKA_HKS_EXECUTION_STACK_UNDERFLOW, instructions_executed, stack_depth);
                }
                --stack_depth;
                break;
            case HENKA_HKS_OPCODE_EMIT_EVENT:
                {
                    henka_script_api_value arguments[2];
                    henka_script_api_value output;
                    henka_result host_result;
                    if (stack_depth == 0U)
                    {
                        return henka_hks_vm_finish(
                            out_report,
                            HENKA_HKS_EXECUTION_STACK_UNDERFLOW,
                            instructions_executed,
                            stack_depth);
                    }
                    value = stack[--stack_depth];
                    if (value.type != HENKA_HKS_TYPE_I32 || value.as.i32 <= 0 ||
                        context == NULL || context->host == NULL ||
                        (uint64_t)value.as.i32 > UINT32_MAX)
                    {
                        if (out_report != NULL)
                        {
                            out_report->host_error = HENKA_ERROR_INVALID_ARGUMENT;
                        }
                        return henka_hks_vm_finish(
                            out_report,
                            HENKA_HKS_EXECUTION_HOST_ERROR,
                            instructions_executed,
                            stack_depth);
                    }
                    arguments[0] = (henka_script_api_value){
                        HENKA_SCRIPT_API_VALUE_EVENT_ID,
                        {.event_id = (uint32_t)value.as.i32}};
                    arguments[1] = (henka_script_api_value){
                        HENKA_SCRIPT_API_VALUE_ENTITY,
                        {.entity = context->entity_id}};
                    host_result = henka_script_host_invoke(
                        context->host,
                        HENKA_SCRIPT_API_EVENTS_EMIT,
                        arguments,
                        2U,
                        &output);
                    if (host_result != HENKA_SUCCESS ||
                        output.type != HENKA_SCRIPT_API_VALUE_RESULT ||
                        output.as.result != HENKA_SUCCESS)
                    {
                        if (out_report != NULL)
                        {
                            out_report->host_error = host_result != HENKA_SUCCESS
                                ? host_result
                                : output.as.result;
                        }
                        return henka_hks_vm_finish(
                            out_report,
                            HENKA_HKS_EXECUTION_HOST_ERROR,
                            instructions_executed,
                            stack_depth);
                    }
                }
                break;
            default:
                return henka_hks_vm_finish(out_report, HENKA_HKS_EXECUTION_INVALID_PROGRAM, instructions_executed, stack_depth);
        }
    }
    return henka_hks_vm_finish(
        out_report,
        HENKA_HKS_EXECUTION_INVALID_PROGRAM,
        instructions_executed,
        stack_depth);
}

henka_hks_execution_result henka_hks_execute(
    const henka_hks_program* program,
    size_t callable_index,
    uint32_t instruction_budget,
    henka_hks_value* out_return_value,
    henka_hks_execution_report* out_report)
{
    return henka_hks_execute_with_context(
        program,
        callable_index,
        instruction_budget,
        NULL,
        out_return_value,
        out_report);
}
