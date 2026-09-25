#ifndef HENKA_LOCALIZATION_H
#define HENKA_LOCALIZATION_H

#include <stddef.h>

#include <henka/result.h>

#define HENKA_LOCALIZATION_MAX_ENTRIES 4096U
#define HENKA_LOCALIZATION_MAX_KEY_BYTES 128U
#define HENKA_LOCALIZATION_MAX_VALUE_BYTES 4096U

typedef struct henka_localization_entry
{
    const char* key;
    const char* value;
} henka_localization_entry;

typedef struct henka_localization_catalog
{
    const henka_localization_entry* entries;
    size_t entry_count;
} henka_localization_catalog;

/* Catalogs are caller-owned immutable arrays sorted by key in ascending byte
 * order with unique non-empty keys. Values may be empty strings but must be
 * bounded UTF-8 byte sequences. The returned value remains caller-owned. */
henka_result henka_localization_catalog_validate(
    const henka_localization_catalog* catalog);

/* Looks up primary first and optional fallback second. Missing keys return
 * HENKA_ERROR_ASSET_SOURCE and leave out_value unchanged. */
henka_result henka_localization_lookup(
    const henka_localization_catalog* primary,
    const henka_localization_catalog* fallback,
    const char* key,
    const char** out_value);

#endif
