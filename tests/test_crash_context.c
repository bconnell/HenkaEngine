#include <stdio.h>
#include <string.h>

#include <henka/crash_context.h>

int main(void)
{
    henka_crash_context context;
    henka_crash_breadcrumb breadcrumb;
    char message[64U];
    size_t index;

    henka_crash_context_reset(&context);
    if (henka_crash_context_get_count(&context) != 0U)
    {
        return 1;
    }

    for (index = 0U; index < HENKA_CRASH_BREADCRUMB_CAPACITY + 3U; ++index)
    {
        (void)snprintf(message, sizeof(message), "event-%zu", index);
        if (henka_crash_context_push(&context, message) != HENKA_SUCCESS)
        {
            return 2;
        }
    }
    if (henka_crash_context_get_count(&context) !=
        HENKA_CRASH_BREADCRUMB_CAPACITY)
    {
        return 3;
    }

    if (henka_crash_context_get(&context, 0U, &breadcrumb) != HENKA_SUCCESS ||
        strcmp(breadcrumb.message, "event-3") != 0 ||
        breadcrumb.sequence != 4U)
    {
        return 4;
    }
    if (henka_crash_context_get(
            &context,
            HENKA_CRASH_BREADCRUMB_CAPACITY - 1U,
            &breadcrumb) != HENKA_SUCCESS ||
        strcmp(breadcrumb.message, "event-34") != 0 ||
        breadcrumb.sequence != 35U)
    {
        return 5;
    }

    memset(&breadcrumb, 0x5a, sizeof(breadcrumb));
    if (henka_crash_context_get(
            &context,
            HENKA_CRASH_BREADCRUMB_CAPACITY,
            &breadcrumb) == HENKA_SUCCESS ||
        breadcrumb.sequence != 0U ||
        breadcrumb.message[0] != '\0' ||
        henka_crash_context_push(&context, "") == HENKA_SUCCESS)
    {
        return 6;
    }

    henka_crash_context_reset(&context);
    if (henka_crash_context_get_count(&context) != 0U)
    {
        return 7;
    }
    return 0;
}
