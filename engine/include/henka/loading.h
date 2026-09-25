#ifndef HENKA_LOADING_H
#define HENKA_LOADING_H

#include <stddef.h>
#include <stdint.h>

#include <henka/result.h>

#define HENKA_LOADING_MAX_TASKS 1024U

typedef enum henka_loading_task_state
{
    HENKA_LOADING_TASK_ACTIVE = 0,
    HENKA_LOADING_TASK_COMPLETE,
    HENKA_LOADING_TASK_CANCELLED,
    HENKA_LOADING_TASK_FAILED
} henka_loading_task_state;

typedef struct henka_loading_task
{
    uint64_t id;
    float weight;
    float progress;
    henka_loading_task_state state;
} henka_loading_task;

typedef struct henka_loading_tracker
{
    henka_loading_task* tasks;
    size_t capacity;
    size_t count;
} henka_loading_tracker;

typedef struct henka_loading_summary
{
    size_t task_count;
    size_t active_count;
    size_t complete_count;
    size_t cancelled_count;
    size_t failed_count;
    float weighted_progress;
} henka_loading_summary;

henka_result henka_loading_tracker_init(
    henka_loading_tracker* tracker,
    henka_loading_task* storage,
    size_t capacity);
henka_result henka_loading_tracker_add(
    henka_loading_tracker* tracker,
    uint64_t id,
    float weight);
henka_result henka_loading_tracker_set_progress(
    henka_loading_tracker* tracker,
    uint64_t id,
    float progress);
henka_result henka_loading_tracker_complete(
    henka_loading_tracker* tracker,
    uint64_t id);
henka_result henka_loading_tracker_cancel(
    henka_loading_tracker* tracker,
    uint64_t id);
henka_result henka_loading_tracker_fail(
    henka_loading_tracker* tracker,
    uint64_t id);
henka_result henka_loading_tracker_get_summary(
    const henka_loading_tracker* tracker,
    henka_loading_summary* out_summary);

#endif
