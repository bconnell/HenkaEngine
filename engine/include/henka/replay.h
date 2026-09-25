#ifndef HENKA_REPLAY_H
#define HENKA_REPLAY_H

#include <stddef.h>
#include <stdint.h>

#include <henka/result.h>

#define HENKA_REPLAY_EVENT_MAX_PAYLOAD_BYTES 32U
#define HENKA_REPLAY_MAX_EVENTS 65536U

typedef struct henka_replay_event
{
    uint64_t tick;
    uint32_t type;
    uint32_t payload_size;
    unsigned char payload[HENKA_REPLAY_EVENT_MAX_PAYLOAD_BYTES];
} henka_replay_event;

typedef struct henka_replay_buffer
{
    henka_replay_event* events;
    size_t capacity;
    size_t count;
} henka_replay_buffer;

/* Initializes a caller-owned fixed-capacity event buffer. Events are recorded
 * in nondecreasing tick order. The buffer performs no allocation. */
henka_result henka_replay_buffer_init(
    henka_replay_buffer* buffer,
    henka_replay_event* storage,
    size_t capacity);
void henka_replay_buffer_clear(henka_replay_buffer* buffer);
henka_result henka_replay_buffer_append(
    henka_replay_buffer* buffer,
    uint64_t tick,
    uint32_t type,
    const void* payload,
    size_t payload_size);
henka_result henka_replay_buffer_get(
    const henka_replay_buffer* buffer,
    size_t index,
    henka_replay_event* out_event);

#endif
