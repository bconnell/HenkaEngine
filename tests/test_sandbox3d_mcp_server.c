#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../examples/sandbox3d/mcp_server.h"

typedef struct test_host_state
{
    size_t observations;
    size_t selections;
    size_t exits;
    uint64_t selected_id;
} test_host_state;

static henka_result test_observe(void* user_data, char* out_json, size_t capacity)
{
    test_host_state* state = (test_host_state*)user_data;
    ++state->observations;
    (void)snprintf(out_json, capacity, "{\"observed\":true,\"selected_id\":%llu}", (unsigned long long)state->selected_id);
    return HENKA_SUCCESS;
}

static henka_result test_select(void* user_data, uint64_t document_id, char* out_json, size_t capacity)
{
    test_host_state* state = (test_host_state*)user_data;
    ++state->selections;
    if (document_id != 7U)
    {
        (void)snprintf(out_json, capacity, "{\"success\":false,\"error\":\"not_found\"}");
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    state->selected_id = document_id;
    (void)snprintf(out_json, capacity, "{\"success\":true,\"document_id\":%llu}", (unsigned long long)document_id);
    return HENKA_SUCCESS;
}

static henka_result test_exit(void* user_data, char* out_json, size_t capacity)
{
    test_host_state* state = (test_host_state*)user_data;
    ++state->exits;
    (void)snprintf(out_json, capacity, "{\"success\":true,\"shutdown_requested\":true}");
    return HENKA_SUCCESS;
}

static int expect_contains(const char* value, const char* needle, const char* label)
{
    if (value == NULL || needle == NULL || strstr(value, needle) == NULL)
    {
        fprintf(stderr, "MCP test failed: %s\n", label);
        return 0;
    }
    return 1;
}

static int expect_balanced_json(const char* value, const char* label)
{
    size_t index;
    int braces = 0;
    int brackets = 0;
    bool in_string = false;
    bool escaped = false;

    if (value == NULL || value[0] == '\0')
    {
        fprintf(stderr, "MCP test failed: %s\n", label);
        return 0;
    }
    for (index = 0U; value[index] != '\0'; ++index)
    {
        const char character = value[index];
        if (in_string)
        {
            if (escaped)
            {
                escaped = false;
            }
            else if (character == '\\')
            {
                escaped = true;
            }
            else if (character == '"')
            {
                in_string = false;
            }
            continue;
        }
        if (character == '"')
        {
            in_string = true;
        }
        else if (character == '{')
        {
            ++braces;
        }
        else if (character == '}')
        {
            --braces;
        }
        else if (character == '[')
        {
            ++brackets;
        }
        else if (character == ']')
        {
            --brackets;
        }
        if (braces < 0 || brackets < 0)
        {
            fprintf(stderr, "MCP test failed: %s\n", label);
            return 0;
        }
    }
    if (in_string || escaped || braces != 0 || brackets != 0)
    {
        fprintf(stderr, "MCP test failed: %s\n", label);
        return 0;
    }
    return 1;
}

int main(void)
{
    sandbox3d_mcp_host host;
    sandbox3d_mcp_server* server = NULL;
    test_host_state state = {0};
    char response[SANDBOX3D_MCP_MAX_RESPONSE_BYTES];
    char oversized[SANDBOX3D_MCP_MAX_REQUEST_BYTES + 2U];
    int result = 1;

    host.user_data = &state;
    host.observe = test_observe;
    host.select_object = test_select;
    host.request_exit = test_exit;
    memset(oversized, 'x', sizeof(oversized));
    oversized[sizeof(oversized) - 1U] = '\0';

    if (sandbox3d_mcp_server_create(&host, "candidate-test", &server) != HENKA_SUCCESS)
    {
        return 1;
    }
    if (sandbox3d_mcp_server_process_line(
            server,
            "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"server/discover\"}",
        response,
        sizeof(response)) != HENKA_SUCCESS ||
        !expect_balanced_json(response, "valid discovery JSON") ||
        !expect_contains(response, "2026-07-28", "current MCP spec discovery")) goto cleanup;
    if (sandbox3d_mcp_server_process_line(
            server,
            "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"tools/list\"}",
        response,
        sizeof(response)) != HENKA_SUCCESS ||
        !expect_balanced_json(response, "valid tools JSON") ||
        !expect_contains(response, "henka.select_object", "select tool discovery") ||
        !expect_contains(response, "additionalProperties\":false", "bounded tool schema")) goto cleanup;
    if (sandbox3d_mcp_server_process_line(
            server,
            "{\"jsonrpc\":\"2.0\",\"id\":3,\"method\":\"tools/call\",\"params\":{\"name\":\"henka.observe\",\"arguments\":{}}}",
        response,
        sizeof(response)) != HENKA_SUCCESS ||
        !expect_balanced_json(response, "valid observe JSON") ||
        state.observations != 1U ||
        !expect_contains(response, "candidate-test", "candidate identity") ||
        !expect_contains(response, "observed", "authoritative observation")) goto cleanup;
    if (sandbox3d_mcp_server_process_line(
            server,
            "{\"jsonrpc\":\"2.0\",\"id\":4,\"method\":\"tools/call\",\"params\":{\"name\":\"henka.select_object\",\"arguments\":{\"document_id\":7}}}",
        response,
        sizeof(response)) != HENKA_SUCCESS ||
        !expect_balanced_json(response, "valid selection JSON") ||
        state.selections != 1U ||
        !expect_contains(response, "document_id\":7", "persistent selection identity")) goto cleanup;
    if (sandbox3d_mcp_server_process_line(
            server,
            "{\"jsonrpc\":\"2.0\",\"id\":5,\"method\":\"tools/call\",\"params\":{\"name\":\"henka.select_object\",\"arguments\":{\"document_id\":8}}}",
        response,
        sizeof(response)) != HENKA_ERROR_INVALID_ARGUMENT ||
        !expect_balanced_json(response, "valid semantic error JSON") ||
        !expect_contains(response, "isError\":true", "structured semantic failure")) goto cleanup;
    if (sandbox3d_mcp_server_process_line(
            server,
            "{\"jsonrpc\":\"2.0\",\"id\":6,\"method\":\"tools/call\",\"params\":{\"name\":\"henka.unknown\",\"arguments\":{}}}",
            response,
            sizeof(response)) != HENKA_ERROR_UNKNOWN ||
        !expect_contains(response, "not advertised", "unknown tool rejection")) goto cleanup;
    if (sandbox3d_mcp_server_process_line(
            server,
            "{\"jsonrpc\":\"2.0\",\"id\":7,\"method\":\"tools/call\",\"params\":{\"name\":\"henka.select_object\",\"arguments\":{}}}",
            response,
            sizeof(response)) != HENKA_ERROR_INVALID_ARGUMENT ||
        !expect_contains(response, "-32602", "invalid argument rejection")) goto cleanup;
    if (sandbox3d_mcp_server_process_line(server, oversized, response, sizeof(response)) != HENKA_ERROR_LIMIT ||
        !expect_contains(response, "exceeds", "bounded oversized request rejection")) goto cleanup;
    if (sandbox3d_mcp_server_process_line(
            server,
            "{\"jsonrpc\":\"2.0\",\"id\":8,\"method\":\"tools/call\",\"params\":{\"name\":\"henka.exit\",\"arguments\":{}}}",
            response,
            sizeof(response)) != HENKA_SUCCESS ||
        state.exits != 1U ||
        !expect_contains(response, "shutdown_requested", "bounded shutdown")) goto cleanup;
    result = 0;

cleanup:
    sandbox3d_mcp_server_destroy(server);
    return result;
}
