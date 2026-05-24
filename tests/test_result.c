#include "test_suite.h"

#include <string.h>

#include <henka/engine.h>
#include <henka/result.h>

void henka_test_result(void)
{
    henka_engine* engine;
    henka_engine_config invalid_config;

    HENKA_TEST_ASSERT(HENKA_SUCCESS == 0);
    HENKA_TEST_ASSERT(strcmp(henka_result_to_string(HENKA_SUCCESS), "success") == 0);
    HENKA_TEST_ASSERT(strcmp(henka_result_to_string(HENKA_ERROR_INVALID_ARGUMENT), "invalid argument") == 0);
    HENKA_TEST_ASSERT(strcmp(henka_result_to_string(HENKA_ERROR_OUT_OF_MEMORY), "out of memory") == 0);
    HENKA_TEST_ASSERT(strcmp(henka_result_to_string(HENKA_ERROR_PLATFORM), "platform error") == 0);
    HENKA_TEST_ASSERT(strcmp(henka_result_to_string(HENKA_ERROR_RENDERER), "renderer error") == 0);
    HENKA_TEST_ASSERT(strcmp(henka_result_to_string(HENKA_ERROR_UNKNOWN), "unknown error") == 0);

    invalid_config.application_name = NULL;
    invalid_config.window_width = 640;
    invalid_config.window_height = 480;
    invalid_config.enable_vsync = true;
    invalid_config.asset_base_path = NULL;
    invalid_config.user_data_base_path = NULL;
    invalid_config.on_initialize = NULL;
    invalid_config.on_update = NULL;
    invalid_config.on_shutdown = NULL;
    invalid_config.user_data = NULL;

    HENKA_TEST_ASSERT(henka_engine_create(NULL, &engine) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_engine_create(&invalid_config, &engine) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_engine_create(&invalid_config, NULL) == HENKA_ERROR_INVALID_ARGUMENT);
}
