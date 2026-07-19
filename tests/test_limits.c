#include "test_suite.h"

#include <limits.h>
#include <stdint.h>

#include "../engine/src/core/checked.h"

void henka_test_limits(void)
{
    int int_value;
    size_t capacity;
    size_t size_value;
    uint32_t u32_value;

    HENKA_TEST_ASSERT(henka_checked_size_add(4U, 8U, &size_value));
    HENKA_TEST_ASSERT(size_value == 12U);
    HENKA_TEST_ASSERT(!henka_checked_size_add(SIZE_MAX, 1U, &size_value));

    HENKA_TEST_ASSERT(henka_checked_size_multiply(16U, 32U, &size_value));
    HENKA_TEST_ASSERT(size_value == 512U);
    HENKA_TEST_ASSERT(!henka_checked_size_multiply(SIZE_MAX, 2U, &size_value));

    HENKA_TEST_ASSERT(henka_checked_capacity(0U, 9U, 8U, 64U, &capacity));
    HENKA_TEST_ASSERT(capacity == 16U);
    HENKA_TEST_ASSERT(henka_checked_capacity(16U, 16U, 8U, 64U, &capacity));
    HENKA_TEST_ASSERT(capacity == 16U);
    HENKA_TEST_ASSERT(!henka_checked_capacity(32U, 65U, 8U, 64U, &capacity));

    HENKA_TEST_ASSERT(henka_checked_size_to_int((size_t)INT_MAX, &int_value));
    HENKA_TEST_ASSERT(int_value == INT_MAX);
#if SIZE_MAX > INT_MAX
    HENKA_TEST_ASSERT(!henka_checked_size_to_int((size_t)INT_MAX + 1U, &int_value));
#endif

    HENKA_TEST_ASSERT(henka_checked_size_to_u32((size_t)UINT32_MAX, &u32_value));
    HENKA_TEST_ASSERT(u32_value == UINT32_MAX);
#if SIZE_MAX > UINT32_MAX
    HENKA_TEST_ASSERT(!henka_checked_size_to_u32((size_t)UINT32_MAX + 1U, &u32_value));
#endif

    HENKA_TEST_ASSERT(henka_checked_rgba8_size(1024, 1024, &size_value));
    HENKA_TEST_ASSERT(size_value == 4194304U);
    HENKA_TEST_ASSERT(henka_checked_rgba8_size(8192, 8192, &size_value));
    HENKA_TEST_ASSERT(size_value == HENKA_MAX_TEXTURE_DECODED_BYTES);
    HENKA_TEST_ASSERT(!henka_checked_rgba8_size(16384, 16384, &size_value));
    HENKA_TEST_ASSERT(!henka_checked_rgba8_size(0, 1, &size_value));
    HENKA_TEST_ASSERT(!henka_checked_rgba8_size(HENKA_MAX_TEXTURE_DIMENSION + 1, 1, &size_value));
}
