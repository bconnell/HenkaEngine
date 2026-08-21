#include <henka/script_source.h>

#include <string.h>

#include <henka/henkascript.h>
#include <henka/memory.h>
#include <henka/script_backends.h>

struct henka_script_source_document
{
    henka_script_language language;
    char* source;
    size_t source_capacity;
    size_t source_size;
    uint64_t revision;
    bool dirty;
    henka_script_source_diagnostic diagnostic;
};

static bool henka_script_source_language_is_valid(
    henka_script_language language)
{
    return language == HENKA_SCRIPT_LANGUAGE_LUA ||
        language == HENKA_SCRIPT_LANGUAGE_HENKASCRIPT;
}

static size_t henka_script_source_max_bytes(
    henka_script_language language)
{
    return language == HENKA_SCRIPT_LANGUAGE_LUA
        ? HENKA_LUA_MAX_SOURCE_BYTES
        : HENKA_HKS_MAX_SOURCE_BYTES;
}

static void henka_script_source_clear_diagnostic(
    henka_script_source_diagnostic* diagnostic)
{
    if (diagnostic == NULL)
    {
        return;
    }
    memset(diagnostic, 0, sizeof(*diagnostic));
    diagnostic->result = HENKA_SUCCESS;
}

static void henka_script_source_copy_message(
    char* destination,
    size_t destination_size,
    const char* source)
{
    size_t source_size;
    if (destination == NULL || destination_size == 0U)
    {
        return;
    }
    if (source == NULL)
    {
        destination[0] = '\0';
        return;
    }
    source_size = strlen(source);
    if (source_size >= destination_size)
    {
        source_size = destination_size - 1U;
    }
    memcpy(destination, source, source_size);
    destination[source_size] = '\0';
}

static void henka_script_source_set_hks_diagnostic(
    henka_script_source_diagnostic* destination,
    henka_result result,
    const henka_hks_diagnostic* source)
{
    henka_script_source_clear_diagnostic(destination);
    destination->result = result;
    if (source != NULL)
    {
        destination->line = source->line;
        destination->column = source->column;
        henka_script_source_copy_message(
            destination->message,
            sizeof(destination->message),
            source->message);
    }
}

static void henka_script_source_set_lua_diagnostic(
    henka_script_source_diagnostic* destination,
    henka_result result,
    const henka_lua_diagnostic* source)
{
    henka_script_source_clear_diagnostic(destination);
    destination->result = result;
    if (source != NULL)
    {
        destination->line = source->line;
        destination->column = source->column;
        henka_script_source_copy_message(
            destination->message,
            sizeof(destination->message),
            source->message);
    }
}

