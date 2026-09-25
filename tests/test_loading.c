#include <math.h>

#include <henka/loading.h>

int main(void)
{
    henka_loading_task storage[3];
    henka_loading_tracker tracker;
    henka_loading_summary summary;

    if (henka_loading_tracker_init(&tracker, storage, 3U) != HENKA_SUCCESS ||
        henka_loading_tracker_add(&tracker, 1U, 1.0f) != HENKA_SUCCESS ||
        henka_loading_tracker_add(&tracker, 2U, 3.0f) != HENKA_SUCCESS ||
        henka_loading_tracker_add(&tracker, 2U, 1.0f) !=
            HENKA_ERROR_INVALID_ARGUMENT ||
        henka_loading_tracker_set_progress(
            &tracker, 1U, 0.5f) != HENKA_SUCCESS ||
        henka_loading_tracker_set_progress(
            &tracker, 2U, 0.25f) != HENKA_SUCCESS ||
        henka_loading_tracker_get_summary(
            &tracker, &summary) != HENKA_SUCCESS ||
        summary.task_count != 2U ||
        summary.active_count != 2U ||
        fabsf(summary.weighted_progress - 0.3125f) > 0.0001f)
    {
        return 1;
    }

    if (henka_loading_tracker_set_progress(
            &tracker, 1U, 0.25f) != HENKA_ERROR_INVALID_ARGUMENT ||
        henka_loading_tracker_complete(&tracker, 1U) != HENKA_SUCCESS ||
        henka_loading_tracker_cancel(&tracker, 2U) != HENKA_SUCCESS ||
        henka_loading_tracker_get_summary(
            &tracker, &summary) != HENKA_SUCCESS ||
        summary.complete_count != 1U ||
        summary.cancelled_count != 1U ||
        summary.active_count != 0U ||
        fabsf(summary.weighted_progress - 0.4375f) > 0.0001f)
    {
        return 1;
    }

    if (henka_loading_tracker_add(&tracker, 3U, 2.0f) != HENKA_SUCCESS ||
        henka_loading_tracker_fail(&tracker, 3U) != HENKA_SUCCESS ||
        henka_loading_tracker_get_summary(
            &tracker, &summary) != HENKA_SUCCESS ||
        summary.failed_count != 1U ||
        henka_loading_tracker_complete(
            &tracker, 3U) != HENKA_ERROR_INVALID_ARGUMENT)
    {
        return 1;
    }

    return 0;
}
