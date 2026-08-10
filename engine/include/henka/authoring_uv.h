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
bool henka_authoring_mesh_face_uvs_are_finite(
    const henka_authoring_mesh* mesh,
    henka_authoring_face_id face_id);
bool henka_authoring_mesh_faces_share_uv_seam(
    const henka_authoring_mesh* mesh,
    henka_authoring_face_id first_face_id,
    henka_authoring_face_id second_face_id);

#endif
