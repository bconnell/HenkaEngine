#ifndef HENKA_SCRIPT_SOURCE_H
#define HENKA_SCRIPT_SOURCE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <henka/result.h>
#include <henka/script.h>

#define HENKA_SCRIPT_SOURCE_MAX_DIAGNOSTIC_BYTES 192U

typedef struct henka_script_source_document henka_script_source_document;

typedef struct henka_script_source_diagnostic
{
    henka_result result;
    uint32_t line;
    uint32_t column;
    char message[HENKA_SCRIPT_SOURCE_MAX_DIAGNOSTIC_BYTES];
} henka_script_source_diagnostic;

henka_result henka_script_source_create(
    henka_script_language language,
    henka_script_source_document** out_document);
void henka_script_source_destroy(henka_script_source_document* document);

/* Accepts bounded text without requiring it to compile. This lets an editor
 * retain invalid work while validation and runtime activation remain fail
 * closed. The returned source pointer is borrowed from the document. */
henka_result henka_script_source_set_text(
    henka_script_source_document* document,
    const char* source,
    size_t source_size);
/* Replaces a bounded byte range. Offsets are UTF-8 source byte offsets; the
 * operation preserves all untouched whitespace and indentation. A null
 * replacement is valid only when replacement_size is zero. */
henka_result henka_script_source_replace_range(
    henka_script_source_document* document,
    size_t offset,
    size_t remove_size,
    const char* replacement,
    size_t replacement_size);
henka_result henka_script_source_get_text(
    const henka_script_source_document* document,
    const char** out_source,
    size_t* out_source_size);
henka_script_language henka_script_source_get_language(
    const henka_script_source_document* document);
/* Used by a persistence boundary only after the complete source has been
 * durably replaced. Editors should not call this before a successful save. */
henka_result henka_script_source_mark_clean(
    henka_script_source_document* document);

/* Validation delegates to the selected language backend. It does not retain
 * or activate the temporary compiled backend. */
henka_result henka_script_source_validate(
    henka_script_source_document* document,
    henka_script_source_diagnostic* out_diagnostic);
henka_result henka_script_source_get_diagnostic(
    const henka_script_source_document* document,
    henka_script_source_diagnostic* out_diagnostic);

bool henka_script_source_is_dirty(
    const henka_script_source_document* document);
uint64_t henka_script_source_get_revision(
    const henka_script_source_document* document);

#endif
