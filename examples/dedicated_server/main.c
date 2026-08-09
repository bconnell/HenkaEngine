#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <henka/runtime.h>

#include <henka/terrain_network.h>

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

static int henka_server_run_loopback_smoke(
    henka_network_server* network,
    henka_terrain_server* terrain_server,
    const henka_terrain_world_desc* world_desc,
    henka_terrain_world* terrain_world,
    henka_terrain_collision_runtime* collision_runtime,
    unsigned short port,
    uint64_t* out_revision)
{
    henka_network_client_desc client_desc = henka_network_client_desc_default();
    henka_network_client* client = NULL;
    henka_network_event server_event;
    henka_network_event client_event;
    henka_terrain_region_state region_state;
    uint8_t ping_payload = 0x5AU;
    uint8_t edit_payload[HENKA_TERRAIN_NETWORK_MAX_EDIT_REQUEST_BYTES];
    size_t edit_payload_size = 0U;
    uint32_t iteration;
    int connected = 0;
    int echoed = 0;
    int accepted = 0;

    if (network == NULL || terrain_server == NULL || world_desc == NULL ||
        terrain_world == NULL || out_revision == NULL)
    {
        return 0;
    }
    (void)henka_server_copy_text(
        client_desc.remote_endpoint.host,
        sizeof(client_desc.remote_endpoint.host),
        "127.0.0.1");
    client_desc.remote_endpoint.port = port;
    if (henka_network_client_create(&client_desc, &client) != HENKA_SUCCESS)
    {
        return 0;
    }
    for (iteration = 0U; iteration < 200U && !connected; ++iteration)
    {
        if (henka_network_server_poll(network, 2U, &server_event) != HENKA_SUCCESS ||
            henka_network_client_poll(client, 2U, &client_event) != HENKA_SUCCESS)
        {
            henka_network_client_destroy(client);
            return 0;
        }
        if (collision_runtime != NULL)
        {
            (void)henka_terrain_collision_runtime_pump(collision_runtime, 8U);
        }
        if (server_event.type == HENKA_NETWORK_EVENT_MESSAGE &&
            henka_terrain_server_handle_event(terrain_server, &server_event, iteration) != HENKA_SUCCESS)
        {
            henka_network_client_destroy(client);
            return 0;
        }
        if (collision_runtime != NULL)
        {
            (void)henka_terrain_collision_runtime_pump(collision_runtime, 8U);
        }
        if (client_event.type == HENKA_NETWORK_EVENT_CONNECTED)
        {
            connected = 1;
        }
    }
    if (!connected ||
        henka_network_client_send(
            client, HENKA_NETWORK_CHANNEL_CONTROL, HENKA_NETWORK_MESSAGE_PING,
            &ping_payload, sizeof(ping_payload)) != HENKA_SUCCESS)
    {
        henka_network_client_destroy(client);
        return 0;
    }
    for (iteration = 0U; iteration < 200U && !echoed; ++iteration)
    {
        if (henka_network_server_poll(network, 2U, &server_event) != HENKA_SUCCESS ||
            henka_network_client_poll(client, 2U, &client_event) != HENKA_SUCCESS)
        {
            henka_network_client_destroy(client);
            return 0;
        }
        if (server_event.type == HENKA_NETWORK_EVENT_MESSAGE &&
            henka_terrain_server_handle_event(terrain_server, &server_event, 200U + iteration) != HENKA_SUCCESS)
        {
            henka_network_client_destroy(client);
            return 0;
        }
        if (client_event.type == HENKA_NETWORK_EVENT_MESSAGE &&
            client_event.message.type == HENKA_NETWORK_MESSAGE_PING &&
            client_event.message.payload_size == sizeof(ping_payload) &&
            client_event.message.payload[0] == ping_payload)
        {
            echoed = 1;
        }
    }
    if (!echoed ||
        henka_terrain_world_get_region_state(
            terrain_world, (henka_terrain_region_id){0, 0}, &region_state) != HENKA_SUCCESS)
    {
        henka_network_client_destroy(client);
        return 0;
    }
    *out_revision = region_state.revision;
    if (region_state.revision == 0U)
    {
        henka_terrain_edit_request request = {0};
        henka_terrain_edit_acceptance acceptance;
        request.world_identity = world_desc->world_identity;
        request.base_asset_identity = world_desc->base_asset_identity;
        request.client_nonce = UINT64_C(0x534D4F4B45);
        request.command = henka_terrain_edit_command_default();
        request.command.center_sample_x = 100;
        request.command.center_sample_z = 100;
        request.command.radius_samples = 4U;
        request.command.value_millimeters = 10;
        request.affected_region_count = 1U;
        request.affected_regions[0] = (henka_terrain_network_region_revision){{0, 0}, 0U};
        if (henka_terrain_edit_request_encode(
                &request, edit_payload, sizeof(edit_payload), &edit_payload_size) != HENKA_SUCCESS ||
            henka_network_client_send(
                client, HENKA_NETWORK_CHANNEL_TERRAIN,
                HENKA_NETWORK_MESSAGE_TERRAIN_EDIT_REQUEST,
                edit_payload, edit_payload_size) != HENKA_SUCCESS)
        {
            henka_network_client_destroy(client);
            return 0;
        }
        for (iteration = 0U; iteration < 400U && !accepted; ++iteration)
        {
            if (henka_network_server_poll(network, 2U, &server_event) != HENKA_SUCCESS ||
                henka_network_client_poll(client, 2U, &client_event) != HENKA_SUCCESS)
            {
                henka_network_client_destroy(client);
                return 0;
            }
            if (collision_runtime != NULL)
            {
                (void)henka_terrain_collision_runtime_pump(collision_runtime, 16U);
            }
            if (server_event.type == HENKA_NETWORK_EVENT_MESSAGE &&
                henka_terrain_server_handle_event(terrain_server, &server_event, 400U + iteration) != HENKA_SUCCESS)
            {
                henka_network_client_destroy(client);
                return 0;
            }
            if (collision_runtime != NULL)
            {
                (void)henka_terrain_collision_runtime_pump(collision_runtime, 16U);
            }
            if (client_event.type == HENKA_NETWORK_EVENT_MESSAGE &&
                client_event.message.type == HENKA_NETWORK_MESSAGE_TERRAIN_EDIT_ACCEPTED &&
                henka_terrain_edit_acceptance_decode(
                    client_event.message.payload, client_event.message.payload_size, &acceptance) == HENKA_SUCCESS &&
                acceptance.client_nonce == request.client_nonce &&
                acceptance.affected_regions[0].revision == 1U)
            {
                accepted = 1;
                *out_revision = acceptance.affected_regions[0].revision;
            }
        }
    }
    henka_network_client_destroy(client);
    return accepted || *out_revision > 0U;
}

