#include <henka/terrain_network.h>

#include <string.h>

#define HENKA_TERRAIN_REQUEST_FIXED_BYTES 49U
#define HENKA_TERRAIN_RESPONSE_FIXED_BYTES 17U
#define HENKA_TERRAIN_REJECTION_BYTES 9U
#define HENKA_TERRAIN_DELTA_FIXED_BYTES 57U

static void henka_terrain_network_write_u32(uint8_t* destination, uint32_t value)
{
    destination[0] = (uint8_t)(value & 0xFFU);
    destination[1] = (uint8_t)((value >> 8U) & 0xFFU);
    destination[2] = (uint8_t)((value >> 16U) & 0xFFU);
    destination[3] = (uint8_t)((value >> 24U) & 0xFFU);
}

static void henka_terrain_network_write_u64(uint8_t* destination, uint64_t value)
{
    uint32_t index;
    for (index = 0U; index < 8U; ++index)
    {
        destination[index] = (uint8_t)((value >> (index * 8U)) & 0xFFU);
    }
}

static uint32_t henka_terrain_network_read_u32(const uint8_t* source)
{
    return (uint32_t)source[0] | ((uint32_t)source[1] << 8U) |
        ((uint32_t)source[2] << 16U) | ((uint32_t)source[3] << 24U);
}

static uint64_t henka_terrain_network_read_u64(const uint8_t* source)
{
    uint64_t value = 0U;
    uint32_t index;
    for (index = 0U; index < 8U; ++index)
    {
        value |= (uint64_t)source[index] << (index * 8U);
    }
    return value;
}

static bool henka_terrain_network_region_list_valid(
    uint32_t count,
    const henka_terrain_network_region_revision* regions)
{
    uint32_t index;
    if (count > HENKA_TERRAIN_NETWORK_MAX_AFFECTED_REGIONS ||
        (count > 0U && regions == NULL))
    {
        return false;
    }
    for (index = 0U; index < count; ++index)
    {
        if (regions[index].region_id.x < 0 || regions[index].region_id.z < 0)
        {
            return false;
        }
    }
    return true;
}

static bool henka_terrain_network_command_fields_valid(const henka_terrain_edit_command* command)
{
    return command != NULL && command->algorithm_version == HENKA_TERRAIN_EDIT_ALGORITHM_VERSION &&
        command->operation >= HENKA_TERRAIN_EDIT_RAISE && command->operation <= HENKA_TERRAIN_EDIT_PAINT &&
        command->falloff >= HENKA_TERRAIN_EDIT_FALLOFF_LINEAR &&
        command->falloff <= HENKA_TERRAIN_EDIT_FALLOFF_SMOOTH &&
        command->radius_samples > 0U && command->value_millimeters != INT32_MIN &&
        command->paint_layer < HENKA_TERRAIN_ACTIVE_MATERIAL_COUNT;
}

static size_t henka_terrain_network_write_command(
    uint8_t* buffer,
    const henka_terrain_edit_command* command)
{
    henka_terrain_network_write_u32(buffer + 0U, command->algorithm_version);
    buffer[4] = (uint8_t)command->operation;
    buffer[5] = (uint8_t)command->falloff;
    henka_terrain_network_write_u32(buffer + 6U, (uint32_t)command->center_sample_x);
    henka_terrain_network_write_u32(buffer + 10U, (uint32_t)command->center_sample_z);
    henka_terrain_network_write_u32(buffer + 14U, command->radius_samples);
    henka_terrain_network_write_u32(buffer + 18U, (uint32_t)command->value_millimeters);
    buffer[22] = command->paint_layer;
    buffer[23] = command->paint_strength;
    return 24U;
}

static bool henka_terrain_network_read_command(
    const uint8_t* buffer,
    henka_terrain_edit_command* command)
{
    *command = henka_terrain_edit_command_default();
    command->algorithm_version = henka_terrain_network_read_u32(buffer + 0U);
    command->operation = (henka_terrain_edit_operation)buffer[4];
    command->falloff = (henka_terrain_edit_falloff)buffer[5];
    command->center_sample_x = (int32_t)henka_terrain_network_read_u32(buffer + 6U);
    command->center_sample_z = (int32_t)henka_terrain_network_read_u32(buffer + 10U);
    command->radius_samples = henka_terrain_network_read_u32(buffer + 14U);
    command->value_millimeters = (int32_t)henka_terrain_network_read_u32(buffer + 18U);
    command->paint_layer = buffer[22];
    command->paint_strength = buffer[23];
    return henka_terrain_network_command_fields_valid(command);
}

