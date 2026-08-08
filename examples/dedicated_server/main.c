#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <henka/runtime.h>

#define HENKA_SERVER_PATH_CAPACITY 1024U
#define HENKA_SERVER_BIND_CAPACITY 64U
#define HENKA_SERVER_CONFIG_LINE_CAPACITY 2048U
#define HENKA_SERVER_MAX_SUBSTEPS_PER_LOOP 4U

typedef struct henka_server_options
{
    char bind_address[HENKA_SERVER_BIND_CAPACITY];
    unsigned short port;
    unsigned int max_clients;
    unsigned int tick_rate;
    char world_path[HENKA_SERVER_PATH_CAPACITY];
    char save_root[HENKA_SERVER_PATH_CAPACITY];
    char config_path[HENKA_SERVER_PATH_CAPACITY];
    int has_world_path;
    int has_save_root;
    int has_config_path;
    int smoke;
} henka_server_options;

static volatile sig_atomic_t g_henka_server_running = 1;

static void henka_server_request_shutdown(int signal_number)
{
    (void)signal_number;
    g_henka_server_running = 0;
}

static void henka_server_print_usage(void)
{
    puts("henka_dedicated_server [options]");
    puts("  --bind ADDRESS       Bind address (default 0.0.0.0)");
    puts("  --port PORT          Port 1-65535 (default 7777)");
    puts("  --max-clients COUNT  Client limit 1-256 (default 32)");
    puts("  --tick-rate HZ       Fixed simulation rate 1-240 (default 60)");
    puts("  --world PATH         Base world path");
    puts("  --save-root PATH     Runtime save root");
    puts("  --config PATH        Optional key=value server configuration");
    puts("  --smoke              Initialize, tick, and shut down");
    puts("  --help               Show this help");
}

static int henka_server_copy_text(char* destination, size_t capacity, const char* source)
{
    size_t length;

    if (destination == NULL || source == NULL || source[0] == '\0')
    {
        return 0;
    }
    length = strlen(source);
    if (length == 0U || length >= capacity)
    {
        return 0;
    }
    memcpy(destination, source, length + 1U);
    return 1;
}

static int henka_server_parse_uint(const char* text, unsigned long maximum, unsigned int* out_value)
{
    char* end;
    unsigned long value;

    if (text == NULL || text[0] == '\0' || out_value == NULL)
    {
        return 0;
    }
    errno = 0;
    value = strtoul(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || value == 0UL || value > maximum)
    {
        return 0;
    }
    *out_value = (unsigned int)value;
    return 1;
}

static char* henka_server_trim(char* text)
{
    char* end;

    while (*text != '\0' && isspace((unsigned char)*text))
    {
        ++text;
    }
    end = text + strlen(text);
    while (end > text && isspace((unsigned char)end[-1]))
    {
        --end;
    }
    *end = '\0';
    return text;
}

static int henka_server_apply_config_value(
    henka_server_options* options,
    const char* key,
    const char* value)
{
    unsigned int parsed;

    if (strcmp(key, "bind") == 0)
    {
        return henka_server_copy_text(options->bind_address, sizeof(options->bind_address), value);
    }
    if (strcmp(key, "port") == 0)
    {
        return henka_server_parse_uint(value, 65535UL, &parsed) &&
            (options->port = (unsigned short)parsed, 1);
    }
    if (strcmp(key, "max_clients") == 0)
    {
        return henka_server_parse_uint(value, 256UL, &options->max_clients);
    }
    if (strcmp(key, "tick_rate") == 0)
    {
        return henka_server_parse_uint(value, 240UL, &options->tick_rate);
    }
    if (strcmp(key, "world") == 0)
    {
        return henka_server_copy_text(options->world_path, sizeof(options->world_path), value) &&
            (options->has_world_path = 1, 1);
    }
    if (strcmp(key, "save_root") == 0)
    {
        return henka_server_copy_text(options->save_root, sizeof(options->save_root), value) &&
            (options->has_save_root = 1, 1);
    }
    return 0;
}

