#ifndef HENKA_CRASH_CONTEXT_H
#define HENKA_CRASH_CONTEXT_H

#include <stddef.h>
#include <stdint.h>

#include <henka/result.h>

#define HENKA_CRASH_BREADCRUMB_CAPACITY 32U
#define HENKA_CRASH_BREADCRUMB_MESSAGE_BYTES 160U

typedef struct henka_crash_breadcrumb
{
    uint64_t sequence;
    char message[HENKA_CRASH_BREADCRUMB_MESSAGE_BYTES];
} henka_crash_breadcrumb;

typedef struct henka_crash_context
{
    henka_crash_breadcrumb entries[HENKA_CRASH_BREADCRUMB_CAPACITY];
    uint64_t next_sequence;
    uint32_t count;
    uint32_t write_index;
} henka_crash_context;

void henka_crash_context_reset(henka_crash_context* context);
henka_result henka_crash_context_push(
    henka_crash_context* context,
    const char* message);
size_t henka_crash_context_get_count(const henka_crash_context* context);

/* Returns breadcrumbs in oldest-to-newest order. The returned record is a
 * caller-owned copy. This module captures context only; it installs no crash
 * handler and performs no I/O. */
henka_result henka_crash_context_get(
    const henka_crash_context* context,
    size_t index,
    henka_crash_breadcrumb* out_breadcrumb);

#endif
