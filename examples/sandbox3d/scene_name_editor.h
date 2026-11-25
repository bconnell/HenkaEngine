#ifndef HENKA_SANDBOX3D_SCENE_NAME_EDITOR_H
#define HENKA_SANDBOX3D_SCENE_NAME_EDITOR_H

#include <stdbool.h>

#include <henka/scene_document.h>
#include <henka/ui.h>

#include "editor_layout.h"
#include "game_authoring.h"

typedef struct sandbox3d_scene_name_editor
{
    char edit_buffer[HENKA_SCENE_DOCUMENT_MAX_NAME_BYTES];
    char observed_scene_name[HENKA_SCENE_DOCUMENT_MAX_NAME_BYTES];
    bool initialized;
} sandbox3d_scene_name_editor;

/* Clear only transient editor text so the next draw refreshes from the
 * canonical Scene Document after an explicit scene reload. */
void sandbox3d_scene_name_editor_reset(
    sandbox3d_scene_name_editor* editor);

/* Refresh the draft only when the canonical scene name changes. Unsaved text
 * remains intact while the authoritative name is unchanged. */
henka_result sandbox3d_scene_name_editor_sync(
    sandbox3d_scene_name_editor* editor,
    const char* canonical_scene_name);

/* Draw and apply the Scene Objects name editor through Game Authoring. The
 * Scene Document remains the sole persisted name authority. */
henka_result sandbox3d_scene_name_editor_draw(
    sandbox3d_scene_name_editor* editor,
    henka_ui_context* ui,
    sandbox3d_game_authoring* authoring,
    const sandbox3d_editor_scene_objects_layout* layout,
    bool* out_applied);

#endif
