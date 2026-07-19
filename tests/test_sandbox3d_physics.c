#include "test_suite.h"

#include <string.h>

#include <henka/physics.h>
#include <henka/scene.h>

#include "../examples/sandbox3d/physics_tools.h"

static henka_physics_body_id create_linked_box(
    henka_physics_world* world,
    henka_scene* scene,
    henka_entity entity,
    henka_physics_body_type type,
    henka_transform transform)
{
    henka_physics_body_desc desc;
    henka_physics_body_id body;

    memset(&desc, 0, sizeof(desc));
    desc.type = type;
    desc.transform = transform;
    desc.mass = 1.0f;
    desc.material = henka_physics_material_default();
    desc.collider = henka_physics_collider_box((henka_vec3){0.5f, 0.5f, 0.5f});
    desc.linked_scene = scene;
    desc.linked_entity = entity;
    body = HENKA_INVALID_PHYSICS_BODY_ID;
    if (henka_physics_body_create(world, &desc, &body) != HENKA_SUCCESS)
    {
        return HENKA_INVALID_PHYSICS_BODY_ID;
    }
    return body;
}

void henka_test_sandbox3d_physics(void)
{
    henka_physics_body_id bodies[SANDBOX3D_PHYSICS_SAMPLE_COUNT];
    henka_physics_body_state marker_state;
    henka_physics_body_state selected_state;
    henka_physics_world* world;
    henka_scene* scene;
    henka_entity marker_entity;
    henka_entity selected_entity;
    henka_transform marker_transform;
    henka_transform selected_transform;
    size_t index;

    for (index = 0U; index < SANDBOX3D_PHYSICS_SAMPLE_COUNT; ++index)
    {
        HENKA_TEST_ASSERT(
            sandbox3d_physics_initial_body_type((sandbox3d_physics_sample_slot)index) ==
            HENKA_PHYSICS_BODY_STATIC);
    }
    HENKA_TEST_ASSERT(
        sandbox3d_physics_demo_body_type(SANDBOX3D_PHYSICS_SAMPLE_TEXTURED_CUBE) ==
        HENKA_PHYSICS_BODY_DYNAMIC);
    HENKA_TEST_ASSERT(
        sandbox3d_physics_demo_body_type(SANDBOX3D_PHYSICS_SAMPLE_COLORED_CUBE) ==
        HENKA_PHYSICS_BODY_DYNAMIC);
    HENKA_TEST_ASSERT(
        sandbox3d_physics_demo_body_type(SANDBOX3D_PHYSICS_SAMPLE_OBJ_MARKER) ==
        HENKA_PHYSICS_BODY_DYNAMIC);
    HENKA_TEST_ASSERT(
        sandbox3d_physics_demo_body_type(SANDBOX3D_PHYSICS_SAMPLE_MISSING_TEXTURE) ==
        HENKA_PHYSICS_BODY_STATIC);

    world = NULL;
    scene = NULL;
    HENKA_TEST_ASSERT(henka_scene_create(&scene) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_world_create(&world) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(
        henka_physics_world_set_gravity(world, (henka_vec3){0.0f, -9.81f, 0.0f}) ==
        HENKA_SUCCESS);

    selected_entity = henka_scene_create_entity_named(scene, "Selected Cube");
    marker_entity = henka_scene_create_entity_named(scene, "Unrelated Marker");
    HENKA_TEST_ASSERT(selected_entity != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(marker_entity != HENKA_INVALID_ENTITY);

    selected_transform = henka_transform_identity();
    selected_transform.position = (henka_vec3){0.0f, 4.0f, 0.0f};
    marker_transform = henka_transform_identity();
    marker_transform.position = (henka_vec3){3.0f, 1.0f, 0.0f};
    HENKA_TEST_ASSERT(
        henka_scene_set_entity_transform(scene, selected_entity, selected_transform) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(
        henka_scene_set_entity_transform(scene, marker_entity, marker_transform) == HENKA_SUCCESS);

    for (index = 0U; index < SANDBOX3D_PHYSICS_SAMPLE_COUNT; ++index)
    {
        bodies[index] = HENKA_INVALID_PHYSICS_BODY_ID;
    }
    bodies[SANDBOX3D_PHYSICS_SAMPLE_TEXTURED_CUBE] = create_linked_box(
        world,
        scene,
        selected_entity,
        HENKA_PHYSICS_BODY_DYNAMIC,
        selected_transform);
    bodies[SANDBOX3D_PHYSICS_SAMPLE_OBJ_MARKER] = create_linked_box(
        world,
        scene,
        marker_entity,
        HENKA_PHYSICS_BODY_DYNAMIC,
        marker_transform);
    HENKA_TEST_ASSERT(bodies[SANDBOX3D_PHYSICS_SAMPLE_TEXTURED_CUBE] != HENKA_INVALID_PHYSICS_BODY_ID);
    HENKA_TEST_ASSERT(bodies[SANDBOX3D_PHYSICS_SAMPLE_OBJ_MARKER] != HENKA_INVALID_PHYSICS_BODY_ID);

    HENKA_TEST_ASSERT(
        sandbox3d_physics_activate_only_body(
            world,
            bodies,
            SANDBOX3D_PHYSICS_SAMPLE_COUNT,
            bodies[SANDBOX3D_PHYSICS_SAMPLE_TEXTURED_CUBE],
            scene,
            selected_entity) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(
        henka_physics_body_get_state(
            world,
            bodies[SANDBOX3D_PHYSICS_SAMPLE_TEXTURED_CUBE],
            &selected_state) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(
        henka_physics_body_get_state(
            world,
            bodies[SANDBOX3D_PHYSICS_SAMPLE_OBJ_MARKER],
            &marker_state) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(selected_state.type == HENKA_PHYSICS_BODY_DYNAMIC);
    HENKA_TEST_ASSERT(marker_state.type == HENKA_PHYSICS_BODY_STATIC);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(selected_state.transform.position.y, 4.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(marker_state.transform.position.y, 1.0f, 0.0001f);

    HENKA_TEST_ASSERT(henka_physics_world_step_fixed(world) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(
        henka_physics_body_get_state(
            world,
            bodies[SANDBOX3D_PHYSICS_SAMPLE_TEXTURED_CUBE],
            &selected_state) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(
        henka_physics_body_get_state(
            world,
            bodies[SANDBOX3D_PHYSICS_SAMPLE_OBJ_MARKER],
            &marker_state) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(selected_state.transform.position.y < 4.0f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(marker_state.transform.position.y, 1.0f, 0.0001f);

    henka_physics_world_destroy(world);
    henka_scene_destroy(scene);
}
