#include "test_suite.h"

#include "../examples/sandbox3d/editor_automation_frame_capture.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#endif

void henka_test_sandbox3d_automation_frame_capture(void)
{
    char path[160];
    char short_path[8];
#if defined(_WIN32)
    const char* directory = "C:\\Henka\\capture";
    const char* expected = "C:\\Henka\\capture\\frame-0001-editor-ui.bmp";
    const char* relative_directory = "Henka\\capture";
#else
    const char* directory = "/tmp/henka/capture";
    const char* expected = "/tmp/henka/capture/frame-0001-editor-ui.bmp";
    const char* relative_directory = "Henka/capture";
#endif
    char oversized_label[50];

    HENKA_TEST_ASSERT(sandbox3d_automation_frame_capture_label_is_valid("editor-ui"));
    HENKA_TEST_ASSERT(sandbox3d_automation_frame_capture_label_is_valid("scale_200"));
    HENKA_TEST_ASSERT(!sandbox3d_automation_frame_capture_label_is_valid(""));
    HENKA_TEST_ASSERT(!sandbox3d_automation_frame_capture_label_is_valid("two words"));
    HENKA_TEST_ASSERT(!sandbox3d_automation_frame_capture_label_is_valid("../capture"));
    memset(oversized_label, 'a', sizeof(oversized_label));
    oversized_label[sizeof(oversized_label) - 1U] = '\0';
    HENKA_TEST_ASSERT(!sandbox3d_automation_frame_capture_label_is_valid(oversized_label));

    HENKA_TEST_ASSERT(sandbox3d_automation_frame_capture_format_path(
        path, sizeof(path), directory, 1U, "editor-ui"));
    HENKA_TEST_ASSERT(strcmp(path, expected) == 0);
    HENKA_TEST_ASSERT(!sandbox3d_automation_frame_capture_format_path(
        path, sizeof(path), relative_directory, 1U, "editor-ui"));
    HENKA_TEST_ASSERT(!sandbox3d_automation_frame_capture_format_path(
        path, sizeof(path), directory, 0U, "editor-ui"));
    HENKA_TEST_ASSERT(!sandbox3d_automation_frame_capture_format_path(
        path, sizeof(path), directory, 129U, "editor-ui"));
    HENKA_TEST_ASSERT(!sandbox3d_automation_frame_capture_format_path(
        short_path, sizeof(short_path), directory, 1U, "editor-ui"));
    HENKA_TEST_ASSERT(short_path[0] == '\0');

#if defined(_WIN32)
    {
        char current_directory[MAX_PATH];
        char fixture_directory[MAX_PATH * 3U] = { 0 };
        char output_directory[MAX_PATH * 3U];
        char request_path[MAX_PATH * 3U] = { 0 };
        char fixture_name[80];
        char* previous_output_directory = NULL;
        char* previous_request_path = NULL;
        size_t previous_output_size = 0U;
        size_t previous_request_size = 0U;
        sandbox3d_automation_frame_capture* capture = NULL;
        HANDLE request_lock = INVALID_HANDLE_VALUE;
        FILE* request_file = NULL;
        DWORD process_id = GetCurrentProcessId();
        DWORD tick_count = GetTickCount();
        bool fixture_created = false;
        bool setup_succeeded = false;
        bool request_preserved_while_locked = false;
        bool request_consumed_after_retry = false;

        if (GetCurrentDirectoryA(
                (DWORD)sizeof(current_directory),
                current_directory) == 0U)
        {
            goto frame_capture_retry_cleanup;
        }
        {
            const int fixture_name_length = snprintf(
                fixture_name,
                sizeof(fixture_name),
                "capture-retry-%lu-%lu",
                (unsigned long)process_id,
                (unsigned long)tick_count);
            const int fixture_directory_length = snprintf(
                fixture_directory,
                sizeof(fixture_directory),
                "%s\\test_tmp\\%s",
                current_directory,
                fixture_name);
            const int output_directory_length = snprintf(
                output_directory,
                sizeof(output_directory),
                "%s\\frames",
                fixture_directory);
            const int request_path_length = snprintf(
                request_path,
                sizeof(request_path),
                "%s\\request.txt",
                fixture_directory);
            if (fixture_name_length < 0 ||
                (size_t)fixture_name_length >= sizeof(fixture_name) ||
                fixture_directory_length < 0 ||
                (size_t)fixture_directory_length >= sizeof(fixture_directory) ||
                output_directory_length < 0 ||
                (size_t)output_directory_length >= sizeof(output_directory) ||
                request_path_length < 0 ||
                (size_t)request_path_length >= sizeof(request_path))
            {
                goto frame_capture_retry_cleanup;
            }
        }
        if (!CreateDirectoryA(fixture_directory, NULL))
        {
            goto frame_capture_retry_cleanup;
        }
        fixture_created = true;

        (void)_dupenv_s(
            &previous_output_directory,
            &previous_output_size,
            "HENKA_AUTOMATION_CAPTURE_OUTPUT_DIRECTORY");
        (void)_dupenv_s(
            &previous_request_path,
            &previous_request_size,
            "HENKA_AUTOMATION_CAPTURE_REQUEST_FILE");
        if (_putenv_s(
                "HENKA_AUTOMATION_CAPTURE_OUTPUT_DIRECTORY",
                output_directory) != 0 ||
            _putenv_s(
                "HENKA_AUTOMATION_CAPTURE_REQUEST_FILE",
                request_path) != 0 ||
            sandbox3d_automation_frame_capture_create(&capture) != HENKA_SUCCESS ||
            capture == NULL)
        {
            goto frame_capture_retry_cleanup;
        }

        if (fopen_s(&request_file, request_path, "wb") != 0 ||
            request_file == NULL)
        {
            goto frame_capture_retry_cleanup;
        }
        if (fputs("retry-after-lock\n", request_file) < 0)
        {
            goto frame_capture_retry_cleanup;
        }
        if (fclose(request_file) != 0)
        {
            request_file = NULL;
            goto frame_capture_retry_cleanup;
        }
        request_file = NULL;

        request_lock = CreateFileA(
            request_path,
            GENERIC_READ,
            0U,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL);
        if (request_lock == INVALID_HANDLE_VALUE)
        {
            goto frame_capture_retry_cleanup;
        }

        setup_succeeded = true;
        sandbox3d_automation_frame_capture_update(capture, NULL);
        request_preserved_while_locked =
            GetFileAttributesA(request_path) != INVALID_FILE_ATTRIBUTES;
        (void)CloseHandle(request_lock);
        request_lock = INVALID_HANDLE_VALUE;
        sandbox3d_automation_frame_capture_update(capture, NULL);
        request_consumed_after_retry =
            GetFileAttributesA(request_path) == INVALID_FILE_ATTRIBUTES;

    frame_capture_retry_cleanup:
        if (request_file != NULL)
        {
            (void)fclose(request_file);
        }
        if (request_lock != INVALID_HANDLE_VALUE)
        {
            (void)CloseHandle(request_lock);
        }
        if (fixture_created)
        {
            (void)DeleteFileA(request_path);
            (void)RemoveDirectoryA(fixture_directory);
        }
        sandbox3d_automation_frame_capture_destroy(capture);
        (void)_putenv_s(
            "HENKA_AUTOMATION_CAPTURE_OUTPUT_DIRECTORY",
            previous_output_directory != NULL ? previous_output_directory : "");
        (void)_putenv_s(
            "HENKA_AUTOMATION_CAPTURE_REQUEST_FILE",
            previous_request_path != NULL ? previous_request_path : "");
        free(previous_output_directory);
        free(previous_request_path);

        HENKA_TEST_ASSERT(setup_succeeded);
        HENKA_TEST_ASSERT(request_preserved_while_locked);
        HENKA_TEST_ASSERT(request_consumed_after_retry);
    }
#endif
}
