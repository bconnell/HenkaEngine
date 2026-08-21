#ifndef SANDBOX3D_SCRIPT_EDITOR_H
#define SANDBOX3D_SCRIPT_EDITOR_H

#include <henka/result.h>
#include <henka/scene.h>
#include <henka/scene_document.h>
#include <henka/script_source.h>
#include <henka/ui.h>

#include "script_editor_model.h"

struct henka_engine;
typedef struct sandbox3d_game_authoring sandbox3d_game_authoring;

/* Draws a bounded source editor for one attached behavior. Lua keeps the
 * persisted source and indentation verbatim; HenkaScript syntax spans,
 * presentation classes, and insertion indentation come from compiler-owned
 * token APIs. This surface does not implement a second grammar. */
henka_result sandbox3d_script_editor_draw_preview(
    struct henka_engine* engine,
    henka_ui_context* ui,
    henka_ui_rect bounds,
    const char* project_root,
    const henka_scene_document_behavior* behavior,
    sandbox3d_game_authoring* authoring,
    henka_entity entity,
    sandbox3d_script_editor_model** io_model,
    bool play_active);

#endif
