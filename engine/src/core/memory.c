#include <henka/memory.h>

#include <stdlib.h>

#include <henka/log.h>

static size_t g_allocation_count = 0;

void* henka_malloc(size_t size)
{
    void* pointer;

    pointer = malloc(size);
    if (pointer != NULL)
    {
        ++g_allocation_count;
    }

    return pointer;
}

void* henka_calloc(size_t count, size_t size)
{
    void* pointer;

    pointer = calloc(count, size);
    if (pointer != NULL)
    {
        ++g_allocation_count;
    }

    return pointer;
}

void* henka_realloc(void* pointer, size_t size)
{
    void* resized;

    if (pointer == NULL)
    {
        resized = realloc(NULL, size);
        if (resized != NULL && size > 0U)
        {
            ++g_allocation_count;
        }
        return resized;
    }

    if (size == 0U)
    {
        free(pointer);
        if (g_allocation_count > 0U)
        {
            --g_allocation_count;
        }
        return NULL;
    }

    resized = realloc(pointer, size);
    return resized;
}

void henka_free(void* pointer)
{
    if (pointer != NULL)
    {
        free(pointer);
        if (g_allocation_count > 0U)
        {
            --g_allocation_count;
        }
    }
}

size_t henka_memory_get_allocation_count(void)
{
    return g_allocation_count;
}

void henka_memory_report_leaks(void)
{
#if defined(HENKA_DEBUG)
    if (g_allocation_count > 0U)
    {
        HENKA_LOG_WARN("possible memory leak detected: %zu allocation(s) still active", g_allocation_count);
    }
    else
    {
        HENKA_LOG_INFO("memory shutdown clean: no active allocations tracked");
    }
#endif
}
