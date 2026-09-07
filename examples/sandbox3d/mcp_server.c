#include "mcp_server.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/select.h>
#include <unistd.h>
#endif

#include <henka/memory.h>

struct sandbox3d_mcp_server
{
    sandbox3d_mcp_host host;
    char candidate_identity[SANDBOX3D_MCP_MAX_CANDIDATE_ID_BYTES];
};

typedef struct sandbox3d_mcp_writer
{
    char* buffer;
    size_t capacity;
    size_t length;
    bool valid;
} sandbox3d_mcp_writer;

static void sandbox3d_mcp_writer_init(
    sandbox3d_mcp_writer* writer,
    char* buffer,
    size_t capacity)
{
    if (writer == NULL)
    {
        return;
    }
    writer->buffer = buffer;
    writer->capacity = capacity;
    writer->length = 0U;
    writer->valid = buffer != NULL && capacity > 0U;
    if (writer->valid)
    {
        writer->buffer[0] = '\0';
    }
}

static void sandbox3d_mcp_writer_char(
    sandbox3d_mcp_writer* writer,
    char value)
{
    if (writer == NULL || !writer->valid || writer->length + 1U >= writer->capacity)
    {
        if (writer != NULL)
        {
            writer->valid = false;
        }
        return;
    }
    writer->buffer[writer->length++] = value;
    writer->buffer[writer->length] = '\0';
}

static void sandbox3d_mcp_writer_literal(
    sandbox3d_mcp_writer* writer,
    const char* value)
{
    size_t index;
    if (writer == NULL || value == NULL)
    {
        if (writer != NULL)
        {
            writer->valid = false;
        }
        return;
    }
    for (index = 0U; value[index] != '\0'; ++index)
    {
        sandbox3d_mcp_writer_char(writer, value[index]);
    }
}

static void sandbox3d_mcp_writer_json_string(
    sandbox3d_mcp_writer* writer,
    const char* value)
{
    size_t index;
    if (writer == NULL || value == NULL)
    {
        if (writer != NULL)
        {
            writer->valid = false;
        }
        return;
    }
    sandbox3d_mcp_writer_char(writer, '"');
    for (index = 0U; value[index] != '\0' && writer->valid; ++index)
    {
        const unsigned char character = (unsigned char)value[index];
        switch (character)
        {
            case '"': sandbox3d_mcp_writer_literal(writer, "\\\""); break;
            case '\\': sandbox3d_mcp_writer_literal(writer, "\\\\"); break;
            case '\n': sandbox3d_mcp_writer_literal(writer, "\\n"); break;
            case '\r': sandbox3d_mcp_writer_literal(writer, "\\r"); break;
            case '\t': sandbox3d_mcp_writer_literal(writer, "\\t"); break;
            default:
                if (character < 0x20U)
                {
                    writer->valid = false;
                }
                else
                {
                    sandbox3d_mcp_writer_char(writer, (char)character);
                }
                break;
        }
    }
    sandbox3d_mcp_writer_char(writer, '"');
}

static const char* sandbox3d_mcp_skip_space(const char* cursor)
{
    while (cursor != NULL && *cursor != '\0' && isspace((unsigned char)*cursor))
    {
        ++cursor;
    }
    return cursor;
}

static const char* sandbox3d_mcp_find_field(
    const char* json,
    const char* field)
{
    char needle[64];
    int written;
    if (json == NULL || field == NULL)
    {
        return NULL;
    }
    written = snprintf(needle, sizeof(needle), "\"%s\"", field);
    if (written < 0 || (size_t)written >= sizeof(needle))
    {
        return NULL;
    }
    return strstr(json, needle);
}

static bool sandbox3d_mcp_parse_string_at(
    const char* field_start,
    char* out_value,
    size_t out_capacity)
{
    const char* cursor;
    size_t length = 0U;
    if (field_start == NULL || out_value == NULL || out_capacity == 0U)
    {
        return false;
    }
    cursor = strchr(field_start, ':');
    if (cursor == NULL)
    {
        return false;
    }
    cursor = sandbox3d_mcp_skip_space(cursor + 1);
    if (cursor == NULL || *cursor != '"')
    {
        return false;
    }
    ++cursor;
    while (cursor[length] != '\0' && cursor[length] != '"')
    {
        if (cursor[length] == '\\' || length + 1U >= out_capacity)
        {
            return false;
        }
        out_value[length] = cursor[length];
        ++length;
    }
    if (cursor[length] != '"')
    {
        return false;
    }
    out_value[length] = '\0';
    return true;
}

