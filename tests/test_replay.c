#include <stdint.h>
#include <string.h>

#include <henka/replay.h>

int main(void)
{
    henka_replay_event storage[2];
    henka_replay_buffer buffer;
    henka_replay_event event;
    const uint32_t first_payload = UINT32_C(0x12345678);
    const unsigned char second_payload[3] = {1U, 2U, 3U};
    size_t count_before;

    memset(storage, 0xA5, sizeof(storage));
    if (henka_replay_buffer_init(&buffer, storage, 2U) != HENKA_SUCCESS ||
        buffer.count != 0U ||
        henka_replay_buffer_append(
            &buffer,
            10U,
            1U,
            &first_payload,
            sizeof(first_payload)) != HENKA_SUCCESS ||
        henka_replay_buffer_append(
            &buffer,
            10U,
            2U,
            second_payload,
            sizeof(second_payload)) != HENKA_SUCCESS ||
        buffer.count != 2U)
    {
        return 1;
    }

    if (henka_replay_buffer_get(&buffer, 0U, &event) != HENKA_SUCCESS ||
        event.tick != 10U ||
        event.type != 1U ||
        event.payload_size != sizeof(first_payload) ||
        memcmp(event.payload, &first_payload, sizeof(first_payload)) != 0 ||
        henka_replay_buffer_get(&buffer, 1U, &event) != HENKA_SUCCESS ||
        event.type != 2U ||
        event.payload_size != sizeof(second_payload) ||
        memcmp(event.payload, second_payload, sizeof(second_payload)) != 0)
    {
        return 1;
    }

    count_before = buffer.count;
    if (henka_replay_buffer_append(
            &buffer, 11U, 3U, NULL, 0U) != HENKA_ERROR_LIMIT ||
        buffer.count != count_before)
    {
        return 1;
    }

    henka_replay_buffer_clear(&buffer);
    if (buffer.count != 0U ||
        henka_replay_buffer_append(
            &buffer, 5U, 4U, NULL, 0U) != HENKA_SUCCESS)
    {
        return 1;
    }
    count_before = buffer.count;
    if (henka_replay_buffer_append(
            &buffer, 4U, 5U, NULL, 0U) != HENKA_ERROR_INVALID_ARGUMENT ||
        buffer.count != count_before ||
        henka_replay_buffer_append(
            &buffer, 6U, 0U, NULL, 0U) != HENKA_ERROR_INVALID_ARGUMENT ||
        buffer.count != count_before)
    {
        return 1;
    }


    {
        henka_replay_cursor cursor;

        henka_replay_buffer_clear(&buffer);
        if (henka_replay_buffer_append(
                &buffer, 2U, 10U, NULL, 0U) != HENKA_SUCCESS ||
            henka_replay_buffer_append(
                &buffer, 5U, 11U, NULL, 0U) != HENKA_SUCCESS ||
            henka_replay_cursor_init(&cursor, &buffer) != HENKA_SUCCESS ||
            henka_replay_cursor_seek_tick(&cursor, 3U) != HENKA_SUCCESS ||
            henka_replay_cursor_next(&cursor, &event) != HENKA_SUCCESS ||
            event.tick != 5U ||
            event.type != 11U ||
            henka_replay_cursor_next(&cursor, &event) != HENKA_ERROR_LIMIT)
        {
            return 1;
        }

        if (henka_replay_cursor_seek_tick(&cursor, 0U) != HENKA_SUCCESS ||
            henka_replay_cursor_next(&cursor, &event) != HENKA_SUCCESS ||
            event.tick != 2U)
        {
            return 1;
        }
    }

    return 0;
}
