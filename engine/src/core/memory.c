#include <henka/memory.h>

#include <stdlib.h>
#include <stdint.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <stdatomic.h>
#endif

#include <henka/log.h>

#include "memory_internal.h"

#if defined(_WIN32)
typedef volatile LONG64 henka_memory_counter;
#else
typedef _Atomic(size_t) henka_memory_counter;
#endif

static henka_memory_counter g_allocation_count = 0U;
static henka_memory_counter g_test_allocations_before_failure = SIZE_MAX;

static size_t henka_memory_counter_load(const henka_memory_counter* counter)
{
#if defined(_WIN32)
    return (size_t)InterlockedCompareExchange64(
        (volatile LONG64*)counter,
        0LL,
        0LL);
#else
    return atomic_load_explicit(counter, memory_order_relaxed);
#endif
}

static void henka_memory_counter_store(
    henka_memory_counter* counter,
    size_t value)
{
#if defined(_WIN32)
    (void)InterlockedExchange64((volatile LONG64*)counter, (LONG64)value);
#else
    atomic_store_explicit(counter, value, memory_order_relaxed);
#endif
}

static int henka_memory_counter_compare_exchange(
    henka_memory_counter* counter,
    size_t* expected,
    size_t desired)
{
#if defined(_WIN32)
    const LONG64 previous = InterlockedCompareExchange64(
        (volatile LONG64*)counter,
        (LONG64)desired,
        (LONG64)*expected);
    if (previous == (LONG64)*expected)
    {
        return 1;
    }
    *expected = (size_t)previous;
    return 0;
#else
    return atomic_compare_exchange_weak_explicit(
        counter,
        expected,
        desired,
        memory_order_relaxed,
        memory_order_relaxed);
#endif
}

static void henka_memory_increment_allocation_count(void)
{
#if defined(_WIN32)
    (void)InterlockedIncrement64(&g_allocation_count);
#else
    (void)atomic_fetch_add_explicit(
        &g_allocation_count,
        1U,
        memory_order_relaxed);
#endif
}

static void henka_memory_decrement_allocation_count(void)
{
    size_t current_count = henka_memory_counter_load(&g_allocation_count);

    while (current_count > 0U &&
           !henka_memory_counter_compare_exchange(
               &g_allocation_count,
               &current_count,
               current_count - 1U))
    {
    }
}

static int henka_memory_test_should_fail(void)
{
    size_t remaining = henka_memory_counter_load(
        &g_test_allocations_before_failure);

    for (;;)
    {
        if (remaining == SIZE_MAX)
        {
            return 0;
        }
        if (remaining == 0U)
        {
            return 1;
        }
        if (henka_memory_counter_compare_exchange(
                &g_test_allocations_before_failure,
                &remaining,
                remaining - 1U))
        {
            return 0;
        }
    }
}

void henka_memory_test_fail_after(size_t successful_allocations)
{
    henka_memory_counter_store(
        &g_test_allocations_before_failure,
        successful_allocations);
}

void henka_memory_test_disable_failures(void)
{
    henka_memory_counter_store(
        &g_test_allocations_before_failure,
        SIZE_MAX);
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
        henka_memory_increment_allocation_count();
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
        henka_memory_increment_allocation_count();
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
            henka_memory_increment_allocation_count();
        }
        return resized;
    }

    if (size == 0U)
    {
        free(pointer);
        henka_memory_decrement_allocation_count();
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
        henka_memory_decrement_allocation_count();
    }
}

size_t henka_memory_get_allocation_count(void)
{
    return henka_memory_counter_load(&g_allocation_count);
}

void henka_memory_report_leaks(void)
{
    const size_t allocation_count = henka_memory_counter_load(&g_allocation_count);

    if (allocation_count > 0U)
    {
        HENKA_LOG_WARN("possible memory leak detected: %zu allocation(s) still active", allocation_count);
    }
    else
    {
        HENKA_LOG_INFO("memory shutdown clean: no active allocations tracked");
    }
}
