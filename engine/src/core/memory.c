#include <henka/memory.h>

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#if defined(_WIN32)
#include <windows.h>
#if defined(_DEBUG)
#include <crtdbg.h>
#endif
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
static const void* g_diagnostic_free_target = NULL;
static size_t g_diagnostic_heap_watch_remaining = 0U;
#if defined(_WIN32) && defined(_DEBUG)
static int g_diagnostic_heap_corruption_reported = 0;
#endif

void henka_memory_diagnostic_set_free_target(const void* pointer)
{
    g_diagnostic_free_target = pointer;
}

void henka_memory_diagnostic_check_heap(const char* stage)
{
#if defined(_WIN32) && defined(_DEBUG)
    static const int report_types[] = {_CRT_WARN, _CRT_ERROR, _CRT_ASSERT};
    int previous_modes[sizeof(report_types) / sizeof(report_types[0])];
    _HFILE previous_files[sizeof(report_types) / sizeof(report_types[0])];
    char enabled[2] = {0};
    size_t required_size = 0U;
    size_t report_index;
    int heap_is_valid;

    if (g_diagnostic_heap_corruption_reported ||
        getenv_s(
            &required_size,
            enabled,
            sizeof(enabled),
            "HENKA_AUTOMATION_DIAGNOSTICS") != 0 ||
        required_size != sizeof(enabled) || enabled[0] != '1' || enabled[1] != '\0')
    {
        return;
    }
    (void)printf("HENKA_AUTOMATION_DIAGNOSTIC heap-check stage=%s status=begin\n", stage != NULL ? stage : "unknown");
    (void)fflush(stdout);
    for (report_index = 0U;
         report_index < sizeof(report_types) / sizeof(report_types[0]);
         ++report_index)
    {
        previous_files[report_index] = _CrtSetReportFile(
            report_types[report_index], _CRTDBG_FILE_STDERR);
        previous_modes[report_index] = _CrtSetReportMode(
            report_types[report_index], _CRTDBG_MODE_FILE);
    }
    heap_is_valid = _CrtCheckMemory();
    for (report_index = sizeof(report_types) / sizeof(report_types[0]);
         report_index > 0U;
         --report_index)
    {
        _CrtSetReportMode(report_types[report_index - 1U], previous_modes[report_index - 1U]);
        (void)_CrtSetReportFile(
            report_types[report_index - 1U], previous_files[report_index - 1U]);
    }
    (void)printf(
        "HENKA_AUTOMATION_DIAGNOSTIC heap-check stage=%s status=end heap_valid=%d\n",
        stage != NULL ? stage : "unknown",
        heap_is_valid);
    (void)fflush(stdout);
    if (!heap_is_valid)
    {
        g_diagnostic_heap_corruption_reported = 1;
        g_diagnostic_heap_watch_remaining = 0U;
    }
#else
    (void)stage;
#endif
}

void henka_memory_diagnostic_arm_heap_watch(size_t frame_checks)
{
#if defined(_WIN32) && defined(_DEBUG)
    char enabled[2] = {0};
    size_t required_size = 0U;
    if (!g_diagnostic_heap_corruption_reported &&
        getenv_s(
            &required_size,
            enabled,
            sizeof(enabled),
            "HENKA_AUTOMATION_DIAGNOSTICS") == 0 &&
        required_size == sizeof(enabled) && enabled[0] == '1' && enabled[1] == '\0')
    {
        g_diagnostic_heap_watch_remaining = frame_checks;
    }
    else
    {
        g_diagnostic_heap_watch_remaining = 0U;
    }
#else
    (void)frame_checks;
#endif
}

void henka_memory_diagnostic_check_heap_if_armed(const char* stage)
{
    if (g_diagnostic_heap_watch_remaining > 0U)
    {
        --g_diagnostic_heap_watch_remaining;
        henka_memory_diagnostic_check_heap(stage);
    }
}

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
        const int trace_free = pointer == g_diagnostic_free_target;
        if (trace_free)
        {
            g_diagnostic_free_target = NULL;
            (void)printf("HENKA_AUTOMATION_DIAGNOSTIC allocator-free stage=raw-free-begin pointer=%p\n", pointer);
            (void)fflush(stdout);
        }
        free(pointer);
        if (trace_free)
        {
            (void)printf("HENKA_AUTOMATION_DIAGNOSTIC allocator-free stage=raw-free-end pointer=%p\n", pointer);
            (void)fflush(stdout);
            (void)printf("HENKA_AUTOMATION_DIAGNOSTIC allocator-free stage=count-decrement-begin pointer=%p\n", pointer);
            (void)fflush(stdout);
        }
        henka_memory_decrement_allocation_count();
        if (trace_free)
        {
            (void)printf("HENKA_AUTOMATION_DIAGNOSTIC allocator-free stage=count-decrement-end pointer=%p\n", pointer);
            (void)fflush(stdout);
        }
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
