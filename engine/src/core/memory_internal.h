#ifndef HENKA_MEMORY_INTERNAL_H
#define HENKA_MEMORY_INTERNAL_H

#include <stddef.h>

/* Internal single-threaded test control; production allocation is unchanged while disabled. */
void henka_memory_test_fail_after(size_t successful_allocations);
void henka_memory_test_disable_failures(void);

/* Temporary automation-only probe used to bracket one targeted free. */
void henka_memory_diagnostic_set_free_target(const void* pointer);
void henka_memory_diagnostic_check_heap(const char* stage);
void henka_memory_diagnostic_arm_heap_watch(size_t frame_checks);
void henka_memory_diagnostic_check_heap_if_armed(const char* stage);

#endif
