#ifndef SANDBOX3D_SCRIPT_EDITOR_H
#define SANDBOX3D_SCRIPT_EDITOR_H

#include <henka/result.h>
#include <henka/scene_document.h>
#include <henka/ui.h>

/* Draws a bounded, read-only source preview for one attached behavior. Lua
 * keeps the persisted source and indentation verbatim; HenkaScript syntax
 * colors come from the compiler's public lexer/token kinds. This surface does
 * not implement a second grammar or claim source editing. */
henka_result sandbox3d_script_editor_draw_preview(
    henka_ui_context* ui,
    henka_ui_rect bounds,
    const char* project_root,
    const henka_scene_document_behavior* behavior);

#endif