static int henka_server_load_config(henka_server_options* options)
{
    FILE* file;
    char line[HENKA_SERVER_CONFIG_LINE_CAPACITY];
    unsigned int line_number = 0U;

    if (!options->has_config_path)
    {
        return 1;
    }
#if defined(_WIN32)
    if (fopen_s(&file, options->config_path, "rb") != 0)
    {
        file = NULL;
    }
#else
    file = fopen(options->config_path, "rb");
#endif
    if (file == NULL)
    {
        fprintf(stderr, "unable to open config '%s'\n", options->config_path);
        return 0;
    }
    while (fgets(line, sizeof(line), file) != NULL)
    {
        char* separator;
        char* key;
        char* value;
        ++line_number;
        if (strchr(line, '\n') == NULL && !feof(file))
        {
            fprintf(stderr, "config line %u exceeds the bounded line limit\n", line_number);
            fclose(file);
            return 0;
        }
        separator = strchr(line, '#');
        if (separator != NULL)
        {
            *separator = '\0';
        }
        key = henka_server_trim(line);
        if (key[0] == '\0')
        {
            continue;
        }
        separator = strchr(key, '=');
        if (separator == NULL)
        {
            fprintf(stderr, "config line %u must use key=value\n", line_number);
            fclose(file);
            return 0;
        }
        *separator = '\0';
        value = henka_server_trim(separator + 1);
        key = henka_server_trim(key);
        if (value[0] == '"')
        {
            size_t length = strlen(value);
            if (length < 2U || value[length - 1U] != '"')
            {
                fprintf(stderr, "config line %u has an invalid quoted value\n", line_number);
                fclose(file);
                return 0;
            }
            value[length - 1U] = '\0';
            ++value;
        }
        if (!henka_server_apply_config_value(options, key, value))
        {
            fprintf(stderr, "invalid or unknown config setting on line %u\n", line_number);
            fclose(file);
            return 0;
        }
    }
    if (ferror(file) != 0)
    {
        fclose(file);
        fprintf(stderr, "unable to read config '%s'\n", options->config_path);
        return 0;
    }
    fclose(file);
    return 1;
}

static int henka_server_parse_options(int argc, char** argv, henka_server_options* out_options)
{
    int index;

    if (out_options == NULL)
    {
        return 0;
    }
    memset(out_options, 0, sizeof(*out_options));
    (void)henka_server_copy_text(out_options->bind_address, sizeof(out_options->bind_address), "0.0.0.0");
    out_options->port = 7777U;
    out_options->max_clients = 32U;
    out_options->tick_rate = 60U;

    for (index = 1; index < argc; ++index)
    {
        if (strcmp(argv[index], "--help") == 0)
        {
            henka_server_print_usage();
            return -1;
        }
        if (strcmp(argv[index], "--config") == 0)
        {
            if (index + 1 >= argc ||
                !henka_server_copy_text(
                    out_options->config_path,
                    sizeof(out_options->config_path),
                    argv[++index]))
            {
                fprintf(stderr, "invalid --config\n");
                return 0;
            }
            out_options->has_config_path = 1;
        }
    }
    if (!henka_server_load_config(out_options))
    {
        return 0;
    }
    for (index = 1; index < argc; ++index)
    {
        const char* argument = argv[index];
        if (strcmp(argument, "--smoke") == 0)
        {
            out_options->smoke = 1;
        }
        else if (strcmp(argument, "--config") == 0)
        {
            ++index;
        }
        else if (index + 1 >= argc)
        {
            fprintf(stderr, "missing value for %s\n", argument);
            return 0;
        }
        else if (strcmp(argument, "--bind") == 0)
        {
            if (!henka_server_copy_text(out_options->bind_address, sizeof(out_options->bind_address), argv[++index]))
            {
                fprintf(stderr, "invalid --bind\n");
                return 0;
            }
        }
        else if (strcmp(argument, "--port") == 0)
        {
            unsigned int value;
            if (!henka_server_parse_uint(argv[++index], 65535UL, &value))
            {
                fprintf(stderr, "invalid --port\n");
                return 0;
            }
            out_options->port = (unsigned short)value;
        }
        else if (strcmp(argument, "--max-clients") == 0)
        {
            if (!henka_server_parse_uint(argv[++index], 256UL, &out_options->max_clients))
            {
                fprintf(stderr, "invalid --max-clients\n");
                return 0;
            }
        }
        else if (strcmp(argument, "--tick-rate") == 0)
        {
            if (!henka_server_parse_uint(argv[++index], 240UL, &out_options->tick_rate))
            {
                fprintf(stderr, "invalid --tick-rate\n");
                return 0;
            }
        }
        else if (strcmp(argument, "--world") == 0)
        {
            if (!henka_server_copy_text(out_options->world_path, sizeof(out_options->world_path), argv[++index]))
            {
                fprintf(stderr, "invalid --world\n");
                return 0;
            }
            out_options->has_world_path = 1;
        }
        else if (strcmp(argument, "--save-root") == 0)
        {
            if (!henka_server_copy_text(out_options->save_root, sizeof(out_options->save_root), argv[++index]))
            {
                fprintf(stderr, "invalid --save-root\n");
                return 0;
            }
            out_options->has_save_root = 1;
        }
        else
        {
            fprintf(stderr, "unknown option: %s\n", argument);
            return 0;
        }
    }
    return 1;
}

