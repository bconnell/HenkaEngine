#ifndef HENKA_MODEL_OBJ_INTERNAL_H
#define HENKA_MODEL_OBJ_INTERNAL_H

#include <stdbool.h>

#include <henka/model.h>

bool henka_obj_projected_segments_intersect(
    const henka_model_vertex* a,
    const henka_model_vertex* b,
    const henka_model_vertex* c,
    const henka_model_vertex* d,
    int dropped_axis);

#endif
