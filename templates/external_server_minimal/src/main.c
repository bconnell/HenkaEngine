#include <stdio.h>

#include <henka/runtime.h>

int main(void)
{
    henka_network_server_desc desc = henka_network_server_desc_default();
    henka_network_server* server = NULL;

    desc.bind_endpoint.port = 37992U;
    if (henka_network_server_create(&desc, &server) != HENKA_SUCCESS)
    {
        fprintf(stderr, "External server template could not initialize Henka networking.\n");
        return 1;
    }
    henka_network_server_destroy(server);
    printf("External server template initialized.\n");
    printf("Henka runtime and network ownership remain inside this C17 consumer.\n");
    return 0;
}
