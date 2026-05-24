#ifndef HENKA_MEMORY_H
#define HENKA_MEMORY_H

#include <stddef.h>

void* henka_malloc(size_t size);
void* henka_calloc(size_t count, size_t size);
void* henka_realloc(void* pointer, size_t size);
void henka_free(void* pointer);
size_t henka_memory_get_allocation_count(void);
void henka_memory_report_leaks(void);

#endif
