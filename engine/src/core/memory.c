#include <henka/memory.h>

#include <stdlib.h>
#include <stdint.h>

#include <henka/log.h>

#include "memory_internal.h"

static size_t g_allocation_count = 0;
static size_t g_test_allocations_before_failure = SIZE_MAX;

static int henka_memory_test_should_fail(void)
{
    if (g_test_allocations_before_failure == SIZE_MAX)
    {
        return 0;
    }
    if (g_test_allocations_before_failure == 0U)
    {
        return 1;
    }
    --g_test_allocations_before_failure;
    return 0;
}

void henka_memory_test_fail_after(size_t successful_allocations)
{
    g_test_allocations_before_failure = successful_allocations;
}

void henka_memory_test_disable_failures(void)
{
    g_test_allocations_before_failure = SIZE_MAX;
}

void* henka_malloc(size_t size)
{
    void* pointer;

    if (henka_memory_test_should_fail())
    {
        return NULL;
    }

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

    if (henka_memory_test_should_fail())
    {
        return NULL;
    }

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

    if (size > 0U && henka_memory_test_should_fail())
    {
        return NULL;
    }

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
    if (g_allocation_count > 0U)
    {
        HENKA_LOG_WARN("possible memory leak detected: %zu allocation(s) still active", g_allocation_count);
    }
    else
    {
        HENKA_LOG_INFO("memory shutdown clean: no active allocations tracked");
    }
}
