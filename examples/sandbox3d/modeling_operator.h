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
    SANDBOX3D_MODELING_OPERATOR_PROPORTIONAL_MOVE,
    /* Transactionally relaxes selected vertices toward their topological
     * neighbor average through Preview/Cancel/Apply. */
    SANDBOX3D_MODELING_OPERATOR_SMOOTH_VERTICES,
    SANDBOX3D_MODELING_OPERATOR_ADD_LOOSE_VERTEX,
    SANDBOX3D_MODELING_OPERATOR_ADD_LOOSE_EDGE,
    /* Transactional rotate/scale of the current component selection through
     * the shared candidate Preview/Cancel/Apply session. */
    SANDBOX3D_MODELING_OPERATOR_TRANSFORM,
    SANDBOX3D_MODELING_OPERATOR_EDGE_SLIDE,
    /* Transactionally splits one or a bounded pairwise-disjoint selection of
     * face-backed boundary or interior edges at a factor in (0,1) through the
     * shared Preview/Cancel/Apply session. */
    SANDBOX3D_MODELING_OPERATOR_SPLIT_EDGE,
    SANDBOX3D_MODELING_OPERATOR_BEVEL,
    /* Extrusion of selected vertices, one selected face, or one loose edge.
     * Loose components use the explicit operator axis; connected surface
     * components use their authoring-mesh normal contract. */
    SANDBOX3D_MODELING_OPERATOR_EXTRUDE,
    /* Surface-connected extrusion of one or a bounded batch of selected open
     * boundary edges. */
    SANDBOX3D_MODELING_OPERATOR_EDGE_EXTRUDE,
    /* Transactionally connects two non-adjacent selected vertices on one
     * compatible face through the shared Preview/Cancel/Apply session. */
    SANDBOX3D_MODELING_OPERATOR_CONNECT,
    /* Transactionally triangulates one selected planar face through the shared
     * Preview/Cancel/Apply session. */
    SANDBOX3D_MODELING_OPERATOR_TRIANGULATE,
    /* Transactionally insets one selected face through the shared
     * Preview/Cancel/Apply session. */
    SANDBOX3D_MODELING_OPERATOR_INSET,
    /* Transactionally moves one selected face along its evaluated normal
     * through the shared Preview/Cancel/Apply session. */
    SANDBOX3D_MODELING_OPERATOR_FACE_NORMAL,
    /* Transactionally reverses one selected face winding through the shared
     * Preview/Cancel/Apply session. */
    SANDBOX3D_MODELING_OPERATOR_FLIP_FACE,
    /* Transactionally removes the selected faces while preserving at least
     * one renderable face through the shared Preview/Cancel/Apply session. */
    SANDBOX3D_MODELING_OPERATOR_DELETE_FACES,
    /* Transactionally removes one selected edge and its incident faces through
     * the shared Preview/Cancel/Apply session. */
    SANDBOX3D_MODELING_OPERATOR_DELETE_EDGE,
    /* Transactionally dissolves one selected compatible interior edge through
     * the shared Preview/Cancel/Apply session. */
    SANDBOX3D_MODELING_OPERATOR_DISSOLVE_EDGE,
    /* Transactionally dissolves one or more selected vertices through the
     * shared Preview/Cancel/Apply session. */
    SANDBOX3D_MODELING_OPERATOR_DISSOLVE_VERTICES,
    /* Transactionally removes one or more selected vertices through the
     * shared Preview/Cancel/Apply session. */
    SANDBOX3D_MODELING_OPERATOR_DELETE_VERTICES,
    /* Transactionally merges selected vertices at their center through the
     * shared Preview/Cancel/Apply session. */
    SANDBOX3D_MODELING_OPERATOR_MERGE_VERTICES_CENTER,
    /* Transactionally merges selected vertices at the active vertex through
     * the shared Preview/Cancel/Apply session. */
    SANDBOX3D_MODELING_OPERATOR_MERGE_VERTICES_ACTIVE,
    /* Transactionally merges selected vertices within the configured distance
     * through the shared Preview/Cancel/Apply session. */
    SANDBOX3D_MODELING_OPERATOR_MERGE_VERTICES_DISTANCE,
    /* Transactionally subdivides one selected face through the shared
     * Preview/Cancel/Apply session. */
    SANDBOX3D_MODELING_OPERATOR_SUBDIVIDE,
    /* Transactional bridge of two compatible boundary edges or two compatible
     * equal-length open boundary-edge chains. */
    SANDBOX3D_MODELING_OPERATOR_EDGE_BRIDGE,
    /* Transactionally fills one selected closed boundary edge loop through the
     * shared Preview/Cancel/Apply session. */
    SANDBOX3D_MODELING_OPERATOR_FILL_BOUNDARY_LOOP,
    /* Transactional projection of one selected face onto a principal plane. */
    SANDBOX3D_MODELING_OPERATOR_UV_PROJECT,
    /* Transactional packing of one selected face into the padded unit square. */
    SANDBOX3D_MODELING_OPERATOR_UV_PACK,
    /* Transactional uniform scale of one selected face's UVs about the origin. */
    SANDBOX3D_MODELING_OPERATOR_UV_TRANSFORM,
    /* Transactional uniform scale of the UV island containing one selected face. */
    SANDBOX3D_MODELING_OPERATOR_UV_ISLAND_TRANSFORM,
    /* Transactional packing of the UV island containing one selected face. */
    SANDBOX3D_MODELING_OPERATOR_UV_ISLAND_PACK,
    /* Transactional packing of every UV island in the authoring mesh. */
    SANDBOX3D_MODELING_OPERATOR_UV_PACK_ALL,
    /* Transactional dominant-axis unwrap of connected planar UV islands. */
    SANDBOX3D_MODELING_OPERATOR_UV_UNWRAP_PLANAR,
    /* Transactionally toggles the explicit seam state of selected edges. */
    SANDBOX3D_MODELING_OPERATOR_UV_SEAM_TOGGLE
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
    henka_vec3 transform_scale;
    henka_vec3 transform_axis;
    float transform_radians;
    sandbox3d_authoring_pivot_mode transform_pivot_mode;
    sandbox3d_authoring_orientation_mode transform_orientation_mode;
    bool transform_configured;
    size_t proportional_ring_count;
    henka_vec3 loose_vertex_position;
    henka_vec2 loose_vertex_uv;
    uint32_t loose_vertex_material_region;
    henka_authoring_vertex_id loose_edge_first;
    henka_authoring_vertex_id loose_edge_second;
    bool loose_edge_hard;
    bool loose_configured;
    henka_authoring_edge_id split_first_edge;
    henka_authoring_edge_id split_second_edge;
    henka_authoring_vertex_id* split_vertex_ids;
    henka_authoring_edge_id* split_first_edges;
    henka_authoring_edge_id* split_second_edges;
    size_t split_result_count;
    bool split_configured;
    uint32_t created_component_id;
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
henka_result sandbox3d_modeling_operator_set_proportional_ring_count(
    sandbox3d_modeling_operator_session* session,
    size_t ring_count);
henka_result sandbox3d_modeling_operator_set_loose_vertex(
    sandbox3d_modeling_operator_session* session,
    henka_vec3 position,
    henka_vec2 uv,
    uint32_t material_region);
henka_result sandbox3d_modeling_operator_set_loose_edge(
    sandbox3d_modeling_operator_session* session,
    henka_authoring_vertex_id first,
    henka_authoring_vertex_id second,
    bool hard);
henka_result sandbox3d_modeling_operator_set_transform(
    sandbox3d_modeling_operator_session* session,
    henka_vec3 scale,
    henka_vec3 axis,
    float radians,
    sandbox3d_authoring_pivot_mode pivot_mode,
    sandbox3d_authoring_orientation_mode orientation_mode);
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
uint32_t sandbox3d_modeling_operator_get_created_component_id(
    const sandbox3d_modeling_operator_session* session);
henka_result sandbox3d_modeling_operator_cancel(
    sandbox3d_modeling_operator_session* session);

#endif
