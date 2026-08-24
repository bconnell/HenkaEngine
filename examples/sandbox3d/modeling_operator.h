#ifndef SANDBOX3D_MODELING_OPERATOR_H
#define SANDBOX3D_MODELING_OPERATOR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "object_authoring_tools.h"

#define SANDBOX3D_MODELING_OPERATOR_NUMERIC_CAPACITY 32U

typedef enum sandbox3d_modeling_operator_kind
{
    SANDBOX3D_MODELING_OPERATOR_NONE = 0,
    SANDBOX3D_MODELING_OPERATOR_MOVE,
    SANDBOX3D_MODELING_OPERATOR_EDGE_SLIDE,
    SANDBOX3D_MODELING_OPERATOR_BEVEL,
    /* Explicit-axis extrusion of one selected loose vertex or edge. */
    SANDBOX3D_MODELING_OPERATOR_EXTRUDE
} sandbox3d_modeling_operator_kind;

typedef enum sandbox3d_modeling_operator_state
{
    SANDBOX3D_MODELING_OPERATOR_STATE_IDLE = 0,
    SANDBOX3D_MODELING_OPERATOR_STATE_BEGIN,
    SANDBOX3D_MODELING_OPERATOR_STATE_PREVIEW
} sandbox3d_modeling_operator_state;

typedef enum sandbox3d_modeling_operator_axis
{
    SANDBOX3D_MODELING_OPERATOR_AXIS_NONE = 0,
    SANDBOX3D_MODELING_OPERATOR_AXIS_X,
    SANDBOX3D_MODELING_OPERATOR_AXIS_Y,
    SANDBOX3D_MODELING_OPERATOR_AXIS_Z
} sandbox3d_modeling_operator_axis;

/* Coordinates one bounded direct-modeling transaction.  The authoritative
 * source stays in the authoring bridge; this session owns only its captured
 * source snapshot and selection snapshot while a candidate is previewed. */
typedef struct sandbox3d_modeling_operator_session
{
    bool active;
    sandbox3d_modeling_operator_state state;
    sandbox3d_modeling_operator_kind kind;
    sandbox3d_modeling_operator_axis axis;
    sandbox3d_authoring_object* object;
    sandbox3d_authoring_selection_mode selection_mode;
    uint32_t active_component_id;
    uint32_t* selection_ids;
    size_t selection_count;
    size_t selection_capacity;
    henka_authoring_mesh* source_snapshot;
    float amount;
    size_t preview_rebuild_count;
    bool numeric_active;
    char numeric_text[SANDBOX3D_MODELING_OPERATOR_NUMERIC_CAPACITY];
    size_t numeric_length;
} sandbox3d_modeling_operator_session;

void sandbox3d_modeling_operator_reset(
    sandbox3d_modeling_operator_session* session);
henka_result sandbox3d_modeling_operator_begin(
    sandbox3d_modeling_operator_session* session,
    sandbox3d_authoring_object* object,
    sandbox3d_modeling_operator_kind kind);
henka_result sandbox3d_modeling_operator_set_axis(
    sandbox3d_modeling_operator_session* session,
    sandbox3d_modeling_operator_axis axis);
henka_result sandbox3d_modeling_operator_numeric_begin(
    sandbox3d_modeling_operator_session* session);
henka_result sandbox3d_modeling_operator_numeric_append(
    sandbox3d_modeling_operator_session* session,
    const char* text,
    size_t text_size);
henka_result sandbox3d_modeling_operator_numeric_backspace(
    sandbox3d_modeling_operator_session* session);
henka_result sandbox3d_modeling_operator_numeric_commit(
    sandbox3d_modeling_operator_session* session);
const char* sandbox3d_modeling_operator_get_numeric_text(
    const sandbox3d_modeling_operator_session* session);
henka_result sandbox3d_modeling_operator_preview(
    sandbox3d_modeling_operator_session* session,
    float delta,
    bool snap_active,
    bool fine_active);
henka_result sandbox3d_modeling_operator_commit(
    sandbox3d_modeling_operator_session* session);
henka_result sandbox3d_modeling_operator_cancel(
    sandbox3d_modeling_operator_session* session);

#endif