henka_result henka_script_source_create(
    henka_script_language language,
    henka_script_source_document** out_document)
{
    henka_script_source_document* document;
    size_t max_source_bytes;
    if (out_document == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_document = NULL;
    if (!henka_script_source_language_is_valid(language))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    max_source_bytes = henka_script_source_max_bytes(language);
    document = (henka_script_source_document*)henka_calloc(
        1U,
        sizeof(*document));
    if (document == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    document->source = (char*)henka_calloc(
        max_source_bytes + 1U,
        sizeof(*document->source));
    if (document->source == NULL)
    {
        henka_free(document);
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    document->language = language;
    document->source_capacity = max_source_bytes + 1U;
    henka_script_source_clear_diagnostic(&document->diagnostic);
    *out_document = document;
    return HENKA_SUCCESS;
}

void henka_script_source_destroy(henka_script_source_document* document)
{
    if (document == NULL)
    {
        return;
    }
    henka_free(document->source);
    henka_free(document);
}

henka_result henka_script_source_set_text(
    henka_script_source_document* document,
    const char* source,
    size_t source_size)
{
    if (document == NULL || source == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (source_size >= document->source_capacity ||
        document->revision == UINT64_MAX)
    {
        return HENKA_ERROR_LIMIT;
    }
    memmove(document->source, source, source_size);
    document->source[source_size] = '\0';
    document->source_size = source_size;
    ++document->revision;
    document->dirty = true;
    henka_script_source_clear_diagnostic(&document->diagnostic);
    return HENKA_SUCCESS;
}

henka_result henka_script_source_replace_range(
    henka_script_source_document* document,
    size_t offset,
    size_t remove_size,
    const char* replacement,
    size_t replacement_size)
{
    char* replacement_copy = NULL;
    size_t remaining_size;
    size_t tail_size;
    size_t new_size;
    if (document == NULL ||
        offset > document->source_size ||
        remove_size > document->source_size - offset ||
        (replacement == NULL && replacement_size > 0U))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (remove_size == 0U && replacement_size == 0U)
    {
        return HENKA_SUCCESS;
    }
    if (document->revision == UINT64_MAX)
    {
        return HENKA_ERROR_LIMIT;
    }
    remaining_size = document->source_size - remove_size;
    if (replacement_size > document->source_capacity - 1U - remaining_size)
    {
        return HENKA_ERROR_LIMIT;
    }
    if (replacement_size > 0U)
    {
        replacement_copy = (char*)henka_malloc(replacement_size);
        if (replacement_copy == NULL)
        {
            return HENKA_ERROR_OUT_OF_MEMORY;
        }
        memcpy(replacement_copy, replacement, replacement_size);
    }
    new_size = remaining_size + replacement_size;
    tail_size = document->source_size - offset - remove_size;
    memmove(
        document->source + offset + replacement_size,
        document->source + offset + remove_size,
        tail_size);
    if (replacement_size > 0U)
    {
        memcpy(document->source + offset, replacement_copy, replacement_size);
    }
    henka_free(replacement_copy);
    document->source_size = new_size;
    document->source[new_size] = '\0';
    ++document->revision;
    document->dirty = true;
    henka_script_source_clear_diagnostic(&document->diagnostic);
    return HENKA_SUCCESS;
}

henka_result henka_script_source_get_text(
    const henka_script_source_document* document,
    const char** out_source,
    size_t* out_source_size)
{
    if (out_source != NULL)
    {
        *out_source = NULL;
    }
    if (out_source_size != NULL)
    {
        *out_source_size = 0U;
    }
    if (document == NULL || out_source == NULL || out_source_size == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_source = document->source;
    *out_source_size = document->source_size;
    return HENKA_SUCCESS;
}

henka_script_language henka_script_source_get_language(
    const henka_script_source_document* document)
{
    return document == NULL ? HENKA_SCRIPT_LANGUAGE_NONE : document->language;
}

henka_result henka_script_source_mark_clean(
    henka_script_source_document* document)
{
    if (document == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    document->dirty = false;
    return HENKA_SUCCESS;
}

henka_result henka_script_source_validate(
    henka_script_source_document* document,
    henka_script_source_diagnostic* out_diagnostic)
{
    henka_result result;
    if (out_diagnostic != NULL)
    {
        henka_script_source_clear_diagnostic(out_diagnostic);
    }
    if (document == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (document->language == HENKA_SCRIPT_LANGUAGE_HENKASCRIPT)
    {
        henka_hks_behavior_backend* backend = NULL;
        henka_hks_diagnostic diagnostic;
        memset(&diagnostic, 0, sizeof(diagnostic));
        result = henka_hks_behavior_backend_create(
            document->source,
            document->source_size,
            &backend,
            &diagnostic);
        henka_hks_behavior_backend_destroy(backend);
        henka_script_source_set_hks_diagnostic(
            &document->diagnostic,
            result,
            &diagnostic);
    }
    else if (document->language == HENKA_SCRIPT_LANGUAGE_LUA)
    {
        henka_lua_behavior_backend* backend = NULL;
        henka_lua_diagnostic diagnostic;
        memset(&diagnostic, 0, sizeof(diagnostic));
        result = henka_lua_behavior_backend_create(
            document->source,
            document->source_size,
            &backend,
            &diagnostic);
        henka_lua_behavior_backend_destroy(backend);
        henka_script_source_set_lua_diagnostic(
            &document->diagnostic,
            result,
            &diagnostic);
    }
    else
    {
        result = HENKA_ERROR_INVALID_ARGUMENT;
        henka_script_source_clear_diagnostic(&document->diagnostic);
        document->diagnostic.result = result;
    }
    if (out_diagnostic != NULL)
    {
        *out_diagnostic = document->diagnostic;
    }
    return result;
}

henka_result henka_script_source_get_diagnostic(
    const henka_script_source_document* document,
    henka_script_source_diagnostic* out_diagnostic)
{
    if (document == NULL || out_diagnostic == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_diagnostic = document->diagnostic;
    return HENKA_SUCCESS;
}

bool henka_script_source_is_dirty(
    const henka_script_source_document* document)
{
    return document != NULL && document->dirty;
}

uint64_t henka_script_source_get_revision(
    const henka_script_source_document* document)
{
    return document == NULL ? 0U : document->revision;
}
