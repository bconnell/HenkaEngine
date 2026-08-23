#ifndef SANDBOX3D_MODELING_SELECTION_H
#define SANDBOX3D_MODELING_SELECTION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <henka/math.h>
#include <henka/result.h>

typedef enum sandbox3d_modeling_selection_operation
{
    SANDBOX3D_MODELING_SELECTION_REPLACE = 0,
    SANDBOX3D_MODELING_SELECTION_ADD,
    SANDBOX3D_MODELING_SELECTION_SUBTRACT
} sandbox3d_modeling_selection_operation;

typedef struct sandbox3d_modeling_selection_rect
{
    float minimum_x;
    float minimum_y;
    float maximum_x;
    float maximum_y;
} sandbox3d_modeling_selection_rect;

typedef struct sandbox3d_modeling_selection_session
{
    bool active;
    bool dragging;
    sandbox3d_modeling_selection_operation operation;
    henka_vec2 start;
    henka_vec2 current;
    float viewport_width;
    float viewport_height;
} sandbox3d_modeling_selection_session;

typedef struct sandbox3d_modeling_selection_candidate
{
    uint32_t component_id;
    henka_vec2 screen_position;
    float depth;
    bool front_facing;
    bool visible;
} sandbox3d_modeling_selection_candidate;

void sandbox3d_modeling_selection_reset(
    sandbox3d_modeling_selection_session* session);
sandbox3d_modeling_selection_operation
sandbox3d_modeling_selection_operation_from_modifiers(
    bool control_down,
    bool shift_down);
henka_result sandbox3d_modeling_selection_begin(
    sandbox3d_modeling_selection_session* session,
    henka_vec2 start,
    float viewport_width,
    float viewport_height,
    sandbox3d_modeling_selection_operation operation);
henka_result sandbox3d_modeling_selection_update(
    sandbox3d_modeling_selection_session* session,
    henka_vec2 current,
    float drag_threshold);
henka_result sandbox3d_modeling_selection_get_rect(
    const sandbox3d_modeling_selection_session* session,
    sandbox3d_modeling_selection_rect* out_rect);
henka_result sandbox3d_modeling_selection_build_result(
    const sandbox3d_modeling_selection_session* session,
    const sandbox3d_modeling_selection_candidate* candidates,
    size_t candidate_count,
    bool xray_enabled,
    bool front_facing_only,
    const uint32_t* prior_ids,
    size_t prior_count,
    uint32_t* out_ids,
    size_t output_capacity,
    size_t* out_count,
    uint32_t* out_active_id);

#endif
