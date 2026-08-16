#include "test_suite.h"

#include <string.h>

#include <henka/memory.h>

#if defined(_WIN32)
#include <windows.h>

#define HENKA_MEMORY_THREAD_COUNT 32U
#define HENKA_MEMORY_BLOCKS_PER_THREAD 1024U

typedef struct henka_memory_thread_context
{
    HANDLE release_event;
    volatile LONG ready_count;
    volatile LONG allocation_failures;
} henka_memory_thread_context;

static DWORD WINAPI henka_memory_concurrent_worker(void* argument)
{
    henka_memory_thread_context* context = (henka_memory_thread_context*)argument;
    void* blocks[HENKA_MEMORY_BLOCKS_PER_THREAD];
    size_t block_count = 0U;

    memset(blocks, 0, sizeof(blocks));
    for (block_count = 0U; block_count < HENKA_MEMORY_BLOCKS_PER_THREAD; ++block_count)
    {
        blocks[block_count] = henka_malloc(32U);
        if (blocks[block_count] == NULL)
        {
            InterlockedIncrement(&context->allocation_failures);
            break;
        }
    }

    InterlockedIncrement(&context->ready_count);
    (void)WaitForSingleObject(context->release_event, INFINITE);

    while (block_count > 0U)
    {
        --block_count;
        henka_free(blocks[block_count]);
    }
    return 0U;
}

static void henka_test_memory_concurrent_accounting(void)
{
    henka_memory_thread_context context;
    HANDLE threads[HENKA_MEMORY_THREAD_COUNT];
    size_t thread_count = 0U;
    size_t before_count;
    size_t expected_count;
    ULONGLONG deadline;

    memset(&context, 0, sizeof(context));
    memset(threads, 0, sizeof(threads));
    context.release_event = CreateEventW(NULL, TRUE, FALSE, NULL);
    HENKA_TEST_ASSERT(context.release_event != NULL);

    before_count = henka_memory_get_allocation_count();
    for (thread_count = 0U; thread_count < HENKA_MEMORY_THREAD_COUNT; ++thread_count)
    {
        threads[thread_count] = CreateThread(
            NULL,
            0U,
            henka_memory_concurrent_worker,
            &context,
            0U,
            NULL);
        if (threads[thread_count] == NULL)
        {
            break;
        }
    }

    deadline = GetTickCount64() + 5000ULL;
    while ((size_t)InterlockedCompareExchange(&context.ready_count, 0L, 0L) < thread_count &&
           GetTickCount64() < deadline)
    {
        Sleep(1U);
    }

    expected_count = before_count +
        thread_count * HENKA_MEMORY_BLOCKS_PER_THREAD;
    if ((size_t)InterlockedCompareExchange(&context.ready_count, 0L, 0L) != thread_count ||
        InterlockedCompareExchange(&context.allocation_failures, 0L, 0L) != 0L ||
        henka_memory_get_allocation_count() != expected_count)
    {
        InterlockedExchange(&context.allocation_failures, 1L);
    }

    SetEvent(context.release_event);
    if (thread_count > 0U)
    {
        if (WaitForMultipleObjects((DWORD)thread_count, threads, TRUE, 5000U) != WAIT_OBJECT_0)
        {
            /* Never close the event or stack context while a worker can still use them. */
            (void)WaitForMultipleObjects((DWORD)thread_count, threads, TRUE, INFINITE);
        }
    }
    for (thread_count = 0U; thread_count < HENKA_MEMORY_THREAD_COUNT; ++thread_count)
    {
        if (threads[thread_count] != NULL)
        {
            CloseHandle(threads[thread_count]);
        }
    }
    CloseHandle(context.release_event);

    HENKA_TEST_ASSERT(InterlockedCompareExchange(&context.allocation_failures, 0L, 0L) == 0L);
    HENKA_TEST_ASSERT(henka_memory_get_allocation_count() == before_count);
}
#endif

void henka_test_memory(void)
{
    void* block;
    void* resized;
    size_t before_count;

    before_count = henka_memory_get_allocation_count();

    block = henka_malloc(16U);
    HENKA_TEST_ASSERT(block != NULL);
    HENKA_TEST_ASSERT(henka_memory_get_allocation_count() == before_count + 1U);

    memset(block, 0xAB, 16U);

    resized = henka_realloc(block, 32U);
    HENKA_TEST_ASSERT(resized != NULL);
    HENKA_TEST_ASSERT(henka_memory_get_allocation_count() == before_count + 1U);

    henka_free(resized);
    HENKA_TEST_ASSERT(henka_memory_get_allocation_count() == before_count);

    block = henka_calloc(4U, sizeof(int));
    HENKA_TEST_ASSERT(block != NULL);
    HENKA_TEST_ASSERT(henka_memory_get_allocation_count() == before_count + 1U);

    henka_free(block);
    HENKA_TEST_ASSERT(henka_memory_get_allocation_count() == before_count);

#if defined(_WIN32)
    henka_test_memory_concurrent_accounting();
#endif
}