int main(int argc, char** argv)
{
    henka_server_options options;
    henka_scene* scene = NULL;
    henka_physics_world* physics = NULL;
    henka_terrain_world* terrain_world = NULL;
    henka_terrain_storage* terrain_storage = NULL;
    henka_terrain_server* terrain_server = NULL;
    henka_terrain_streamer* terrain_streamer = NULL;
    henka_terrain_physics* terrain_physics = NULL;
    henka_terrain_collision_runtime* terrain_collision_runtime = NULL;
    henka_terrain_world_desc terrain_world_desc;
    henka_terrain_server_desc terrain_server_desc;
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
    uint64_t smoke_revision = 0U;

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
    terrain_world_desc = henka_terrain_world_desc_default();
    terrain_world_desc.max_resident_regions = options.max_clients;
    if (henka_terrain_world_create(&terrain_world_desc, &terrain_world) != HENKA_SUCCESS ||
        henka_terrain_world_reserve_region(terrain_world, (henka_terrain_region_id){0, 0}) != HENKA_SUCCESS ||
        henka_terrain_world_reserve_chunk(terrain_world, (henka_terrain_chunk_id){0, 0}) != HENKA_SUCCESS ||
        henka_terrain_world_set_region_residency(
            terrain_world, (henka_terrain_region_id){0, 0}, true, false, false) != HENKA_SUCCESS ||
        henka_terrain_storage_create(
            &terrain_world_desc,
            options.has_save_root ? options.save_root : "build/server-save",
            &terrain_storage) != HENKA_SUCCESS ||
        henka_terrain_storage_recover(terrain_storage) != HENKA_SUCCESS)
    {
        fprintf(stderr, "terrain runtime initialization failed\n");
        exit_code = 4;
        goto shutdown;
    }
    {
        henka_terrain_layout layout;
        henka_terrain_sample* samples = NULL;
        henka_terrain_region_storage_info info;
        if (henka_terrain_world_desc_get_layout(&terrain_world_desc, &layout) != HENKA_SUCCESS)
        {
            exit_code = 4;
            goto shutdown;
        }
        samples = henka_calloc(layout.samples_per_region, sizeof(*samples));
        if (samples == NULL)
        {
            exit_code = 4;
            goto shutdown;
        }
        if (henka_terrain_storage_load_region(
                terrain_storage, (henka_terrain_region_id){0, 0}, &info,
                samples, layout.samples_per_region) == HENKA_SUCCESS &&
            henka_terrain_world_apply_region_snapshot(
                terrain_world, info, samples, layout.samples_per_region) != HENKA_SUCCESS)
        {
            henka_free(samples);
            exit_code = 4;
            goto shutdown;
        }
        henka_free(samples);
    }
    {
        henka_terrain_physics_desc terrain_physics_desc = henka_terrain_physics_desc_default();
        henka_terrain_collision_runtime_desc collision_desc = henka_terrain_collision_runtime_desc_default();
        if (henka_terrain_physics_create(&terrain_physics_desc, &terrain_physics) != HENKA_SUCCESS ||
            henka_terrain_collision_runtime_create(
                terrain_world, terrain_physics, &collision_desc,
                &terrain_collision_runtime) != HENKA_SUCCESS ||
            henka_terrain_collision_runtime_request_chunk(
                terrain_collision_runtime, (henka_terrain_chunk_id){0, 0}) != HENKA_SUCCESS ||
            henka_terrain_collision_runtime_pump(terrain_collision_runtime, 1U) != HENKA_SUCCESS)
        {
            fprintf(stderr, "terrain collision initialization failed\n");
            exit_code = 4;
            goto shutdown;
        }
    }
#if defined(_WIN32)
    {
        henka_terrain_stream_desc stream_desc = henka_terrain_stream_desc_default();
        henka_terrain_stream_observer observer = {1U, {0, 0}, 1U, 1U, 0U, 2U};
        if (henka_terrain_streamer_create(
                terrain_world, terrain_storage, &stream_desc, &terrain_streamer) != HENKA_SUCCESS ||
            henka_terrain_streamer_add_observer(terrain_streamer, &observer) != HENKA_SUCCESS)
        {
            fprintf(stderr, "terrain streaming initialization failed\n");
            exit_code = 4;
            goto shutdown;
        }
    }
#endif
    fixed_timestep = 1.0 / (double)options.tick_rate;
    if (henka_physics_world_set_fixed_timestep(physics, (float)fixed_timestep) != HENKA_SUCCESS)
    {
        fprintf(stderr, "fixed timestep configuration failed\n");
        exit_code = 5;
        goto shutdown;
    }
    network_desc = henka_network_server_desc_default();
    if (!henka_server_copy_text(
            network_desc.bind_endpoint.host,
            sizeof(network_desc.bind_endpoint.host),
            options.bind_address))
    {
        fprintf(stderr, "bind address exceeds the bounded endpoint limit\n");
        exit_code = 6;
        goto shutdown;
    }
    network_desc.bind_endpoint.port = options.port;
    network_desc.max_clients = options.max_clients;
    if (henka_network_server_create(&network_desc, &network) != HENKA_SUCCESS)
    {
        fprintf(stderr, "network server initialization failed\n");
        exit_code = 7;
        goto shutdown;
    }
    terrain_server_desc = henka_terrain_server_desc_default();
    terrain_server_desc.network = network;
    terrain_server_desc.world = terrain_world;
    terrain_server_desc.storage = terrain_storage;
    terrain_server_desc.collision_runtime = terrain_collision_runtime;
    terrain_server_desc.max_clients = options.max_clients;
    if (henka_terrain_server_create(&terrain_server_desc, &terrain_server) != HENKA_SUCCESS)
    {
        fprintf(stderr, "terrain server initialization failed\n");
        exit_code = 8;
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
            exit_code = 9;
        }
        if (terrain_streamer != NULL)
        {
            (void)henka_terrain_streamer_pump(terrain_streamer, 4U);
        }
        (void)henka_terrain_collision_runtime_pump(terrain_collision_runtime, 4U);
        if (exit_code == 0 &&
            !henka_server_run_loopback_smoke(
                network, terrain_server, &terrain_world_desc, terrain_world,
                terrain_collision_runtime, options.port, &smoke_revision))
        {
            fprintf(stderr, "dedicated server loopback smoke failed\n");
            exit_code = 12;
        }
        if (exit_code == 0)
        {
            printf("Dedicated server smoke initialized; loopback client connected; terrain revision %llu.\n",
                (unsigned long long)smoke_revision);
        }
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
            if (henka_terrain_server_handle_event(
                    terrain_server, &event,
                    (uint64_t)(time_state.total_seconds * 1000.0)) != HENKA_SUCCESS)
            {
                fprintf(stderr, "terrain network dispatch failed\n");
                exit_code = 10;
                g_henka_server_running = 0;
            }
        }
        henka_time_tick(&time_state);
        if (terrain_streamer != NULL)
        {
            (void)henka_terrain_streamer_pump(terrain_streamer, 4U);
        }
        (void)henka_terrain_collision_runtime_pump(terrain_collision_runtime, 4U);
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
                exit_code = 11;
                g_henka_server_running = 0;
                break;
            }
            accumulator -= fixed_timestep;
        }
    }
    henka_server_disconnect_peers(network, peers, &peer_count);

shutdown:
    henka_terrain_server_destroy(terrain_server);
    henka_terrain_collision_runtime_destroy(terrain_collision_runtime);
    henka_terrain_streamer_destroy(terrain_streamer);
    henka_terrain_physics_destroy(terrain_physics);
    henka_network_server_destroy(network);
    henka_terrain_storage_destroy(terrain_storage);
    henka_terrain_world_destroy(terrain_world);
    henka_physics_world_destroy(physics);
    henka_scene_destroy(scene);
    return exit_code;
}
