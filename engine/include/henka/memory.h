#ifndef HENKA_MEMORY_H
#define HENKA_MEMORY_H

#include <stddef.h>

typedef struct henka_memory_diagnostics
{
    size_t active_allocations;
    size_t peak_allocations;
    /* Successful allocation creations since process start. Reallocating an
     * existing live block does not increment this value. */
    size_t total_allocations;
} henka_memory_diagnostics;

void* henka_malloc(size_t size);
void* henka_calloc(size_t count, size_t size);
void* henka_realloc(void* pointer, size_t size);
void henka_free(void* pointer);
size_t henka_memory_get_allocation_count(void);
henka_memory_diagnostics henka_memory_get_diagnostics(void);
void henka_memory_report_leaks(void);

#endif
