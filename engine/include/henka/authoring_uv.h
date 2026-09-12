#ifndef HENKA_AUTHORING_UV_H
#define HENKA_AUTHORING_UV_H

#include <stdbool.h>

#include <henka/authoring_mesh.h>

typedef enum henka_authoring_uv_projection_axis
{
    HENKA_AUTHORING_UV_PROJECT_X = 0,
    HENKA_AUTHORING_UV_PROJECT_Y,
    HENKA_AUTHORING_UV_PROJECT_Z
} henka_authoring_uv_projection_axis;

henka_result henka_authoring_mesh_project_face_uv(
    henka_authoring_mesh* mesh,
    henka_authoring_face_id face_id,
    henka_authoring_uv_projection_axis axis);
henka_result henka_authoring_mesh_transform_face_uv(
    henka_authoring_mesh* mesh,
    henka_authoring_face_id face_id,
    henka_vec2 scale,
    henka_vec2 offset);
henka_result henka_authoring_mesh_pack_face_uv(
    henka_authoring_mesh* mesh,
    henka_authoring_face_id face_id,
    float padding);
/* Applies one bounded UV transform to the complete topological island that
 * contains seed_face_id.  Faces are connected only across edges whose
 * per-corner UVs do not form a seam.  The candidate is published atomically. */
henka_result henka_authoring_mesh_transform_uv_island(
    henka_authoring_mesh* mesh,
    henka_authoring_face_id seed_face_id,
    henka_vec2 scale,
    henka_vec2 offset);
/* Packs the complete UV island that contains seed_face_id into the unit square
 * with the requested uniform padding.  Existing seams remain boundaries and
 * the operation is transactional. */
henka_result henka_authoring_mesh_pack_uv_island(
    henka_authoring_mesh* mesh,
    henka_authoring_face_id seed_face_id,
    float padding);
bool henka_authoring_mesh_face_uvs_are_finite(
    const henka_authoring_mesh* mesh,
    henka_authoring_face_id face_id);
bool henka_authoring_mesh_faces_share_uv_seam(
    const henka_authoring_mesh* mesh,
    henka_authoring_face_id first_face_id,
    henka_authoring_face_id second_face_id);

#endif
