#ifndef SANDBOX3D_MCP_SERVER_H
#define SANDBOX3D_MCP_SERVER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <henka/result.h>

#define SANDBOX3D_MCP_MAX_REQUEST_BYTES 8192U
#define SANDBOX3D_MCP_MAX_RESPONSE_BYTES 32768U
#define SANDBOX3D_MCP_MAX_CANDIDATE_ID_BYTES 256U

typedef struct sandbox3d_mcp_server sandbox3d_mcp_server;

/* Callbacks are owned by the product host.  The protocol layer owns only the
 * bounded request/response framing and never stores product state or IDs. */
typedef struct sandbox3d_mcp_host
{
    void* user_data;
    henka_result (*observe)(
        void* user_data,
        char* out_json,
        size_t out_json_capacity);
    henka_result (*select_object)(
        void* user_data,
        uint64_t document_id,
        char* out_json,
        size_t out_json_capacity);
    henka_result (*request_exit)(
        void* user_data,
        char* out_json,
        size_t out_json_capacity);
} sandbox3d_mcp_host;

henka_result sandbox3d_mcp_server_create(
    const sandbox3d_mcp_host* host,
    const char* candidate_identity,
    sandbox3d_mcp_server** out_server);
void sandbox3d_mcp_server_destroy(sandbox3d_mcp_server* server);

/* Processes one complete newline-delimited JSON-RPC request without touching
 * stdin/stdout.  This is the bounded protocol seam used by self-tests. */
henka_result sandbox3d_mcp_server_process_line(
    sandbox3d_mcp_server* server,
    const char* line,
    char* out_response,
    size_t out_response_capacity);

/* Polls local stdio without blocking the engine frame.  At most one request
 * is consumed per call.  Responses are written to stdout and flushed. */
bool sandbox3d_mcp_server_poll(sandbox3d_mcp_server* server);

#endif
