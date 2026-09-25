#include <henka/save_game.h>

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static bool henka_save_game_string_terminated(
    const char* value,
    size_t capacity)
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
            return true;
        }
    }
    return false;
}

static bool henka_save_game_slot_id_is_valid(const char* slot_id)
{
    size_t index;

    if (slot_id == NULL || slot_id[0] == '\0')
    {
        return false;
    }
    for (index = 0U; slot_id[index] != '\0'; ++index)
    {
        const unsigned char ch = (unsigned char)slot_id[index];
        if (!(isalnum(ch) || ch == '_' || ch == '-'))
        {
            return false;
        }
    }
    return true;
}

henka_save_game_slot_metadata henka_save_game_slot_metadata_default(void)
{
    henka_save_game_slot_metadata metadata;

    memset(&metadata, 0, sizeof(metadata));
    metadata.format_version = HENKA_SAVE_GAME_FORMAT_VERSION;
    return metadata;
}

henka_result henka_save_game_slot_metadata_validate(
    const henka_save_game_slot_metadata* metadata)
{
    if (metadata == NULL ||
        metadata->format_version != HENKA_SAVE_GAME_FORMAT_VERSION ||
        !henka_save_game_string_terminated(
            metadata->slot_id,
            HENKA_SAVE_GAME_SLOT_ID_BYTES) ||
        !henka_save_game_string_terminated(
            metadata->display_name,
            HENKA_SAVE_GAME_DISPLAY_NAME_BYTES) ||
        !henka_save_game_slot_id_is_valid(metadata->slot_id) ||
        metadata->display_name[0] == '\0')
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return HENKA_SUCCESS;
}

henka_result henka_save_game_slot_build_relative_path(
    const henka_save_game_slot_metadata* metadata,
    char* out_path,
    size_t out_path_capacity)
{
    int written;

    if (out_path != NULL && out_path_capacity > 0U)
    {
        out_path[0] = '\0';
    }
    if (out_path == NULL ||
        out_path_capacity == 0U ||
        henka_save_game_slot_metadata_validate(metadata) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    written = snprintf(
        out_path,
        out_path_capacity,
        "saves/%s.hsave",
        metadata->slot_id);
    if (written < 0 || (size_t)written >= out_path_capacity)
    {
        out_path[0] = '\0';
        return HENKA_ERROR_LIMIT;
    }
    return HENKA_SUCCESS;
}
