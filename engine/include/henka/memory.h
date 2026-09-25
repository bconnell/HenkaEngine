#ifndef HENKA_MEMORY_H
#define HENKA_MEMORY_H

#include <stddef.h>
#include <stdint.h>

typedef struct henka_memory_diagnostics
{
    size_t active_allocation_count;
    size_t peak_active_allocation_count;
    uint64_t successful_allocation_count;
} henka_memory_diagnostics;

void* henka_malloc(size_t size);
void* henka_calloc(size_t count, size_t size);
void* henka_realloc(void* pointer, size_t size);
void henka_free(void* pointer);
size_t henka_memory_get_allocation_count(void);
void henka_memory_get_diagnostics(henka_memory_diagnostics* out_diagnostics);
void henka_memory_report_leaks(void);

#endif