henka_result henka_terrain_edit_request_encode(
    const henka_terrain_edit_request* request,
    uint8_t* buffer,
    size_t buffer_capacity,
    size_t* out_size)
{
    size_t size;
    uint32_t index;
    if (request == NULL || buffer == NULL || out_size == NULL ||
        !henka_terrain_network_command_fields_valid(&request->command) ||
        !henka_terrain_network_region_list_valid(request->affected_region_count, request->affected_regions))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    size = HENKA_TERRAIN_REQUEST_FIXED_BYTES +
        (size_t)request->affected_region_count * 16U;
    if (buffer_capacity < size)
    {
        return HENKA_ERROR_LIMIT;
    }
    henka_terrain_network_write_u64(buffer + 0U, request->world_identity);
    henka_terrain_network_write_u64(buffer + 8U, request->base_asset_identity);
    henka_terrain_network_write_u64(buffer + 16U, request->client_nonce);
    henka_terrain_network_write_command(buffer + 24U, &request->command);
    buffer[48] = (uint8_t)request->affected_region_count;
    for (index = 0U; index < request->affected_region_count; ++index)
    {
        size_t offset = HENKA_TERRAIN_REQUEST_FIXED_BYTES + (size_t)index * 16U;
        henka_terrain_network_write_u32(buffer + offset, (uint32_t)request->affected_regions[index].region_id.x);
        henka_terrain_network_write_u32(buffer + offset + 4U, (uint32_t)request->affected_regions[index].region_id.z);
        henka_terrain_network_write_u64(buffer + offset + 8U, request->affected_regions[index].revision);
    }
    *out_size = size;
    return HENKA_SUCCESS;
}

henka_result henka_terrain_edit_request_decode(
    const uint8_t* buffer,
    size_t buffer_size,
    henka_terrain_edit_request* out_request)
{
    size_t expected_size;
    uint32_t index;
    if (buffer == NULL || out_request == NULL || buffer_size < HENKA_TERRAIN_REQUEST_FIXED_BYTES)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    memset(out_request, 0, sizeof(*out_request));
    out_request->world_identity = henka_terrain_network_read_u64(buffer + 0U);
    out_request->base_asset_identity = henka_terrain_network_read_u64(buffer + 8U);
    out_request->client_nonce = henka_terrain_network_read_u64(buffer + 16U);
    if (!henka_terrain_network_read_command(buffer + 24U, &out_request->command))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    out_request->command.client_nonce = out_request->client_nonce;
    out_request->affected_region_count = buffer[48];
    if (!henka_terrain_network_region_list_valid(
            out_request->affected_region_count, out_request->affected_regions))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    expected_size = HENKA_TERRAIN_REQUEST_FIXED_BYTES +
        (size_t)out_request->affected_region_count * 16U;
    if (buffer_size != expected_size)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    for (index = 0U; index < out_request->affected_region_count; ++index)
    {
        size_t offset = HENKA_TERRAIN_REQUEST_FIXED_BYTES + (size_t)index * 16U;
        out_request->affected_regions[index].region_id.x = (int32_t)henka_terrain_network_read_u32(buffer + offset);
        out_request->affected_regions[index].region_id.z = (int32_t)henka_terrain_network_read_u32(buffer + offset + 4U);
        out_request->affected_regions[index].revision = henka_terrain_network_read_u64(buffer + offset + 8U);
        if (out_request->affected_regions[index].region_id.x < 0 ||
            out_request->affected_regions[index].region_id.z < 0)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
    }
    return HENKA_SUCCESS;
}

