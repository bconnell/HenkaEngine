#ifndef HENKA_MEMORY_INTERNAL_H
#define HENKA_MEMORY_INTERNAL_H

#include <stddef.h>

/* Internal single-threaded test control; production allocation is unchanged while disabled. */
void henka_memory_test_fail_after(size_t successful_allocations);
void henka_memory_test_disable_failures(void);

#endif
