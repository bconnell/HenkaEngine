#include "test_suite.h"

#include <string.h>

#include <henka/memory.h>

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
}