henka_result henka_terrain_edit_acceptance_encode(
    const henka_terrain_edit_acceptance* acceptance,
    uint8_t* buffer,
    size_t buffer_capacity,
    size_t* out_size)
{
    size_t size;
    uint32_t index;
    if (acceptance == NULL || buffer == NULL || out_size == NULL || acceptance->server_command_id == 0U ||
        !henka_terrain_network_region_list_valid(acceptance->affected_region_count, acceptance->affected_regions))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    size = HENKA_TERRAIN_RESPONSE_FIXED_BYTES + (size_t)acceptance->affected_region_count * 16U;
    if (buffer_capacity < size)
    {
        return HENKA_ERROR_LIMIT;
    }
    henka_terrain_network_write_u64(buffer, acceptance->client_nonce);
    henka_terrain_network_write_u64(buffer + 8U, acceptance->server_command_id);
    buffer[16] = (uint8_t)acceptance->affected_region_count;
    for (index = 0U; index < acceptance->affected_region_count; ++index)
    {
        size_t offset = HENKA_TERRAIN_RESPONSE_FIXED_BYTES + (size_t)index * 16U;
        henka_terrain_network_write_u32(buffer + offset, (uint32_t)acceptance->affected_regions[index].region_id.x);
        henka_terrain_network_write_u32(buffer + offset + 4U, (uint32_t)acceptance->affected_regions[index].region_id.z);
        henka_terrain_network_write_u64(buffer + offset + 8U, acceptance->affected_regions[index].revision);
    }
    *out_size = size;
    return HENKA_SUCCESS;
}

henka_result henka_terrain_edit_acceptance_decode(
    const uint8_t* buffer,
    size_t buffer_size,
    henka_terrain_edit_acceptance* out_acceptance)
{
    size_t expected_size;
    uint32_t index;
    if (buffer == NULL || out_acceptance == NULL || buffer_size < HENKA_TERRAIN_RESPONSE_FIXED_BYTES)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    memset(out_acceptance, 0, sizeof(*out_acceptance));
    out_acceptance->client_nonce = henka_terrain_network_read_u64(buffer);
    out_acceptance->server_command_id = henka_terrain_network_read_u64(buffer + 8U);
    out_acceptance->affected_region_count = buffer[16];
    expected_size = HENKA_TERRAIN_RESPONSE_FIXED_BYTES +
        (size_t)out_acceptance->affected_region_count * 16U;
    if (out_acceptance->server_command_id == 0U ||
        !henka_terrain_network_region_list_valid(out_acceptance->affected_region_count, out_acceptance->affected_regions) ||
        buffer_size != expected_size)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    for (index = 0U; index < out_acceptance->affected_region_count; ++index)
    {
        size_t offset = HENKA_TERRAIN_RESPONSE_FIXED_BYTES + (size_t)index * 16U;
        out_acceptance->affected_regions[index].region_id.x = (int32_t)henka_terrain_network_read_u32(buffer + offset);
        out_acceptance->affected_regions[index].region_id.z = (int32_t)henka_terrain_network_read_u32(buffer + offset + 4U);
        out_acceptance->affected_regions[index].revision = henka_terrain_network_read_u64(buffer + offset + 8U);
        if (out_acceptance->affected_regions[index].region_id.x < 0 || out_acceptance->affected_regions[index].region_id.z < 0)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
    }
    return HENKA_SUCCESS;
}

henka_result henka_terrain_edit_rejection_encode(
    const henka_terrain_edit_rejection* rejection,
    uint8_t* buffer,
    size_t buffer_capacity,
    size_t* out_size)
{
    if (rejection == NULL || buffer == NULL || out_size == NULL ||
        rejection->reason < HENKA_TERRAIN_EDIT_REJECT_INVALID ||
        rejection->reason > HENKA_TERRAIN_EDIT_REJECT_LIMIT || buffer_capacity < HENKA_TERRAIN_REJECTION_BYTES)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    henka_terrain_network_write_u64(buffer, rejection->client_nonce);
    buffer[8] = (uint8_t)rejection->reason;
    *out_size = HENKA_TERRAIN_REJECTION_BYTES;
    return HENKA_SUCCESS;
}

