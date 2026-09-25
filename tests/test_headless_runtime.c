#include <stdio.h>

#include <henka/camera.h>
#include <henka/math.h>
#include <henka/physics.h>
#include <henka/scene.h>
#include <henka/time.h>

int main(void)
{
    henka_camera camera;
    henka_scene* scene = NULL;
    henka_physics_world* physics = NULL;
    henka_entity entity;
    henka_time_state time_state;
    henka_frame_time_stats frame_stats;
    double average_seconds = 0.0;

    camera = henka_camera_create_perspective(1.0f, 1.0f, 0.1f, 100.0f);
    if (!henka_camera_is_valid(&camera))
    {
        return 1;
    }
    if (henka_scene_create(&scene) != HENKA_SUCCESS || scene == NULL)
    {
        return 2;
    }
    if (henka_scene_set_camera(scene, &camera) != HENKA_SUCCESS)
    {
        henka_scene_destroy(scene);
        return 3;
    }
    entity = henka_scene_create_entity_named(scene, "headless");
    if (entity == HENKA_INVALID_ENTITY || !henka_scene_is_entity_valid(scene, entity))
    {
        henka_scene_destroy(scene);
        return 4;
    }
    if (henka_physics_world_create(&physics) != HENKA_SUCCESS || physics == NULL)
    {
        henka_scene_destroy(scene);
        return 5;
    }
    henka_time_reset(&time_state);
    henka_time_tick(&time_state);
    if (!time_state.initialized)
    {
        henka_physics_world_destroy(physics);
        henka_scene_destroy(scene);
        return 6;
    }

    henka_frame_time_stats_reset(&frame_stats);
    if (henka_frame_time_stats_push(&frame_stats, 0.010) != HENKA_SUCCESS ||
        henka_frame_time_stats_push(&frame_stats, 0.030) != HENKA_SUCCESS ||
        henka_frame_time_stats_get_average(
            &frame_stats, &average_seconds) != HENKA_SUCCESS ||
        frame_stats.sample_count != 2U ||
        frame_stats.minimum_seconds != 0.010 ||
        frame_stats.maximum_seconds != 0.030 ||
        average_seconds < 0.019999 ||
        average_seconds > 0.020001 ||
        henka_frame_time_stats_push(&frame_stats, -1.0) !=
            HENKA_ERROR_INVALID_ARGUMENT)
    {
        henka_physics_world_destroy(physics);
        henka_scene_destroy(scene);
        return 7;
    }

    henka_physics_world_destroy(physics);
    henka_scene_destroy(scene);
    puts("headless runtime smoke passed");
    return 0;
}
