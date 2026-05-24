#include "test_suite.h"

int g_henka_test_failures = 0;

int main(void)
{
    henka_test_camera();
    henka_test_math();
    henka_test_material();
    henka_test_result();
    henka_test_memory();
    henka_test_scene();

    if (g_henka_test_failures > 0)
    {
        fprintf(stderr, "%d test(s) failed\n", g_henka_test_failures);
        return 1;
    }

    printf("all tests passed\n");
    return 0;
}