henka_result henka_terrain_edit_rejection_decode(
    const uint8_t* buffer,
    size_t buffer_size,
    henka_terrain_edit_rejection* out_rejection)
{
    if (buffer == NULL || out_rejection == NULL || buffer_size != HENKA_TERRAIN_REJECTION_BYTES ||
        buffer[8] > HENKA_TERRAIN_EDIT_REJECT_LIMIT)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    out_rejection->client_nonce = henka_terrain_network_read_u64(buffer);
    out_rejection->reason = (henka_terrain_edit_rejection_reason)buffer[8];
    return HENKA_SUCCESS;
}

henka_result henka_terrain_edit_delta_encode(
    const henka_terrain_edit_delta* delta,
    uint8_t* buffer,
    size_t buffer_capacity,
    size_t* out_size)
{
    size_t size;
    uint32_t index;
    if (delta == NULL || buffer == NULL || out_size == NULL || delta->server_command_id == 0U ||
        !henka_terrain_network_command_fields_valid(&delta->command) ||
        !henka_terrain_network_region_list_valid(delta->affected_region_count, delta->affected_regions))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    size = HENKA_TERRAIN_DELTA_FIXED_BYTES + (size_t)delta->affected_region_count * 16U;
    if (buffer_capacity < size)
    {
        return HENKA_ERROR_LIMIT;
    }
    henka_terrain_network_write_u64(buffer + 0U, delta->world_identity);
    henka_terrain_network_write_u64(buffer + 8U, delta->base_asset_identity);
    henka_terrain_network_write_u64(buffer + 16U, delta->client_nonce);
    henka_terrain_network_write_u64(buffer + 24U, delta->server_command_id);
    henka_terrain_network_write_command(buffer + 32U, &delta->command);
    buffer[56] = (uint8_t)delta->affected_region_count;
    for (index = 0U; index < delta->affected_region_count; ++index)
    {
        size_t offset = HENKA_TERRAIN_DELTA_FIXED_BYTES + (size_t)index * 16U;
        henka_terrain_network_write_u32(buffer + offset, (uint32_t)delta->affected_regions[index].region_id.x);
        henka_terrain_network_write_u32(buffer + offset + 4U, (uint32_t)delta->affected_regions[index].region_id.z);
        henka_terrain_network_write_u64(buffer + offset + 8U, delta->affected_regions[index].revision);
    }
    *out_size = size;
    return HENKA_SUCCESS;
}

henka_result henka_terrain_edit_delta_decode(
    const uint8_t* buffer,
    size_t buffer_size,
    henka_terrain_edit_delta* out_delta)
{
    size_t expected_size;
    uint32_t index;
    if (buffer == NULL || out_delta == NULL || buffer_size < HENKA_TERRAIN_DELTA_FIXED_BYTES)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    memset(out_delta, 0, sizeof(*out_delta));
    out_delta->world_identity = henka_terrain_network_read_u64(buffer + 0U);
    out_delta->base_asset_identity = henka_terrain_network_read_u64(buffer + 8U);
    out_delta->client_nonce = henka_terrain_network_read_u64(buffer + 16U);
    out_delta->server_command_id = henka_terrain_network_read_u64(buffer + 24U);
    if (!henka_terrain_network_read_command(buffer + 32U, &out_delta->command))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    out_delta->command.client_nonce = out_delta->client_nonce;
    out_delta->affected_region_count = buffer[56];
    expected_size = HENKA_TERRAIN_DELTA_FIXED_BYTES +
        (size_t)out_delta->affected_region_count * 16U;
    if (out_delta->server_command_id == 0U ||
        !henka_terrain_network_region_list_valid(
            out_delta->affected_region_count, out_delta->affected_regions) ||
        buffer_size != expected_size)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    for (index = 0U; index < out_delta->affected_region_count; ++index)
    {
        size_t offset = HENKA_TERRAIN_DELTA_FIXED_BYTES + (size_t)index * 16U;
        out_delta->affected_regions[index].region_id.x =
            (int32_t)henka_terrain_network_read_u32(buffer + offset);
        out_delta->affected_regions[index].region_id.z =
            (int32_t)henka_terrain_network_read_u32(buffer + offset + 4U);
        out_delta->affected_regions[index].revision =
            henka_terrain_network_read_u64(buffer + offset + 8U);
        if (out_delta->affected_regions[index].region_id.x < 0 ||
            out_delta->affected_regions[index].region_id.z < 0)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
    }
    return HENKA_SUCCESS;
}
