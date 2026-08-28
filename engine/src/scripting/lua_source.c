#include <henka/script_backends.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

static bool henka_lua_is_alpha(unsigned char value)
{
    return (value >= (unsigned char)'A' && value <= (unsigned char)'Z') ||
        (value >= (unsigned char)'a' && value <= (unsigned char)'z') ||
        value == (unsigned char)'_';
}

static bool henka_lua_is_digit(unsigned char value)
{
    return value >= (unsigned char)'0' && value <= (unsigned char)'9';
}

static bool henka_lua_is_hex_digit(unsigned char value)
{
    return henka_lua_is_digit(value) ||
        (value >= (unsigned char)'A' && value <= (unsigned char)'F') ||
        (value >= (unsigned char)'a' && value <= (unsigned char)'f');
}

static bool henka_lua_is_word_byte(unsigned char value)
{
    return henka_lua_is_alpha(value) || henka_lua_is_digit(value);
}

static bool henka_lua_text_equals(
    const char* source,
    const henka_lua_token* token,
    const char* text)
{
    const size_t text_size = text == NULL ? 0U : strlen(text);
    return source != NULL && token != NULL && text != NULL &&
        token->length == text_size &&
        memcmp(source + token->offset, text, text_size) == 0;
}

static void henka_lua_set_diagnostic(
    henka_lua_diagnostic* diagnostic,
    henka_lua_diagnostic_code code,
    uint32_t line,
    uint32_t column,
    const char* message)
{
    if (diagnostic == NULL)
    {
        return;
    }
    memset(diagnostic, 0, sizeof(*diagnostic));
    diagnostic->code = code;
    diagnostic->line = line;
    diagnostic->column = column;
    if (message != NULL)
    {
        (void)snprintf(
            diagnostic->message,
            sizeof(diagnostic->message),
            "%s",
            message);
    }
}

static void henka_lua_advance_location(
    const char* source,
    size_t start,
    size_t end,
    uint32_t* line,
    uint32_t* column)
{
    size_t index;
    if (source == NULL || line == NULL || column == NULL)
    {
        return;
    }
    for (index = start; index < end; ++index)
    {
        if (source[index] == '\n')
        {
            ++(*line);
            *column = 1U;
        }
        else
        {
            ++(*column);
        }
    }
}

/* Returns the byte length of a Lua long-bracket opener, or zero when the
 * bytes at start are not an opener. */
static size_t henka_lua_long_bracket_open_length(
    const char* source,
    size_t source_size,
    size_t start)
{
    size_t index;
    if (source == NULL || start >= source_size || source[start] != '[')
    {
        return 0U;
    }
    index = start + 1U;
    while (index < source_size && source[index] == '=')
    {
        ++index;
    }
    return index < source_size && source[index] == '['
        ? index - start + 1U
        : 0U;
}

static size_t henka_lua_long_bracket_end(
    const char* source,
    size_t source_size,
    size_t content_start,
    size_t opener_length)
{
    const size_t equal_count = opener_length - 2U;
    size_t index;
    if (source == NULL || opener_length < 2U)
    {
        return SIZE_MAX;
    }
    for (index = content_start; index < source_size; ++index)
    {
        size_t equal_index;
        if (source[index] != ']')
        {
            continue;
        }
        equal_index = index + 1U;
        while (equal_index < source_size &&
               source[equal_index] == '=' &&
               equal_index - index - 1U < equal_count)
        {
            ++equal_index;
        }
        if (equal_index - index - 1U == equal_count &&
            equal_index < source_size && source[equal_index] == ']')
        {
            return equal_index + 1U;
        }
    }
    return SIZE_MAX;
}

