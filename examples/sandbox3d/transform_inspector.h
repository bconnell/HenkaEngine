#ifndef SANDBOX3D_TRANSFORM_INSPECTOR_H
#define SANDBOX3D_TRANSFORM_INSPECTOR_H

#include <stdbool.h>
#include <stddef.h>

#include <henka/henka.h>

#define SANDBOX3D_TRANSFORM_INSPECTOR_FIELD_COUNT 9U
#define SANDBOX3D_TRANSFORM_INSPECTOR_FIELD_CAPACITY 32U

typedef enum sandbox3d_transform_inspector_row
{
    SANDBOX3D_TRANSFORM_INSPECTOR_POSITION = 0,
    SANDBOX3D_TRANSFORM_INSPECTOR_ROTATION,
    SANDBOX3D_TRANSFORM_INSPECTOR_SCALE,
    SANDBOX3D_TRANSFORM_INSPECTOR_ROW_COUNT
} sandbox3d_transform_inspector_row;

typedef struct sandbox3d_transform_inspector
{
    henka_entity entity;
    henka_transform baseline;
    char fields[SANDBOX3D_TRANSFORM_INSPECTOR_FIELD_COUNT]
        [SANDBOX3D_TRANSFORM_INSPECTOR_FIELD_CAPACITY];
    bool initialized;
    bool dirty;
} sandbox3d_transform_inspector;

void sandbox3d_transform_inspector_init(
    sandbox3d_transform_inspector* inspector);
henka_result sandbox3d_transform_inspector_sync(
    sandbox3d_transform_inspector* inspector,
    henka_entity entity,
    henka_transform authoritative,
    bool* out_pending_edits_refreshed);
void sandbox3d_transform_inspector_discard(
    sandbox3d_transform_inspector* inspector);
bool sandbox3d_transform_inspector_has_pending_edits(
    const sandbox3d_transform_inspector* inspector);
bool sandbox3d_transform_inspector_candidate_changes(
    const sandbox3d_transform_inspector* inspector,
    henka_transform candidate);
henka_result sandbox3d_transform_inspector_build_candidate(
    const sandbox3d_transform_inspector* inspector,
    henka_transform* out_candidate);
henka_result sandbox3d_transform_inspector_layout_row(
    henka_ui_rect bounds,
    henka_ui_rect* out_label,
    henka_ui_rect out_fields[3]);
henka_result sandbox3d_transform_inspector_layout_actions(
    henka_ui_rect bounds,
    bool editable,
    henka_ui_rect* out_label,
    henka_ui_rect* out_apply,
    henka_ui_rect* out_revert);
henka_result sandbox3d_transform_inspector_draw_axis_header(
    henka_ui_context* ui,
    henka_ui_rect bounds);
henka_result sandbox3d_transform_inspector_draw_row(
    sandbox3d_transform_inspector* inspector,
    henka_ui_context* ui,
    sandbox3d_transform_inspector_row row,
    henka_ui_rect bounds,
    bool editable);
henka_result sandbox3d_transform_inspector_draw_actions(
    const sandbox3d_transform_inspector* inspector,
    henka_ui_context* ui,
    henka_ui_rect bounds,
    bool editable,
    bool* out_apply,
    bool* out_discard);

#endif
