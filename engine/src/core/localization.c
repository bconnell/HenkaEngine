#include <henka/localization.h>

#include <stdbool.h>
#include <string.h>

static bool henka_localization_string_is_bounded(
    const char* value,
    size_t capacity,
    bool require_nonempty)
{
    size_t index;

    if (value == NULL || capacity == 0U)
    {
        return false;
    }
    for (index = 0U; index < capacity; ++index)
    {
        if (value[index] == '\0')
        {
            return !require_nonempty || index > 0U;
        }
    }
    return false;
}

henka_result henka_localization_catalog_validate(
    const henka_localization_catalog* catalog)
{
    size_t index;

    if (catalog == NULL ||
        catalog->entry_count > HENKA_LOCALIZATION_MAX_ENTRIES ||
        (catalog->entry_count > 0U && catalog->entries == NULL))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    for (index = 0U; index < catalog->entry_count; ++index)
    {
        const henka_localization_entry* entry = &catalog->entries[index];
        if (!henka_localization_string_is_bounded(
                entry->key,
                HENKA_LOCALIZATION_MAX_KEY_BYTES,
                true) ||
            !henka_localization_string_is_bounded(
                entry->value,
                HENKA_LOCALIZATION_MAX_VALUE_BYTES,
                false))
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        if (index > 0U &&
            strcmp(catalog->entries[index - 1U].key, entry->key) >= 0)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
    }

    return HENKA_SUCCESS;
}

static const char* henka_localization_find(
    const henka_localization_catalog* catalog,
    const char* key)
{
    size_t low = 0U;
    size_t high = catalog->entry_count;

    while (low < high)
    {
        const size_t middle = low + (high - low) / 2U;
        const int comparison = strcmp(key, catalog->entries[middle].key);
        if (comparison == 0)
        {
            return catalog->entries[middle].value;
        }
        if (comparison < 0)
        {
            high = middle;
        }
        else
        {
            low = middle + 1U;
        }
    }
    return NULL;
}

henka_result henka_localization_lookup(
    const henka_localization_catalog* primary,
    const henka_localization_catalog* fallback,
    const char* key,
    const char** out_value)
{
    const char* value;

    if (out_value == NULL ||
        !henka_localization_string_is_bounded(
            key,
            HENKA_LOCALIZATION_MAX_KEY_BYTES,
            true) ||
        henka_localization_catalog_validate(primary) != HENKA_SUCCESS ||
        (fallback != NULL &&
            henka_localization_catalog_validate(fallback) != HENKA_SUCCESS))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    value = henka_localization_find(primary, key);
    if (value == NULL && fallback != NULL)
    {
        value = henka_localization_find(fallback, key);
    }
    if (value == NULL)
    {
        return HENKA_ERROR_ASSET_SOURCE;
    }

    *out_value = value;
    return HENKA_SUCCESS;
}
