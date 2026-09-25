#ifndef HENKA_SAVE_GAME_H
#define HENKA_SAVE_GAME_H

#include <stddef.h>
#include <stdint.h>

#include <henka/result.h>

#define HENKA_SAVE_GAME_FORMAT_VERSION UINT32_C(1)
#define HENKA_SAVE_GAME_SLOT_ID_BYTES 64U
#define HENKA_SAVE_GAME_DISPLAY_NAME_BYTES 128U
#define HENKA_SAVE_GAME_RELATIVE_PATH_BYTES 128U

typedef struct henka_save_game_slot_metadata
{
    uint32_t format_version;
    uint64_t sequence;
    char slot_id[HENKA_SAVE_GAME_SLOT_ID_BYTES];
    char display_name[HENKA_SAVE_GAME_DISPLAY_NAME_BYTES];
} henka_save_game_slot_metadata;

henka_save_game_slot_metadata henka_save_game_slot_metadata_default(void);
henka_result henka_save_game_slot_metadata_validate(
    const henka_save_game_slot_metadata* metadata);

/* Builds a confined project-relative slot path such as saves/slot_1.hsave.
 * This function does not create directories or write save data. */
henka_result henka_save_game_slot_build_relative_path(
    const henka_save_game_slot_metadata* metadata,
    char* out_path,
    size_t out_path_capacity);

#endif