static void henka_server_track_peer(
    henka_network_peer_id* peers,
    size_t* peer_count,
    henka_network_peer_id peer_id)
{
    if (*peer_count < 256U)
    {
        peers[*peer_count] = peer_id;
        *peer_count += 1U;
    }
}

static void henka_server_untrack_peer(
    henka_network_peer_id* peers,
    size_t* peer_count,
    henka_network_peer_id peer_id)
{
    size_t index;
    for (index = 0U; index < *peer_count; ++index)
    {
        if (peers[index] == peer_id)
        {
            peers[index] = peers[*peer_count - 1U];
            *peer_count -= 1U;
            return;
        }
    }
}

static void henka_server_disconnect_peers(
    henka_network_server* server,
    henka_network_peer_id* peers,
    size_t* peer_count)
{
    size_t index;
    henka_network_event event;

    for (index = 0U; index < *peer_count; ++index)
    {
        (void)henka_network_server_disconnect(
            server,
            peers[index],
            HENKA_NETWORK_DISCONNECT_REASON_SERVER_SHUTDOWN);
    }
    for (index = 0U; index < 32U && *peer_count > 0U; ++index)
    {
        if (henka_network_server_poll(server, 5U, &event) != HENKA_SUCCESS)
        {
            break;
        }
        if (event.type == HENKA_NETWORK_EVENT_DISCONNECTED)
        {
            henka_server_untrack_peer(peers, peer_count, event.peer_id);
        }
    }
}

