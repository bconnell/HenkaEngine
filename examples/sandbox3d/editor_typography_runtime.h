#ifndef SANDBOX3D_EDITOR_TYPOGRAPHY_RUNTIME_H
#define SANDBOX3D_EDITOR_TYPOGRAPHY_RUNTIME_H

#include <stdbool.h>

#include <henka/engine.h>
#include <henka/persistence.h>
#include <henka/ui.h>

#include "workspace_tools.h"

typedef struct sandbox3d_editor_typography_runtime
    sandbox3d_editor_typography_runtime;

/* Creates the production typography owner and installs its verified bundled
 * font into the main editor context before workspace restoration. */
henka_result sandbox3d_editor_typography_runtime_initialize(
    sandbox3d_editor_typography_runtime** out_runtime,
    henka_engine* engine,
    henka_settings* settings,
    henka_ui_context* main_context,
    henka_ui_context* native_context,
    henka_ui_context* detached_contexts[SANDBOX3D_WORKSPACE_PANEL_COUNT],
    const char* asset_base_path,
    const char* settings_path);

/* Attach/detach a window context around the existing engine tool-window
 * lifecycle. Failed attachment leaves the caller responsible for closing the
 * just-opened window; the bounded profile remains available for a retry. */
henka_result sandbox3d_editor_typography_runtime_attach_context(
    sandbox3d_editor_typography_runtime* runtime,
    henka_ui_context* context,
    henka_window_id window_id);
void sandbox3d_editor_typography_runtime_detach_context(
    sandbox3d_editor_typography_runtime* runtime,
    henka_ui_context* context);

/* Reconcile the active context profiles with live OS display scale. A failed
 * replacement retains the previous complete profile/layout state. */
henka_result sandbox3d_editor_typography_runtime_sync_display_scales(
    sandbox3d_editor_typography_runtime* runtime);

henka_result sandbox3d_editor_typography_runtime_draw_scale_control(
    sandbox3d_editor_typography_runtime* runtime,
    henka_ui_context* ui,
    henka_ui_rect bounds);
/* Publishes a UI-requested scale change at the next pre-UI-frame update
 * boundary, after all contexts have ended the frame that received the input. */
henka_result sandbox3d_editor_typography_runtime_apply_pending_scale(
    sandbox3d_editor_typography_runtime* runtime);

int sandbox3d_editor_typography_runtime_get_scale_percent(
    const sandbox3d_editor_typography_runtime* runtime);
float sandbox3d_editor_typography_runtime_get_layout_scale(
    const sandbox3d_editor_typography_runtime* runtime);
const char* sandbox3d_editor_typography_runtime_failure_stage_label(int stage);

/* UI contexts are borrowed. Destroy them before releasing this owner. */
void sandbox3d_editor_typography_runtime_destroy(
    sandbox3d_editor_typography_runtime* runtime);

#endif