static henka_lua_token_class henka_lua_word_class(
    const char* source,
    size_t offset,
    size_t length)
{
    static const char* const keywords[] = {
        "and", "break", "do", "else", "elseif", "end", "false",
        "for", "function", "goto", "if", "in", "local", "nil",
        "not", "or", "repeat", "return", "then", "true", "until",
        "while"
    };
    static const char* const builtins[] = {
        "Audio", "Entity", "Events", "Input", "Interaction", "Physics",
        "State", "Transform"
    };
    size_t index;
    if (source == NULL)
    {
        return HENKA_LUA_TOKEN_CLASS_NONE;
    }
    for (index = 0U; index < sizeof(keywords) / sizeof(keywords[0]); ++index)
    {
        if (strlen(keywords[index]) == length &&
            memcmp(source + offset, keywords[index], length) == 0)
        {
            return (strcmp(keywords[index], "true") == 0 ||
                    strcmp(keywords[index], "false") == 0 ||
                    strcmp(keywords[index], "nil") == 0)
                ? HENKA_LUA_TOKEN_CLASS_LITERAL
                : HENKA_LUA_TOKEN_CLASS_KEYWORD;
        }
    }
    for (index = 0U; index < sizeof(builtins) / sizeof(builtins[0]); ++index)
    {
        if (strlen(builtins[index]) == length &&
            memcmp(source + offset, builtins[index], length) == 0)
        {
            return HENKA_LUA_TOKEN_CLASS_BUILTIN;
        }
    }
    return HENKA_LUA_TOKEN_CLASS_IDENTIFIER;
}

static bool henka_lua_emit_token(
    henka_lua_token* tokens,
    size_t token_capacity,
    size_t* token_count,
    henka_lua_token token,
    henka_lua_diagnostic* diagnostic,
    uint32_t line,
    uint32_t column)
{
    if (tokens == NULL || token_count == NULL ||
        *token_count >= token_capacity - 1U ||
        *token_count >= HENKA_LUA_MAX_TOKENS - 1U)
    {
        henka_lua_set_diagnostic(
            diagnostic,
            HENKA_LUA_DIAGNOSTIC_LIMIT,
            line,
            column,
            "Lua editor token stream exceeds its bounded limit");
        return false;
    }
    tokens[(*token_count)++] = token;
    return true;
}

