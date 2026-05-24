#ifndef HENKA_TEST_SUITE_H
#define HENKA_TEST_SUITE_H

#include <stdio.h>

extern int g_henka_test_failures;

#define HENKA_TEST_ASSERT(condition)                                                                    \
    do                                                                                                  \
    {                                                                                                   \
        if (!(condition))                                                                               \
        {                                                                                               \
            fprintf(stderr, "assertion failed at %s:%d: %s\n", __FILE__, __LINE__, #condition);       \
            ++g_henka_test_failures;                                                                    \
            return;                                                                                     \
        }                                                                                               \
    } while (0)

void henka_test_result(void);
void henka_test_memory(void);

#endif
