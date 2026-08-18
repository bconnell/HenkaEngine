#include "test_suite.h"

#include <henka/core.h>
#include <henka/camera.h>
#include <henka/scene.h>

#include "../examples/sandbox3d/camera_tools.h"

static void sandbox3d_camera_test_set_bounds(
    henka_scene* scene,
    henka_entity entity,
    henka_vec3 center,
    henka_vec3 extents)
{
    const henka_bounds bounds =
    {
        center,
        extents
    };

    HENKA_TEST_ASSERT(
        henka_scene_set_entity_local_bounds(
            scene,
            entity,
            bounds) == HENKA_SUCCESS);
}

void henka_test_sandbox3d_camera_tools(void)
{
    henka_bounds content_bounds;
    henka_bounds preferred_bounds;
    henka_camera camera;
    henka_entity content_a;
    henka_entity content_b;
    henka_entity ground;
    henka_entity helper;
    henka_entity hidden;
    henka_scene* scene;
    henka_vec2 screen_point;
    float depth;
    henka_camera_preset preset;

    scene = NULL;

    HENKA_TEST_ASSERT(
        henka_scene_create(&scene) == HENKA_SUCCESS);

    ground =
        henka_scene_create_entity_named(
            scene,
            "Locked Foundation");

    helper =
        henka_scene_create_entity_named(
            scene,
            "Editor Helper");

    hidden =
        henka_scene_create_entity_named(
            scene,
            "Hidden Giant");

    content_a =
        henka_scene_create_entity_named(
            scene,
            "Content A");

    content_b =
        henka_scene_create_entity_named(
            scene,
            "Content B");

    HENKA_TEST_ASSERT(
        ground != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(
        helper != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(
        hidden != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(
        content_a != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(
        content_b != HENKA_INVALID_ENTITY);

    sandbox3d_camera_test_set_bounds(
        scene,
        ground,
        (henka_vec3){0.0f, 0.0f, 0.0f},
        (henka_vec3){500.0f, 1.0f, 500.0f});

    sandbox3d_camera_test_set_bounds(
        scene,
        helper,
        (henka_vec3){1000.0f, 1000.0f, 1000.0f},
        (henka_vec3){500.0f, 500.0f, 500.0f});

    sandbox3d_camera_test_set_bounds(
        scene,
        hidden,
        (henka_vec3){-1000.0f, -1000.0f, -1000.0f},
        (henka_vec3){500.0f, 500.0f, 500.0f});

    sandbox3d_camera_test_set_bounds(
        scene,
        content_a,
        (henka_vec3){-2.0f, 1.0f, 0.0f},
        (henka_vec3){1.0f, 1.0f, 1.0f});

    sandbox3d_camera_test_set_bounds(
        scene,
        content_b,
        (henka_vec3){4.0f, 2.0f, 2.0f},
        (henka_vec3){2.0f, 2.0f, 1.0f});

    HENKA_TEST_ASSERT(
        henka_scene_set_entity_flags(
            scene,
            ground,
            HENKA_SCENE_ENTITY_FLAG_TRANSFORM_LOCKED) ==
        HENKA_SUCCESS);

    HENKA_TEST_ASSERT(
        henka_scene_set_entity_flags(
            scene,
            helper,
            HENKA_SCENE_ENTITY_FLAG_HELPER) ==
        HENKA_SUCCESS);

    HENKA_TEST_ASSERT(
        henka_scene_set_entity_visible(
            scene,
            hidden,
            false) == HENKA_SUCCESS);

    /*
     * No preferred user object:
     *
     * locked ground + helper + hidden objects must not destroy framing.
     */
    HENKA_TEST_ASSERT(
        sandbox3d_camera_resolve_focus_bounds(
            scene,
            HENKA_INVALID_ENTITY,
            NULL,
            &content_bounds));

    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        content_bounds.center.x,
        1.5f,
        0.0001f);

    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        content_bounds.center.y,
        2.0f,
        0.0001f);

    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        content_bounds.center.z,
        1.0f,
        0.0001f);

    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        content_bounds.extents.x,
        4.5f,
        0.0001f);

    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        content_bounds.extents.y,
        2.0f,
        0.0001f);

    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        content_bounds.extents.z,
        2.0f,
        0.0001f);

    /*
     * A normal preferred user object takes precedence.
     */
    preferred_bounds =
        (henka_bounds){
            {-2.0f, 1.0f, 0.0f},
            {1.0f, 1.0f, 1.0f}};

    HENKA_TEST_ASSERT(
        sandbox3d_camera_resolve_focus_bounds(
            scene,
            content_a,
            &preferred_bounds,
            &content_bounds));

    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        content_bounds.center.x,
        -2.0f,
        0.0001f);

    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        content_bounds.center.y,
        1.0f,
        0.0001f);

    /*
     * A selected locked foundation is deliberately not auto-framed.
     * The useful editable scene content wins instead.
     */
    preferred_bounds =
        (henka_bounds){
            {0.0f, 0.0f, 0.0f},
            {500.0f, 1.0f, 500.0f}};

    HENKA_TEST_ASSERT(
        sandbox3d_camera_resolve_focus_bounds(
            scene,
            ground,
            &preferred_bounds,
            &content_bounds));

    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        content_bounds.center.x,
        1.5f,
        0.0001f);

    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        content_bounds.extents.x,
        4.5f,
        0.0001f);

    /*
     * Every preset must deterministically center the same bounds.
     */
    for (preset = HENKA_CAMERA_PRESET_PERSPECTIVE_3D;
         preset < HENKA_CAMERA_PRESET_COUNT;
         preset = (henka_camera_preset)(preset + 1))
    {
        henka_camera first_pose;

        camera =
            henka_camera_create_perspective(
                60.0f * HENKA_DEG_TO_RAD,
                1280.0f / 720.0f,
                0.1f,
                1000.0f);

        HENKA_TEST_ASSERT(
            sandbox3d_camera_apply_framed_preset(
                &camera,
                preset,
                content_bounds));

        HENKA_TEST_ASSERT(
            henka_camera_world_to_screen(
                &camera,
                1280,
                720,
                content_bounds.center,
                &screen_point,
                &depth) == HENKA_SUCCESS);

        HENKA_TEST_ASSERT_FLOAT_CLOSE(
            screen_point.x,
            640.0f,
            0.75f);

        HENKA_TEST_ASSERT_FLOAT_CLOSE(
            screen_point.y,
            360.0f,
            0.75f);

        first_pose = camera;

        HENKA_TEST_ASSERT(
            sandbox3d_camera_apply_framed_preset(
                &camera,
                preset,
                content_bounds));

        HENKA_TEST_ASSERT_FLOAT_CLOSE(
            camera.position.x,
            first_pose.position.x,
            0.0001f);

        HENKA_TEST_ASSERT_FLOAT_CLOSE(
            camera.position.y,
            first_pose.position.y,
            0.0001f);

        HENKA_TEST_ASSERT_FLOAT_CLOSE(
            camera.position.z,
            first_pose.position.z,
            0.0001f);

        HENKA_TEST_ASSERT_FLOAT_CLOSE(
            camera.yaw_radians,
            first_pose.yaw_radians,
            0.0001f);

        HENKA_TEST_ASSERT_FLOAT_CLOSE(
            camera.pitch_radians,
            first_pose.pitch_radians,
            0.0001f);

        HENKA_TEST_ASSERT_FLOAT_CLOSE(
            camera.orthographic_height,
            first_pose.orthographic_height,
            0.0001f);
    }

    HENKA_TEST_ASSERT(
        !sandbox3d_camera_resolve_focus_bounds(
            NULL,
            HENKA_INVALID_ENTITY,
            NULL,
            &content_bounds));

    HENKA_TEST_ASSERT(
        !sandbox3d_camera_resolve_focus_bounds(
            scene,
            HENKA_INVALID_ENTITY,
            NULL,
            NULL));

    henka_scene_destroy(scene);
}