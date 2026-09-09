#include "reflection_probe_policy.h"

bool henka_reflection_probe_desc_equal(
    const henka_scene_reflection_probe_desc* left,
    const henka_scene_reflection_probe_desc* right)
{
    return left != NULL && right != NULL &&
        left->position.x == right->position.x &&
        left->position.y == right->position.y &&
        left->position.z == right->position.z &&
        left->extents.x == right->extents.x &&
        left->extents.y == right->extents.y &&
        left->extents.z == right->extents.z &&
        left->influence == right->influence &&
        left->enabled == right->enabled &&
        left->box_projection == right->box_projection;
}
