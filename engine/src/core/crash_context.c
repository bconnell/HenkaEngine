#include <henka/crash_context.h>

#include <string.h>

static bool henka_crash_message_is_valid(const char* message)
{
    size_t index;

    if (message == NULL || message[0] == '\0')
    {
        return false;
    }
    for (index = 0U; index < HENKA_CRASH_BREADCRUMB_MESSAGE_BYTES; ++index)
    {
        if (message[index] == '\0')
        {
            return true;
        }
    }
    return false;
}

void henka_crash_context_reset(henka_crash_context* context)
{
    if (context != NULL)
    {
        memset(context, 0, sizeof(*context));
        context->next_sequence = 1U;
    }
}

henka_result henka_crash_context_push(
    henka_crash_context* context,
    const char* message)
{
    henka_crash_breadcrumb* entry;

    if (context == NULL || !henka_crash_message_is_valid(message))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (context->next_sequence == 0U)
    {
        return HENKA_ERROR_LIMIT;
    }

    entry = &context->entries[context->write_index];
    memset(entry, 0, sizeof(*entry));
    entry->sequence = context->next_sequence++;
    memcpy(entry->message, message, strlen(message) + 1U);

    context->write_index =
        (context->write_index + 1U) % HENKA_CRASH_BREADCRUMB_CAPACITY;
    if (context->count < HENKA_CRASH_BREADCRUMB_CAPACITY)
    {
        ++context->count;
    }
    return HENKA_SUCCESS;
}

size_t henka_crash_context_get_count(const henka_crash_context* context)
{
    return context == NULL ? 0U : context->count;
}

henka_result henka_crash_context_get(
    const henka_crash_context* context,
    size_t index,
    henka_crash_breadcrumb* out_breadcrumb)
{
    uint32_t oldest;
    uint32_t physical_index;

    if (out_breadcrumb != NULL)
    {
        memset(out_breadcrumb, 0, sizeof(*out_breadcrumb));
    }
    if (context == NULL ||
        out_breadcrumb == NULL ||
        index >= context->count)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    oldest = context->count == HENKA_CRASH_BREADCRUMB_CAPACITY
        ? context->write_index
        : 0U;
    physical_index =
        (oldest + (uint32_t)index) % HENKA_CRASH_BREADCRUMB_CAPACITY;
    *out_breadcrumb = context->entries[physical_index];
    return HENKA_SUCCESS;
}