henka_result henka_lua_lex(
    const char* source,
    size_t source_size,
    henka_lua_token* tokens,
    size_t token_capacity,
    size_t* out_token_count,
    henka_lua_diagnostic* out_diagnostic)
{
    size_t offset = 0U;
    size_t token_count = 0U;
    uint32_t line = 1U;
    uint32_t column = 1U;

    if (out_token_count != NULL)
    {
        *out_token_count = 0U;
    }
    if (out_diagnostic != NULL)
    {
        henka_lua_set_diagnostic(
            out_diagnostic,
            HENKA_LUA_DIAGNOSTIC_NONE,
            0U,
            0U,
            NULL);
    }
    if ((source == NULL && source_size != 0U) ||
        source_size > HENKA_LUA_MAX_SOURCE_BYTES || tokens == NULL ||
        token_capacity < 2U || out_token_count == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    while (offset < source_size)
    {
        const size_t start = offset;
        const uint32_t start_line = line;
        const uint32_t start_column = column;
        unsigned char current = (unsigned char)source[offset];
        henka_lua_token_class token_class;

        if (current == (unsigned char)' ' || current == (unsigned char)'\t' ||
            current == (unsigned char)'\r' || current == (unsigned char)'\n')
        {
            ++offset;
            henka_lua_advance_location(
                source, start, offset, &line, &column);
            continue;
        }

        if (current == (unsigned char)'-' && offset + 1U < source_size &&
            source[offset + 1U] == '-')
        {
            size_t long_open_length = 0U;
            size_t end;
            offset += 2U;
            if (offset < source_size && source[offset] == '[')
            {
                long_open_length = henka_lua_long_bracket_open_length(
                    source, source_size, offset);
            }
            if (long_open_length != 0U)
            {
                end = henka_lua_long_bracket_end(
                    source,
                    source_size,
                    offset + long_open_length,
                    long_open_length);
                if (end == SIZE_MAX)
                {
                    henka_lua_set_diagnostic(
                        out_diagnostic,
                        HENKA_LUA_DIAGNOSTIC_COMPILE,
                        start_line,
                        start_column,
                        "unterminated Lua long comment");
                    return HENKA_ERROR_ASSET_SOURCE;
                }
                offset = end;
            }
            else
            {
                while (offset < source_size && source[offset] != '\n')
                {
                    ++offset;
                }
            }
            token_class = HENKA_LUA_TOKEN_CLASS_COMMENT;
            if (!henka_lua_emit_token(
                    tokens,
                    token_capacity,
                    &token_count,
                    (henka_lua_token){
                        token_class,
                        start,
                        offset - start,
                        start_line,
                        start_column},
                    out_diagnostic,
                    start_line,
                    start_column))
            {
                return HENKA_ERROR_LIMIT;
            }
            henka_lua_advance_location(
                source, start, offset, &line, &column);
            continue;
        }

        if (current == (unsigned char)'\'' || current == (unsigned char)'"')
        {
            const unsigned char quote = current;
            bool closed = false;
            ++offset;
            while (offset < source_size)
            {
                if ((unsigned char)source[offset] == (unsigned char)'\\')
                {
                    offset += offset + 1U < source_size ? 2U : 1U;
                    continue;
                }
                if ((unsigned char)source[offset] == quote)
                {
                    ++offset;
                    closed = true;
                    break;
                }
                if (source[offset] == '\n' || source[offset] == '\r')
                {
                    break;
                }
                ++offset;
            }
            if (!closed)
            {
                henka_lua_set_diagnostic(
                    out_diagnostic,
                    HENKA_LUA_DIAGNOSTIC_COMPILE,
                    start_line,
                    start_column,
                    "unterminated Lua string literal");
                return HENKA_ERROR_ASSET_SOURCE;
            }
            token_class = HENKA_LUA_TOKEN_CLASS_LITERAL;
        }
        else if (current == (unsigned char)'[' &&
                 henka_lua_long_bracket_open_length(
                     source, source_size, offset) != 0U)
        {
            const size_t open_length = henka_lua_long_bracket_open_length(
                source, source_size, offset);
            const size_t end = henka_lua_long_bracket_end(
                source, source_size, offset + open_length, open_length);
            if (end == SIZE_MAX)
            {
                henka_lua_set_diagnostic(
                    out_diagnostic,
                    HENKA_LUA_DIAGNOSTIC_COMPILE,
                    start_line,
                    start_column,
                    "unterminated Lua long string literal");
                return HENKA_ERROR_ASSET_SOURCE;
            }
            offset = end;
            token_class = HENKA_LUA_TOKEN_CLASS_LITERAL;
        }
        else if (henka_lua_is_alpha(current))
        {
            ++offset;
            while (offset < source_size &&
                   henka_lua_is_word_byte((unsigned char)source[offset]))
            {
                ++offset;
            }
            token_class = henka_lua_word_class(
                source, start, offset - start);
        }
        else if (henka_lua_is_digit(current) ||
                 (current == (unsigned char)'.' &&
                  offset + 1U < source_size &&
                  henka_lua_is_digit((unsigned char)source[offset + 1U])))
        {
            bool hexadecimal = current == (unsigned char)'0' &&
                offset + 1U < source_size &&
                (source[offset + 1U] == 'x' || source[offset + 1U] == 'X');
            if (hexadecimal)
            {
                offset += 2U;
                while (offset < source_size &&
                       (henka_lua_is_hex_digit((unsigned char)source[offset]) ||
                        source[offset] == '.'))
                {
                    ++offset;
                }
            }
            else
            {
                if (current == (unsigned char)'.')
                {
                    ++offset;
                }
                while (offset < source_size &&
                       henka_lua_is_digit((unsigned char)source[offset]))
                {
                    ++offset;
                }
                if (offset < source_size && source[offset] == '.')
                {
                    ++offset;
                    while (offset < source_size &&
                           henka_lua_is_digit((unsigned char)source[offset]))
                    {
                        ++offset;
                    }
                }
                if (offset < source_size &&
                    (source[offset] == 'e' || source[offset] == 'E'))
                {
                    ++offset;
                    if (offset < source_size &&
                        (source[offset] == '+' || source[offset] == '-'))
                    {
                        ++offset;
                    }
                    while (offset < source_size &&
                           henka_lua_is_digit((unsigned char)source[offset]))
                    {
                        ++offset;
                    }
                }
            }
            token_class = HENKA_LUA_TOKEN_CLASS_LITERAL;
        }
        else
        {
            static const char* const operators[] = {
                "...", "<<", ">>", "//", "..", "==", "~=", "<=", ">=", "::"
            };
            size_t operator_length = 0U;
            size_t index;
            for (index = 0U; index < sizeof(operators) / sizeof(operators[0]); ++index)
            {
                const size_t candidate_length = strlen(operators[index]);
                if (offset + candidate_length <= source_size &&
                    memcmp(source + offset, operators[index], candidate_length) == 0)
                {
                    operator_length = candidate_length;
                    break;
                }
            }
            if (operator_length == 0U &&
                strchr("+-*/%^#=<>~&|", (int)current) != NULL)
            {
                operator_length = 1U;
            }
            offset += operator_length == 0U ? 1U : operator_length;
            token_class = operator_length == 0U
                ? HENKA_LUA_TOKEN_CLASS_PUNCTUATION
                : HENKA_LUA_TOKEN_CLASS_OPERATOR;
        }

        if (!henka_lua_emit_token(
                tokens,
                token_capacity,
                &token_count,
                (henka_lua_token){
                    token_class,
                    start,
                    offset - start,
                    start_line,
                    start_column},
                out_diagnostic,
                start_line,
                start_column))
        {
            return HENKA_ERROR_LIMIT;
        }
        henka_lua_advance_location(source, start, offset, &line, &column);
    }

    tokens[token_count++] = (henka_lua_token){
        HENKA_LUA_TOKEN_CLASS_NONE,
        source_size,
        0U,
        line,
        column};
    *out_token_count = token_count;
    return HENKA_SUCCESS;
}

static bool henka_lua_token_is(
    const char* source,
    size_t source_size,
    const henka_lua_token* token,
    const char* text)
{
    return token != NULL && token->offset <= source_size &&
        token->length <= source_size - token->offset &&
        henka_lua_text_equals(source, token, text);
}

henka_result henka_lua_token_stream_get_indent_level(
    const char* source,
    size_t source_size,
    const henka_lua_token* tokens,
    size_t token_count,
    size_t source_offset,
    uint32_t source_line,
    uint32_t* out_indent_level)
{
    uint32_t depth = 0U;
    bool pending_if = false;
    size_t index;
    if (out_indent_level != NULL)
    {
        *out_indent_level = 0U;
    }
    if (source == NULL || source_offset > source_size || source_line == 0U ||
        token_count > HENKA_LUA_MAX_TOKENS ||
        (tokens == NULL && token_count != 0U) || out_indent_level == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    for (index = 0U; index < token_count; ++index)
    {
        const henka_lua_token* token = &tokens[index];
        if (token->token_class == HENKA_LUA_TOKEN_CLASS_NONE ||
            token->offset >= source_offset)
        {
            if (token->offset >= source_offset && token->line == source_line &&
                (henka_lua_token_is(source, source_size, token, "end") ||
                 henka_lua_token_is(source, source_size, token, "until") ||
                 henka_lua_token_is(source, source_size, token, "else") ||
                 henka_lua_token_is(source, source_size, token, "elseif")) &&
                depth > 0U)
            {
                --depth;
            }
            break;
        }
        if (token->token_class != HENKA_LUA_TOKEN_CLASS_KEYWORD)
        {
            continue;
        }
        if (henka_lua_token_is(source, source_size, token, "if"))
        {
            pending_if = true;
        }
        else if (henka_lua_token_is(source, source_size, token, "elseif"))
        {
            pending_if = false;
        }
        else if (henka_lua_token_is(source, source_size, token, "then"))
        {
            if (pending_if)
            {
                ++depth;
            }
            pending_if = false;
        }
        else if (henka_lua_token_is(source, source_size, token, "do"))
        {
            ++depth;
        }
        else if (henka_lua_token_is(source, source_size, token, "function") ||
                 henka_lua_token_is(source, source_size, token, "repeat"))
        {
            ++depth;
        }
        else if (henka_lua_token_is(source, source_size, token, "end") ||
                 henka_lua_token_is(source, source_size, token, "until"))
        {
            if (depth > 0U)
            {
                --depth;
            }
            pending_if = false;
        }
    }
    *out_indent_level = depth;
    return HENKA_SUCCESS;
}