static bool sandbox3d_mcp_parse_uint64_at(
    const char* field_start,
    uint64_t* out_value)
{
    const char* cursor;
    uint64_t value = 0U;
    bool has_digit = false;
    if (field_start == NULL || out_value == NULL)
    {
        return false;
    }
    cursor = strchr(field_start, ':');
    if (cursor == NULL)
    {
        return false;
    }
    cursor = sandbox3d_mcp_skip_space(cursor + 1);
    if (cursor == NULL || *cursor == '-')
    {
        return false;
    }
    while (cursor != NULL && *cursor >= '0' && *cursor <= '9')
    {
        const uint64_t digit = (uint64_t)(*cursor - '0');
        if (value > (UINT64_MAX - digit) / 10U)
        {
            return false;
        }
        value = value * 10U + digit;
        has_digit = true;
        ++cursor;
    }
    if (!has_digit)
    {
        return false;
    }
    *out_value = value;
    return true;
}

static bool sandbox3d_mcp_parse_float_at(
    const char* field_start,
    float* out_value)
{
    const char* cursor;
    char* end = NULL;
    float value;

    if (field_start == NULL || out_value == NULL)
    {
        return false;
    }
    cursor = strchr(field_start, ':');
    if (cursor == NULL)
    {
        return false;
    }
    cursor = sandbox3d_mcp_skip_space(cursor + 1);
    if (cursor == NULL || *cursor == '\0')
    {
        return false;
    }
    errno = 0;
    value = strtof(cursor, &end);
    if (end == cursor || errno == ERANGE || !isfinite(value))
    {
        return false;
    }
    end = (char*)sandbox3d_mcp_skip_space(end);
    if (*end != '\0' && *end != ',' && *end != '}' && *end != ']')
    {
        return false;
    }
    *out_value = value;
    return true;
}

static bool sandbox3d_mcp_parse_id(
    const char* json,
    char* out_id,
    size_t out_id_capacity)
{
    const char* field;
    const char* cursor;
    size_t length = 0U;
    if (json == NULL || out_id == NULL || out_id_capacity == 0U)
    {
        return false;
    }
    field = sandbox3d_mcp_find_field(json, "id");
    if (field == NULL)
    {
        return false;
    }
    cursor = strchr(field, ':');
    if (cursor == NULL)
    {
        return false;
    }
    cursor = sandbox3d_mcp_skip_space(cursor + 1);
    if (cursor == NULL || *cursor == '\0')
    {
        return false;
    }
    if (*cursor == '"')
    {
        ++cursor;
        while (cursor[length] != '\0' && cursor[length] != '"')
        {
            if (cursor[length] == '\\' || length + 1U >= out_id_capacity)
            {
                return false;
            }
            out_id[length] = cursor[length];
            ++length;
        }
        if (cursor[length] != '"')
        {
            return false;
        }
    }
    else
    {
        while (cursor[length] != '\0' &&
            cursor[length] != ',' && cursor[length] != '}' &&
            !isspace((unsigned char)cursor[length]))
        {
            if (length + 1U >= out_id_capacity)
            {
                return false;
            }
            out_id[length] = cursor[length];
            ++length;
        }
    }
    if (length == 0U)
    {
        return false;
    }
    out_id[length] = '\0';
    return true;
}

static void sandbox3d_mcp_write_semantic_result(
    const sandbox3d_mcp_server* server,
    const char* request_id,
    bool is_error,
    const char* text,
    const char* state_json,
    char* out_response,
    size_t out_response_capacity)
{
    sandbox3d_mcp_writer writer;
    sandbox3d_mcp_writer_init(&writer, out_response, out_response_capacity);
    sandbox3d_mcp_writer_literal(&writer, "{\"jsonrpc\":\"2.0\",\"id\":");
    sandbox3d_mcp_writer_literal(&writer, request_id);
    sandbox3d_mcp_writer_literal(&writer, ",\"result\":{\"isError\":");
    sandbox3d_mcp_writer_literal(&writer, is_error ? "true" : "false");
    sandbox3d_mcp_writer_literal(&writer, ",\"content\":[{\"type\":\"text\",\"text\":");
    sandbox3d_mcp_writer_json_string(&writer, text == NULL ? "" : text);
    sandbox3d_mcp_writer_literal(&writer, "}],\"structuredContent\":{\"candidate_identity\":");
    if (server != NULL && server->candidate_identity[0] != '\0')
    {
        sandbox3d_mcp_writer_json_string(&writer, server->candidate_identity);
    }
    else
    {
        sandbox3d_mcp_writer_literal(&writer, "null");
    }
    sandbox3d_mcp_writer_literal(&writer, ",\"state\":");
    sandbox3d_mcp_writer_literal(&writer, state_json == NULL ? "null" : state_json);
    sandbox3d_mcp_writer_literal(&writer, "}}}");
    if (!writer.valid)
    {
        if (out_response != NULL && out_response_capacity > 0U)
        {
            out_response[0] = '\0';
        }
    }
}

