#ifndef HENKA_MESH_VALIDATION_H
#define HENKA_MESH_VALIDATION_H

#include "../henka_internal.h"

/* Validates the complete CPU-side payload owned by the Terrain upload path. */
henka_result henka_mesh_validate_terrain_upload_data(
    const henka_vertex* vertices,
    size_t vertex_count,
    const unsigned int* indices,
    size_t index_count);

#endif