int main(int argc, char** argv)
{
    henka_server_options options;
    henka_scene* scene = NULL;
    henka_physics_world* physics = NULL;
    henka_network_server* network = NULL;
    henka_network_server_desc network_desc;
    henka_network_event event;
    henka_network_peer_id peers[256];
    size_t peer_count = 0U;
    henka_time_state time_state = {0};
    double accumulator = 0.0;
    double fixed_timestep;
    int parse_result;
    int exit_code = 0;

    parse_result = henka_server_parse_options(argc, argv, &options);
    if (parse_result <= 0)
    {
        return parse_result < 0 ? 0 : 2;
    }
    if (henka_scene_create(&scene) != HENKA_SUCCESS ||
        henka_physics_world_create(&physics) != HENKA_SUCCESS)
    {
        henka_physics_world_destroy(physics);
        henka_scene_destroy(scene);
        fprintf(stderr, "headless runtime initialization failed\n");
        return 3;
    }
    fixed_timestep = 1.0 / (double)options.tick_rate;
    if (henka_physics_world_set_fixed_timestep(physics, (float)fixed_timestep) != HENKA_SUCCESS)
    {
        fprintf(stderr, "fixed timestep configuration failed\n");
        exit_code = 4;
        goto shutdown;
    }
    network_desc = henka_network_server_desc_default();
    if (!henka_server_copy_text(
            network_desc.bind_endpoint.host,
            sizeof(network_desc.bind_endpoint.host),
            options.bind_address))
    {
        fprintf(stderr, "bind address exceeds the bounded endpoint limit\n");
        exit_code = 5;
        goto shutdown;
    }
    network_desc.bind_endpoint.port = options.port;
    network_desc.max_clients = options.max_clients;
    if (henka_network_server_create(&network_desc, &network) != HENKA_SUCCESS)
    {
        fprintf(stderr, "network server initialization failed\n");
        exit_code = 6;
        goto shutdown;
    }
    (void)signal(SIGINT, henka_server_request_shutdown);
#if defined(SIGTERM)
    (void)signal(SIGTERM, henka_server_request_shutdown);
#endif
    henka_time_reset(&time_state);
    printf("Henka dedicated server running: %s:%u, max clients %u, tick rate %u Hz\n",
        options.bind_address,
        (unsigned int)options.port,
        options.max_clients,
        options.tick_rate);
    if (options.has_world_path)
    {
        printf("world: %s\n", options.world_path);
    }
    if (options.has_save_root)
    {
        printf("save root: %s\n", options.save_root);
    }
    if (options.has_config_path)
    {
        printf("config: %s\n", options.config_path);
    }
    if (options.smoke)
    {
        if (henka_physics_world_step_fixed(physics) != HENKA_SUCCESS)
        {
            fprintf(stderr, "headless simulation step failed\n");
            exit_code = 7;
        }
        puts("Dedicated server smoke initialized.");
        g_henka_server_running = 0;
    }
    while (g_henka_server_running)
    {
        henka_result poll_result = henka_network_server_poll(network, 10U, &event);
        if (poll_result != HENKA_SUCCESS && poll_result != HENKA_ERROR_INVALID_ARGUMENT)
        {
            fprintf(stderr, "network polling failed\n");
            exit_code = 8;
            break;
        }
        if (poll_result == HENKA_SUCCESS)
        {
            if (event.type == HENKA_NETWORK_EVENT_CONNECTED)
            {
                henka_server_track_peer(peers, &peer_count, event.peer_id);
            }
            else if (event.type == HENKA_NETWORK_EVENT_DISCONNECTED)
            {
                henka_server_untrack_peer(peers, &peer_count, event.peer_id);
            }
            else if (event.type == HENKA_NETWORK_EVENT_MESSAGE &&
                event.message.type == HENKA_NETWORK_MESSAGE_PING)
            {
                (void)henka_network_server_send(
                    network,
                    event.peer_id,
                    event.message.channel,
                    HENKA_NETWORK_MESSAGE_PING,
                    event.message.payload,
                    event.message.payload_size);
            }
        }
        henka_time_tick(&time_state);
        accumulator += time_state.delta_seconds;
        if (accumulator > fixed_timestep * HENKA_SERVER_MAX_SUBSTEPS_PER_LOOP)
        {
            accumulator = fixed_timestep * HENKA_SERVER_MAX_SUBSTEPS_PER_LOOP;
        }
        while (accumulator >= fixed_timestep)
        {
            if (henka_physics_world_step_fixed(physics) != HENKA_SUCCESS)
            {
                fprintf(stderr, "headless simulation step failed\n");
                exit_code = 9;
                g_henka_server_running = 0;
                break;
            }
            accumulator -= fixed_timestep;
        }
    }
    henka_server_disconnect_peers(network, peers, &peer_count);

shutdown:
    henka_network_server_destroy(network);
    henka_physics_world_destroy(physics);
    henka_scene_destroy(scene);
    return exit_code;
}
