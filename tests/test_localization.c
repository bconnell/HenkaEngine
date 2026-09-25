#include <string.h>

#include <henka/localization.h>

int main(void)
{
    const henka_localization_entry primary_entries[] = {
        {"menu.exit", "Quitter"},
        {"menu.play", "Jouer"}};
    const henka_localization_entry fallback_entries[] = {
        {"menu.exit", "Exit"},
        {"menu.options", "Options"},
        {"menu.play", "Play"}};
    const henka_localization_entry duplicate_entries[] = {
        {"menu.play", "One"},
        {"menu.play", "Two"}};
    const henka_localization_entry unsorted_entries[] = {
        {"menu.play", "Play"},
        {"menu.exit", "Exit"}};
    const henka_localization_catalog primary = {
        primary_entries,
        sizeof(primary_entries) / sizeof(primary_entries[0])};
    const henka_localization_catalog fallback = {
        fallback_entries,
        sizeof(fallback_entries) / sizeof(fallback_entries[0])};
    const henka_localization_catalog duplicate = {
        duplicate_entries,
        sizeof(duplicate_entries) / sizeof(duplicate_entries[0])};
    const henka_localization_catalog unsorted = {
        unsorted_entries,
        sizeof(unsorted_entries) / sizeof(unsorted_entries[0])};
    const henka_localization_catalog empty = {NULL, 0U};
    const char* value = "unchanged";

    if (henka_localization_catalog_validate(&primary) != HENKA_SUCCESS ||
        henka_localization_catalog_validate(&fallback) != HENKA_SUCCESS ||
        henka_localization_catalog_validate(&empty) != HENKA_SUCCESS ||
        henka_localization_catalog_validate(&duplicate) !=
            HENKA_ERROR_INVALID_ARGUMENT ||
        henka_localization_catalog_validate(&unsorted) !=
            HENKA_ERROR_INVALID_ARGUMENT)
    {
        return 1;
    }

    if (henka_localization_lookup(
            &primary, &fallback, "menu.play", &value) != HENKA_SUCCESS ||
        strcmp(value, "Jouer") != 0 ||
        henka_localization_lookup(
            &primary, &fallback, "menu.options", &value) != HENKA_SUCCESS ||
        strcmp(value, "Options") != 0)
    {
        return 1;
    }

    value = "unchanged";
    if (henka_localization_lookup(
            &primary, &fallback, "menu.missing", &value) !=
            HENKA_ERROR_ASSET_SOURCE ||
        strcmp(value, "unchanged") != 0 ||
        henka_localization_lookup(
            &duplicate, &fallback, "menu.play", &value) !=
            HENKA_ERROR_INVALID_ARGUMENT ||
        strcmp(value, "unchanged") != 0 ||
        henka_localization_lookup(
            &primary, &fallback, "", &value) != HENKA_ERROR_INVALID_ARGUMENT)
    {
        return 1;
    }

    return 0;
}
