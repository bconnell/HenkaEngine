#ifndef HENKA_AUTHORING_MESH_INTERNAL_H
#define HENKA_AUTHORING_MESH_INTERNAL_H

#include <henka/authoring_mesh.h>

typedef struct henka_authoring_face_loop_update
{
    henka_authoring_face_id face_id;
    bool remove;
    const henka_authoring_vertex_id* vertices;
    const henka_vec2* uvs;
    size_t corner_count;
    uint32_t material_region;
    bool smooth;
} henka_authoring_face_loop_update;

/* Replaces selected face loops on a candidate mesh while preserving face IDs
 * and reconciling the complete active edge relation. All update storage is
 * borrowed for the duration of the call and remains caller-owned. */
henka_result henka_authoring_mesh_apply_face_loop_updates_internal(
    henka_authoring_mesh* mesh,
    const henka_authoring_face_loop_update* updates,
    size_t update_count);

#endif
