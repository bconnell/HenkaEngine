#include <math.h>
#include <string.h>

#include <henka/animation.h>

static int transform_close(
    henka_transform value,
    henka_vec3 position,
    henka_vec3 scale)
{
    const float epsilon = 0.0001f;
    return fabsf(value.position.x - position.x) < epsilon &&
        fabsf(value.position.y - position.y) < epsilon &&
        fabsf(value.position.z - position.z) < epsilon &&
        fabsf(value.scale.x - scale.x) < epsilon &&
        fabsf(value.scale.y - scale.y) < epsilon &&
        fabsf(value.scale.z - scale.z) < epsilon;
}

int main(void)
{
    henka_animation_transform_key keys[2];
    henka_animation_transform_track track;
    henka_transform sampled;
    henka_transform before;
    henka_quat expected_rotation;

    keys[0].time_seconds = 0.0;
    keys[0].transform = henka_transform_identity();
    keys[1].time_seconds = 2.0;
    keys[1].transform = henka_transform_identity();
    keys[1].transform.position = (henka_vec3){10.0f, 4.0f, -2.0f};
    keys[1].transform.rotation = henka_quat_from_axis_angle(
        (henka_vec3){0.0f, 1.0f, 0.0f},
        3.14159265358979323846f);
    keys[1].transform.scale = (henka_vec3){2.0f, 3.0f, 4.0f};

    track.keys = keys;
    track.key_count = 2U;
    track.duration_seconds = 2.0;
    track.looping = false;

    sampled = henka_transform_identity();
    if (henka_animation_sample_transform_track(
            &track, 1.0, &sampled) != HENKA_SUCCESS ||
        !transform_close(
            sampled,
            (henka_vec3){5.0f, 2.0f, -1.0f},
            (henka_vec3){1.5f, 2.0f, 2.5f}))
    {
        return 1;
    }
    expected_rotation = henka_quat_from_axis_angle(
        (henka_vec3){0.0f, 1.0f, 0.0f},
        1.57079632679489661923f);
    if (fabsf(fabsf(
            sampled.rotation.x * expected_rotation.x +
            sampled.rotation.y * expected_rotation.y +
            sampled.rotation.z * expected_rotation.z +
            sampled.rotation.w * expected_rotation.w) - 1.0f) > 0.0002f)
    {
        return 1;
    }

    if (henka_animation_sample_transform_track(
            &track, 3.0, &sampled) != HENKA_SUCCESS ||
        !transform_close(
            sampled,
            keys[1].transform.position,
            keys[1].transform.scale))
    {
        return 1;
    }

    track.looping = true;
    if (henka_animation_sample_transform_track(
            &track, 2.5, &sampled) != HENKA_SUCCESS ||
        !transform_close(
            sampled,
            (henka_vec3){2.5f, 1.0f, -0.5f},
            (henka_vec3){1.25f, 1.5f, 1.75f}))
    {
        return 1;
    }

    before = sampled;
    keys[1].time_seconds = 0.0;
    if (henka_animation_sample_transform_track(
            &track, 0.5, &sampled) != HENKA_ERROR_INVALID_ARGUMENT ||
        memcmp(&sampled, &before, sizeof(sampled)) != 0)
    {
        return 1;
    }

    keys[1].time_seconds = 2.0;
    keys[1].transform.rotation =
        (henka_quat){0.0f, 0.0f, 0.0f, 0.0f};
    if (henka_animation_sample_transform_track(
            &track, 0.5, &sampled) != HENKA_ERROR_INVALID_ARGUMENT ||
        henka_animation_sample_transform_track(
            &track, -0.5, &sampled) != HENKA_ERROR_INVALID_ARGUMENT)
    {
        return 1;
    }


    {
        const henka_animation_event events[] = {
            {0.0, 10U},
            {0.5, 20U},
            {1.5, 30U},
            {2.0, 40U}};
        henka_animation_event_track event_track = {
            events,
            sizeof(events) / sizeof(events[0]),
            2.0,
            false};
        uint32_t ids[4] = {99U, 99U, 99U, 99U};
        size_t count = 0U;

        if (henka_animation_collect_events(
                &event_track, 0.0, 1.5, ids, 4U, &count) != HENKA_SUCCESS ||
            count != 2U || ids[0] != 20U || ids[1] != 30U)
        {
            return 1;
        }

        ids[0] = 77U;
        ids[1] = 88U;
        if (henka_animation_collect_events(
                &event_track, 0.0, 2.0, ids, 1U, &count) !=
                HENKA_ERROR_LIMIT ||
            count != 3U || ids[0] != 77U || ids[1] != 88U)
        {
            return 1;
        }

        event_track.looping = true;
        if (henka_animation_collect_events(
                &event_track, 1.25, 2.5, ids, 4U, &count) != HENKA_SUCCESS ||
            count != 3U || ids[0] != 30U || ids[1] != 40U || ids[2] != 10U)
        {
            return 1;
        }
        if (henka_animation_collect_events(
                &event_track, 0.25, 2.25, ids, 4U, &count) != HENKA_SUCCESS ||
            count != 4U || ids[0] != 10U || ids[1] != 20U ||
            ids[2] != 30U || ids[3] != 40U)
        {
            return 1;
        }
        if (henka_animation_collect_events(
                &event_track, 0.0, 2.1, ids, 4U, &count) !=
                HENKA_ERROR_INVALID_ARGUMENT)
        {
            return 1;
        }
    }

    return 0;
}
