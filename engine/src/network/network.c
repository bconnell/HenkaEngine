#include <henka/network.h>

#include <string.h>

#include <henka/memory.h>

#include "network_internal.h"

struct henka_network_server
{
    henka_network_transport* transport;
};

struct henka_network_client
{
    henka_network_transport* transport;
};

static henka_network_endpoint henka_network_endpoint_make(const char* host, uint16_t port)
{
    henka_network_endpoint endpoint = {{0}, port};
    if (host != NULL)
    {
        size_t length = strlen(host);
        if (length < sizeof(endpoint.host))
        {
            memcpy(endpoint.host, host, length + 1U);
        }
    }
    return endpoint;
}

static bool henka_network_endpoint_is_valid(const henka_network_endpoint* endpoint)
{
    return endpoint != NULL && endpoint->host[0] != '\0' &&
        memchr(endpoint->host, '\0', sizeof(endpoint->host)) != NULL &&
        endpoint->port != 0U;
}

henka_network_server_desc henka_network_server_desc_default(void)
{
    henka_network_server_desc desc;
    desc.bind_endpoint = henka_network_endpoint_make("0.0.0.0", 7777U);
    desc.max_clients = 32U;
    return desc;
}

henka_network_client_desc henka_network_client_desc_default(void)
{
    henka_network_client_desc desc;
    desc.remote_endpoint = henka_network_endpoint_make("127.0.0.1", 7777U);
    return desc;
}

henka_result henka_network_server_create(
    const henka_network_server_desc* desc,
    henka_network_server** out_server)
{
    henka_network_server* server;

    if (out_server == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_server = NULL;
    if (desc == NULL || !henka_network_endpoint_is_valid(&desc->bind_endpoint) ||
        desc->max_clients == 0U || desc->max_clients > 256U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    server = henka_calloc(1U, sizeof(*server));
    if (server == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    {
        henka_result result = henka_network_transport_create(
            true,
            &desc->bind_endpoint,
            desc->max_clients,
            &server->transport);
        if (result != HENKA_SUCCESS)
        {
            henka_free(server);
            return result;
        }
    }
    *out_server = server;
    return HENKA_SUCCESS;
}

void henka_network_server_destroy(henka_network_server* server)
{
    if (server == NULL)
    {
        return;
    }
    henka_network_transport_destroy(server->transport);
    henka_free(server);
}

henka_result henka_network_client_create(
    const henka_network_client_desc* desc,
    henka_network_client** out_client)
{
    henka_network_client* client;

    if (out_client == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_client = NULL;
    if (desc == NULL || !henka_network_endpoint_is_valid(&desc->remote_endpoint))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    client = henka_calloc(1U, sizeof(*client));
    if (client == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    {
        henka_result result = henka_network_transport_create(
            false,
            &desc->remote_endpoint,
            1U,
            &client->transport);
        if (result != HENKA_SUCCESS)
        {
            henka_free(client);
            return result;
        }
    }
    *out_client = client;
    return HENKA_SUCCESS;
}

void henka_network_client_destroy(henka_network_client* client)
{
    if (client == NULL)
    {
        return;
    }
    henka_network_transport_destroy(client->transport);
    henka_free(client);
}

henka_result henka_network_server_poll(
    henka_network_server* server,
    uint32_t timeout_milliseconds,
    henka_network_event* out_event)
{
    if (server == NULL || timeout_milliseconds > HENKA_NETWORK_MAX_POLL_TIMEOUT_MILLISECONDS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return henka_network_transport_poll(server->transport, timeout_milliseconds, out_event);
}

henka_result henka_network_client_poll(
    henka_network_client* client,
    uint32_t timeout_milliseconds,
    henka_network_event* out_event)
{
    if (client == NULL || timeout_milliseconds > HENKA_NETWORK_MAX_POLL_TIMEOUT_MILLISECONDS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return henka_network_transport_poll(client->transport, timeout_milliseconds, out_event);
}

henka_result henka_network_server_send(
    henka_network_server* server,
    henka_network_peer_id peer_id,
    henka_network_channel channel,
    henka_network_message_type type,
    const void* payload,
    size_t payload_size)
{
    if (server == NULL || peer_id == HENKA_NETWORK_INVALID_PEER_ID)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return henka_network_transport_send(server->transport, peer_id, channel, type, payload, payload_size);
}

henka_result henka_network_client_send(
    henka_network_client* client,
    henka_network_channel channel,
    henka_network_message_type type,
    const void* payload,
    size_t payload_size)
{
    if (client == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return henka_network_transport_send(
        client->transport,
        HENKA_NETWORK_INVALID_PEER_ID,
        channel,
        type,
        payload,
        payload_size);
}

henka_result henka_network_server_disconnect(
    henka_network_server* server,
    henka_network_peer_id peer_id,
    henka_network_disconnect_reason reason)
{
    if (server == NULL || peer_id == HENKA_NETWORK_INVALID_PEER_ID ||
        reason <= HENKA_NETWORK_DISCONNECT_REASON_NONE ||
        reason > HENKA_NETWORK_DISCONNECT_REASON_APPLICATION)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return henka_network_transport_disconnect(server->transport, peer_id, reason);
}

void henka_network_server_get_diagnostics(
    const henka_network_server* server,
    henka_network_diagnostics* out_diagnostics)
{
    if (out_diagnostics == NULL)
    {
        return;
    }
    *out_diagnostics = (henka_network_diagnostics){0};
    if (server != NULL)
    {
        henka_network_transport_get_diagnostics(server->transport, out_diagnostics);
    }
}

void henka_network_client_get_diagnostics(
    const henka_network_client* client,
    henka_network_diagnostics* out_diagnostics)
{
    if (out_diagnostics == NULL)
    {
        return;
    }
    *out_diagnostics = (henka_network_diagnostics){0};
    if (client != NULL)
    {
        henka_network_transport_get_diagnostics(client->transport, out_diagnostics);
    }
}
