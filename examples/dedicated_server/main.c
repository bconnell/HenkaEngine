#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <henka/runtime.h>

typedef struct henka_server_options
{
    const char* bind_address;
    unsigned short port;
    unsigned int max_clients;
    unsigned int tick_rate;
    const char* world_path;
    const char* save_root;
    const char* config_path;
    int smoke;
} henka_server_options;

static void henka_server_print_usage(void)
{
    puts("henka_dedicated_server [options]");
    puts("  --bind ADDRESS       Bind address (default 0.0.0.0)");
    puts("  --port PORT          Port 1-65535 (default 7777)");
    puts("  --max-clients COUNT  Client limit 1-256 (default 32)");
    puts("  --tick-rate HZ       Fixed simulation rate 1-240 (default 60)");
    puts("  --world PATH         Base world path");
    puts("  --save-root PATH     Runtime save root");
    puts("  --config PATH        Optional server configuration path");
    puts("  --smoke              Initialize and shut down after one headless tick");
    puts("  --help               Show this help");
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

static int henka_server_parse_options(int argc, char** argv, henka_server_options* out_options)
{
    int index;

    if (out_options == NULL)
    {
        return 0;
    }
    *out_options = (henka_server_options){"0.0.0.0", 7777U, 32U, 60U, NULL, NULL, NULL, 0};
    for (index = 1; index < argc; ++index)
    {
        const char* argument = argv[index];
        if (strcmp(argument, "--help") == 0)
        {
            henka_server_print_usage();
            return 0;
        }
        if (strcmp(argument, "--smoke") == 0)
        {
            out_options->smoke = 1;
            continue;
        }
        if (index + 1 >= argc)
        {
            fprintf(stderr, "missing value for %s\n", argument);
            return 0;
        }
        if (strcmp(argument, "--bind") == 0)
        {
            out_options->bind_address = argv[++index];
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
            out_options->world_path = argv[++index];
        }
        else if (strcmp(argument, "--save-root") == 0)
        {
            out_options->save_root = argv[++index];
        }
        else if (strcmp(argument, "--config") == 0)
        {
            out_options->config_path = argv[++index];
        }
        else
        {
            fprintf(stderr, "unknown option: %s\n", argument);
            return 0;
        }
    }
    return 1;
}

int main(int argc, char** argv)
{
    henka_server_options options;
    henka_scene* scene = NULL;
    henka_physics_world* physics = NULL;

    if (!henka_server_parse_options(argc, argv, &options))
    {
        return argc > 1 && strcmp(argv[1], "--help") == 0 ? 0 : 2;
    }
    if (henka_scene_create(&scene) != HENKA_SUCCESS ||
        henka_physics_world_create(&physics) != HENKA_SUCCESS)
    {
        henka_physics_world_destroy(physics);
        henka_scene_destroy(scene);
        fprintf(stderr, "headless runtime initialization failed\n");
        return 3;
    }
    if (henka_physics_world_step_fixed(physics) != HENKA_SUCCESS)
    {
        henka_physics_world_destroy(physics);
        henka_scene_destroy(scene);
        fprintf(stderr, "headless simulation step failed\n");
        return 4;
    }

    printf("Henka dedicated server foundation: %s:%u, max clients %u, tick rate %u Hz\n",
        options.bind_address,
        (unsigned int)options.port,
        options.max_clients,
        options.tick_rate);
    if (options.world_path != NULL)
    {
        printf("world: %s\n", options.world_path);
    }
    if (options.save_root != NULL)
    {
        printf("save root: %s\n", options.save_root);
    }
    if (options.config_path != NULL)
    {
        printf("config: %s\n", options.config_path);
    }

    henka_physics_world_destroy(physics);
    henka_scene_destroy(scene);
    return options.smoke ? 0 : 0;
}
