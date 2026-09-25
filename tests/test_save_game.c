#include <string.h>

#include <henka/save_game.h>

int main(void)
{
    henka_save_game_slot_metadata metadata =
        henka_save_game_slot_metadata_default();
    henka_save_game_slot_metadata invalid;
    henka_save_game_slot_catalog catalog;
    const henka_save_game_slot_metadata* found;
    char path[HENKA_SAVE_GAME_RELATIVE_PATH_BYTES];
    char tiny[8U];

    (void)snprintf(
        metadata.slot_id,
        sizeof(metadata.slot_id),
        "%s",
        "slot_01");
    (void)snprintf(
        metadata.display_name,
        sizeof(metadata.display_name),
        "%s",
        "Checkpoint One");
    metadata.sequence = 42U;

    if (henka_save_game_slot_metadata_validate(&metadata) != HENKA_SUCCESS ||
        henka_save_game_slot_build_relative_path(
            &metadata, path, sizeof(path)) != HENKA_SUCCESS ||
        strcmp(path, "saves/slot_01.hsave") != 0)
    {
        return 1;
    }

    invalid = metadata;
    (void)snprintf(
        invalid.slot_id,
        sizeof(invalid.slot_id),
        "%s",
        "../slot");
    if (henka_save_game_slot_metadata_validate(&invalid) == HENKA_SUCCESS)
    {
        return 2;
    }

    invalid = metadata;
    invalid.format_version = HENKA_SAVE_GAME_FORMAT_VERSION + 1U;
    if (henka_save_game_slot_metadata_validate(&invalid) == HENKA_SUCCESS)
    {
        return 3;
    }

    invalid = metadata;
    memset(invalid.display_name, 'x', sizeof(invalid.display_name));
    if (henka_save_game_slot_metadata_validate(&invalid) == HENKA_SUCCESS)
    {
        return 4;
    }

    memset(tiny, 0x5a, sizeof(tiny));
    if (henka_save_game_slot_build_relative_path(
            &metadata, tiny, sizeof(tiny)) != HENKA_ERROR_LIMIT ||
        tiny[0] != '\0')
    {
        return 5;
    }


    henka_save_game_slot_catalog_reset(&catalog);
    if (catalog.count != 0U ||
        henka_save_game_slot_catalog_upsert(&catalog, &metadata) != HENKA_SUCCESS ||
        catalog.count != 1U)
    {
        return 6;
    }

    found = henka_save_game_slot_catalog_find(&catalog, "slot_01");
    if (found == NULL || found->sequence != 42U)
    {
        return 7;
    }

    metadata.sequence = 43U;
    (void)snprintf(
        metadata.display_name,
        sizeof(metadata.display_name),
        "%s",
        "Checkpoint One Updated");
    if (henka_save_game_slot_catalog_upsert(&catalog, &metadata) != HENKA_SUCCESS ||
        catalog.count != 1U)
    {
        return 8;
    }
    found = henka_save_game_slot_catalog_find(&catalog, "slot_01");
    if (found == NULL ||
        found->sequence != 43U ||
        strcmp(found->display_name, "Checkpoint One Updated") != 0)
    {
        return 9;
    }

    if (henka_save_game_slot_catalog_remove(&catalog, "slot_01") != HENKA_SUCCESS ||
        catalog.count != 0U ||
        henka_save_game_slot_catalog_find(&catalog, "slot_01") != NULL ||
        henka_save_game_slot_catalog_remove(&catalog, "slot_01") == HENKA_SUCCESS)
    {
        return 10;
    }

    return 0;
}
