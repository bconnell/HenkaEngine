#include <stdio.h>

#include <henka/core.h>
#include <henka/scene.h>

#include "../examples/sandbox3d/scene_hierarchy_projection.h"

static bool include_non_helpers(
    const henka_scene* scene,
    henka_entity entity,
    void* context)
{
    (void)context;
    return scene != NULL && entity != HENKA_INVALID_ENTITY &&
        !henka_scene_is_entity_helper(scene, entity);
}

static size_t find_row(
    const sandbox3d_scene_hierarchy_row* rows,
    size_t count,
    henka_entity entity)
{
    size_t index;
    for (index = 0U; index < count; ++index)
    {
        if (rows[index].entity == entity)
        {
            return index;
        }
    }
    return SIZE_MAX;
}

int main(void)
{
    henka_scene* scene = NULL;
    sandbox3d_scene_hierarchy_row rows[16];
    size_t count = 0U;
    size_t index;
    henka_entity root_a;
    henka_entity child_a1;
    henka_entity grandchild_a1;
    henka_entity child_a2;
    henka_entity root_b;
    henka_entity child_b1;
    henka_entity duplicate_a;
    henka_entity duplicate_b;
    henka_entity helper;
    int result = 1;

    if (henka_scene_create(&scene) != HENKA_SUCCESS)
    {
        fprintf(stderr, "scene hierarchy projection setup failed\n");
        return 1;
    }
    root_a = henka_scene_create_entity_named(scene, "Root A");
    child_a1 = henka_scene_create_entity_named(scene, "Child");
    grandchild_a1 = henka_scene_create_entity_named(scene, "Grandchild");
    child_a2 = henka_scene_create_entity_named(scene, "Child");
    root_b = henka_scene_create_entity_named(scene, "Root B");
    child_b1 = henka_scene_create_entity_named(scene, "Child");
    duplicate_a = henka_scene_create_entity_named(scene, "Duplicate");
    duplicate_b = henka_scene_create_entity_named(scene, "Duplicate");
    helper = henka_scene_create_entity_named(scene, "Editor Helper");
    if (root_a == HENKA_INVALID_ENTITY || child_a1 == HENKA_INVALID_ENTITY ||
        grandchild_a1 == HENKA_INVALID_ENTITY || child_a2 == HENKA_INVALID_ENTITY ||
        root_b == HENKA_INVALID_ENTITY || child_b1 == HENKA_INVALID_ENTITY ||
        duplicate_a == HENKA_INVALID_ENTITY || duplicate_b == HENKA_INVALID_ENTITY ||
        helper == HENKA_INVALID_ENTITY ||
        henka_scene_set_entity_flags(scene, helper, HENKA_SCENE_ENTITY_FLAG_HELPER) != HENKA_SUCCESS ||
        henka_scene_set_entity_parent(scene, child_a1, root_a, HENKA_SCENE_PARENT_KEEP_LOCAL) != HENKA_SUCCESS ||
        henka_scene_set_entity_parent(scene, grandchild_a1, child_a1, HENKA_SCENE_PARENT_KEEP_LOCAL) != HENKA_SUCCESS ||
        henka_scene_set_entity_parent(scene, child_a2, root_a, HENKA_SCENE_PARENT_KEEP_LOCAL) != HENKA_SUCCESS ||
        henka_scene_set_entity_parent(scene, child_b1, root_b, HENKA_SCENE_PARENT_KEEP_LOCAL) != HENKA_SUCCESS)
    {
        fprintf(stderr, "scene hierarchy projection graph setup failed\n");
        goto cleanup;
    }

    if (sandbox3d_scene_hierarchy_projection_build(
            scene,
            include_non_helpers,
            NULL,
            rows,
            16U,
            &count) != HENKA_SUCCESS || count != 8U)
    {
        fprintf(stderr, "scene hierarchy projection did not build the authored tree\n");
        goto cleanup;
    }

    if (rows[0].entity != root_a || rows[0].depth != 0U ||
        rows[1].entity != child_a1 || rows[1].depth != 1U ||
        rows[2].entity != grandchild_a1 || rows[2].depth != 2U ||
        rows[3].entity != child_a2 || rows[3].depth != 1U ||
        rows[4].entity != root_b || rows[4].depth != 0U ||
        rows[5].entity != child_b1 || rows[5].depth != 1U ||
        rows[6].entity != duplicate_a || rows[6].depth != 0U ||
        rows[7].entity != duplicate_b || rows[7].depth != 0U ||
        find_row(rows, count, duplicate_a) == find_row(rows, count, duplicate_b))
    {
        fprintf(stderr, "scene hierarchy projection ordering or identity failed\n");
        goto cleanup;
    }

    if (henka_scene_set_entity_parent(
            scene,
            child_a1,
            root_b,
            HENKA_SCENE_PARENT_KEEP_LOCAL) != HENKA_SUCCESS ||
        sandbox3d_scene_hierarchy_projection_build(
            scene,
            include_non_helpers,
            NULL,
            rows,
            16U,
            &count) != HENKA_SUCCESS ||
        rows[find_row(rows, count, child_a1)].depth != 1U ||
        rows[find_row(rows, count, grandchild_a1)].depth != 2U)
    {
        fprintf(stderr, "scene hierarchy projection did not follow reparenting\n");
        goto cleanup;
    }

    if (henka_scene_set_entity_parent(
            scene,
            child_a1,
            HENKA_INVALID_ENTITY,
            HENKA_SCENE_PARENT_KEEP_LOCAL) != HENKA_SUCCESS ||
        sandbox3d_scene_hierarchy_projection_build(
            scene,
            include_non_helpers,
            NULL,
            rows,
            16U,
            &count) != HENKA_SUCCESS ||
        rows[find_row(rows, count, child_a1)].depth != 0U ||
        rows[find_row(rows, count, grandchild_a1)].depth != 1U)
    {
        fprintf(stderr, "scene hierarchy projection did not follow unparenting\n");
        goto cleanup;
    }

    henka_scene_destroy_entity(scene, duplicate_a);
    if (sandbox3d_scene_hierarchy_projection_build(
            scene,
            include_non_helpers,
            NULL,
            rows,
            16U,
            &count) != HENKA_SUCCESS ||
        find_row(rows, count, duplicate_a) != SIZE_MAX ||
        find_row(rows, count, duplicate_b) == SIZE_MAX)
    {
        fprintf(stderr, "scene hierarchy projection retained a deleted identity\n");
        goto cleanup;
    }

    for (index = 0U; index < count; ++index)
    {
        if (rows[index].entity == helper)
        {
            fprintf(stderr, "scene hierarchy projection exposed an editor helper\n");
            goto cleanup;
        }
    }
    result = 0;

cleanup:
    henka_scene_destroy(scene);
    return result;
}
