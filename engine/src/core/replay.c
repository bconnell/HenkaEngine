#include <henka/replay.h>

#include <string.h>

henka_result henka_replay_buffer_init(
    henka_replay_buffer* buffer,
    henka_replay_event* storage,
    size_t capacity)
{
    if (buffer == NULL ||
        storage == NULL ||
        capacity == 0U ||
        capacity > HENKA_REPLAY_MAX_EVENTS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    buffer->events = storage;
    buffer->capacity = capacity;
    buffer->count = 0U;
    return HENKA_SUCCESS;
}

void henka_replay_buffer_clear(henka_replay_buffer* buffer)
{
    if (buffer != NULL && buffer->events != NULL && buffer->capacity > 0U)
    {
        buffer->count = 0U;
    }
}

henka_result henka_replay_buffer_append(
    henka_replay_buffer* buffer,
    uint64_t tick,
    uint32_t type,
    const void* payload,
    size_t payload_size)
{
    henka_replay_event candidate;

    if (buffer == NULL ||
        buffer->events == NULL ||
        buffer->capacity == 0U ||
        buffer->capacity > HENKA_REPLAY_MAX_EVENTS ||
        type == 0U ||
        payload_size > HENKA_REPLAY_EVENT_MAX_PAYLOAD_BYTES ||
        (payload_size > 0U && payload == NULL))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (buffer->count >= buffer->capacity)
    {
        return HENKA_ERROR_LIMIT;
    }
    if (buffer->count > 0U &&
        tick < buffer->events[buffer->count - 1U].tick)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    memset(&candidate, 0, sizeof(candidate));
    candidate.tick = tick;
    candidate.type = type;
    candidate.payload_size = (uint32_t)payload_size;
    if (payload_size > 0U)
    {
        memcpy(candidate.payload, payload, payload_size);
    }
    buffer->events[buffer->count] = candidate;
    ++buffer->count;
    return HENKA_SUCCESS;
}

henka_result henka_replay_buffer_get(
    const henka_replay_buffer* buffer,
    size_t index,
    henka_replay_event* out_event)
{
    if (buffer == NULL ||
        buffer->events == NULL ||
        out_event == NULL ||
        index >= buffer->count)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_event = buffer->events[index];
    return HENKA_SUCCESS;
}
