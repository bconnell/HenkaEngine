#include <henka/loading.h>

#include <math.h>
#include <string.h>

static bool henka_loading_tracker_is_valid(const henka_loading_tracker* tracker)
{
    return tracker != NULL &&
        tracker->tasks != NULL &&
        tracker->capacity > 0U &&
        tracker->capacity <= HENKA_LOADING_MAX_TASKS &&
        tracker->count <= tracker->capacity;
}

static henka_loading_task* henka_loading_find_task(
    henka_loading_tracker* tracker,
    uint64_t id)
{
    size_t index;

    if (!henka_loading_tracker_is_valid(tracker) || id == 0U)
    {
        return NULL;
    }
    for (index = 0U; index < tracker->count; ++index)
    {
        if (tracker->tasks[index].id == id)
        {
            return &tracker->tasks[index];
        }
    }
    return NULL;
}

henka_result henka_loading_tracker_init(
    henka_loading_tracker* tracker,
    henka_loading_task* storage,
    size_t capacity)
{
    if (tracker == NULL ||
        storage == NULL ||
        capacity == 0U ||
        capacity > HENKA_LOADING_MAX_TASKS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    memset(storage, 0, capacity * sizeof(*storage));
    tracker->tasks = storage;
    tracker->capacity = capacity;
    tracker->count = 0U;
    return HENKA_SUCCESS;
}

henka_result henka_loading_tracker_add(
    henka_loading_tracker* tracker,
    uint64_t id,
    float weight)
{
    henka_loading_task* task;

    if (!henka_loading_tracker_is_valid(tracker) ||
        id == 0U ||
        !isfinite(weight) ||
        weight <= 0.0f)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (henka_loading_find_task(tracker, id) != NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (tracker->count >= tracker->capacity)
    {
        return HENKA_ERROR_LIMIT;
    }
    task = &tracker->tasks[tracker->count++];
    task->id = id;
    task->weight = weight;
    task->progress = 0.0f;
    task->state = HENKA_LOADING_TASK_ACTIVE;
    return HENKA_SUCCESS;
}

henka_result henka_loading_tracker_set_progress(
    henka_loading_tracker* tracker,
    uint64_t id,
    float progress)
{
    henka_loading_task* task = henka_loading_find_task(tracker, id);

    if (task == NULL ||
        task->state != HENKA_LOADING_TASK_ACTIVE ||
        !isfinite(progress) ||
        progress < task->progress ||
        progress < 0.0f ||
        progress > 1.0f)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    task->progress = progress;
    return HENKA_SUCCESS;
}

henka_result henka_loading_tracker_complete(
    henka_loading_tracker* tracker,
    uint64_t id)
{
    henka_loading_task* task = henka_loading_find_task(tracker, id);

    if (task == NULL || task->state != HENKA_LOADING_TASK_ACTIVE)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    task->progress = 1.0f;
    task->state = HENKA_LOADING_TASK_COMPLETE;
    return HENKA_SUCCESS;
}

henka_result henka_loading_tracker_cancel(
    henka_loading_tracker* tracker,
    uint64_t id)
{
    henka_loading_task* task = henka_loading_find_task(tracker, id);

    if (task == NULL || task->state != HENKA_LOADING_TASK_ACTIVE)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    task->state = HENKA_LOADING_TASK_CANCELLED;
    return HENKA_SUCCESS;
}

henka_result henka_loading_tracker_fail(
    henka_loading_tracker* tracker,
    uint64_t id)
{
    henka_loading_task* task = henka_loading_find_task(tracker, id);

    if (task == NULL || task->state != HENKA_LOADING_TASK_ACTIVE)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    task->state = HENKA_LOADING_TASK_FAILED;
    return HENKA_SUCCESS;
}

henka_result henka_loading_tracker_get_summary(
    const henka_loading_tracker* tracker,
    henka_loading_summary* out_summary)
{
    double total_weight = 0.0;
    double weighted_progress = 0.0;
    size_t index;
    henka_loading_summary summary = {0};

    if (!henka_loading_tracker_is_valid(tracker) || out_summary == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    summary.task_count = tracker->count;
    for (index = 0U; index < tracker->count; ++index)
    {
        const henka_loading_task* task = &tracker->tasks[index];
        if (task->id == 0U ||
            !isfinite(task->weight) ||
            task->weight <= 0.0f ||
            !isfinite(task->progress) ||
            task->progress < 0.0f ||
            task->progress > 1.0f)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        total_weight += task->weight;
        weighted_progress += (double)task->weight * (double)task->progress;
        switch (task->state)
        {
            case HENKA_LOADING_TASK_ACTIVE:
                ++summary.active_count;
                break;
            case HENKA_LOADING_TASK_COMPLETE:
                ++summary.complete_count;
                break;
            case HENKA_LOADING_TASK_CANCELLED:
                ++summary.cancelled_count;
                break;
            case HENKA_LOADING_TASK_FAILED:
                ++summary.failed_count;
                break;
            default:
                return HENKA_ERROR_INVALID_ARGUMENT;
        }
    }

    if (total_weight > 0.0)
    {
        const double ratio = weighted_progress / total_weight;
        if (!isfinite(ratio) || ratio < 0.0 || ratio > 1.0)
        {
            return HENKA_ERROR_NUMERIC_RANGE;
        }
        summary.weighted_progress = (float)ratio;
    }

    *out_summary = summary;
    return HENKA_SUCCESS;
}