static void sandbox3d_mcp_write_error(
    const char* request_id,
    int code,
    const char* message,
    char* out_response,
    size_t out_response_capacity)
{
    sandbox3d_mcp_writer writer;
    sandbox3d_mcp_writer_init(&writer, out_response, out_response_capacity);
    sandbox3d_mcp_writer_literal(&writer, "{\"jsonrpc\":\"2.0\",\"id\":");
    sandbox3d_mcp_writer_literal(&writer, request_id == NULL ? "null" : request_id);
    sandbox3d_mcp_writer_literal(&writer, ",\"error\":{\"code\":");
    {
        char number[32];
        (void)snprintf(number, sizeof(number), "%d", code);
        sandbox3d_mcp_writer_literal(&writer, number);
    }
    sandbox3d_mcp_writer_literal(&writer, ",\"message\":");
    sandbox3d_mcp_writer_json_string(&writer, message == NULL ? "MCP request rejected" : message);
    sandbox3d_mcp_writer_literal(&writer, "}}");
    if (!writer.valid && out_response != NULL && out_response_capacity > 0U)
    {
        out_response[0] = '\0';
    }
}

henka_result sandbox3d_mcp_server_create(
    const sandbox3d_mcp_host* host,
    const char* candidate_identity,
    sandbox3d_mcp_server** out_server)
{
    sandbox3d_mcp_server* server;
    int written;
    if (out_server == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_server = NULL;
    if (host == NULL || host->observe == NULL || host->select_object == NULL ||
        host->set_authoring_selection_mode == NULL || host->select_face == NULL ||
        host->extrude_selected_faces == NULL || host->undo_authoring == NULL ||
        host->redo_authoring == NULL || host->request_exit == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    server = (sandbox3d_mcp_server*)henka_calloc(1U, sizeof(*server));
    if (server == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    server->host = *host;
    if (candidate_identity != NULL && candidate_identity[0] != '\0')
    {
        written = snprintf(
            server->candidate_identity,
            sizeof(server->candidate_identity),
            "%s",
            candidate_identity);
        if (written < 0 || (size_t)written >= sizeof(server->candidate_identity))
        {
            henka_free(server);
            return HENKA_ERROR_LIMIT;
        }
    }
    *out_server = server;
    return HENKA_SUCCESS;
}

void sandbox3d_mcp_server_destroy(sandbox3d_mcp_server* server)
{
    henka_free(server);
}

static void sandbox3d_mcp_write_discovery(
    const sandbox3d_mcp_server* server,
    const char* request_id,
    char* out_response,
    size_t out_response_capacity)
{
    const char* state =
        "{\"protocol\":\"mcp\",\"spec_version\":\"2026-07-28\","
        "\"transport\":\"stdio\",\"stateless\":true,"
        "\"tools\":[\"henka.observe\",\"henka.select_object\","
        "\"henka.authoring_set_selection_mode\",\"henka.authoring_select_face\","
        "\"henka.authoring_extrude_faces\",\"henka.authoring_undo\","
        "\"henka.authoring_redo\",\"henka.exit\"]}";
    sandbox3d_mcp_write_semantic_result(
        server,
        request_id,
        false,
        "Henka MCP capability discovery",
        state,
        out_response,
        out_response_capacity);
}

static void sandbox3d_mcp_write_tools(
    const sandbox3d_mcp_server* server,
    const char* request_id,
    char* out_response,
    size_t out_response_capacity)
{
    const char* state =
        "{\"tools\":["
        "{\"name\":\"henka.observe\",\"description\":\"Read authoritative live scene, persistent identity, revision, viewport, and selection state.\",\"inputSchema\":{\"type\":\"object\",\"properties\":{},\"additionalProperties\":false}},"
        "{\"name\":\"henka.select_object\",\"description\":\"Select one live editable scene object by persistent Scene Document ID through the canonical Action API.\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"document_id\":{\"type\":\"integer\",\"minimum\":1}},\"required\":[\"document_id\"],\"additionalProperties\":false}},"
        "{\"name\":\"henka.authoring_set_selection_mode\",\"description\":\"Set the canonical authoring component-selection mode for one selected editable object.\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"document_id\":{\"type\":\"integer\",\"minimum\":1},\"mode\":{\"type\":\"string\",\"enum\":[\"vertex\",\"edge\",\"face\"]}},\"required\":[\"document_id\",\"mode\"],\"additionalProperties\":false}},"
        "{\"name\":\"henka.authoring_select_face\",\"description\":\"Select one real source-authoritative face by persistent object identity and face identity.\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"document_id\":{\"type\":\"integer\",\"minimum\":1},\"face_id\":{\"type\":\"integer\",\"minimum\":1}},\"required\":[\"document_id\",\"face_id\"],\"additionalProperties\":false}},"
        "{\"name\":\"henka.authoring_extrude_faces\",\"description\":\"Extrude the currently selected real faces through the canonical transactional authoring operation.\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"document_id\":{\"type\":\"integer\",\"minimum\":1},\"distance\":{\"type\":\"number\",\"minimum\":-1000000,\"maximum\":1000000}},\"required\":[\"document_id\",\"distance\"],\"additionalProperties\":false}},"
        "{\"name\":\"henka.authoring_undo\",\"description\":\"Move the selected real editable object through its canonical authoring history undo path.\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"document_id\":{\"type\":\"integer\",\"minimum\":1}},\"required\":[\"document_id\"],\"additionalProperties\":false}},"
        "{\"name\":\"henka.authoring_redo\",\"description\":\"Move the selected real editable object through its canonical authoring history redo path.\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"document_id\":{\"type\":\"integer\",\"minimum\":1}},\"required\":[\"document_id\"],\"additionalProperties\":false}},"
        "{\"name\":\"henka.exit\",\"description\":\"Request clean shutdown of this local validation candidate.\",\"inputSchema\":{\"type\":\"object\",\"properties\":{},\"additionalProperties\":false}}"
        "]}";
    sandbox3d_mcp_write_semantic_result(
        server,
        request_id,
        false,
        "Henka MCP tools",
        state,
        out_response,
        out_response_capacity);
}

henka_result sandbox3d_mcp_server_process_line(
    sandbox3d_mcp_server* server,
    const char* line,
    char* out_response,
    size_t out_response_capacity)
{
    char request_id[96];
    char method[64];
    char tool_name[96];
    char mode[32];
    uint64_t document_id = 0U;
    uint64_t face_id = 0U;
    float distance = 0.0f;
    henka_result result;
    char state_json[SANDBOX3D_MCP_MAX_RESPONSE_BYTES];
    if (out_response == NULL || out_response_capacity == 0U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    out_response[0] = '\0';
    if (server == NULL || line == NULL || strlen(line) > SANDBOX3D_MCP_MAX_REQUEST_BYTES)
    {
        sandbox3d_mcp_write_error("null", -32600, "Request is missing or exceeds the bounded MCP input size.", out_response, out_response_capacity);
        return HENKA_ERROR_LIMIT;
    }
    if (!sandbox3d_mcp_parse_id(line, request_id, sizeof(request_id)) ||
        !sandbox3d_mcp_parse_string_at(
            sandbox3d_mcp_find_field(line, "method"), method, sizeof(method)))
    {
        sandbox3d_mcp_write_error("null", -32600, "A non-notification JSON-RPC method and id are required.", out_response, out_response_capacity);
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (strcmp(method, "server/discover") == 0)
    {
        sandbox3d_mcp_write_discovery(server, request_id, out_response, out_response_capacity);
        return HENKA_SUCCESS;
    }
    if (strcmp(method, "tools/list") == 0)
    {
        sandbox3d_mcp_write_tools(server, request_id, out_response, out_response_capacity);
        return HENKA_SUCCESS;
    }
    if (strcmp(method, "tools/call") != 0)
    {
        sandbox3d_mcp_write_error(request_id, -32601, "MCP method is not supported.", out_response, out_response_capacity);
        return HENKA_ERROR_UNKNOWN;
    }
    if (!sandbox3d_mcp_parse_string_at(
            sandbox3d_mcp_find_field(line, "name"), tool_name, sizeof(tool_name)))
    {
        sandbox3d_mcp_write_error(request_id, -32602, "tools/call requires a bounded tool name.", out_response, out_response_capacity);
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    state_json[0] = '\0';
    if (strcmp(tool_name, "henka.observe") == 0)
    {
        result = server->host.observe(
            server->host.user_data,
            state_json,
            sizeof(state_json));
        sandbox3d_mcp_write_semantic_result(
            server,
            request_id,
            result != HENKA_SUCCESS,
            result == HENKA_SUCCESS ? "Henka observation" : henka_result_to_string(result),
            state_json[0] == '\0' ? "null" : state_json,
            out_response,
            out_response_capacity);
        return result;
    }
    if (strcmp(tool_name, "henka.select_object") == 0)
    {
        const char* document_field = sandbox3d_mcp_find_field(line, "document_id");
        if (!sandbox3d_mcp_parse_uint64_at(document_field, &document_id) || document_id == 0U)
        {
            sandbox3d_mcp_write_error(request_id, -32602, "henka.select_object requires a positive document_id.", out_response, out_response_capacity);
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        result = server->host.select_object(
            server->host.user_data,
            document_id,
            state_json,
            sizeof(state_json));
        sandbox3d_mcp_write_semantic_result(
            server,
            request_id,
            result != HENKA_SUCCESS,
            result == HENKA_SUCCESS ? "Henka selection" : henka_result_to_string(result),
            state_json[0] == '\0' ? "null" : state_json,
            out_response,
            out_response_capacity);
        return result;
    }
    if (strcmp(tool_name, "henka.authoring_set_selection_mode") == 0)
    {
        const char* document_field = sandbox3d_mcp_find_field(line, "document_id");
        if (!sandbox3d_mcp_parse_uint64_at(document_field, &document_id) || document_id == 0U ||
            !sandbox3d_mcp_parse_string_at(
                sandbox3d_mcp_find_field(line, "mode"), mode, sizeof(mode)))
        {
            sandbox3d_mcp_write_error(request_id, -32602, "henka.authoring_set_selection_mode requires document_id and mode.", out_response, out_response_capacity);
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        result = server->host.set_authoring_selection_mode(
            server->host.user_data,
            document_id,
            mode,
            state_json,
            sizeof(state_json));
        sandbox3d_mcp_write_semantic_result(
            server,
            request_id,
            result != HENKA_SUCCESS,
            result == HENKA_SUCCESS ? "Henka authoring selection mode" : henka_result_to_string(result),
            state_json[0] == '\0' ? "null" : state_json,
            out_response,
            out_response_capacity);
        return result;
    }
    if (strcmp(tool_name, "henka.authoring_select_face") == 0)
    {
        const char* document_field = sandbox3d_mcp_find_field(line, "document_id");
        const char* face_field = sandbox3d_mcp_find_field(line, "face_id");
        if (!sandbox3d_mcp_parse_uint64_at(document_field, &document_id) || document_id == 0U ||
            !sandbox3d_mcp_parse_uint64_at(face_field, &face_id) || face_id == 0U ||
            face_id > UINT32_MAX)
        {
            sandbox3d_mcp_write_error(request_id, -32602, "henka.authoring_select_face requires positive bounded document_id and face_id.", out_response, out_response_capacity);
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        result = server->host.select_face(
            server->host.user_data,
            document_id,
            face_id,
            state_json,
            sizeof(state_json));
        sandbox3d_mcp_write_semantic_result(
            server,
            request_id,
            result != HENKA_SUCCESS,
            result == HENKA_SUCCESS ? "Henka authoring face selection" : henka_result_to_string(result),
            state_json[0] == '\0' ? "null" : state_json,
            out_response,
            out_response_capacity);
        return result;
    }
    if (strcmp(tool_name, "henka.authoring_extrude_faces") == 0)
    {
        const char* document_field = sandbox3d_mcp_find_field(line, "document_id");
        if (!sandbox3d_mcp_parse_uint64_at(document_field, &document_id) || document_id == 0U ||
            !sandbox3d_mcp_parse_float_at(
                sandbox3d_mcp_find_field(line, "distance"), &distance))
        {
            sandbox3d_mcp_write_error(request_id, -32602, "henka.authoring_extrude_faces requires document_id and finite distance.", out_response, out_response_capacity);
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        result = server->host.extrude_selected_faces(
            server->host.user_data,
            document_id,
            distance,
            state_json,
            sizeof(state_json));
        sandbox3d_mcp_write_semantic_result(
            server,
            request_id,
            result != HENKA_SUCCESS,
            result == HENKA_SUCCESS ? "Henka authoring face extrusion" : henka_result_to_string(result),
            state_json[0] == '\0' ? "null" : state_json,
            out_response,
            out_response_capacity);
        return result;
    }
    if (strcmp(tool_name, "henka.authoring_undo") == 0 ||
        strcmp(tool_name, "henka.authoring_redo") == 0)
    {
        const char* document_field = sandbox3d_mcp_find_field(line, "document_id");
        if (!sandbox3d_mcp_parse_uint64_at(document_field, &document_id) || document_id == 0U)
        {
            sandbox3d_mcp_write_error(
                request_id,
                -32602,
                strcmp(tool_name, "henka.authoring_undo") == 0
                    ? "henka.authoring_undo requires a positive document_id."
                    : "henka.authoring_redo requires a positive document_id.",
                out_response,
                out_response_capacity);
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        result = strcmp(tool_name, "henka.authoring_undo") == 0
            ? server->host.undo_authoring(
                server->host.user_data,
                document_id,
                state_json,
                sizeof(state_json))
            : server->host.redo_authoring(
                server->host.user_data,
                document_id,
                state_json,
                sizeof(state_json));
        sandbox3d_mcp_write_semantic_result(
            server,
            request_id,
            result != HENKA_SUCCESS,
            result == HENKA_SUCCESS
                ? (strcmp(tool_name, "henka.authoring_undo") == 0
                    ? "Henka authoring undo"
                    : "Henka authoring redo")
                : henka_result_to_string(result),
            state_json[0] == '\0' ? "null" : state_json,
            out_response,
            out_response_capacity);
        return result;
    }
    if (strcmp(tool_name, "henka.exit") == 0)
    {
        result = server->host.request_exit(
            server->host.user_data,
            state_json,
            sizeof(state_json));
        sandbox3d_mcp_write_semantic_result(
            server,
            request_id,
            result != HENKA_SUCCESS,
            result == HENKA_SUCCESS ? "Henka candidate shutdown requested" : henka_result_to_string(result),
            state_json[0] == '\0' ? "null" : state_json,
            out_response,
            out_response_capacity);
        return result;
    }
    sandbox3d_mcp_write_error(request_id, -32601, "Henka MCP tool is not advertised by this candidate.", out_response, out_response_capacity);
    return HENKA_ERROR_UNKNOWN;
}

static bool sandbox3d_mcp_stdin_ready(void)
{
#if defined(_WIN32)
    HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
    DWORD available = 0U;
    if (input == NULL || input == INVALID_HANDLE_VALUE ||
        GetFileType(input) != FILE_TYPE_PIPE)
    {
        return false;
    }
    return PeekNamedPipe(input, NULL, 0U, NULL, &available, NULL) != 0 && available > 0U;
#else
    fd_set read_set;
    struct timeval timeout = {0, 0};
    FD_ZERO(&read_set);
    FD_SET(STDIN_FILENO, &read_set);
    return select(STDIN_FILENO + 1, &read_set, NULL, NULL, &timeout) > 0;
#endif
}

bool sandbox3d_mcp_server_poll(sandbox3d_mcp_server* server)
{
    char line[SANDBOX3D_MCP_MAX_REQUEST_BYTES + 2U];
    char response[SANDBOX3D_MCP_MAX_RESPONSE_BYTES];
    size_t line_length;
    if (server == NULL || !sandbox3d_mcp_stdin_ready())
    {
        return false;
    }
    if (fgets(line, sizeof(line), stdin) == NULL)
    {
        return false;
    }
    line_length = strlen(line);
    if (line_length > 0U && line[line_length - 1U] == '\n')
    {
        line[line_length - 1U] = '\0';
    }
    (void)sandbox3d_mcp_server_process_line(
        server,
        line,
        response,
        sizeof(response));
    if (response[0] != '\0')
    {
        fputs(response, stdout);
        fputc('\n', stdout);
        fflush(stdout);
    }
    return true;
}
